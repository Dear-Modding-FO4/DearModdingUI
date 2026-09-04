#include "FakeData.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <DearModdingUI/Client.h>
#include <DearModdingUI/MCM/ActionExecutor.h>
#include <DearModdingUI/MCM/Availability.h>
#include <DearModdingUI/MCM/GlobalValue.h>
#include <DearModdingUI/MCM/Keybinds.h>
#include <DearModdingUI/MCM/SettingsIni.h>
#include <DearModdingUI/MCM/TextRendering.h>
#include <DearModdingUI/MCM/ValueSource.h>

#include <array>
#include <cstdint>
#include <exception>
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
			const char* category;
			const char* summary;
		};

		struct ClientSpec
		{
			const char* id;
			const char* displayName;
			dmui::Version version;
			const char* iconName;
			PageSpec page;
		};

		enum class ClientConnection
		{
			kLockstep,
			kForwarding
		};

		struct SettingsValues
		{
			bool fixesEnabled;
			std::string preset;
			std::string profileName;
			int64_t workerThreads;
			double animationSpeed;
			double framePacingWindow;

			bool operator==(const SettingsValues&) const = default;
		};

		struct SettingsState
		{
			SettingsValues defaults{
				true,
				"Balanced",
				"Commonwealth",
				4,
				1.0,
				2.0
			};

			SettingsValues committed{
				true,
				"Quality",
				"4K Preview",
				8,
				1.15,
				2.5
			};
			SettingsValues draft{ committed };
		};

		constexpr std::string_view kMcmConfig = R"json({
			"modName": "PreviewMCM",
			"displayName": "MCM Bridge Preview",
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
				 "sourceForm":"PreviewMCM.esp|800","default":0,
				 "options":["59 (Utility) slot","60 (Animation) slot","61 (FX) slot"]}},
				{"id":"QuantizedScale","type":"slider","text":"Quantized scale",
				 "help":"Moves in 0.2 increments anchored at 0.1.",
				 "valueOptions":{"sourceType":"GlobalValue",
				 "sourceForm":"PreviewMCM.esp|802","default":0.5,
				 "min":0.1,"max":0.9,"step":0.2,"format":"%.1f"}},
				{"id":"divider","type":"section","text":""},
				{"id":"FeatureEnabled","type":"switcher","text":"Enable feature",
				 "valueOptions":{"sourceType":"GlobalValue",
				 "sourceForm":"PreviewMCM.esp|801","default":false}}
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

		void DrawFixturePage(
			const std::string& a_name,
			const std::string& a_summary) noexcept
		{
			ImGui::TextWrapped("%s", a_summary.c_str());
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			ImGui::TextDisabled("Standalone preview fixture");
			ImGui::BulletText("%s is registered with the production host.", a_name.c_str());
			ImGui::BulletText("Navigation, status, actions, typography, and shell layout are live.");
		}

		template <dmui::SettingValueAlternative T>
		[[nodiscard]] dmui::SettingDescriptor MakeBoundSetting(
			SettingsState* a_state,
			T SettingsValues::* a_member,
			std::string a_id,
			std::string a_label,
			std::string a_description,
			dmui::SettingControl a_control,
			dmui::SettingApplyTiming a_timing = dmui::SettingApplyTiming::kNextLaunch)
		{
			dmui::SettingDescriptor setting;
			setting.id = std::move(a_id);
			setting.label = std::move(a_label);
			setting.description = std::move(a_description);
			setting.control = std::move(a_control);
			setting.defaultValue = a_state->defaults.*a_member;
			setting.binding = dmui::BindSetting(
				[a_state, a_member]() -> T {
					return a_state->draft.*a_member;
				},
				[a_state, a_member](T a_value) -> T {
					a_state->draft.*a_member = std::move(a_value);
					return a_state->draft.*a_member;
				});
			setting.applyTiming = a_timing;
			setting.isDirty = [a_state, a_member]() {
				return a_state->draft.*a_member !=
					a_state->committed.*a_member;
			};
			setting.isModified = [a_state, a_member]() {
				return a_state->draft.*a_member !=
					a_state->defaults.*a_member;
			};
			return setting;
		}

		[[nodiscard]] dmui::SettingsPage MakeAddictolSettingsPage(
			SettingsState* a_state)
		{
			dmui::SettingsPage page;
			page.filterOptions.searchHint = "Search Addictol settings...";
			page.notes.push_back({
				"These controls are preview-only values rendered through dmui::SettingsPage.",
				true
			});
			page.actions.showReset = true;
			page.actions.reset = [a_state]() {
				a_state->draft = a_state->defaults;
			};
			page.actions.revert = [a_state]() {
				a_state->draft = a_state->committed;
			};
			page.actions.apply = [a_state]() {
				a_state->committed = a_state->draft;
			};

			dmui::SettingGroup general{
				"general",
				"General",
				0,
				{},
				true
			};
			general.settings.push_back(MakeBoundSetting(
				a_state,
				&SettingsValues::fixesEnabled,
				"fixes-enabled",
				"Enable fixes",
				"Enables Addictol's runtime fixes.",
				dmui::CheckboxSettingControl{},
				dmui::SettingApplyTiming::kImmediate));
			general.settings.push_back(MakeBoundSetting(
				a_state,
				&SettingsValues::preset,
				"preset",
				"Preset",
				"Selects a balanced, quality, or performance profile.",
				dmui::ChoiceSettingControl{
					{
						{ "Balanced", "Balanced" },
						{ "Quality", "Quality" },
						{ "Performance", "Performance" }
					}
				}));
			general.settings.push_back(MakeBoundSetting(
				a_state,
				&SettingsValues::profileName,
				"profile-name",
				"Profile name",
				"Names the local configuration profile.",
				dmui::TextSettingControl{ 96 }));

			dmui::SettingGroup performance{
				"performance",
				"Performance",
				0,
				{},
				true
			};
			dmui::SignedSettingControl workerControl;
			workerControl.range =
				dmui::NumericSettingRange<int64_t>{ int64_t{ 1 }, int64_t{ 16 } };
			workerControl.format = "%lld threads";
			performance.settings.push_back(MakeBoundSetting(
				a_state,
				&SettingsValues::workerThreads,
				"worker-threads",
				"Worker threads",
				"Controls the number of background workers.",
				std::move(workerControl)));

			dmui::DoubleSettingControl animationControl;
			animationControl.range =
				dmui::NumericSettingRange<double>{ 0.5, 2.0 };
			animationControl.format = "%.2fx";
			performance.settings.push_back(MakeBoundSetting(
				a_state,
				&SettingsValues::animationSpeed,
				"animation-speed",
				"Animation speed",
				"Scales interface transition speed.",
				std::move(animationControl),
				dmui::SettingApplyTiming::kImmediate));

			dmui::DoubleSettingControl pacingControl;
			pacingControl.range = dmui::NumericSettingRange<double>{ 0.25, std::nullopt };
			pacingControl.format = "%.2f ms";
			pacingControl.dragSpeed = 0.05f;
			performance.settings.push_back(MakeBoundSetting(
				a_state,
				&SettingsValues::framePacingWindow,
				"frame-pacing-window",
				"Frame pacing window",
				"Adjusts the smoothing window with a drag control.",
				std::move(pacingControl)));

			dmui::SettingGroup information{
				"information",
				"Information",
				0,
				{},
				true
			};
			dmui::SettingDescriptor runtime;
			runtime.id = "runtime";
			runtime.label = "Runtime";
			runtime.description = "Read-only fixture information.";
			runtime.control = dmui::ReadOnlySettingControl{
				[]() {
					ImGui::TextUnformatted("Fallout 4 1.10.163 (preview)");
				}
			};
			runtime.showReset = false;
			information.settings.push_back(std::move(runtime));

			page.groups.push_back(std::move(general));
			page.groups.push_back(std::move(performance));
			page.groups.push_back(std::move(information));
			return page;
		}

		[[nodiscard]] bool AddPages(
			dmui::Client& a_client,
			std::span<const PageSpec> a_pages,
			std::string& a_error)
		{
			int32_t sortKey = 0;
			for (const auto& page : a_pages)
			{
				const auto handle = a_client.AddPage(
					page.id,
					page.displayName,
					page.category,
					[name = std::string{ page.displayName },
						summary = std::string{ page.summary }]() {
						DrawFixturePage(name, summary);
					},
					page.summary,
					sortKey);
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
		SettingsState settings;
		PreviewValueSource mcmValues;
		PreviewActionExecutor mcmActions;
		std::vector<std::unique_ptr<dmui::Client>> clients;

		[[nodiscard]] dmui::Client* AddClient(
			std::string_view a_id,
			std::string_view a_displayName,
			dmui::Version a_version,
			std::string_view a_iconName,
			std::string& a_error,
			ClientConnection a_connection = ClientConnection::kLockstep)
		{
			auto client = a_connection == ClientConnection::kForwarding ?
				std::make_unique<dmui::Client>(
					a_id,
					a_displayName,
					a_version,
					dmui::kForwardingClient,
					a_iconName) :
				std::make_unique<dmui::Client>(
					a_id,
					a_displayName,
					a_version,
					a_iconName);
			if (!client->Connect())
			{
				a_error = "Could not connect fake client " +
					std::string{ a_id } + " (result " +
					std::to_string(client->LastResult()) + ").";
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

	bool FakeData::Register(std::string& a_error) noexcept
	{
		try
		{
			static constexpr std::array addictolPages{
				PageSpec{
					"overview",
					"Overview",
					"General",
					"Runtime summary and active compatibility fixes."
				},
				PageSpec{
					"fixes",
					"Fixes",
					"General",
					"Individual engine fixes and their current state."
				},
				PageSpec{
					"performance",
					"Performance",
					"Performance",
					"Frame pacing, budgets, and background work."
				},
				PageSpec{
					"rendering",
					"Rendering",
					"Visuals",
					"Renderer compatibility and presentation options."
				},
				PageSpec{
					"camera",
					"Camera",
					"Visuals",
					"First-person and third-person camera behavior."
				},
				PageSpec{
					"diagnostics",
					"Diagnostics",
					"Diagnostics",
					"Runtime diagnostics and support information."
				}
			};
			static constexpr std::array communityShadersPages{
				PageSpec{ "overview", "Overview", "General", "Renderer and feature status." },
				PageSpec{ "screen-space-shadows", "Screen-Space Shadows", "Lighting", "Contact shadow settings." },
				PageSpec{ "grass-lighting", "Grass Lighting", "Lighting", "Per-blade lighting controls." },
				PageSpec{ "wetness", "Wetness Effects", "Lighting", "Rain and surface wetness." },
				PageSpec{ "subsurface-scattering", "Subsurface Scattering", "Lighting", "Skin and foliage scattering." },
				PageSpec{ "complex-parallax", "Complex Parallax", "Visuals", "Material parallax controls." },
				PageSpec{ "terrain-parallax", "Terrain Parallax", "Visuals", "Terrain displacement options." },
				PageSpec{ "water-caustics", "Water Caustics", "Visuals", "Underwater light projection." },
				PageSpec{ "skylighting", "Skylighting", "Lighting", "Ambient sky illumination." },
				PageSpec{ "cloud-shadows", "Cloud Shadows", "Lighting", "Dynamic cloud shadowing." },
				PageSpec{ "interior-shadows", "Interior Shadows", "Lighting", "Interior shadow generation." },
				PageSpec{ "upscaling", "Upscaling", "Performance", "Resolution scaling and sharpening." },
				PageSpec{ "shader-cache", "Shader Cache", "Performance", "Compilation and cache status." },
				PageSpec{ "debug-view", "Debug View", "Diagnostics", "Renderer visualization modes." },
				PageSpec{ "compatibility", "Compatibility", "Compatibility", "Detected patches and conflicts." }
			};
			static constexpr std::array additionalClients{
				ClientSpec{
					"buffout4",
					"Buffout 4",
					{ 1, 28 },
					"terminal-window",
					{ "diagnostics", "Crash Diagnostics", "Diagnostics", "Crash logging and runtime checks." }
				},
				ClientSpec{
					"highfpsphysicsfix",
					"High FPS Physics Fix",
					{ 0, 8 },
					"gauge",
					{ "timing", "Frame Timing", "Performance", "Physics timing and loading controls." }
				},
				ClientSpec{
					"xcell",
					"X-Cell",
					{ 1, 5 },
					"squares-four",
					{ "memory", "Memory", "Performance", "Memory allocation and reclamation." }
				},
				ClientSpec{
					"prp",
					"Previsibines Repair Pack",
					{ 74, 0 },
					"files",
					{ "coverage", "Coverage", "Compatibility", "Loaded previs and precombine coverage." }
				},
				ClientSpec{
					"nacx",
					"NAC X",
					{ 1, 0 },
					"palette",
					{ "weather", "Weather", "Visuals", "Weather and post-process configuration." }
				},
				ClientSpec{
					"longloadingtimesfix",
					"Long Loading Times Fix",
					{ 1, 0 },
					"arrow-counter-clockwise",
					{ "loading", "Loading", "Performance", "Loading-screen timing and diagnostics." }
				},
				ClientSpec{
					"weapondebriscrashfix",
					"Weapon Debris Crash Fix",
					{ 1, 2 },
					"shield-check",
					{ "status", "Status", "Stability", "Debris patch status and compatibility." }
				},
				ClientSpec{
					"fallui",
					"FallUI",
					{ 2, 3 },
					"monitor",
					{ "interface", "Interface", "Interface", "HUD and inventory interface settings." }
				}
			};

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
						keybinds);
				}
			}
			m_impl->mcmValues.Seed("DisplaySlot", 2.0f);
			m_impl->mcmValues.Seed("QuantizedScale", 0.7f);
			m_impl->mcmValues.Seed("FeatureEnabled", 1.0f);
			m_impl->mcmValues.Seed("bDisplayCondition:Misc", 1.0f);
			m_impl->mcmValues.Seed("bDisplayConditionInvert:Misc", 1.0f);
			auto* mcmClient = m_impl->AddClient(
				"dearmodding.mcm-preview",
				"MCM Bridge Preview",
				{ 1, 0 },
				"sliders",
				a_error,
				ClientConnection::kForwarding);
			if (!mcmClient)
			{
				a_error = "Could not register the MCM preview fixture.";
				return false;
			}
			for (auto& mcmPage : mcm.pages)
			{
				DearModdingUI::MCM::BindPage(
					mcmPage,
					m_impl->mcmValues,
					[mcmState] { return mcmState; });
				DearModdingUI::MCM::BindActions(
					mcmPage,
					m_impl->mcmActions,
					m_impl->mcmValues);
				DearModdingUI::MCM::AttachTextRendering(mcmPage);
				if (!mcmClient->AddSettingsPage(
						mcmPage.id.c_str(),
						mcmPage.displayName.c_str(),
						"MCM",
						std::move(mcmPage.settings),
						"Parsed and bound MCM compatibility controls."))
				{
					a_error = "Could not register the MCM preview fixture.";
					return false;
				}
			}

			auto* addictol = m_impl->AddClient(
				"dearmodding.addictol",
				"Addictol",
				{ 1, 4 },
				"puzzle-piece",
				a_error);
			if (!addictol || !AddPages(*addictol, addictolPages, a_error))
				return false;
			if (!addictol->AddSettingsPage(
					"settings",
					"Settings",
					"General",
					MakeAddictolSettingsPage(&m_impl->settings),
					"Declarative settings controls used by Addictol.",
					60))
			{
				a_error = "Could not register Addictol settings (result " +
					std::to_string(addictol->LastResult()) + ").";
				return false;
			}
			if (!addictol->AddAction(
					"copy-diagnostics",
					"Copy diagnostics",
					"clipboard-text",
					"Copy a diagnostic summary.",
					[]() {}) ||
				!addictol->AddAction(
					"reload-configuration",
					"Reload configuration",
					"arrows-clockwise",
					"Reload Addictol's configuration.",
					[]() {},
					10) ||
				!addictol->AddAction(
					"restore-settings",
					"Restore settings",
					"restore-settings",
					"Restore Addictol's saved settings.",
					[]() {},
					20))
			{
				a_error = "Could not register Addictol actions.";
				return false;
			}

			auto* communityShaders = m_impl->AddClient(
				"dearmodding.communityshaders",
				"Community Shaders",
				{ 1, 3 },
				"sun",
				a_error);
			if (!communityShaders ||
				!AddPages(*communityShaders, communityShadersPages, a_error))
				return false;
			if (!communityShaders->AddAction(
					"clear-cache",
					"Clear shader cache",
					"clear-cache",
					"Clear compiled shaders before the next launch.",
					[]() {}))
			{
				a_error = "Could not register Community Shaders actions.";
				return false;
			}

			dmui::Client* buffout{};
			for (const auto& clientSpec : additionalClients)
			{
				auto* client = m_impl->AddClient(
					clientSpec.id,
					clientSpec.displayName,
					clientSpec.version,
					clientSpec.iconName,
					a_error);
				if (!client ||
					!AddPages(
						*client,
						std::span{ &clientSpec.page, size_t{ 1 } },
						a_error))
					return false;
				if (std::string_view{ clientSpec.id } == "buffout4")
					buffout = client;
			}

			if (!communityShaders->SetStatus(
					DMUI_STATUS_SEVERITY_WARNING,
					"Shader cache is rebuilding after a driver update.") ||
				!buffout ||
				!buffout->SetStatus(
					DMUI_STATUS_SEVERITY_ERROR,
					"One incompatible runtime patch was detected."))
			{
				a_error = "Could not register fake client statuses.";
				return false;
			}
			return true;
		}
		catch (const std::exception& a_exception)
		{
			a_error = a_exception.what();
			return false;
		}
		catch (...)
		{
			a_error = "Unknown fake-data registration failure.";
			return false;
		}
	}
}
