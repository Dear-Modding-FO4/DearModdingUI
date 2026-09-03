#include <DearModdingUI/MCM/GlobalValue.h>

#include "Harness.h"

#include <cmath>
#include <limits>

namespace vmm_tests
{
	using namespace DearModdingUI::MCM;

	void run_mcm_global_value_checks(Runner& runner)
	{
		runner.test("MCM global form references parse hexadecimal local ids", [] {
			const auto reference =
				ParseGlobalFormReference("Plugin.esp|F99");
			require(reference.has_value(), "valid sourceForm was rejected");
			require(reference->plugin == "Plugin.esp",
				"plugin name was not preserved");
			require(reference->localId == 0xF99,
				"local form id was not parsed as hexadecimal");
		});

		runner.test("MCM global form references reject malformed input", [] {
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

		runner.test("MCM globals coerce reads to the descriptor alternative", [] {
			const auto boolean =
				GlobalToSettingValue(2.0f, dmui::SettingValue{ false });
			const auto signedNumber =
				GlobalToSettingValue(12.75f, dmui::SettingValue{ int64_t{} });
			const auto unsignedNumber =
				GlobalToSettingValue(9.5f, dmui::SettingValue{ uint64_t{} });
			const auto number =
				GlobalToSettingValue(3.5f, dmui::SettingValue{ 0.0 });

			require(boolean && std::get<bool>(*boolean),
				"nonzero global did not become true");
			require(signedNumber && std::get<int64_t>(*signedNumber) == 12,
				"global did not become a signed integer");
			require(unsignedNumber &&
					std::get<uint64_t>(*unsignedNumber) == 9u,
				"global did not become an unsigned integer");
			require(number && std::get<double>(*number) == 3.5,
				"global did not become a double");
		});

		runner.test("MCM global reads match every descriptor alternative", [] {
			const dmui::SettingValue targets[]{
				dmui::SettingValue{ false },
				dmui::SettingValue{ 0.0 },
				dmui::SettingValue{ int64_t{} },
				dmui::SettingValue{ uint64_t{} },
				dmui::SettingValue{ std::string{} }
			};
			for (const auto& target : targets)
			{
				const auto read = GlobalToSettingValue(1.0f, target);
				require(read && read->index() == target.index(),
					"a descriptor alternative lost its coercion");
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
		});

		runner.test("MCM globals coerce descriptor writes to floats", [] {
			require(SettingValueToGlobal(dmui::SettingValue{ true }) == 1.0f,
				"true did not become one");
			require(SettingValueToGlobal(
						dmui::SettingValue{ int64_t{ -7 } }) == -7.0f,
				"signed integer did not become a float");
			require(SettingValueToGlobal(
						dmui::SettingValue{ uint64_t{ 9 } }) == 9.0f,
				"unsigned integer did not become a float");
			require(SettingValueToGlobal(dmui::SettingValue{ 2.25 }) == 2.25f,
				"double did not become a float");
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
