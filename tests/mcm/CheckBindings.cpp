#include <DearModdingUI/MCM/ValueSource.h>

#include "../Harness.h"

#include <cmath>
#include <limits>
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
				 "sourceForm":"ExampleCore.esp|801","min":0,"max":10,"step":1,"default":1}},
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
					 "sourceForm":"Fixture.esp|801","min":0,"max":10,"step":1,"default":1}},
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
				values_[a_binding.descriptorId] = effective;
				++generation;
				return ReadyValue{ std::move(effective), generation };
			}

			void Seed(std::string a_id, dmui::SettingValue a_value)
			{
				values_[std::move(a_id)] = std::move(a_value);
			}

			size_t refreshes{};
			size_t writes{};
			bool readOnly{};
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
			require(source.refreshes == 0,
				"reading or writing a row triggered a dispatching refresh");
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
			auto& unsupported = BoundSetting(page, "bStoredSwitch:Main");
			require(unsupported.isEnabled && !unsupported.isEnabled() &&
					unsupported.binding.get && unsupported.binding.set &&
					!unsupported.showReset,
				"an unsupported descriptor stayed operable or resettable");
			const auto applied =
				unsupported.binding.set(dmui::SettingValue{ true });
			require(!std::get<bool>(applied) && source.writes == 0,
				"an unsupported descriptor reached the source");

			source.forced = PendingValue{ 17 };
			require(!BoundSetting(page, "bGlobalSwitch:Main").isEnabled(),
				"a pending source value left the row operable");
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

		runner.test("MCM sliders clamp before zero-anchored Math.round snapping", [] {
			auto result = ParseConfig(R"json({
				"modName":"SliderNormalization",
				"content":[
					{"id":"positive","type":"slider","valueOptions":{
						"sourceType":"GlobalValueFloat",
						"sourceForm":"Fixture.esp|1",
						"min":0.1,"max":0.9,"step":0.2}},
					{"id":"negative","type":"slider","valueOptions":{
						"sourceType":"GlobalValueFloat",
						"sourceForm":"Fixture.esp|2",
						"min":-1,"max":1,"step":1}},
					{"id":"upperBoundary","type":"slider","valueOptions":{
						"sourceType":"GlobalValueInt",
						"sourceForm":"Fixture.esp|3",
						"min":-9223372036854775808,
						"max":9223372036854774784,
						"step":2048}},
					{"id":"lowerBoundary","type":"slider","valueOptions":{
						"sourceType":"GlobalValueInt",
						"sourceForm":"Fixture.esp|4",
						"min":-9223372036854775808,
						"max":0,
						"step":3}},
					{"id":"halfStep","type":"slider","valueOptions":{
						"sourceType":"GlobalValueInt",
						"sourceForm":"Fixture.esp|5",
						"min":-10,
						"max":10,
						"step":2}}
				]
			})json", "slider-normalization.json");
			auto page = std::move(result.pages.front());
			FakeValueSource source{ SourceFamily::kGlobal };
			source.Seed("positive", 0.5);
			source.Seed("negative", -0.5);
			source.Seed("upperBoundary", int64_t{ 0 });
			source.Seed("lowerBoundary", int64_t{ 0 });
			source.Seed("halfStep", int64_t{ 0 });
			BindPage(page, source);

			auto effective = BoundSetting(page, "positive").binding.set(
				dmui::SettingValue{ -1.0 });
			require(std::abs(std::get<double>(effective) - 0.2) < 1e-9,
				"slider snapped before clamping to its non-grid minimum");
			effective = BoundSetting(page, "positive").binding.set(
				dmui::SettingValue{ 0.5 });
			require(std::abs(std::get<double>(effective) - 0.6) < 1e-9,
				"slider quantization was not anchored at zero");
			effective = BoundSetting(page, "negative").binding.set(
				dmui::SettingValue{ -0.5 });
			require(std::get<double>(effective) == 0.0,
				"negative half-step did not use ActionScript Math.round semantics");

			const auto maximum = (std::numeric_limits<int64_t>::max)();
			const auto minimum = (std::numeric_limits<int64_t>::min)();
			effective = BoundSetting(page, "upperBoundary").binding.set(
				dmui::SettingValue{ maximum });
			require(std::get<int64_t>(effective) == int64_t{ 9223372036854773760 },
				"positive half-step overflow did not choose the nearest grid point");
			effective = BoundSetting(page, "lowerBoundary").binding.set(
				dmui::SettingValue{ minimum });
			require(std::get<int64_t>(effective) == minimum + 2,
				"negative snap overflow did not choose the nearest grid point");
			effective = BoundSetting(page, "halfStep").binding.set(
				dmui::SettingValue{ int64_t{ -5 } });
			require(std::get<int64_t>(effective) == -4,
				"negative integer half-step stopped rounding toward positive infinity");
		});

		runner.test("MCM slider resets converge on their effective defaults", [] {
			auto result = ParseConfig(R"json({
				"modName":"SliderDefaults",
				"displayName":"Slider defaults",
				"content":[
					{"id":"floating","type":"slider","valueOptions":{
						"sourceType":"GlobalValueFloat","sourceForm":"Fixture.esp|1",
						"min":0.1,"max":0.9,"step":0.2,"default":0.5}},
					{"id":"integer","type":"slider","valueOptions":{
						"sourceType":"GlobalValueInt","sourceForm":"Fixture.esp|2",
						"min":1,"max":10,"step":3,"default":5}}
				]
			})json");
			require(result.diagnostics.empty(), "valid slider defaults were rejected");
			auto page = std::move(result.pages.front());
			FakeValueSource source{ SourceFamily::kGlobal };
			source.Seed("floating", 0.8);
			source.Seed("integer", int64_t{ 9 });
			BindPage(page, source);
			size_t edits{};
			for (const auto id : { "floating", "integer" })
			{
				auto& setting = BoundSetting(page, id);
				setting.onEdit = [&](const dmui::SettingEditEvent& event) {
					require(event.changed && event.completed, "reset edit was not completed");
					++edits;
				};
				const auto before = source.writes;
				const auto reset = dmui::ResetSettingToDefault(setting);
				require(reset && *reset == setting.defaultValue &&
						dmui::IsSettingDefault(setting, setting.binding.get()) &&
						source.writes == before + 1,
					"slider reset did not reach the effective default");
				(void)dmui::ResetSettingToDefault(setting);
				require(source.writes == before + 1,
					"already-default slider reset repeated its write");
			}
			require(edits == 2 &&
					std::abs(std::get<double>(
						BoundSetting(page, "floating").defaultValue) - 0.6) < 1e-9 &&
					std::get<int64_t>(BoundSetting(page, "integer").defaultValue) == 6,
				"slider defaults did not use the same zero-grid normalization as edits");
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
