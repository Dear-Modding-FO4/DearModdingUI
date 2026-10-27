#include <DearModdingUI/MCM/FileChoices.h>

#include <DearModdingUI/MCM/DiagnosticReporter.h>
#include <DearModdingUI/MCM/PageLookup.h>
#include <DearModdingUI/MCM/TaskScheduler.h>

#include <algorithm>
#include <atomic>
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

		[[nodiscard]] Support::Message FailureDescription(
			std::string_view a_path,
			std::string_view a_mask,
			const Support::Message& a_error)
		{
			if (a_error.empty())
				return Support::Message{ BridgeText::kFileListFailed, a_path, a_mask };
			return Support::Message{
				BridgeText::kFileListFailedDetail,
				a_path,
				a_mask,
				a_error
			};
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
		explicit Impl(std::string a_noneLabel) :
			noneLabel(std::move(a_noneLabel)),
			options{ { "", noneLabel } }
		{}

		mutable std::mutex mutex;
		// Rendered once at attach time, so scans never touch the catalog.
		const std::string noneLabel;
		Availability availability{ Availability::kPending };
		std::vector<dmui::ChoiceSettingOption> options;
		Support::Message description;
		uint64_t generation{};
	};

	FileChoiceState::FileChoiceState(const Support::TextCatalog& a_catalog) :
		impl_(std::make_unique<Impl>(
			BridgeText::Render(BridgeText::kNone, a_catalog)))
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

	Support::Message FileChoiceState::Description() const
	{
		const std::scoped_lock lock{ impl_->mutex };
		return impl_->description;
	}

	bool FileChoiceState::Update(
		FileScanResult a_listing,
		std::string_view a_path,
		std::string_view a_mask)
	{
		std::vector<dmui::ChoiceSettingOption> options{
			{ "", impl_->noneLabel }
		};
		auto availability = Availability::kFailed;
		Support::Message description;
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

		void Scan() noexcept;
		void Drain() noexcept;

		FileListingAdapter* files{};
		TaskScheduler* scheduler{};
		DiagnosticReporter* diagnostics{};
		std::string source;
		std::vector<Entry> entries;
		std::atomic<bool> running{ false };
		std::atomic<bool> rerun{ false };
	};

	FileChoiceController::FileChoiceController(std::shared_ptr<Impl> a_impl) :
		impl_(std::move(a_impl))
	{}

	FileChoiceController::operator bool() const noexcept
	{
		return impl_ && !impl_->entries.empty();
	}

	void FileChoiceController::Impl::Scan() noexcept
	{
		for (auto& entry : entries)
		{
			FileScanResult listing = std::unexpected(
				Support::Message{ BridgeText::kFilePathMissing });
			if (entry.metadata.path && !entry.metadata.path->empty())
			{
				try
				{
					auto listed = files->List(
						*entry.metadata.path,
						entry.metadata.mask);
					if (listed)
						listing = std::move(*listed);
					else
						listing = std::unexpected(
							Support::Message::Text(std::move(listed.error())));
				}
				catch (const std::exception& a_error)
				{
					listing = std::unexpected(
						Support::Message::Text(a_error.what()));
				}
			}
			const auto failed = !listing;
			const auto error = failed ? listing.error() : Support::Message{};
			if (entry.state->Update(
					std::move(listing),
					entry.metadata.path.value_or(std::string{}),
					entry.metadata.mask) &&
				failed)
			{
				diagnostics->ReportTransient({
					DiagnosticSeverity::kWarning,
					source,
					entry.metadata.location,
					FailureDescription(
						entry.metadata.path.value_or(std::string{}),
						entry.metadata.mask,
						error).English()
				});
			}
		}
	}

	void FileChoiceController::Impl::Drain() noexcept
	{
		// A request that arrives mid-scan must observe the filesystem after it.
		do
		{
			rerun.store(false, std::memory_order_release);
			Scan();
			running.store(false, std::memory_order_release);
		} while (rerun.load(std::memory_order_acquire) &&
			!running.exchange(true, std::memory_order_acq_rel));
	}

	void FileChoiceController::Refresh() noexcept
	{
		if (!impl_ || impl_->entries.empty())
			return;
		impl_->rerun.store(true, std::memory_order_release);
		if (impl_->running.exchange(true, std::memory_order_acq_rel))
			return;
		try
		{
			impl_->scheduler->ScheduleBackground(
				[impl = impl_] { impl->Drain(); });
		}
		catch (const std::exception& a_error)
		{
			impl_->running.store(false, std::memory_order_release);
			impl_->diagnostics->ReportTransient({
				DiagnosticSeverity::kWarning,
				impl_->source,
				{},
				"File choices could not be refreshed: " +
					std::string{ a_error.what() }
			});
		}
	}

	FileChoiceController AttachFileChoices(
		MappedPage& a_page,
		FileListingAdapter& a_files,
		TaskScheduler& a_scheduler,
		DiagnosticReporter& a_diagnostics,
		std::string a_source,
		const Support::TextCatalog& a_catalog)
	{
		auto impl = std::make_shared<FileChoiceController::Impl>();
		impl->files = &a_files;
		impl->scheduler = &a_scheduler;
		impl->diagnostics = &a_diagnostics;
		impl->source = std::move(a_source);
		for (auto& row : a_page.rows)
		{
			if (!row.fileChoices)
				continue;
			row.fileChoiceState = std::make_shared<FileChoiceState>(a_catalog);
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
					if (auto* descriptor = FindSettingDescriptor(
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
