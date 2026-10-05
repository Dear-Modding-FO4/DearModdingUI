#include <DearModdingUI/MCM/SettingsIni.h>
#include <DearModdingUI/MCM/ExternalEvents.h>
#include <DearModdingUI/MCM/ValueSource.h>

#include "../Harness.h"

#include <algorithm>
#include <string>
#include <vector>

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI::MCM;

		class FakeEvents final : public McmEventDispatcher
		{
		public:
			void SettingChanged(
				std::string_view a_modName,
				std::string_view a_controlId) noexcept override
			{
				changes.push_back(
					std::string{ a_modName } + "/" +
					std::string{ a_controlId });
			}

			std::vector<std::string> changes;
		};

		[[nodiscard]] MappedBinding SettingBinding(
			DeclarationState a_declaration,
			std::string a_id = "bOption:Main")
		{
			return {
				std::move(a_id),
				false,
				SourceValueKind::kBool,
				"ModSettingBool",
				ModSettingBinding{
					"Main",
					"bOption",
					a_declaration
				}
			};
		}

		class SnapshotSource final : public ValueSource
		{
		public:
			[[nodiscard]] bool Supports(SourceFamily) const noexcept override
			{
				return true;
			}

			[[nodiscard]] ValueSnapshot Read(
				const MappedBinding&) const override
			{
				++reads;
				return snapshot;
			}

			[[nodiscard]] uint64_t Refresh(const MappedBinding&) override
			{
				++refreshes;
				return Generation(snapshot);
			}

			[[nodiscard]] ValueSnapshot Write(
				const MappedBinding& a_binding,
				const dmui::SettingValue&) override
			{
				++writes;
				NotifyAcceptedModSettingWrite(events, "Fixture", a_binding);
				return snapshot;
			}

			ValueSnapshot snapshot{ MissingValue{} };
			mutable size_t reads{};
			size_t refreshes{};
			size_t writes{};
			FakeEvents events;
		};

		[[nodiscard]] MappedPage ConditionPage()
		{
			auto result = ParseConfig(R"json({
				"modName":"ConditionRuntimeFixture",
				"content":[
					{"id":"bController:Main","type":"hiddenSwitcher","groupControl":1,
					 "valueOptions":{"sourceType":"ModSettingBool","default":false}},
					{"id":"dependent","type":"text","text":"Dependent","groupCondition":1}
				]
			})json", "condition-runtime.json");
			return std::move(result.pages.front());
		}

		[[nodiscard]] MappedPage InteractiveConditionPage()
		{
			auto result = ParseConfig(R"json({
				"modName":"InteractiveConditionRuntimeFixture",
				"content":[
					{"id":"bController:Main","type":"switcher","groupControl":1,
					 "valueOptions":{"sourceType":"ModSettingBool","default":false}},
					{"id":"dependent","type":"text","text":"Dependent","groupCondition":1}
				]
			})json", "interactive-condition-runtime.json");
			return std::move(result.pages.front());
		}

		[[nodiscard]] MappedPage IdlessInteractiveConditionPage(
			std::string_view a_modName = "IdlessConditionRuntimeFixture")
		{
			auto result = ParseConfig(
				std::string{ R"json({"modName":")json" } +
					std::string{ a_modName } +
					R"json(","displayName":"Idless condition","content":[
						{"type":"switcher","groupControl":9,
						 "valueOptions":{"sourceType":"ModSettingBool","default":true}},
						{"id":"dependent","type":"text","text":"Dependent",
						 "groupCondition":{"AND":[{"OR":[9]}]}}
					]})json",
				"idless-condition-runtime.json");
			return std::move(result.pages.front());
		}

		[[nodiscard]] dmui::SettingDescriptor& Controller(MappedPage& a_page)
		{
			for (auto& group : a_page.settings.groups)
			{
				for (auto& setting : group.settings)
				if (setting.id == "bController:Main")
					return setting;
			}
			throw Failure("missing condition controller row");
		}

		[[nodiscard]] dmui::SettingDescriptor& Dependent(MappedPage& a_page)
		{
			for (auto& group : a_page.settings.groups)
			{
				for (auto& setting : group.settings)
				{
					if (setting.id == "dependent")
						return setting;
				}
			}
			throw Failure("missing dependent condition row");
		}

		[[nodiscard]] dmui::SettingDescriptor& IdlessController(
			MappedPage& a_page)
		{
			return a_page.settings.groups.front().settings.front();
		}

		[[nodiscard]] bool HasConditionNote(const MappedPage& a_page)
		{
			return std::ranges::any_of(
				a_page.settings.notes,
				[](const dmui::SettingsPageNote& a_note) {
					return a_note.text.find("condition") != std::string::npos;
				});
		}
	}

	void run_mcm_runtime_checks(Runner& runner)
	{
		runner.test("MCM value cache rejects stale and unknown completions", [] {
			ValueCache cache;
			require(std::holds_alternative<MissingValue>(cache.Read("setting")),
				"a new cache entry was not missing");
			const auto generation = cache.BeginRefresh("setting");
			require(generation == 1 &&
					std::holds_alternative<PendingValue>(cache.Read("setting")),
				"a refresh did not publish its pending generation");
			require(cache.Complete(
						"setting",
						ReadyValue{ true, generation }) &&
					std::get<bool>(
						std::get<ReadyValue>(cache.Read("setting")).value),
				"a matching completion did not become ready");
			const auto refresh = cache.BeginRefresh("setting");
			const auto written = cache.Store("setting", dmui::SettingValue{ true });
			require(Generation(written.snapshot) > refresh,
				"write did not advance beyond the pending refresh");
			require(!cache.Complete(
						"setting",
						ReadyValue{ false, refresh }),
				"a stale completion was accepted");
			const auto current = std::get<ReadyValue>(cache.Read("setting"));
			require(std::get<bool>(current.value) &&
					current.generation == Generation(written.snapshot),
				"a stale completion replaced the newer write");
			require(!cache.Complete("missing", ReadyValue{ true, 0 }) &&
					std::holds_alternative<MissingValue>(cache.Read("missing")),
				"a rejected completion inserted a default ready value");
		});

		runner.test("MCM event decisions require accepted declared identified writes", [] {
			FakeEvents events;
			NotifyAcceptedModSettingWrite(
				events,
				"Fixture",
				SettingBinding(DeclarationState::kDeclared));
			NotifyAcceptedModSettingWrite(
				events,
				"Fixture",
				SettingBinding(DeclarationState::kUndeclared));
			NotifyAcceptedModSettingWrite(
				events,
				"Fixture",
				SettingBinding(DeclarationState::kUnknown, "iUnknown:Main"));
			NotifyAcceptedModSettingWrite(
				events,
				"Fixture",
				SettingBinding(DeclarationState::kDeclared, ""));
			require(events.changes == std::vector<std::string>{
						"Fixture/bOption:Main",
						"Fixture/iUnknown:Main"
					},
				"setting change events ignored declaration or control id gating");
		});

		runner.test("MCM idless visibility toggles are bindingless local state", [] {
			auto page = IdlessInteractiveConditionPage();
			SnapshotSource source;
			source.snapshot = ReadyValue{ true, 3 };
			BindPage(page, source, [] { return McmState{}; });
			page.settings.prepareView(page.settings);

			auto& toggle = IdlessController(page);
			const auto& row = page.rows.front();
			const auto summary = SummarizeCompatibility(page);
			require(row.valueRoute == ValueRoute::kLocalUiState &&
					!row.binding &&
					!row.unmappedSource &&
					toggle.binding.get &&
					!std::get<bool>(toggle.binding.get()) &&
					!toggle.showReset &&
					Dependent(page).isVisible &&
					!Dependent(page).isVisible() &&
					summary.localUiStateRows == 1 &&
					summary.bindings == 0 &&
					summary.unknownBindings == 0,
				"an idless local controller retained storage or its configured default");

			(void)toggle.binding.set(dmui::SettingValue{ true });
			require(std::get<bool>(toggle.binding.get()) &&
					Dependent(page).isVisible(),
				"idless local state did not reveal its nested dependent");

			auto otherPage = IdlessInteractiveConditionPage();
			auto otherMod =
				IdlessInteractiveConditionPage("OtherIdlessFixture");
			BindPage(otherPage, source);
			BindPage(otherMod, source);
			otherPage.settings.prepareView(otherPage.settings);
			otherMod.settings.prepareView(otherMod.settings);
			page.settings.prepareView(page.settings);
			require(std::get<bool>(toggle.binding.get()) &&
					!std::get<bool>(
						IdlessController(otherPage).binding.get()) &&
					!Dependent(otherPage).isVisible() &&
					!std::get<bool>(
						IdlessController(otherMod).binding.get()) &&
					!Dependent(otherMod).isVisible(),
				"idless local state leaked across pages or mods");

			(void)toggle.binding.set(dmui::SettingValue{ false });
			require(!Dependent(page).isVisible() &&
					source.reads == 0 &&
					source.writes == 0 &&
					source.refreshes == 0 &&
					!row.writeValue &&
					source.events.changes.empty(),
				"idless local state touched storage, emitted an event, or failed to collapse");

			auto undeclared = InteractiveConditionPage();
			std::get<ModSettingBinding>(
				std::ranges::find_if(
					undeclared.rows,
					[](const MappedRow& a_row) {
						return a_row.groupControl.has_value();
					})->binding->source).declaration =
						DeclarationState::kUndeclared;
			SnapshotSource undeclaredSource;
			undeclaredSource.snapshot = ReadyValue{ false, 3 };
			BindPage(undeclared, undeclaredSource);
			undeclared.settings.prepareView(undeclared.settings);
			auto& undeclaredToggle = Controller(undeclared);
			require(undeclaredToggle.isEnabled && undeclaredToggle.isEnabled() &&
					!Dependent(undeclared).isVisible(),
				"an undeclared visibility toggle was not an interactive local controller");
			(void)undeclaredToggle.binding.set(dmui::SettingValue{ true });
			undeclaredSource.RefreshPage(undeclared, { true, true });
			require(Dependent(undeclared).isVisible() &&
					undeclaredSource.reads == 0 &&
					undeclaredSource.writes == 0 &&
					undeclaredSource.refreshes == 0 &&
					undeclaredSource.events.changes.empty(),
				"an undeclared visibility toggle reached persistent storage");

			for (const auto* type : { "section", "spacer" })
			{
				auto result = ParseConfig(
					std::string{ R"({
						"modName":"LocalOwnership",
						"displayName":"Local ownership",
						"content":[
							{"type":"switcher","groupControl":1,
							 "valueOptions":{"sourceType":"ModSettingBool"}},
							{"type":")" } + type + R"(","text":"Dependent",
							 "groupCondition":1}
						]
					})");
				require(result.pages.size() == 1 && result.diagnostics.empty(),
					"a descriptorless reference prevented local ownership");
				auto& page = result.pages.front();
				require(page.rows.size() == 1 &&
						page.rows.front().valueRoute == ValueRoute::kLocalUiState &&
						SummarizeCompatibility(page).localUiStateRows == 1,
					"the mapper did not assign bindingless local ownership");

				SnapshotSource source;
				BindPage(page, source, [] { return McmState{}; });
				auto& toggle = page.settings.groups.front().settings.front();
				require(toggle.binding.get && toggle.binding.set &&
						toggle.isEnabled && toggle.isEnabled() &&
						!std::get<bool>(toggle.binding.get()) &&
						SummarizeCompatibility(page).localUiStateRows == 1,
					"binding lost explicit local ownership after presentation mapping");
				(void)toggle.binding.set(dmui::SettingValue{ true });
				source.RefreshPage(page, {});
				require(std::get<bool>(toggle.binding.get()) &&
						source.reads == 0 && source.writes == 0 &&
						source.refreshes == 0 && source.events.changes.empty(),
					"an explicitly local toggle was unbound or reached persistent storage");
			}
		});

		runner.test("MCM missing and failed conditions fail open with diagnostics", [] {
			for (const auto snapshot : std::array<ValueSnapshot, 2>{
					 ValueSnapshot{ MissingValue{ 3 } },
					 ValueSnapshot{ FailedValue{ 4 } } })
			{
				auto page = ConditionPage();
				SnapshotSource source;
				source.snapshot = snapshot;
				BindPage(page, source);
				page.settings.prepareView(page.settings);

				require(Dependent(page).isVisible &&
						Dependent(page).isVisible(),
					"an unavailable dependency hid its dependent");
				require(HasConditionNote(page),
					"an unavailable dependency produced no page diagnostic");
				require(SummarizeActionableCompatibility(page).empty(),
					"an unavailable snapshot created a permanent startup warning");
			}

			auto pending = ConditionPage();
			SnapshotSource pendingSource;
			pendingSource.snapshot = PendingValue{ 3 };
			BindPage(pending, pendingSource);
			pending.settings.prepareView(pending.settings);
			require(Dependent(pending).isVisible &&
					!Dependent(pending).isVisible() &&
					!pending.settings.notes.empty() &&
					SummarizeActionableCompatibility(pending).empty(),
				"a pending condition did not hide with a loading indication");

			auto operable = InteractiveConditionPage();
			SnapshotSource operableSource;
			operableSource.snapshot = ReadyValue{ false, 3 };
			BindPage(operable, operableSource);
			operable.settings.prepareView(operable.settings);
			require(!Dependent(operable).isVisible() &&
					!HasConditionNote(operable),
				"an operable false controller failed open");
			operableSource.snapshot = ReadyValue{ true, 4 };
			require(Dependent(operable).isVisible(),
				"an operable controller stopped reading its real value source");
		});


	}
}
