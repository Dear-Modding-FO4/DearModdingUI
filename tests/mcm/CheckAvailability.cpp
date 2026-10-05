#include <DearModdingUI/MCM/Availability.h>
#include <DearModdingUI/MCM/ValueSource.h>

#include "../Harness.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <tuple>

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
				std::tuple{ McmState{ false, false }, false, InertReason::kMcmNotInstalled },
				std::tuple{ McmState{ false, true }, false, InertReason::kMcmNotInstalled },
				std::tuple{ McmState{ true, false }, false, InertReason::kRuntimeNotReady },
				std::tuple{ McmState{ true, true }, true, InertReason::kNone }
			};
			for (const auto& [next, enabled, reason] : cases)
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
						inert.governingReason == reason,
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
							InertReason::kNone,
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


	}
}
