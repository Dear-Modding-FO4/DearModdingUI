#include <DearModdingUI/MCM/ValueSource.h>

#include "Harness.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI::MCM;

		constexpr std::string_view kBindingConfig = R"json({
			"modName": "BindingExample",
			"displayName": "Binding Example",
			"content": [
				{"id":"bGlobalSwitch:Main","text":"Global switch","type":"switcher",
				 "valueOptions":{"sourceType":"GlobalValue",
				 "sourceForm":"ExampleCore.esp|800","default":false}},
				{"id":"fGlobalSlider:Main","text":"Global slider","type":"slider",
				 "valueOptions":{"sourceType":"GlobalValue",
				 "sourceForm":"ExampleCore.esp|801","min":0,"max":10,"default":1}},
				{"id":"bStoredSwitch:Main","text":"Stored switch","type":"switcher",
				 "valueOptions":{"sourceType":"ModSettingBool","default":false}}
			]
		})json";

		constexpr std::string_view kMixedSourceConfig = R"json({
			"modName":"MixedSourceFixture",
			"displayName":"Mixed Source Fixture",
			"pages":[{
				"id":"workshop",
				"pageDisplayName":"Workshop",
				"content":[
					{"id":"fGlobalRange","text":"Global range","type":"slider",
					 "valueOptions":{"sourceType":"GlobalValue",
					 "sourceForm":"Fixture.esp|801","min":0,"max":10,"default":1}},
					{"id":"bStoredOption:Main","text":"Stored option","type":"switcher",
					 "valueOptions":{"sourceType":"ModSettingBool","default":false}}
				]
			}]
		})json";

		class FakeValueSource final : public ValueSource
		{
		public:
			explicit FakeValueSource(SourceFamily a_supported) :
				supported_(a_supported)
			{}

			[[nodiscard]] bool Supports(
				SourceFamily a_family) const noexcept override
			{
				return a_family == supported_;
			}

			[[nodiscard]] ValueSnapshot Read(
				const MappedBinding& a_binding) const override
			{
				++reads;
				if (forced)
					return *forced;
				const auto entry = values_.find(a_binding.descriptorId);
				return entry == values_.end() ?
					ValueSnapshot{ MissingValue{ generation } } :
					ValueSnapshot{ ReadyValue{ entry->second, generation } };
			}

			[[nodiscard]] uint64_t Refresh(const MappedBinding&) override
			{
				++refreshes;
				return ++generation;
			}

			[[nodiscard]] ValueSnapshot Write(
				const MappedBinding& a_binding,
				const dmui::SettingValue& a_value) override
			{
				++writes;
				if (readOnly)
					return Read(a_binding);
				auto effective = a_value;
				if (quantized)
					effective = 2.0;
				values_[a_binding.descriptorId] = effective;
				++generation;
				return ReadyValue{ std::move(effective), generation };
			}

			void Seed(std::string a_id, dmui::SettingValue a_value)
			{
				values_[std::move(a_id)] = std::move(a_value);
			}

			mutable size_t reads{};
			size_t refreshes{};
			size_t writes{};
			bool readOnly{};
			bool quantized{};
			uint64_t generation{};
			std::optional<ValueSnapshot> forced;

		private:
			SourceFamily supported_;
			std::unordered_map<std::string, dmui::SettingValue> values_;
		};

		[[nodiscard]] dmui::SettingDescriptor& BoundSetting(
			MappedPage& a_page,
			std::string_view a_id)
		{
			for (auto& group : a_page.settings.groups)
			{
				for (auto& setting : group.settings)
				{
					if (setting.id == a_id)
						return setting;
				}
			}
			throw Failure("missing descriptor " + std::string{ a_id });
		}

		[[nodiscard]] MappedPage LoadBindingPage()
		{
			auto result = ParseConfig(kBindingConfig, "binding-config.json");
			if (result.pages.empty())
				throw Failure("binding config produced no pages");
			return std::move(result.pages.front());
		}
	}

	void run_mcm_binding_checks(Runner& runner)
	{
		runner.test("MCM bindings route reads and writes through the source", [] {
			auto page = LoadBindingPage();
			FakeValueSource source{ SourceFamily::kGlobal };
			source.Seed("bGlobalSwitch:Main", true);
			BindPage(page, source);

			auto& setting = BoundSetting(page, "bGlobalSwitch:Main");
			require(setting.binding.get && setting.binding.set,
				"a supported descriptor was left unbound");
			require(std::get<bool>(setting.binding.get()),
				"the seeded source value did not reach the descriptor");

			const auto applied = setting.binding.set(dmui::SettingValue{ false });
			require(!std::get<bool>(applied) && source.writes == 1,
				"the write did not route through the source");
			require(!std::get<bool>(setting.binding.get()),
				"the written value was not read back");
		});

		runner.test("MCM bindings never refresh while reading", [] {
			auto page = LoadBindingPage();
			FakeValueSource source{ SourceFamily::kGlobal };
			source.Seed("fGlobalSlider:Main", 4.0);
			BindPage(page, source);

			auto& setting = BoundSetting(page, "fGlobalSlider:Main");
			for (auto frame = 0; frame < 64; ++frame)
				(void)setting.binding.get();
			(void)setting.binding.set(dmui::SettingValue{ 6.0 });

			require(source.reads >= 64,
				"the descriptor stopped consulting the source");
			require(source.refreshes == 0,
				"drawing a row triggered a dispatching refresh");
		});

		runner.test("MCM bindings disable unsupported families", [] {
			auto page = LoadBindingPage();
			FakeValueSource source{ SourceFamily::kGlobal };
			BindPage(page, source);

			auto& setting = BoundSetting(page, "bStoredSwitch:Main");
			require(setting.isEnabled && !setting.isEnabled(),
				"an unsupported descriptor stayed enabled");
			require(setting.binding.get && setting.binding.set,
				"an unsupported descriptor was left unbound");
			require(!setting.showReset,
				"an unsupported descriptor offered a reset");

			const auto applied = setting.binding.set(dmui::SettingValue{ true });
			require(!std::get<bool>(applied) && source.writes == 0,
				"an unsupported descriptor reached the source");
		});

		runner.test("MCM bindings survive absent and mistyped source values", [] {
			auto page = LoadBindingPage();
			FakeValueSource source{ SourceFamily::kGlobal };
			source.Seed("bGlobalSwitch:Main", std::string{ "not a bool" });
			BindPage(page, source);

			require(!std::get<bool>(
						BoundSetting(page, "bGlobalSwitch:Main").binding.get()),
				"a mistyped source value was not replaced by the default");
			require(!BoundSetting(
						page,
						"bGlobalSwitch:Main").isEnabled(),
				"a mistyped source value did not mark the row unavailable");
			require(std::get<double>(
						BoundSetting(page, "fGlobalSlider:Main").binding.get()) ==
					1.0,
				"an absent source value was not replaced by the default");
		});

		runner.test("MCM bindings keep the stored value when a write fails", [] {
			auto page = LoadBindingPage();
			FakeValueSource source{ SourceFamily::kGlobal };
			source.Seed("bGlobalSwitch:Main", true);
			source.readOnly = true;
			BindPage(page, source);

			auto& setting = BoundSetting(page, "bGlobalSwitch:Main");
			const auto applied = setting.binding.set(dmui::SettingValue{ false });
			require(std::get<bool>(applied),
				"a rejected write did not report the effective value");
			require(source.writes == 1,
				"the rejected write never reached the source");
		});

		runner.test(
			"MCM mixed global and modsetting page is fully operable",
			[] {
				auto result = ParseConfig(
					kMixedSourceConfig,
					"mixed-source-config.json");
				require(result.pages.size() == 1,
					"mixed-source fixture did not map one page");
				auto page = std::move(result.pages.front());
				FakeValueSource globals{ SourceFamily::kGlobal };
				FakeValueSource settings{ SourceFamily::kModSetting };
				globals.Seed("fGlobalRange", 4.0);
				settings.Seed("bStoredOption:Main", true);
				CompositeValueSource source;
				source.Add(globals);
				source.Add(settings);
				BindPage(page, source);

				auto& global = BoundSetting(page, "fGlobalRange");
				auto& modSetting = BoundSetting(page, "bStoredOption:Main");
				require((!global.isEnabled || global.isEnabled()) &&
						std::get<double>(global.binding.get()) == 4.0,
					"supported global control was not operable");
				require(modSetting.isEnabled && modSetting.isEnabled() &&
						std::get<bool>(modSetting.binding.get()),
					"supported modsetting control was not operable");
			});

		runner.test("MCM writes report the effective quantized value", [] {
			auto page = LoadBindingPage();
			FakeValueSource source{ SourceFamily::kGlobal };
			source.Seed("fGlobalSlider:Main", 1.0);
			source.quantized = true;
			BindPage(page, source);

			const auto applied = BoundSetting(
				page,
				"fGlobalSlider:Main").binding.set(dmui::SettingValue{ 8.0 });
			require(std::get<double>(applied) == 2.0 &&
					source.generation == 1,
				"effective quantized value or generation was lost");
		});

		runner.test("MCM pending values stay drawable and disable their row", [] {
			auto page = LoadBindingPage();
			FakeValueSource source{ SourceFamily::kGlobal };
			source.forced = PendingValue{ 17 };
			BindPage(page, source);

			auto& setting = BoundSetting(page, "bGlobalSwitch:Main");
			require(!std::get<bool>(setting.binding.get()) &&
					setting.isEnabled && !setting.isEnabled(),
				"pending state was not mapped to a disabled drawable fallback");
			require(Generation(*source.forced) == 17,
				"pending request generation was lost");
		});

		runner.test("MCM snapshot failures have distinct authoritative reasons", [] {
			auto page = LoadBindingPage();
			FakeValueSource source{ SourceFamily::kGlobal };
			source.forced = PendingValue{ 1 };
			BindPage(page, source);
			auto& setting = BoundSetting(page, "bGlobalSwitch:Main");
			require(
				setting.resolveDescription() ==
					"Waiting for this setting's value.",
				"pending value reason was not authoritative");
			source.forced = MissingValue{ 2 };
			require(
				setting.resolveDescription() ==
					"This setting's value is unavailable.",
				"missing value reason was not distinct");
			source.forced = FailedValue{ 3 };
			require(
				setting.resolveDescription() ==
					"This setting's value could not be read.",
				"failed value reason was not distinct");
		});

		runner.test("MCM hidden controls drive visibility from snapshots", [] {
			auto result = ParseConfig(R"json({
				"modName":"ConditionFixture",
				"content":[
					{"id":"bController:Main","type":"hiddenSwitcher","groupControl":7,
					 "valueOptions":{"sourceType":"ModSettingBool","default":false}},
					{"id":"dependent","type":"text","text":"Dependent","groupCondition":7}
				]
			})json", "condition-fixture.json");
			auto page = std::move(result.pages.front());
			FakeValueSource source{ SourceFamily::kModSetting };
			source.Seed("bController:Main", true);
			BindPage(page, source);

			auto& dependent = BoundSetting(page, "dependent");
			require(dependent.isVisible && dependent.isVisible(),
				"a ready hidden controller did not reveal its dependent");
			source.forced = PendingValue{ 3 };
			require(!dependent.isVisible(),
				"a pending hidden controller used its false default");
			require(source.refreshes == 0,
				"visibility evaluation dispatched a refresh");
		});

		runner.test("MCM inert undeclared toggles stay disabled with a reason", [] {
			auto page = LoadBindingPage();
			auto& binding = *std::ranges::find_if(
				page.rows,
				[](const MappedRow& a_row) {
					return a_row.binding &&
						a_row.binding->Family() == SourceFamily::kModSetting;
				})->binding;
			std::get<ModSettingBinding>(binding.source).declaration =
				DeclarationState::kUndeclared;
			FakeValueSource source{ SourceFamily::kModSetting };
			source.Seed(binding.descriptorId, false);
			BindPage(page, source);

			auto& setting = BoundSetting(page, binding.descriptorId);
			require(setting.isEnabled && !setting.isEnabled(),
				"an undeclared setting stayed enabled");
			(void)setting.binding.set(dmui::SettingValue{ true });
			require(source.writes == 0 &&
					setting.resolveDescription &&
					setting.resolveDescription().find("not declared") !=
						std::string::npos,
				"an inert undeclared toggle became local or lacked a reason");
		});
	}
}
