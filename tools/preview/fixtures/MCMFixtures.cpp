#include "MCMFixtures.h"

#include <DearModdingUI/Client.h>
#include <DearModdingUI/MCM/ActionExecutor.h>
#include <DearModdingUI/MCM/DiagnosticReporter.h>
#include <DearModdingUI/MCM/FileChoices.h>
#include <DearModdingUI/MCM/Keybinds.h>
#include <DearModdingUI/MCM/SettingsIni.h>
#include <DearModdingUI/MCM/TextRendering.h>
#include <DearModdingUI/MCM/ValueSource.h>
#include <DearModdingUI/MCM/Win32FileListingAdapter.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <new>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace DmuiTestFixtures
{
	namespace dmui = ::dmui;

	namespace
	{
		[[nodiscard]] bool IsDataComponent(
			const std::filesystem::path& a_component)
		{
			const auto text = a_component.u8string();
			return text.size() == 4 &&
				std::ranges::equal(
					text,
					std::string_view{ "Data" },
					[](char8_t a_left, char a_right) {
						return std::tolower(
							static_cast<unsigned char>(a_left)) ==
							std::tolower(
								static_cast<unsigned char>(a_right));
					});
		}

		class FixtureFileListingAdapter final :
			public DearModdingUI::MCM::FileListingAdapter
		{
		public:
			explicit FixtureFileListingAdapter(
				std::filesystem::path a_dataRoot) :
				m_dataRoot(std::move(a_dataRoot))
			{}

			[[nodiscard]] DearModdingUI::MCM::FileListingResult List(
				std::string_view a_path,
				std::string_view a_mask) override
			{
				auto path = std::filesystem::path{
					std::u8string{ a_path.begin(), a_path.end() }
				};
				if (path.is_relative())
				{
					auto component = path.begin();
					if (component != path.end() &&
						IsDataComponent(*component))
					{
						auto resolved = m_dataRoot;
						for (++component; component != path.end(); ++component)
							resolved /= *component;
						path = std::move(resolved);
					}
				}
				const auto encoded = path.u8string();
				return m_files.List(
					std::string{ encoded.begin(), encoded.end() }, a_mask);
			}

		private:
			std::filesystem::path m_dataRoot;
			DearModdingUI::MCM::Win32FileListingAdapter m_files;
		};

		class FixtureValueSource final :
			public DearModdingUI::MCM::ValueSource
		{
		public:
			[[nodiscard]] bool Supports(
				DearModdingUI::MCM::SourceFamily a_family) const noexcept override
			{
				return a_family != DearModdingUI::MCM::SourceFamily::kUnknown;
			}

			[[nodiscard]] DearModdingUI::MCM::ValueSnapshot Read(
				const DearModdingUI::MCM::MappedBinding& a_binding) const override
			{
				const auto value = m_values.find(a_binding.cacheKey);
				if (value == m_values.end())
					return DearModdingUI::MCM::MissingValue{ m_generation };
				return DearModdingUI::MCM::ReadyValue{
					value->second,
					m_generation
				};
			}

			[[nodiscard]] uint64_t Refresh(
				const DearModdingUI::MCM::MappedBinding&) override
			{
				return ++m_generation;
			}

			[[nodiscard]] DearModdingUI::MCM::ValueSnapshot Write(
				const DearModdingUI::MCM::MappedBinding& a_binding,
				const dmui::SettingValue& a_value) override
			{
				m_values.insert_or_assign(a_binding.cacheKey, a_value);
				++m_generation;
				return DearModdingUI::MCM::ReadyValue{
					a_value,
					m_generation
				};
			}

			void Seed(const DearModdingUI::MCM::MappedPage& a_page)
			{
				for (const auto& row : a_page.rows)
				{
					if (row.binding)
						m_values.try_emplace(
							row.binding->cacheKey,
							row.binding->target);
				}
			}

		private:
			std::unordered_map<std::string, dmui::SettingValue> m_values;
			uint64_t m_generation{};
		};

		class FixtureActionExecutor final :
			public DearModdingUI::MCM::ActionExecutor
		{
		public:
			[[nodiscard]] std::optional<std::string> UnsupportedReason(
				const DearModdingUI::MCM::Action& a_action) const noexcept override
			{
				if (std::holds_alternative<
						DearModdingUI::MCM::CallFunctionAction>(a_action) ||
					std::holds_alternative<
						DearModdingUI::MCM::CallGlobalFunctionAction>(a_action))
					return "Papyrus actions are unavailable in the desktop preview.";
				if (std::holds_alternative<
						DearModdingUI::MCM::CallExternalFunctionAction>(a_action))
					return "External game actions are unavailable in the desktop preview.";
				return "This game action is not supported by the desktop preview.";
			}

			void Execute(
				DearModdingUI::MCM::ActionInvocation,
				DearModdingUI::MCM::ActionCompletion a_completion) override
			{
				a_completion({
					DearModdingUI::MCM::ActionExecutionStatus::kUnsupported,
					"The desktop preview does not execute game actions."
				});
			}
		};

		class FixtureDiagnosticReporter final :
			public DearModdingUI::MCM::DiagnosticReporter
		{
		public:
			void Report(
				DearModdingUI::MCM::Diagnostic a_diagnostic) noexcept override
			{
				try
				{
					diagnostics.push_back(std::move(a_diagnostic));
					if (client)
						Publish(diagnostics.back());
				}
				catch (const std::bad_alloc&)
				{
					Fail(DMUI_RESULT_RESOURCE_EXHAUSTED);
				}
				catch (...)
				{
					Fail(DMUI_RESULT_CALLBACK_FAILED);
				}
			}

			[[nodiscard]] bool Attach(dmui::Client& a_client) noexcept
			{
				client = &a_client;
				for (const auto& diagnostic : diagnostics)
					Publish(diagnostic);
				return publishResult == DMUI_RESULT_OK;
			}

			[[nodiscard]] DMUI_Result PublishResult() const noexcept
			{
				return publishResult;
			}

			std::vector<DearModdingUI::MCM::Diagnostic> diagnostics;

		private:
			void Fail(DMUI_Result a_result) noexcept
			{
				if (publishResult == DMUI_RESULT_OK)
					std::fprintf(
						stderr,
						"dmui-preview: MCM diagnostic publication failed: %s\n",
						DMUI_ResultToString(a_result));
				publishResult = a_result;
			}

			void Publish(
				const DearModdingUI::MCM::Diagnostic& a_diagnostic) noexcept
			{
				if (publishResult != DMUI_RESULT_OK ||
					client->ReportDiagnostic({
						a_diagnostic.severity ==
								DearModdingUI::MCM::DiagnosticSeverity::kWarning ?
							DMUI_STATUS_SEVERITY_WARNING :
							DMUI_STATUS_SEVERITY_ERROR,
						a_diagnostic.source.empty() ?
							nullptr :
							a_diagnostic.source.c_str(),
						a_diagnostic.message.c_str(),
						a_diagnostic.location.empty() ?
							nullptr :
							a_diagnostic.location.c_str()
					}))
					return;
				Fail(client->LastResult());
			}

			dmui::Client* client{};
			DMUI_Result publishResult{ DMUI_RESULT_OK };
		};
	}

	struct McmFixture::Impl
	{
		struct PageRuntime
		{
			DMUI_PageHandle handle{ DMUI_INVALID_PAGE_HANDLE };
			DearModdingUI::MCM::FileChoiceController fileChoices;
		};

		FixtureValueSource values;
		FixtureActionExecutor actions;
		FixtureDiagnosticReporter diagnostics;
		std::unique_ptr<FixtureFileListingAdapter> files;
		std::vector<PageRuntime> pages;
		std::unique_ptr<dmui::Client> client;
	};

	McmFixture::McmFixture() :
		m_impl(std::make_unique<Impl>())
	{}

	McmFixture::~McmFixture() = default;

	bool McmFixture::Register(
		const McmFixtureOptions& a_options,
		std::string& a_error) noexcept
	{
		try
		{
			a_error.clear();
			if (m_impl->client)
			{
				a_error = "The preview MCM fixture was already registered.";
				return false;
			}
			if (a_options.configPath.empty())
			{
				a_error = "The preview MCM configuration path is empty.";
				return false;
			}
			if (a_options.dataRoot.empty())
			{
				a_error = "The preview data root is empty.";
				return false;
			}

			auto mcm = DearModdingUI::MCM::LoadConfig(a_options.configPath);
			for (auto& diagnostic : mcm.diagnostics)
				m_impl->diagnostics.Report(std::move(diagnostic));
			if (!mcm.configuration || mcm.pages.empty())
			{
				a_error = "Could not load preview MCM configuration '" +
					a_options.configPath.string() + "'";
				if (!m_impl->diagnostics.diagnostics.empty())
				{
					const auto& diagnostic =
						m_impl->diagnostics.diagnostics.front();
					a_error += ": ";
					a_error += diagnostic.message;
					if (!diagnostic.location.empty())
					{
						a_error += " (";
						a_error += diagnostic.location;
						a_error += ")";
					}
				}
				a_error += ".";
				return false;
			}

			const auto declarations =
				DearModdingUI::MCM::LoadSettingsIni(
					a_options.configPath.parent_path() / "settings.ini");
			const auto definitions =
				DearModdingUI::MCM::LoadKeybindDefinitions(
					a_options.configPath.parent_path() / "keybinds.json");
			const auto keybinds =
				DearModdingUI::MCM::LoadUserKeybinds(
					a_options.userKeybindsPath);
			for (auto& page : mcm.pages)
			{
				DearModdingUI::MCM::ApplyDeclarations(page, declarations);
				DearModdingUI::MCM::ApplyKeybinds(
					page,
					definitions,
					keybinds,
					m_impl->diagnostics);
				m_impl->values.Seed(page);
			}

			const auto& configuration = *mcm.configuration;
			auto displayName = mcm.displayName;
			if (displayName.empty())
				displayName = configuration.displayName;
			if (displayName.empty())
				displayName = configuration.modName;
			if (displayName.empty())
			{
				a_error = "Preview MCM configuration '" +
					a_options.configPath.string() +
					"' does not declare a displayName or modName.";
				return false;
			}

			m_impl->files =
				std::make_unique<FixtureFileListingAdapter>(
					a_options.dataRoot);
			m_impl->client = std::make_unique<dmui::Client>(
				"dearmodding.tests.mcm",
				displayName,
				dmui::Version{ 1, 0 },
				"plugs-connected",
				dmui::ClientOrigin{
					dmui::ClientOriginKind::kBridged,
					"MCM"
				});
			auto& client = *m_impl->client;
			if (!client.Connect())
			{
				a_error = "Could not connect preview MCM client '" +
					displayName + "' (result " +
					std::string{ DMUI_ResultToString(client.LastResult()) } + ").";
				return false;
			}
			if (!m_impl->diagnostics.Attach(client))
			{
				a_error = "Could not publish MCM diagnostics (result ";
				a_error += DMUI_ResultToString(
					m_impl->diagnostics.PublishResult());
				a_error += ").";
				return false;
			}

			const auto source = a_options.configPath.string();
			for (auto& page : mcm.pages)
			{
				auto fileChoices = DearModdingUI::MCM::AttachFileChoices(
					page,
					*m_impl->files,
					m_impl->diagnostics,
					source);
				DearModdingUI::MCM::BindPage(
					page,
					m_impl->values,
					[state = a_options.state] { return state; });
				DearModdingUI::MCM::ResolveActionAvailability(
					page,
					m_impl->actions);
				DearModdingUI::MCM::BindActions(
					page,
					m_impl->actions,
					m_impl->values,
					m_impl->diagnostics);
				if (m_impl->diagnostics.PublishResult() != DMUI_RESULT_OK)
				{
					a_error = "Could not publish MCM binding diagnostic (result ";
					a_error += DMUI_ResultToString(
						m_impl->diagnostics.PublishResult());
					a_error += ").";
					return false;
				}
				DearModdingUI::MCM::AttachTextRendering(page);
				page.settings.notes.push_back({
					"Desktop preview values start from parsed defaults and "
					"remain in memory; no Papyrus, global, or MCM storage "
					"operation is executed.",
					true
				});
				const auto registered = client.AddSettingsPage(
					{
						.id = page.id.c_str(),
						.displayName = page.displayName.c_str(),
						.summary =
							"Canonical MCM fixture rendered with preview-simulated values.",
						.sortKey = static_cast<int32_t>(m_impl->pages.size())
					},
					std::move(page.settings));
				if (!registered)
				{
					a_error = "Could not register preview MCM page '" +
						page.id + "' (result " +
						std::string{
							DMUI_ResultToString(client.LastResult())
						} + ").";
					return false;
				}
				m_impl->pages.push_back({
					*registered,
					std::move(fileChoices)
				});
			}

			auto* implementation = m_impl.get();
			if (!client.AddPageActivityObserver(
					[implementation](const dmui::PageActivity& a_activity) {
						if (a_activity.kind !=
							dmui::PageActivityKind::kActivated)
							return;
						const auto page = std::ranges::find(
							implementation->pages,
							a_activity.activePage,
							&Impl::PageRuntime::handle);
						if (page != implementation->pages.end())
							page->fileChoices.Refresh();
					}))
			{
				a_error =
					"Could not register MCM page activity observation (result " +
					std::string{ DMUI_ResultToString(client.LastResult()) } + ").";
				return false;
			}
			return true;
		}
		catch (const std::exception& a_exception)
		{
			a_error = "Preview MCM fixture registration threw: ";
			a_error += a_exception.what();
			return false;
		}
	}

	size_t McmFixture::PageCount() const noexcept
	{
		return m_impl->pages.size();
	}

	size_t McmFixture::DiagnosticCount() const noexcept
	{
		return m_impl->diagnostics.diagnostics.size();
	}
}
