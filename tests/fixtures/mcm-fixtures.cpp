#include "mcm-fixtures.h"

#include <DearModdingUI/Client.h>
#include <DearModdingUI/MCM/ActionExecutor.h>
#include <DearModdingUI/MCM/DiagnosticReporter.h>
#include <DearModdingUI/MCM/GlobalValue.h>
#include <DearModdingUI/MCM/Keybinds.h>
#include <DearModdingUI/MCM/SettingsIni.h>
#include <DearModdingUI/MCM/TextRendering.h>
#include <DearModdingUI/MCM/ValueSource.h>

#include <algorithm>
#include <exception>
#include <variant>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace DmuiTestFixtures
{
	namespace dmui = ::dmui;

	namespace
	{
		constexpr std::string_view kBuiltinMcmConfig = R"json({
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
					return "Papyrus actions are unavailable in the synthetic fixture.";
				if (std::holds_alternative<
						DearModdingUI::MCM::CallExternalFunctionAction>(a_action))
					return "This Scaleform action is unavailable in the synthetic fixture.";
				return "This action is not supported by the synthetic fixture.";
			}

			void Execute(
				DearModdingUI::MCM::ActionInvocation,
				DearModdingUI::MCM::ActionCompletion a_completion) override
			{
				a_completion({
					DearModdingUI::MCM::ActionExecutionStatus::kUnsupported,
					"The synthetic fixture does not execute game actions."
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
				diagnostics.push_back(std::move(a_diagnostic));
			}

			std::vector<DearModdingUI::MCM::Diagnostic> diagnostics;
		};
	}

	DearModdingUI::MCM::FileListingResult
		BuiltinMcmFileListingAdapter::List(
			std::string_view,
			std::string_view)
	{
		return std::vector<std::string>{
			"HUD Classic.xml",
			"None",
			"Wide Screen.xml"
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
		BuiltinMcmFileListingAdapter builtinFiles;
		std::vector<PageRuntime> pages;
		std::unique_ptr<dmui::Client> client;

		void SeedValues()
		{
			values.Seed("DisplaySlot", 2.0f);
			values.Seed("QuantizedScale", 0.7f);
			values.Seed("FeatureEnabled", 1.0f);
			values.Seed(
				"sPreviewFile:Files",
				dmui::SettingValue{ std::string{ "HUD Classic.xml" } });
			values.Seed("bDisplayCondition:Misc", 1.0f);
			values.Seed("bDisplayConditionInvert:Misc", 1.0f);
		}
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
				a_error = "The synthetic MCM fixture was already registered.";
				return false;
			}
			if (a_options.configPath && !a_options.fileListing)
			{
				a_error =
					"An external MCM fixture requires an injected file listing adapter.";
				return false;
			}

			auto mcm = a_options.configPath ?
				DearModdingUI::MCM::LoadConfig(*a_options.configPath) :
				DearModdingUI::MCM::ParseConfig(
					kBuiltinMcmConfig,
					"preview-mcm-config.json");
			if (mcm.pages.empty())
			{
				a_error = "Could not parse the synthetic MCM fixture.";
				return false;
			}

			if (a_options.configPath)
			{
				const auto& configPath = *a_options.configPath;
				const auto declarations =
					DearModdingUI::MCM::LoadSettingsIni(
						configPath.parent_path() / "settings.ini");
				const auto definitions =
					DearModdingUI::MCM::LoadKeybindDefinitions(
						configPath.parent_path() / "keybinds.json");
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
				}
			}

			m_impl->SeedValues();
			m_impl->client = std::make_unique<dmui::Client>(
				"dearmodding.tests.synthetic.mcm",
				"[Fixture] Synthetic MCM Bridge",
				dmui::Version{ 1, 0 },
				"plugs-connected",
				dmui::ClientOrigin{
					dmui::ClientOriginKind::kBridged,
					"MCM"
				});
			auto& client = *m_impl->client;
			if (!client.Connect())
			{
				a_error = "Could not connect the synthetic MCM fixture (result " +
					std::string{ DMUI_ResultToString(client.LastResult()) } + ").";
				return false;
			}

			for (size_t index = 0; index < 17; ++index)
			{
				if (!client.ReportDiagnostic({
						DMUI_STATUS_SEVERITY_WARNING,
						"preview-mcm-config.json",
						"Expected a boolean value.",
						"Several html fields contain string values."
					}))
				{
					a_error = "Could not register repeated synthetic diagnostics.";
					return false;
				}
			}
			if (!client.ReportDiagnostic({
					DMUI_STATUS_SEVERITY_ERROR,
					"General",
					"ModSetting source requires a setting id.",
					"The affected control cannot read or write its value."
				}))
			{
				a_error = "Could not register the synthetic diagnostic error.";
				return false;
			}

			auto& files = a_options.fileListing ?
				*a_options.fileListing :
				static_cast<DearModdingUI::MCM::FileListingAdapter&>(
					m_impl->builtinFiles);
			const auto source = a_options.configPath ?
				a_options.configPath->string() :
				std::string{ "preview-mcm-config.json" };
			for (auto& page : mcm.pages)
			{
				auto fileChoices = DearModdingUI::MCM::AttachFileChoices(
					page,
					files,
					m_impl->diagnostics,
					source);
				DearModdingUI::MCM::BindPage(
					page,
					m_impl->values,
					[state = a_options.state] { return state; });
				DearModdingUI::MCM::BindActions(
					page,
					m_impl->actions,
					m_impl->values,
					m_impl->diagnostics);
				DearModdingUI::MCM::AttachTextRendering(page);
				const auto registered = client.AddSettingsPage(
					{
						.id = page.id.c_str(),
						.displayName = page.displayName.c_str(),
						.summary = "Parsed and bound MCM compatibility controls."
					},
					std::move(page.settings));
				if (!registered)
				{
					a_error = "Could not register the synthetic MCM fixture (result " +
						std::string{ DMUI_ResultToString(client.LastResult()) } + ").";
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
			a_error = "Synthetic MCM fixture registration threw: ";
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
