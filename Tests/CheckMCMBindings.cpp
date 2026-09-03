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

			[[nodiscard]] std::optional<dmui::SettingValue> Read(
				const MappedBinding& a_binding) const override
			{
				++reads;
				const auto entry = values_.find(a_binding.descriptorId);
				return entry == values_.end() ?
					std::nullopt :
					std::optional{ entry->second };
			}

			void Refresh(const MappedBinding&) override
			{
				++refreshes;
			}

			[[nodiscard]] bool Write(
				const MappedBinding& a_binding,
				const dmui::SettingValue& a_value) override
			{
				++writes;
				if (readOnly)
					return false;
				values_[a_binding.descriptorId] = a_value;
				return true;
			}

			void Seed(std::string a_id, dmui::SettingValue a_value)
			{
				values_[std::move(a_id)] = std::move(a_value);
			}

			mutable size_t reads{};
			size_t refreshes{};
			size_t writes{};
			bool readOnly{};

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
	}
}
