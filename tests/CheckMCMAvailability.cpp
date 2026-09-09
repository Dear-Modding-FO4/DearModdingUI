#include <DearModdingUI/MCM/Availability.h>
#include <DearModdingUI/MCM/ModSettingValueSource.h>
#include <DearModdingUI/MCM/ValueSource.h>

#include "Harness.h"
#include "FakeDiagnosticReporter.h"

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
		FakeDiagnosticReporter diagnostics;

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
		runner.test("MCM production composition gates every value route", [] {
			ReadySource source;
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
				auto page = StatePage();
				auto state = next;
				BindPage(page, source, [&state] { return state; });
				page.settings.prepareView(page.settings);
				const auto inert = page.rows.front().resolveInertState();
				auto& descriptor = Descriptor(page);
				require(
					descriptor.isEnabled &&
						descriptor.isEnabled() == enabled &&
						inert.governingReason == reason &&
						(inert.governingReason == InertReason::kNone) == enabled &&
						descriptor.resolveDescription().empty() &&
						EnvironmentNote(page) == note,
					"production composition drifted from its authoritative reason");
			}

			for (const auto state : {
					 McmState{ false, false },
					 McmState{ true, false },
					 McmState{ false, true },
					 McmState{ true, true } })
			{
				auto global = StatePage("GlobalValue");
				BindPage(global, source, [state] { return state; });
				global.settings.prepareView(global.settings);
				require(Descriptor(global).isEnabled &&
						Descriptor(global).isEnabled() &&
						global.rows.front().resolveInertState().governingReason ==
							InertReason::kNone,
					"global composition depended on MCM or Papyrus readiness");

				auto local = LocalStatePage();
				BindPage(local, source, [state] { return state; });
				local.settings.prepareView(local.settings);
				require(Descriptor(local).isEnabled &&
						Descriptor(local).isEnabled() &&
						local.rows.front().resolveInertState().governingReason ==
							InertReason::kNone &&
						Descriptor(local).resolveDescription().empty(),
					"local UI composition depended on MCM or Papyrus readiness");
			}

			for (const auto state : {
					 McmState{ false, false },
					 McmState{ true, false },
					 McmState{ false, true } })
			{
				auto property = StatePage("PropertyValueBool");
				BindPage(property, source, [state] { return state; });
				property.settings.prepareView(property.settings);
				const auto expected = state.runtimeReady;
				require(Descriptor(property).isEnabled &&
						Descriptor(property).isEnabled() == expected &&
						property.rows.front()
								.resolveInertState()
								.governingReason ==
							(expected ?
								 InertReason::kNone :
								 InertReason::kRuntimeNotReady),
					"property composition did not track Papyrus readiness");
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
				dispatcher,
				diagnostics
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
