#include <DearModdingUI/MCM/Availability.h>
#include <DearModdingUI/MCM/ModSettingValueSource.h>
#include <DearModdingUI/MCM/ValueSource.h>

#include "Harness.h"

#include <algorithm>
#include <array>
#include <string>
#include <tuple>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI::MCM;

	namespace
	{
		class ReadySource final : public ValueSource
		{
		public:
			[[nodiscard]] bool Supports(SourceFamily) const noexcept override
			{
				return true;
			}

			[[nodiscard]] ValueSnapshot Read(
				const MappedBinding& a_binding) const override
			{
				return ReadyValue{ a_binding.target, 1 };
			}

			[[nodiscard]] uint64_t Refresh(const MappedBinding&) override
			{
				return 1;
			}

			[[nodiscard]] ValueSnapshot Write(
				const MappedBinding&,
				const dmui::SettingValue& a_value) override
			{
				return ReadyValue{ a_value, 2 };
			}
		};

		class ImmediateScheduler final : public TaskScheduler
		{
		public:
			void Schedule(std::function<void()> a_work) override
			{
				a_work();
			}

			void ScheduleUi(std::function<void()> a_work) override
			{
				a_work();
			}
		};

		class FakeDispatcher final : public PapyrusDispatcher
		{
		public:
			[[nodiscard]] bool DispatchStatic(
				std::string_view a_script,
				std::string_view a_function,
				std::span<const PapyrusArgument> a_arguments,
				const std::optional<dmui::SettingValue>&,
				PapyrusDispatchCompletion a_completion) override
			{
				script = a_script;
				function = a_function;
				arguments.assign(a_arguments.begin(), a_arguments.end());
				if (!accepted)
					return false;
				if (a_completion)
					a_completion(true, result);
				return true;
			}

			bool accepted{ true };
			std::optional<dmui::SettingValue> result{ true };
			std::string script;
			std::string function;
			std::vector<PapyrusArgument> arguments;
		};

		class FakeEvents final : public McmEventDispatcher
		{
		public:
			void SettingChanged(
				std::string_view a_modName,
				std::string_view a_controlId) noexcept override
			{
				change = std::string{ a_modName } + "/" +
					std::string{ a_controlId };
			}


			std::string change;
		};

		[[nodiscard]] MappedPage StatePage(
			std::string_view a_sourceType = "ModSettingBool")
		{
			auto result = ParseConfig(
				std::string{
					R"({"modName":"Availability","content":[{"id":"setting","type":"switcher","valueOptions":{"sourceType":")"
				} +
				std::string{ a_sourceType } +
				R"(","sourceForm":"Fixture.esp|800","scriptName":"FixtureScript","propertyName":"Enabled","default":false}}]})");
			auto page = std::move(result.pages.front());
			if (auto* setting =
					std::get_if<ModSettingBinding>(&page.rows.front().binding->source))
				setting->declaration = DeclarationState::kDeclared;
			return page;
		}

		[[nodiscard]] dmui::SettingDescriptor& Descriptor(MappedPage& a_page)
		{
			return a_page.settings.groups.front().settings.front();
		}

		[[nodiscard]] std::string_view EnvironmentNote(
			const MappedPage& a_page)
		{
			const auto found = std::ranges::find_if(
				a_page.settings.notes,
				[](const dmui::SettingsPageNote& a_note) {
					return a_note.noteId == "dearmodding.mcm.availability";
				});
			return found == a_page.settings.notes.end() ?
				std::string_view{} :
				found->text;
		}

		[[nodiscard]] MappedPage LocalStatePage()
		{
			auto result = ParseConfig(R"({
				"modName":"LocalState",
				"content":[
					{"id":"controller","type":"switcher","groupControl":1,
					 "valueOptions":{"sourceType":"ModSettingBool","default":false}},
					{"id":"dependent","type":"text","text":"Dependent",
					 "groupCondition":1}
				]
			})");
			auto page = std::move(result.pages.front());
			std::get<ModSettingBinding>(
				page.rows.front().binding->source).declaration =
					DeclarationState::kUndeclared;
			return page;
		}
	}

	void run_mcm_availability_checks(Runner& runner)
	{
		runner.test("MCM mod-setting state matrix has exact operability reasons", [] {
			constexpr std::array cases{
				std::tuple{ McmState{ false, false },
					false,
					std::string_view{ "Mod Configuration Menu is not installed." } },
				std::tuple{ McmState{ false, true },
					false,
					std::string_view{ "Mod Configuration Menu is not installed." } },
				std::tuple{ McmState{ true, false },
					false,
					std::string_view{ "Load a save to change these settings." } },
				std::tuple{ McmState{ true, true }, true, std::string_view{} }
			};
			for (const auto& [state, expected, reason] : cases)
			{
				require(
					IsControlOperable(state, SourceFamily::kModSetting) == expected &&
						ControlUnavailableReason(
							state,
							SourceFamily::kModSetting) == reason,
					"mod-setting installation/readiness state was conflated");
			}
		});

		runner.test("MCM local rows stay interactive in every runtime state", [] {
			auto page = LocalStatePage();
			ReadySource source;
			auto state = McmState{};
			BindPage(page, source, [&state] { return state; });
			auto& descriptor = Descriptor(page);
			for (const auto installed : { false, true })
			{
				for (const auto ready : { false, true })
				{
					state = { installed, ready };
					require(
						IsControlOperable(
							state,
							SourceFamily::kModSetting,
							ValueRoute::kLocalUiState) &&
							descriptor.isEnabled &&
							descriptor.isEnabled() &&
							descriptor.resolveDescription().empty(),
						"local UI state depended on MCM or Papyrus");
				}
			}
		});

		runner.test("MCM global rows stay operable without a loaded game", [] {
			auto page = StatePage("GlobalValue");
			ReadySource source;
			BindPage(page, source, [] { return McmState{ false, false }; });
			require(
				IsControlOperable({ false, false }, SourceFamily::kGlobal) &&
					IsControlOperable({ true, false }, SourceFamily::kGlobal) &&
					Descriptor(page).isEnabled &&
					Descriptor(page).isEnabled(),
				"global access regressed to require MCM or a loaded game");
		});

		runner.test("MCM property rows require a loaded game", [] {
			require(
				!IsControlOperable({ false, false }, SourceFamily::kProperty) &&
					!IsControlOperable({ true, false }, SourceFamily::kProperty) &&
					IsControlOperable({ false, true }, SourceFamily::kProperty) &&
					ControlUnavailableReason(
						{ true, false },
						SourceFamily::kProperty) ==
						"Load a save to change these settings.",
				"property dispatch did not track Papyrus readiness");
		});

		runner.test("MCM production composition keeps reason and enablement aligned", [] {
			auto page = StatePage();
			ReadySource source;
			auto state = McmState{ true, false };
			BindPage(page, source, [&state] { return state; });
			auto& descriptor = Descriptor(page);

			constexpr std::array cases{
				std::tuple{ McmState{ false, false },
					false,
					InertReason::kMcmNotInstalled,
					std::string_view{
						"Mod Configuration Menu is not installed, so mod settings cannot be changed."
					} },
				std::tuple{ McmState{ false, true },
					false,
					InertReason::kMcmNotInstalled,
					std::string_view{
						"Mod Configuration Menu is not installed, so mod settings cannot be changed."
					} },
				std::tuple{ McmState{ true, false },
					false,
					InertReason::kRuntimeNotReady,
					std::string_view{ "Load a save to change these settings." } },
				std::tuple{
					McmState{ true, true },
					true,
					InertReason::kNone,
					std::string_view{}
				}
			};
			for (const auto& [next, enabled, reason, note] : cases)
			{
				state = next;
				page.settings.prepareView(page.settings);
				const auto inert = page.rows.front().resolveInertState();
				require(
					descriptor.isEnabled &&
						descriptor.isEnabled() == enabled &&
						inert.governingReason == reason &&
						(inert.governingReason == InertReason::kNone) == enabled &&
						descriptor.resolveDescription().empty() &&
						EnvironmentNote(page) == note,
					"production composition drifted from its authoritative reason");
			}
		});

		runner.test("MCM environment gates preserve row-specific explanations", [] {
			auto page = StatePage();
			std::get<ModSettingBinding>(
				page.rows.front().binding->source).declaration =
					DeclarationState::kUndeclared;
			ReadySource source;
			BindPage(
				page,
				source,
				[] { return McmState{ true, false }; });
			page.settings.prepareView(page.settings);
			const auto inert = page.rows.front().resolveInertState();

			require(
				inert.governingReason == InertReason::kRuntimeNotReady &&
					inert.rowReason == InertReason::kUndeclaredModSetting &&
					!Descriptor(page).isEnabled() &&
					Descriptor(page).resolveDescription() ==
						"This setting is not declared in MCM settings.ini." &&
					EnvironmentNote(page) ==
						"Load a save to change these settings.",
				"environment scope buried a durable row-specific reason");
		});

		runner.test("MCM static dispatch seam exercises mod-setting reads and writes", [] {
			auto page = StatePage();
			const auto binding = *page.rows.front().binding;
			ImmediateScheduler scheduler;
			FakeDispatcher dispatcher;
			FakeEvents events;
			ModSettingValueSource source{
				"Fixture",
				events,
				scheduler,
				dispatcher
			};

			source.RefreshPage(page, { true, false });
			require(
				dispatcher.function.empty(),
				"mod-setting dispatch ran before Papyrus was ready");
			source.RefreshPage(page, { true, true });
			source.Pump();
			require(
				dispatcher.script == "MCM" &&
					dispatcher.function == "GetModSettingBool" &&
					dispatcher.arguments.size() == 2 &&
					std::get<bool>(
						std::get<ReadyValue>(source.Read(binding)).value),
				"fake dispatcher could not complete a mod-setting read");
			(void)source.Write(binding, dmui::SettingValue{ false });
			require(
				dispatcher.function == "SetModSettingBool" &&
					dispatcher.arguments.size() == 3 &&
					events.change == "Fixture/setting",
				"fake dispatcher could not observe a mod-setting write");
		});
	}
}
