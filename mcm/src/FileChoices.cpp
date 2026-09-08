#include <DearModdingUI/MCM/FileChoices.h>

#include <DearModdingUI/MCM/DiagnosticReporter.h>

#include <algorithm>
#include <exception>
#include <limits>
#include <mutex>
#include <utility>

namespace DearModdingUI::MCM
{
	namespace
	{
		enum class Availability : uint8_t
		{
			kPending,
			kReady,
			kFailed
		};

		[[nodiscard]] dmui::SettingDescriptor* FindDescriptor(
			dmui::SettingsPage& a_page,
			std::string_view a_id)
		{
			for (auto& group : a_page.groups)
			{
				const auto setting = std::ranges::find(
					group.settings,
					a_id,
					&dmui::SettingDescriptor::id);
				if (setting != group.settings.end())
					return &*setting;
			}
			return nullptr;
		}

		[[nodiscard]] std::string FailureDescription(
			std::string_view a_path,
			std::string_view a_mask,
			std::string_view a_error)
		{
			auto result = "Could not list files from path '" +
				std::string{ a_path } + "' with mask '" +
				std::string{ a_mask } + "'";
			if (!a_error.empty())
				result += ": " + std::string{ a_error };
			result.push_back('.');
			return result;
		}

		[[nodiscard]] bool SameOptions(
			const std::vector<dmui::ChoiceSettingOption>& a_left,
			const std::vector<dmui::ChoiceSettingOption>& a_right)
		{
			return a_left.size() == a_right.size() &&
				std::ranges::equal(
					a_left,
					a_right,
					{},
					&dmui::ChoiceSettingOption::value,
					&dmui::ChoiceSettingOption::value) &&
				std::ranges::equal(
					a_left,
					a_right,
					{},
					&dmui::ChoiceSettingOption::label,
					&dmui::ChoiceSettingOption::label);
		}
	}

	class FileChoiceState::Impl
	{
	public:
		mutable std::mutex mutex;
		Availability availability{ Availability::kPending };
		std::vector<dmui::ChoiceSettingOption> options{
			{ "", "None" }
		};
		std::string description;
		uint64_t generation{};
	};

	FileChoiceState::FileChoiceState() :
		impl_(std::make_unique<Impl>())
	{}

	FileChoiceState::~FileChoiceState() = default;

	InertReason FileChoiceState::Reason() const noexcept
	{
		const std::scoped_lock lock{ impl_->mutex };
		if (impl_->availability == Availability::kPending)
			return InertReason::kFileChoicesPending;
		if (impl_->availability == Availability::kFailed)
			return InertReason::kFileChoicesFailed;
		return InertReason::kNone;
	}

	std::string FileChoiceState::Description() const
	{
		const std::scoped_lock lock{ impl_->mutex };
		return impl_->description;
	}

	bool FileChoiceState::Update(
		FileListingResult a_listing,
		std::string_view a_path,
		std::string_view a_mask)
	{
		std::vector<dmui::ChoiceSettingOption> options{
			{ "", "None" }
		};
		auto availability = Availability::kFailed;
		std::string description;
		if (a_listing)
		{
			availability = Availability::kReady;
			options.reserve(a_listing->size() + 1);
			for (auto& name : *a_listing)
				options.push_back({ name, std::move(name) });
		}
		else
		{
			description = FailureDescription(
				a_path,
				a_mask,
				a_listing.error());
		}

		const std::scoped_lock lock{ impl_->mutex };
		if (impl_->availability == availability &&
			SameOptions(impl_->options, options) &&
			impl_->description == description)
			return false;
		impl_->availability = availability;
		impl_->options = std::move(options);
		impl_->description = std::move(description);
		++impl_->generation;
		return true;
	}

	void FileChoiceState::Apply(
		dmui::SettingDescriptor& a_descriptor,
		uint64_t& a_appliedGeneration) const
	{
		const std::scoped_lock lock{ impl_->mutex };
		if (a_appliedGeneration == impl_->generation)
			return;
		if (auto* control =
				std::get_if<dmui::ChoiceSettingControl>(&a_descriptor.control))
			control->options = impl_->options;
		a_appliedGeneration = impl_->generation;
	}

	class FileChoiceController::Impl
	{
	public:
		struct Entry
		{
			std::string descriptorId;
			FileChoiceMetadata metadata;
			std::shared_ptr<FileChoiceState> state;
			uint64_t appliedGeneration{
				(std::numeric_limits<uint64_t>::max)()
			};
		};

		FileListingAdapter* files{};
		DiagnosticReporter* diagnostics{};
		std::string source;
		std::vector<Entry> entries;
	};

	FileChoiceController::FileChoiceController(std::shared_ptr<Impl> a_impl) :
		impl_(std::move(a_impl))
	{}

	FileChoiceController::operator bool() const noexcept
	{
		return impl_ && !impl_->entries.empty();
	}

	void FileChoiceController::Refresh() noexcept
	{
		if (!impl_)
			return;
		for (auto& entry : impl_->entries)
		{
			FileListingResult listing = std::unexpected(
				"the configured path is missing or empty");
			if (entry.metadata.path && !entry.metadata.path->empty())
			{
				try
				{
					listing = impl_->files->List(
						*entry.metadata.path,
						entry.metadata.mask);
				}
				catch (const std::exception& a_error)
				{
					listing =
						std::unexpected(std::string{ a_error.what() });
				}
			}
			const auto failed = !listing;
			const auto error = failed ? listing.error() : std::string{};
			if (entry.state->Update(
					std::move(listing),
					entry.metadata.path.value_or(std::string{}),
					entry.metadata.mask) &&
				failed)
			{
				impl_->diagnostics->ReportTransient({
					DiagnosticSeverity::kWarning,
					impl_->source,
					entry.metadata.location,
					FailureDescription(
						entry.metadata.path.value_or(std::string{}),
						entry.metadata.mask,
						error)
				});
			}
		}
	}

	FileChoiceController AttachFileChoices(
		MappedPage& a_page,
		FileListingAdapter& a_files,
		DiagnosticReporter& a_diagnostics,
		std::string a_source)
	{
		auto impl = std::make_shared<FileChoiceController::Impl>();
		impl->files = &a_files;
		impl->diagnostics = &a_diagnostics;
		impl->source = std::move(a_source);
		for (auto& row : a_page.rows)
		{
			if (!row.fileChoices)
				continue;
			row.fileChoiceState = std::make_shared<FileChoiceState>();
			impl->entries.push_back({
				row.id,
				*row.fileChoices,
				row.fileChoiceState
			});
		}

		auto priorPrepare = std::move(a_page.settings.prepareView);
		a_page.settings.prepareView =
			[priorPrepare = std::move(priorPrepare), impl](
				dmui::SettingsPage& a_settings) mutable {
				if (priorPrepare)
					priorPrepare(a_settings);
				for (auto& entry : impl->entries)
				{
					if (auto* descriptor = FindDescriptor(
							a_settings,
							entry.descriptorId))
					{
						entry.state->Apply(
							*descriptor,
							entry.appliedGeneration);
					}
				}
			};
		return FileChoiceController{ std::move(impl) };
	}
}
