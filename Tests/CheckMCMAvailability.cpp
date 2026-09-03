#include <DearModdingUI/MCM/Availability.h>
#include <DearModdingUI/MCM/ValueSource.h>

#include "Harness.h"

#include <array>
#include <tuple>

namespace vmm_tests
{
	using namespace DearModdingUI::MCM;

	namespace
	{
		class AvailabilitySource final : public ValueSource
		{
		public:
			[[nodiscard]] bool Supports(SourceFamily a_family) const noexcept override
			{
				return a_family == SourceFamily::kModSetting;
			}

			[[nodiscard]] ValueSnapshot Read(const MappedBinding&) const override
			{
				return ReadyValue{ false };
			}

			[[nodiscard]] uint64_t Refresh(const MappedBinding&) override
			{
				return 1;
			}

			[[nodiscard]] ValueSnapshot Write(
				const MappedBinding&,
				const dmui::SettingValue& a_value) override
			{
				return ReadyValue{ a_value, 1 };
			}

			void Pump() noexcept override {}
		};

		[[nodiscard]] MappedPage AvailabilityPage(bool a_local)
		{
			auto result = ParseConfig(a_local ? R"({
				"modName":"Availability",
				"content":[
					{"id":"controller","type":"switcher","groupControl":1,
					 "valueOptions":{"sourceType":"ModSettingBool","default":false}},
					{"id":"dependent","type":"text","text":"Dependent",
					 "groupCondition":1}
				]
			})" : R"({
				"modName":"Availability",
				"content":[
					{"id":"declared","type":"switcher",
					 "valueOptions":{"sourceType":"ModSettingBool","default":false}}
				]
			})");
			auto page = std::move(result.pages.front());
			auto& binding = *page.rows.front().binding;
			std::get<ModSettingBinding>(binding.source).declaration =
				a_local ?
					DeclarationState::kUndeclared :
					DeclarationState::kDeclared;
			return page;
		}
	}

	void run_mcm_availability_checks(Runner& runner)
	{
		runner.test("MCM availability gates only MCM-dependent families", [] {
			constexpr std::array cases{
				std::tuple{ AvailabilityState::kPresent,
					SourceFamily::kGlobal, true },
				std::tuple{ AvailabilityState::kPresent,
					SourceFamily::kModSetting, true },
				std::tuple{ AvailabilityState::kPresent,
					SourceFamily::kProperty, true },
				std::tuple{ AvailabilityState::kAbsent,
					SourceFamily::kGlobal, true },
				std::tuple{ AvailabilityState::kAbsent,
					SourceFamily::kModSetting, false },
				std::tuple{ AvailabilityState::kAbsent,
					SourceFamily::kProperty, true },
				std::tuple{ AvailabilityState::kUnknown,
					SourceFamily::kGlobal, true },
				std::tuple{ AvailabilityState::kUnknown,
					SourceFamily::kModSetting, false },
				std::tuple{ AvailabilityState::kUnknown,
					SourceFamily::kProperty, true }
			};

			for (const auto& [availability, family, expected] : cases)
			{
				require(IsControlOperable(availability, family) == expected,
					"availability and source-family matrix changed");
			}
		});

		runner.test("MCM plugin pipeline keeps local UI state availability-independent", [] {
			auto page = AvailabilityPage(true);
			AvailabilitySource source;
			BindPage(page, source);
			auto availability = AvailabilityState::kUnknown;
			ComposeMcmAvailability(page, [&] { return availability; });
			auto& descriptor = page.settings.groups.front().settings.front();

			for (const auto state : {
					 AvailabilityState::kUnknown,
					 AvailabilityState::kAbsent,
					 AvailabilityState::kPresent })
			{
				availability = state;
				require(page.rows.front().valueRoute ==
							ValueRoute::kLocalUiState &&
						descriptor.isEnabled && descriptor.isEnabled() &&
						descriptor.resolveDescription &&
						descriptor.resolveDescription().empty(),
					"availability disabled or annotated local UI state");
			}
			(void)descriptor.binding.set(dmui::SettingValue{ true });
			require(std::get<bool>(descriptor.binding.get()),
				"availability composition replaced the local binding");
		});

		runner.test("MCM plugin pipeline explains unavailable source routes", [] {
			auto page = AvailabilityPage(false);
			AvailabilitySource source;
			BindPage(page, source);
			auto availability = AvailabilityState::kUnknown;
			ComposeMcmAvailability(page, [&] { return availability; });
			auto& descriptor = page.settings.groups.front().settings.front();

			require(page.rows.front().valueRoute == ValueRoute::kSource &&
					descriptor.isEnabled && !descriptor.isEnabled() &&
					descriptor.resolveDescription().find("not been determined") !=
						std::string::npos,
				"unknown MCM availability disabled without an explanation");
			availability = AvailabilityState::kAbsent;
			require(!descriptor.isEnabled() &&
					descriptor.resolveDescription().find("not installed") !=
						std::string::npos,
				"absent MCM disabled without an explanation");
			availability = AvailabilityState::kPresent;
			require(descriptor.isEnabled() &&
					descriptor.resolveDescription().empty(),
				"present MCM remained disabled or explained as unavailable");
		});
	}
}
