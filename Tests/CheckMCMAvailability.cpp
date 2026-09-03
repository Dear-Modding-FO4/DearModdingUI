#include <DearModdingUI/MCM/Availability.h>

#include "Harness.h"

#include <array>
#include <tuple>

namespace vmm_tests
{
	using namespace DearModdingUI::MCM;

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
	}
}
