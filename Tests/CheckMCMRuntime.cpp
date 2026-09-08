#include <DearModdingUI/MCM/SettingsIni.h>
#include <DearModdingUI/MCM/ExternalEvents.h>
#include <DearModdingUI/MCM/ValueSource.h>

#include "Harness.h"

#include <algorithm>
#include <iterator>
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

		void AppendEvents(
			std::vector<McmExternalEvent>& a_target,
			std::vector<McmExternalEvent> a_events)
		{
			a_target.insert(
				a_target.end(),
				std::make_move_iterator(a_events.begin()),
				std::make_move_iterator(a_events.end()));
		}
	}

	void run_mcm_runtime_checks(Runner& runner)
	{
		runner.test("MCM value cache transitions from missing through pending", [] {
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
		});

		runner.test("MCM late refresh cannot overwrite a newer write", [] {
			ValueCache cache;
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
		});

		runner.test("MCM cache completion misses do not create ready values", [] {
			ValueCache cache;
			require(!cache.Complete("missing", ReadyValue{ true, 0 }) &&
					std::holds_alternative<MissingValue>(cache.Read("missing")),
				"a rejected completion inserted a default ready value");
		});

		runner.test("MCM conditions retain pending instead of using defaults", [] {
			const GroupCondition control{
				ConditionType::kControl,
				7
			};
			require(EvaluateCondition(
						control,
						[](int64_t) -> ValueSnapshot {
							return PendingValue{ 4 };
						}) == ConditionResult::kPending,
				"a pending dependency was treated as false or its default");

			const GroupCondition any{
				ConditionType::kAny,
				0,
				{},
				{
					GroupCondition{ ConditionType::kControl, 1 },
					GroupCondition{ ConditionType::kControl, 2 }
				}
			};
			require(EvaluateCondition(
						any,
						[](int64_t a_control) -> ValueSnapshot {
							return a_control == 1 ?
								ValueSnapshot{ PendingValue{ 2 } } :
								ValueSnapshot{ ReadyValue{ false, 2 } };
						}) == ConditionResult::kPending,
				"OR discarded an unresolved dependency");
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

		runner.test("MCM menu events match shipped names and zero argument arity", [] {
			std::vector<McmExternalEvent> events;
			AppendEvents(events, OverlayOpenedExternalEvents());
			AppendEvents(events, MenuOpenedExternalEvents("Fixture"));
			AppendEvents(events, MenuClosedExternalEvents("Fixture"));
			AppendEvents(events, OverlayClosedExternalEvents());
			require(events == std::vector<McmExternalEvent>{
						{ "OnMCMOpen", {} },
						{ "OnMCMMenuOpen", {} },
						{ "OnMCMMenuOpen|Fixture", {} },
						{ "OnMCMMenuClose|Fixture", {} },
						{ "OnMCMClose", {} }
					},
				"MCM menu event names or shipped zero-argument arity changed");
		});

		runner.test("MCM overlay opening emits whole and mod menu events", [] {
			McmEventLifecycle lifecycle;
			require(lifecycle.PageActivated("ModA") ==
					std::vector<McmExternalEvent>{
						{ "OnMCMOpen", {} },
						{ "OnMCMMenuOpen", {} },
						{ "OnMCMMenuOpen|ModA", {} }
					},
				"opening the overlay on a mod emitted the wrong event sequence");
		});

		runner.test("MCM mod transitions omit whole menu close and open events", [] {
			McmEventLifecycle lifecycle;
			(void)lifecycle.PageActivated("ModA");
			auto events = lifecycle.PageDeactivated("ModA", true);
			AppendEvents(events, lifecycle.PageActivated("ModB"));
			require(events == std::vector<McmExternalEvent>{
						{ "OnMCMMenuClose|ModA", {} },
						{ "OnMCMMenuOpen", {} },
						{ "OnMCMMenuOpen|ModB", {} }
					},
				"switching MCM mods over-fired whole menu events");
		});

		runner.test("MCM overlay closing emits mod close then whole menu close", [] {
			McmEventLifecycle lifecycle;
			(void)lifecycle.PageActivated("ModB");
			require(lifecycle.PageDeactivated("ModB", false) ==
					std::vector<McmExternalEvent>{
						{ "OnMCMMenuClose|ModB", {} },
						{ "OnMCMClose", {} }
					},
				"closing the overlay emitted the wrong event sequence");
		});

		runner.test("MCM whole close waits for the overlay after leaving mod pages", [] {
			McmEventLifecycle lifecycle;
			(void)lifecycle.PageActivated("ModA");
			auto events = lifecycle.PageDeactivated("ModA", true);
			AppendEvents(
				events,
				lifecycle.OverlayVisibilityChanged(false));
			require(events == std::vector<McmExternalEvent>{
						{ "OnMCMMenuClose|ModA", {} },
						{ "OnMCMClose", {} }
					},
				"whole menu close fired before the overlay actually closed");
		});

		runner.test("MCM filtered menu events require a nonempty mod name", [] {
			McmEventLifecycle lifecycle;
			const auto opened = lifecycle.PageActivated("");
			const auto closed = lifecycle.PageDeactivated("", false);
			require(opened == std::vector<McmExternalEvent>{
						{ "OnMCMOpen", {} },
						{ "OnMCMMenuOpen", {} }
					} &&
					closed == std::vector<McmExternalEvent>{
						{ "OnMCMClose", {} }
					},
				"an empty mod name emitted a filtered MCM event");
		});

		runner.test("MCM unknown declarations remain attemptable", [] {
			auto result = ParseConfig(R"json({
				"modName":"UnknownDeclarations",
				"content":[{"id":"bOption:Main","type":"switcher",
					"valueOptions":{"sourceType":"ModSettingBool",
						"default":false}}]
			})json");
			auto& page = result.pages.front();
			SnapshotSource source;
			source.snapshot = ReadyValue{ true, 1 };
			BindPage(page, source);
			auto& descriptor = page.settings.groups.front().settings.front();
			require(descriptor.isEnabled && descriptor.isEnabled(),
				"an unknown settings.ini declaration was disabled");
		});

		runner.test("MCM pending conditions hide with a loading indication", [] {
			auto page = ConditionPage();
			SnapshotSource source;
			source.snapshot = PendingValue{ 3 };
			BindPage(page, source);
			page.settings.prepareView(page.settings);

			const auto summary = SummarizeCompatibility(page, source);
			require(Dependent(page).isVisible &&
					!Dependent(page).isVisible() &&
					summary.pendingConditions == 1 &&
					summary.visibleRows == 0,
				"a pending condition did not hide its dependent");
			require(std::ranges::any_of(
						page.settings.notes,
						[](const dmui::SettingsPageNote& a_note) {
							return a_note.text.find("Loading") != std::string::npos;
						}),
				"an all-pending page still appeared empty");
			require(SummarizeActionableCompatibility(page).empty(),
				"a pending condition created a permanent startup warning");
		});

		runner.test("MCM inoperable visibility toggles use local state", [] {
			auto page = InteractiveConditionPage();
			auto& controller = *std::ranges::find_if(
				page.rows,
				[](const MappedRow& a_row) {
					return a_row.groupControl.has_value();
				})->binding;
			std::get<ModSettingBinding>(controller.source).declaration =
				DeclarationState::kUndeclared;
			SnapshotSource source;
			source.snapshot = ReadyValue{ false, 3 };
			BindPage(page, source);
			page.settings.prepareView(page.settings);

			const auto summary = SummarizeCompatibility(page, source);
			auto& toggle = Controller(page);
			require(toggle.isEnabled && toggle.isEnabled() &&
					!toggle.showReset &&
					toggle.description.find("not declared") == std::string::npos &&
					Dependent(page).isVisible &&
					!Dependent(page).isVisible() &&
					summary.unevaluableConditions == 0 &&
					SummarizeCompatibility(page).undeclaredModSettings == 1 &&
					summary.localUiStateRows == 1 &&
					!HasConditionNote(page),
				"a local visibility controller was not interactive and collapsed");
			require(SummarizeActionableCompatibility(page).empty(),
				"a locally owned undeclared toggle looked like a persisted fault");
			(void)toggle.binding.set(dmui::SettingValue{ true });
			require(Dependent(page).isVisible() &&
					source.reads == 0 &&
					source.writes == 0 &&
					source.events.changes.empty(),
				"a local visibility controller used the source or stayed collapsed");
			source.RefreshPage(page, { true, true });
			require(source.refreshes == 0,
				"a local visibility controller refreshed persistent storage");

			auto otherPage = InteractiveConditionPage();
			auto& otherController = *std::ranges::find_if(
				otherPage.rows,
				[](const MappedRow& a_row) {
					return a_row.groupControl.has_value();
				})->binding;
			std::get<ModSettingBinding>(otherController.source).declaration =
				DeclarationState::kUndeclared;
			BindPage(otherPage, source);
			require(!Dependent(otherPage).isVisible(),
				"local visibility state leaked between pages or mods");
		});

		runner.test("MCM idless visibility toggles are bindingless local state", [] {
			auto page = IdlessInteractiveConditionPage();
			SnapshotSource source;
			source.snapshot = ReadyValue{ true, 3 };
			auto state = McmState{};
			BindPage(page, source, [&state] { return state; });
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

			for (const auto installed : { false, true })
			{
				for (const auto ready : { false, true })
				{
					state = { installed, ready };
					const auto open = installed != ready;
					(void)toggle.binding.set(dmui::SettingValue{ open });
					require(toggle.isEnabled && toggle.isEnabled() &&
							row.resolveInertState().governingReason ==
								InertReason::kNone &&
							std::get<bool>(toggle.binding.get()) == open &&
							Dependent(page).isVisible() == open,
						"idless local state depended on MCM readiness");
					source.RefreshPage(page, state);
				}
			}

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
		});

		runner.test("MCM explicit local ownership survives descriptorless references", [] {
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

		runner.test("MCM operable false controller conditions stay hidden", [] {
			auto page = InteractiveConditionPage();
			SnapshotSource source;
			source.snapshot = ReadyValue{ false, 3 };
			BindPage(page, source);
			page.settings.prepareView(page.settings);

			const auto summary = SummarizeCompatibility(page, source);
			const auto priorReads = source.reads;
			require(Dependent(page).isVisible &&
					!Dependent(page).isVisible() &&
					summary.unevaluableConditions == 0 &&
					!HasConditionNote(page),
				"an operable false controller failed open");
			require(SummarizeActionableCompatibility(page).empty(),
				"a false condition created a permanent startup warning");
			source.snapshot = ReadyValue{ true, 4 };
			require(Dependent(page).isVisible() && source.reads > priorReads,
				"an operable controller stopped reading its real value source");
		});

		runner.test("MCM missing conditions fail open with a diagnostic", [] {
			auto page = ConditionPage();
			SnapshotSource source;
			source.snapshot = MissingValue{ 3 };
			BindPage(page, source);
			page.settings.prepareView(page.settings);

			const auto summary = SummarizeCompatibility(page, source);
			require(Dependent(page).isVisible &&
					Dependent(page).isVisible() &&
					summary.unevaluableConditions == 1,
				"a missing dependency hid its dependent");
			require(HasConditionNote(page),
				"a missing dependency produced no page diagnostic");
			require(SummarizeActionableCompatibility(page).empty(),
				"an unvisited value snapshot created a permanent startup warning");
		});

		runner.test("MCM not-ready state stays a live availability note", [] {
			auto result = ParseConfig(R"json({
				"modName":"NotReady",
				"content":[{"id":"bOption:Main","type":"switcher",
					"valueOptions":{"sourceType":"ModSettingBool",
						"default":false}}]
			})json");
			auto& page = result.pages.front();
			auto& binding = *page.rows.front().binding;
			std::get<ModSettingBinding>(binding.source).declaration =
				DeclarationState::kDeclared;
			SnapshotSource source;
			BindPage(page, source, [] { return McmState{ true, false }; });
			page.settings.prepareView(page.settings);

			require(
				page.rows.front().resolveInertState().governingReason ==
						InertReason::kRuntimeNotReady &&
					SummarizeActionableCompatibility(page).empty(),
				"a pre-save runtime state created a permanent startup warning");
		});

		runner.test("MCM failed conditions fail open with a diagnostic", [] {
			auto page = ConditionPage();
			SnapshotSource source;
			source.snapshot = FailedValue{ 3 };
			BindPage(page, source);
			page.settings.prepareView(page.settings);

			const auto summary = SummarizeCompatibility(page, source);
			require(Dependent(page).isVisible &&
					Dependent(page).isVisible() &&
					summary.unevaluableConditions == 1,
				"a failed dependency hid its dependent");
			require(HasConditionNote(page),
				"a failed dependency produced no page diagnostic");
		});

		runner.test("MCM condition notes survive unrelated note reordering", [] {
			auto page = ConditionPage();
			SnapshotSource source;
			source.snapshot = MissingValue{ 1 };
			BindPage(page, source);
			page.settings.prepareView(page.settings);
			page.settings.notes.insert(
				page.settings.notes.begin(),
				{ "Fixture note", false, "fixture.note" });
			source.snapshot = ReadyValue{ true, 2 };
			page.settings.prepareView(page.settings);

			require(!HasConditionNote(page) &&
					std::ranges::any_of(
						page.settings.notes,
						[](const dmui::SettingsPageNote& a_note) {
							return a_note.noteId == "fixture.note";
						}),
				"condition note removal depended on a mutable vector index");
		});
	}
}
