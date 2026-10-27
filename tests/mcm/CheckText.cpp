#include "../support/MCMTestSupport.h"

#include <DearModdingUI/MCM/PageLookup.h>

#include <algorithm>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace vmm_tests
{
	using namespace DearModdingUI::MCM;
	using namespace support::mcm;

	namespace
	{
		namespace Support = DearModdingUI::Support;

		// Tags every translated template so tests see which text was rendered.
		class TaggedCatalog final : public Support::TextCatalog
		{
		public:
			[[nodiscard]] std::optional<std::string_view> Find(
				std::string_view a_key) const noexcept override
			{
				const auto found = texts.find(a_key);
				if (found == texts.end())
					return std::nullopt;
				return std::string_view{ found->second };
			}

			[[nodiscard]] Support::PluralCategory Plural(
				uint64_t a_count) const noexcept override
			{
				return Support::EnglishCatalog().Plural(a_count);
			}

			std::map<std::string, std::string, std::less<>> texts;
		};

		class SnapshotSource final : public ValueSource
		{
		public:
			[[nodiscard]] bool Supports(SourceFamily) const noexcept override
			{
				return true;
			}

			[[nodiscard]] ValueSnapshot Read(const MappedBinding&) const override
			{
				return snapshot;
			}

			[[nodiscard]] uint64_t Refresh(const MappedBinding&) override
			{
				return Generation(snapshot);
			}

			[[nodiscard]] ValueSnapshot Write(
				const MappedBinding&,
				const dmui::SettingValue&) override
			{
				return snapshot;
			}

			ValueSnapshot snapshot{ PendingValue{ 1 } };
		};

		constexpr auto kBridgeConfig = R"json({
			"modName":"BridgeText",
			"content":[
				{"id":"bController:Main","type":"hiddenSwitcher","groupControl":1,
				 "valueOptions":{"sourceType":"ModSettingBool","default":false}},
				{"id":"first","type":"text","text":"First","groupCondition":1},
				{"id":"second","type":"text","text":"Second","groupCondition":1},
				{"id":"go","type":"button","text":"Go","groupCondition":1,
				 "action":{"type":"CallFunction","form":"Fixture.esp|1","function":"Apply"}}
			],
			"pages":[{"content":[
				{"id":"plain","type":"text","text":"Plain"},
				{"id":"idle","type":"button","text":"Idle"}
			]}]
		})json";

		[[nodiscard]] const dmui::SettingsPageNote* FindNote(
			const MappedPage& a_page,
			std::string_view a_id)
		{
			const auto found = std::ranges::find(
				a_page.settings.notes,
				a_id,
				&dmui::SettingsPageNote::noteId);
			return found == a_page.settings.notes.end() ? nullptr : &*found;
		}
	}

	void run_mcm_text_checks(Runner& runner)
	{
		runner.test("MCM display localization preserves identities and stored values", [] {
			const TextResolver resolver = [](std::string_view a_key)
				-> std::optional<std::string> {
				if (a_key == "$CLIENT NAME")
					return "Localized Client";
				if (a_key == "$LABEL")
					return "Localized Label";
				if (a_key == "$OPTION")
					return "Localized Option";
				return std::nullopt;
			};
			constexpr auto json = R"json({
				"modName":"IdentityMod",
				"displayName":"$CLIENT NAME",
				"content":[
					{"id":"sMode:Main","type":"dropdown","text":"$LABEL",
					 "valueOptions":{"sourceType":"ModSettingString",
					 "default":"$OPTION","options":["$OPTION","literal"]}},
					{"id":"action","type":"button","text":"$LABEL",
					 "action":{"type":"SendEvent","event":"$EVENT","params":["$ARG"]}},
					{"id":"rich","type":"text",
					 "text":"<b>Prefix</b> $OPTION","html":true},
					{"id":"literal","type":"text",
					 "text":"<b>Prefix</b> $OPTION"}
				]
			})json";
			const auto result = ParseConfig(json, "localized.json", resolver);
			require(result.configuration && result.pages.size() == 1 &&
					result.diagnostics.empty(),
				"localized fixture did not map cleanly");
			require(result.configuration->modName == "IdentityMod" &&
					result.configuration->displayName == "$CLIENT NAME" &&
					result.displayName == "Localized Client" &&
					result.pages.front().id == "main",
				"localized presentation changed raw configuration identity");

			const auto& root = result.pages.front();
			const auto& choice = SettingNamed(root, "sMode:Main");
			const auto* choiceControl =
				std::get_if<dmui::ChoiceSettingControl>(&choice.control);
			require(choice.label == "Localized Label" &&
					choiceControl &&
					choiceControl->options.size() == 2 &&
					choiceControl->options[0].value == "$OPTION" &&
					choiceControl->options[0].label == "Localized Option" &&
					std::get<std::string>(choice.defaultValue) == "$OPTION",
				"choice localization changed its stored value or default");

			const auto& action =
				std::get<SendEventAction>(*RowNamed(root, "action").action);
			require(action.event == "$EVENT" &&
					std::get<std::string>(action.arguments.front()) == "$ARG",
				"localization changed raw action arguments");
			const auto& rich = SettingNamed(root, "rich");
			const auto& literal = SettingNamed(root, "literal");
			require(std::get<std::string>(rich.defaultValue) ==
						"Prefix Localized Option" &&
					std::get<std::string>(literal.defaultValue) ==
						"<b>Prefix</b> $OPTION",
				"prose HTML opt-in or literal markup mapping changed");
		});

		runner.test("MCM failed text resolvers preserve tokens", [] {
			const TextResolver failed =
				[](std::string_view) -> std::optional<std::string> {
					throw std::runtime_error("resolver offline");
				};
			const auto failure = ParseConfig(R"json({
				"modName":"Failure","displayName":"$TITLE",
				"content":[{"id":"text","type":"text","text":"$BODY"}]
			})json", "failed-resolver.json", failed);
			require(ErrorCount(failure) == 1 &&
					HasDiagnostic(
						failure,
						"text resolver failed; localization keys were preserved") &&
					failure.pages.front().displayName == "$TITLE" &&
					std::get<std::string>(
						SettingNamed(failure.pages.front(), "text").defaultValue) ==
						"$BODY",
				"a failed resolver discarded tokens or masqueraded as missing keys");
		});

		runner.test("MCM mapped fallbacks follow the supplied catalog and ids stay English", [] {
			TaggedCatalog catalog;
			catalog.texts = {
				{ "$DMUI_MCM_General", "x:General" },
				{ "$DMUI_MCM_ButtonNoAction", "x:No action" },
				{ "$DMUI_MCM_PageName", "x:Page {}" }
			};
			const auto english = ParseConfig(kBridgeConfig, "bridge.json");
			auto translated =
				ParseConfig(kBridgeConfig, "bridge.json", {}, catalog);
			require(english.pages.size() == 2 && translated.pages.size() == 2,
				"bridge fixture did not map two pages");
			require(english.pages[0].settings.groups.front().label == "General" &&
					english.pages[1].displayName == "Page 1",
				"the default catalog changed the English fallbacks");
			const auto& group = translated.pages[0].settings.groups.front();
			require(group.label == "x:General" &&
					group.id == "general" &&
					translated.pages[1].displayName == "x:Page 1" &&
					translated.pages[1].id == "page-1",
				"fallback labels were not translated or an id followed the language");
			const auto* idle = FindActionRow(translated.pages[1].settings, "idle");
			require(idle && idle->description == "x:No action",
				"an action-less button kept English text");
		});

		runner.test("MCM bound rows and notes render in the supplied language every frame", [] {
			TaggedCatalog catalog;
			catalog.texts = {
				{ "$DMUI_MCM_ReasonConditionPending", "x:Waiting" },
				{ "$DMUI_MCM_LoadingVisibility", "x:Loading" },
				{ "$DMUI_MCM_ConditionsNotApplied", "x:unapplied {}" },
				{ "$DMUI_MCM_CompatibilitySummary", "x:Summary {}" }
			};
			auto result = ParseConfig(kBridgeConfig, "bridge.json");
			auto page = std::move(result.pages.front());
			SnapshotSource source;
			BindPage(
				page,
				source,
				[] { return McmState{ true, true }; },
				catalog);

			// A pending controller hides every dependent row, buttons included.
			page.settings.prepareView(page.settings);
			const auto* first = FindSettingDescriptor(page.settings, "first");
			const auto* goRow = FindActionRow(page.settings, "go");
			require(goRow, "the bound button row was missing");
			const auto& go = *goRow;
			const auto* loading =
				FindNote(page, "dearmodding.mcm.condition.pending");
			require(first && first->resolveDescription &&
					first->resolveDescription() == "x:Waiting" &&
					go.description == "x:Waiting" &&
					loading && loading->text == "x:Loading",
				"pending-state text ignored the supplied catalog");

			// Once the controller resolves, a button must not keep its stale sentence.
			source.snapshot = ReadyValue{ true, 2 };
			page.settings.prepareView(page.settings);
			require(go.description.empty() &&
					!FindNote(page, "dearmodding.mcm.condition.pending"),
				"an action row kept the explanation it had when it was bound");

			// A failed controller shows its content and counts the unapplied conditions.
			source.snapshot = FailedValue{ 3 };
			page.settings.prepareView(page.settings);
			const auto* unapplied =
				FindNote(page, "dearmodding.mcm.condition.unavailable");
			require(unapplied && unapplied->text == "x:unapplied 3",
				"the unapplied-condition note did not render through the catalog");

			for (auto& row : page.rows)
			{
				if (row.id == "go")
					row.actionInertReason = InertReason::kUnsupportedAction;
			}
			const auto summary = SummarizeActionableCompatibility(page);
			require(summary.English() == "Compatibility: unsupported actions: 1." &&
					summary.Render(catalog) == "x:Summary unsupported actions: 1",
				"the summary did not render per viewer");
		});

	}
}
