#include <DearModdingUI/MCM/GlobalValue.h>

#include "../Harness.h"

#include <cmath>
#include <limits>

namespace vmm_tests
{
	using namespace DearModdingUI::MCM;

	void run_mcm_global_value_checks(Runner& runner)
	{
		runner.test("MCM global form references parse valid and reject malformed input", [] {
			const auto reference =
				ParseGlobalFormReference("Plugin.esp|F99");
			require(reference.has_value(), "valid sourceForm was rejected");
			require(reference->plugin == "Plugin.esp",
				"plugin name was not preserved");
			require(reference->localId == 0xF99,
				"local form id was not parsed as hexadecimal");

			for (const auto source : {
					 "",
					 "Plugin.esp",
					 "|F99",
					 "Plugin.esp|",
					 "Plugin.esp|xyz",
					 "Plugin.esp|F99|1",
					 "Plugin.esp|100000000"
				 })
			{
				require(!ParseGlobalFormReference(source),
					std::string{ "malformed sourceForm was accepted: " } + source);
			}
		});

		runner.test("MCM globals reject out-of-range integral reads", [] {
			require(!GlobalToSettingValue(
						-1.0f, dmui::SettingValue{ uint64_t{} }),
				"negative global became an unsigned integer");
			require(!GlobalToSettingValue(
						(std::numeric_limits<float>::infinity)(),
						dmui::SettingValue{ int64_t{} }),
				"non-finite global became a signed integer");
			require(!SettingValueToGlobal(
						dmui::SettingValue{
							(std::numeric_limits<double>::infinity)() }),
				"non-finite descriptor value was accepted");
		});

		runner.test("MCM global choices translate only numeric index strings", [] {
			const auto read =
				GlobalToSettingValue(2.0f, dmui::SettingValue{ std::string{} });
			require(read && std::get<std::string>(*read) == "2",
				"global choice did not become an index string");
			require(SettingValueToGlobal(
						dmui::SettingValue{ std::string{ "7" } }) == 7.0f,
				"global choice index did not become a float");
			require(!SettingValueToGlobal(
						dmui::SettingValue{ std::string{ "61 (FX) slot" } }),
				"global choice accepted an option label instead of an index");
			require(!SettingValueToGlobal(
						dmui::SettingValue{ std::string{ "2 trailing" } }),
				"global choice accepted a partially numeric string");
		});
	}
}
