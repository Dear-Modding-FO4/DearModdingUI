#include "FakeData.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <DearModdingUI/Client.h>
#include <DearModdingUI/MCM/ActionExecutor.h>
#include <DearModdingUI/MCM/Availability.h>
#include <DearModdingUI/MCM/DiagnosticReporter.h>
#include <DearModdingUI/MCM/FileChoices.h>
#include <DearModdingUI/MCM/GlobalValue.h>
#include <DearModdingUI/MCM/Keybinds.h>
#include <DearModdingUI/MCM/SettingsIni.h>
#include <DearModdingUI/MCM/TextRendering.h>
#include <DearModdingUI/MCM/ValueSource.h>
#include <DearModdingUI/MCM/Win32FileListingAdapter.h>

#include <GeneralTestSuite.h>

#include <d3d11.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace DearModdingUIPreview
{
	namespace
	{
		struct PageSpec
		{
			const char* id;
			const char* displayName;
			const char* categoryId;
			const char* categoryDisplayName;
			const char* summary;
		};

		enum class ClientConnection
		{
			kLockstep,
			kForwarding
		};

		class PreviewEnvironment final : public DmuiTests::Environment
		{
		public:
			void SetDevice(ID3D11Device* a_device) noexcept
			{
				m_device = a_device;
			}

			[[nodiscard]] Microsoft::WRL::ComPtr<ID3D11Device>
				AcquireRendererDevice() noexcept override
			{
				Microsoft::WRL::ComPtr<ID3D11Device> result;
				if (m_device)
					result = m_device;
				return result;
			}

			[[nodiscard]] bool SupportsGameInputContexts() const noexcept override
			{
				return false;
			}

			void Log(
				DmuiTests::LogLevel a_level,
				std::string_view a_message) noexcept override
			{
				const char* level = a_level == DmuiTests::LogLevel::kError ?
					"error" : a_level == DmuiTests::LogLevel::kWarning ?
					"warning" : "info";
				std::fprintf(stderr, "[%s] %.*s\n", level,
					static_cast<int>(a_message.size()), a_message.data());
			}

		private:
			ID3D11Device* m_device{};
		};

		constexpr std::string_view kMcmConfig = R"json({
			"modName": "DmuiSyntheticMCM",
			"displayName": "[Fixture] Synthetic MCM Bridge",
			"content": [
				{"id":"bridge","type":"section","text":"MCM Bridge"},
				{"id":"BridgeDescription","type":"text",
				 "text":"This long MCM text control demonstrates that explanatory prose wraps cleanly instead of being replaced or clipped at the value-column boundary."},
				{"id":"MarkupDescription","type":"text","html":true,
				 "text":"<i>Italic source</i> and <font size='30'>large source</font><br /><p align='center'>Centered markup paragraph</p>"},
				{"id":"LiteralDescription","type":"text","html":false,
				 "text":"Literal angle brackets survive: <Press E>"},
				{"id":"AlignedDescription","type":"text","align":"center",
				 "text":"Control-level centered text"},
				{"id":"DisplaySlot","type":"dropdown","text":"Display slot",
				 "valueOptions":{"sourceType":"GlobalValue",
				 "sourceForm":"DmuiSyntheticMCM.esp|800","default":0,
				 "options":["59 (Utility) slot","60 (Animation) slot","61 (FX) slot"]}},
				{"id":"QuantizedScale","type":"slider","text":"Quantized scale",
				 "help":"Moves in 0.2 increments on MCM's zero-anchored grid.",
				 "valueOptions":{"sourceType":"GlobalValue",
				 "sourceForm":"DmuiSyntheticMCM.esp|802","default":0.5,
				 "min":0.1,"max":0.9,"step":0.2,"format":"%.1f"}},
				{"id":"sPreviewFile:Files","type":"dropdownFiles",
				 "text":"File preset","help":"Refreshed when this page is activated.",
				 "valueOptions":{"sourceType":"ModSettingString",
				 "path":"DMUI_PREVIEW_FILES","mask":"*.xml"}},
				{"id":"divider","type":"section","text":""},
				{"id":"FeatureEnabled","type":"switcher","text":"Enable feature",
				 "valueOptions":{"sourceType":"GlobalValue",
				 "sourceForm":"DmuiSyntheticMCM.esp|801","default":false}}
			]
		})json";

		class PreviewValueSource final :
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
				if (const auto overridden =
						m_overrides.find(a_binding.descriptorId);
					overridden != m_overrides.end())
					return DearModdingUI::MCM::ReadyValue{
						overridden->second,
						m_generation
					};
				const auto value = m_values.find(a_binding.descriptorId);
				if (value == m_values.end())
					return DearModdingUI::MCM::ReadyValue{
						a_binding.target,
						m_generation
					};
				auto converted = DearModdingUI::MCM::GlobalToSettingValue(
					value->second,
					a_binding.target);
				return converted ?
					DearModdingUI::MCM::ValueSnapshot{
						DearModdingUI::MCM::ReadyValue{
							std::move(*converted),
							m_generation
						} } :
					DearModdingUI::MCM::ValueSnapshot{
						DearModdingUI::MCM::FailedValue{ m_generation } };
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
				m_overrides.insert_or_assign(a_binding.descriptorId, a_value);
				++m_generation;
				return DearModdingUI::MCM::ReadyValue{
					a_value,
					m_generation
				};
			}

			void Seed(std::string a_id, float a_value)
			{
				m_values.emplace(std::move(a_id), a_value);
			}

			void Seed(std::string a_id, dmui::SettingValue a_value)
			{
				m_overrides.insert_or_assign(
					std::move(a_id),
					std::move(a_value));
			}

		private:
			std::unordered_map<std::string, float> m_values;
			std::unordered_map<std::string, dmui::SettingValue> m_overrides;
			uint64_t m_generation{};
		};

		class PreviewActionExecutor final :
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
					return std::nullopt;
				if (std::holds_alternative<
						DearModdingUI::MCM::CallExternalFunctionAction>(a_action))
					return "This Scaleform action is unavailable in the preview.";
				return "This action is not supported in the preview.";
			}

			void Execute(
				DearModdingUI::MCM::ActionInvocation,
				DearModdingUI::MCM::ActionCompletion a_completion) override
			{
				a_completion({
					DearModdingUI::MCM::ActionExecutionStatus::kSucceeded,
					{}
				});
			}
		};

		class PreviewDiagnosticReporter final :
			public DearModdingUI::MCM::DiagnosticReporter
		{
		public:
			void Report(
				DearModdingUI::MCM::Diagnostic a_diagnostic) noexcept override
			{
				diagnostics.push_back(std::move(a_diagnostic));
			}

			std::vector<DearModdingUI::MCM::Diagnostic> diagnostics;
		};

		class BuiltinFileListingAdapter final :
			public DearModdingUI::MCM::FileListingAdapter
		{
		public:
			[[nodiscard]] DearModdingUI::MCM::FileListingResult List(
				std::string_view,
				std::string_view) override
			{
				return std::vector<std::string>{
					"HUD Classic.xml",
					"None",
					"Wide Screen.xml"
				};
			}
		};

		[[nodiscard]] bool AddPages(
			dmui::Client& a_client,
			std::span<const PageSpec> a_pages,
			std::string& a_error)
		{
			std::vector<std::string_view> categories;
			for (const auto& page : a_pages)
			{
				if (!page.categoryId)
					continue;
				const auto existing = std::ranges::find(
					categories,
					std::string_view{ page.categoryId });
				if (existing != categories.end())
					continue;
				if (!a_client.AddCategory({
						.id = page.categoryId,
						.displayName = page.categoryDisplayName
					}))
				{
					a_error = "Could not register category " +
						std::string{ page.categoryId } + " (result " +
						std::to_string(a_client.LastResult()) + ").";
					return false;
				}
				categories.push_back(page.categoryId);
			}
			int32_t sortKey = 0;
			for (const auto& page : a_pages)
			{
				const auto handle = a_client.AddPage(
					{
						.id = page.id,
						.displayName = page.displayName,
						.categoryId = page.categoryId,
						.summary = page.summary,
						.sortKey = sortKey
					},
					[client = &a_client,
					 name = std::string{ page.displayName },
						summary = std::string{ page.summary }]() {
						(void)client->DrawSectionHeader(name.c_str());
						(void)client->DrawBulletText(summary.c_str());
						(void)client->DrawBulletText(
							"Dedicated preview-only navigation layout fixture.");
					});
				if (!handle)
				{
					a_error = "Could not register page " +
						std::string{ page.id } + " (result " +
						std::to_string(a_client.LastResult()) + ").";
					return false;
				}
				sortKey += 10;
			}
			return true;
		}
	}

	struct FakeData::Impl
	{
		struct McmPageRuntime
		{
			DMUI_PageHandle handle{ DMUI_INVALID_PAGE_HANDLE };
			DearModdingUI::MCM::FileChoiceController fileChoices;
		};

		PreviewValueSource mcmValues;
		PreviewActionExecutor mcmActions;
		PreviewDiagnosticReporter mcmDiagnostics;
		BuiltinFileListingAdapter builtinMcmFiles;
		DearModdingUI::MCM::Win32FileListingAdapter realMcmFiles;
		std::vector<McmPageRuntime> mcmPages;
		std::vector<std::unique_ptr<dmui::Client>> clients;
		PreviewEnvironment testEnvironment;
		DmuiTests::GeneralTestSuite testSuite{ testEnvironment };

		[[nodiscard]] dmui::Client* AddClient(
			std::string_view a_id,
			std::string_view a_displayName,
			dmui::Version a_version,
			std::string_view a_iconName,
			std::string& a_error,
			ClientConnection a_connection = ClientConnection::kLockstep,
			dmui::ClientOrigin a_origin = {})
		{
			auto client = a_connection == ClientConnection::kForwarding ?
				std::make_unique<dmui::Client>(
					a_id,
					a_displayName,
					a_version,
					dmui::kForwardingClient,
					a_iconName,
					a_origin) :
				std::make_unique<dmui::Client>(
					a_id,
					a_displayName,
					a_version,
					a_iconName,
					a_origin);
			if (!client->Connect())
			{
				a_error = "Could not connect fake client " +
					std::string{ a_id } + " (result " +
					DMUI_ResultToString(client->LastResult()) + ").";
				return nullptr;
			}
			auto* result = client.get();
			clients.push_back(std::move(client));
			return result;
		}
	};

	FakeData::FakeData() :
		m_impl(std::make_unique<Impl>())
	{}

	FakeData::~FakeData() = default;

	void FakeData::Stop() noexcept
	{
		m_impl->testSuite.Stop();
	}

	bool FakeData::Register(
		ID3D11Device* a_device,
		std::string& a_error,
		bool a_includeNavigationComparisonFixtures) noexcept
	{
		try
		{
			a_error.clear();
			m_impl->testEnvironment.SetDevice(a_device);
			if (!m_impl->testSuite.Initialize())
			{
				a_error = "Could not register the shared DMUI test fixture.";
				return false;
			}

			const auto* configOverride =
				std::getenv("DMUI_PREVIEW_MCM_CONFIG");
			const auto environmentFlag = [](const char* a_name, bool a_default) {
				const auto* value = std::getenv(a_name);
				return value ? std::string_view{ value } != "0" : a_default;
			};
			const DearModdingUI::MCM::McmState mcmState{
				environmentFlag("DMUI_PREVIEW_MCM_INSTALLED", true),
				environmentFlag("DMUI_PREVIEW_GAME_LOADED", true)
			};
			auto mcm = configOverride ?
				DearModdingUI::MCM::LoadConfig(
					std::filesystem::path{ configOverride }) :
				DearModdingUI::MCM::ParseConfig(
					kMcmConfig,
					"preview-mcm-config.json");
			if (mcm.pages.empty())
			{
				a_error = "Could not parse the MCM preview fixture.";
				return false;
			}
			if (configOverride)
			{
				const auto configPath = std::filesystem::path{ configOverride };
				const auto declarations =
					DearModdingUI::MCM::LoadSettingsIni(
						configPath.parent_path() / "settings.ini");
				const auto definitions =
					DearModdingUI::MCM::LoadKeybindDefinitions(
						configPath.parent_path() / "keybinds.json");
				const auto keybinds =
					DearModdingUI::MCM::LoadUserKeybinds(
						std::filesystem::current_path() /
						"Data" / "MCM" / "Settings" / "Keybinds.json");
				for (auto& page : mcm.pages)
				{
					DearModdingUI::MCM::ApplyDeclarations(page, declarations);
					DearModdingUI::MCM::ApplyKeybinds(
						page,
						definitions,
						keybinds,
						m_impl->mcmDiagnostics);
				}
			}
			auto& mcmFiles = configOverride ?
				static_cast<DearModdingUI::MCM::FileListingAdapter&>(
					m_impl->realMcmFiles) :
				static_cast<DearModdingUI::MCM::FileListingAdapter&>(
					m_impl->builtinMcmFiles);
			m_impl->mcmValues.Seed("DisplaySlot", 2.0f);
			m_impl->mcmValues.Seed("QuantizedScale", 0.7f);
			m_impl->mcmValues.Seed("FeatureEnabled", 1.0f);
			m_impl->mcmValues.Seed(
				"sPreviewFile:Files",
				dmui::SettingValue{ std::string{ "HUD Classic.xml" } });
			m_impl->mcmValues.Seed("bDisplayCondition:Misc", 1.0f);
			m_impl->mcmValues.Seed("bDisplayConditionInvert:Misc", 1.0f);
			auto* mcmClient = m_impl->AddClient(
				"dearmodding.tests.synthetic.mcm",
				"[Fixture] Synthetic MCM Bridge",
				{ 1, 0 },
				"plugs-connected",
				a_error,
				ClientConnection::kForwarding,
				{
					dmui::ClientOriginKind::kBridged,
					"MCM"
				});
			if (!mcmClient)
			{
				a_error = "Could not register the MCM preview fixture.";
				return false;
			}
			for (size_t index = 0; index < 17; ++index)
			{
				if (!mcmClient->ReportDiagnostic({
						DMUI_STATUS_SEVERITY_WARNING,
						"preview-mcm-config.json",
						"Expected a boolean value.",
						"Several html fields contain string values."
					}))
				{
					a_error = "Could not register repeated fake diagnostics.";
					return false;
				}
			}
			if (!mcmClient->ReportDiagnostic({
					DMUI_STATUS_SEVERITY_ERROR,
					"General",
					"ModSetting source requires a setting id.",
					"The affected control cannot read or write its value."
				}))
			{
				a_error = "Could not register the fake diagnostic error.";
				return false;
			}
			for (auto& mcmPage : mcm.pages)
			{
				auto fileChoices =
					DearModdingUI::MCM::AttachFileChoices(
						mcmPage,
						mcmFiles,
						m_impl->mcmDiagnostics,
						configOverride ?
							std::string{ configOverride } :
							"preview-mcm-config.json");
				DearModdingUI::MCM::BindPage(
					mcmPage,
					m_impl->mcmValues,
					[mcmState] { return mcmState; });
				DearModdingUI::MCM::BindActions(
					mcmPage,
					m_impl->mcmActions,
					m_impl->mcmValues,
					m_impl->mcmDiagnostics);
				DearModdingUI::MCM::AttachTextRendering(mcmPage);
				const auto registered = mcmClient->AddSettingsPage(
						{
							.id = mcmPage.id.c_str(),
							.displayName = mcmPage.displayName.c_str(),
							.summary =
								"Parsed and bound MCM compatibility controls."
						},
						std::move(mcmPage.settings));
				if (!registered)
				{
					a_error = "Could not register the MCM preview fixture.";
					return false;
				}
				m_impl->mcmPages.push_back({
					*registered,
					std::move(fileChoices)
				});
			}
			auto* implementation = m_impl.get();
			if (!mcmClient->AddPageActivityObserver(
					[implementation](const dmui::PageActivity& a_activity) {
						if (a_activity.kind !=
							dmui::PageActivityKind::kActivated)
							return;
						const auto page = std::ranges::find(
							implementation->mcmPages,
							a_activity.activePage,
							&Impl::McmPageRuntime::handle);
						if (page != implementation->mcmPages.end())
							page->fileChoices.Refresh();
					}))
			{
				a_error =
					"Could not register MCM page activity observation (result " +
					std::string{
						DMUI_ResultToString(mcmClient->LastResult())
					} + ").";
				return false;
			}

			if (a_includeNavigationComparisonFixtures)
			{
				static constexpr std::array navigationPages{
					PageSpec{
						"overview",
						"Overview",
						nullptr,
						nullptr,
						"Synthetic navigation-only bridge fixture."
					},
					PageSpec{
						"tuning",
						"Tuning",
						"configuration",
						"Configuration",
						"Generic bridged configuration controls."
					},
					PageSpec{
						"diagnostics",
						"Diagnostics",
						"diagnostics",
						"Diagnostics",
						"A category intentionally matching its page name."
					}
				};
				struct NavigationFixtureClient
				{
					const char* id;
					const char* displayName;
					const char* source;
				};
				static constexpr std::array navigationClients{
					NavigationFixtureClient{
						"dearmodding.tests.synthetic.navigation.unnamed-a",
						"[Fixture] Navigation Unnamed Alpha",
						""
					},
					NavigationFixtureClient{
						"dearmodding.tests.synthetic.navigation.unnamed-b",
						"[Fixture] Navigation Unnamed Beta",
						""
					},
					NavigationFixtureClient{
						"dearmodding.tests.synthetic.navigation.long-a",
						"[Fixture] Navigation Long Source Alpha",
						"A Very Long Navigation Bridge Source Label for Layout Stress"
					},
					NavigationFixtureClient{
						"dearmodding.tests.synthetic.navigation.long-b",
						"[Fixture] Navigation Long Source Beta",
						"A Very Long Navigation Bridge Source Label for Layout Stress"
					},
					NavigationFixtureClient{
						"dearmodding.tests.synthetic.navigation.long-c",
						"[Fixture] Navigation Long Source Gamma",
						"A Very Long Navigation Bridge Source Label for Layout Stress"
					},
					NavigationFixtureClient{
						"dearmodding.tests.synthetic.navigation.long-d",
						"[Fixture] Navigation Long Source Delta",
						"A Very Long Navigation Bridge Source Label for Layout Stress"
					},
					NavigationFixtureClient{
						"dearmodding.tests.synthetic.navigation.script-a",
						"[Fixture] Navigation Script Alpha",
						"Synthetic Configuration Bridge"
					},
					NavigationFixtureClient{
						"dearmodding.tests.synthetic.navigation.script-b",
						"[Fixture] Navigation Script Beta",
						"Synthetic Configuration Bridge"
					},
					NavigationFixtureClient{
						"dearmodding.tests.synthetic.navigation.script-c",
						"[Fixture] Navigation Script Gamma",
						"Synthetic Configuration Bridge"
					},
					NavigationFixtureClient{
						"dearmodding.tests.synthetic.navigation.script-d",
						"[Fixture] Navigation Script Delta",
						"Synthetic Configuration Bridge"
					},
					NavigationFixtureClient{
						"dearmodding.tests.synthetic.navigation.script-e",
						"[Fixture] Navigation Script Epsilon",
						"Synthetic Configuration Bridge"
					}
				};
				for (const auto& fixture : navigationClients)
				{
					auto* client = m_impl->AddClient(
						fixture.id,
						fixture.displayName,
						{ 0, 1 },
						"share-network",
						a_error,
						ClientConnection::kLockstep,
						{
							dmui::ClientOriginKind::kBridged,
							fixture.source
						});
					if (!client ||
						!AddPages(*client, navigationPages, a_error))
						return false;
				}
			}
			return true;
		}
		catch (const std::exception& a_exception)
		{
			a_error = "Preview fixture registration threw: ";
			a_error += a_exception.what();
			return false;
		}
	}
}
