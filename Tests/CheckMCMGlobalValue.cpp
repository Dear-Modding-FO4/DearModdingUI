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

		runner.test("MCM globals coerce reads to descriptor alternatives", [] {
			const auto boolean =
				GlobalToSettingValue(2.0f, SourceValueKind::kBool);
			const auto integer =
				GlobalToSettingValue(12.75f, SourceValueKind::kInt);
			const auto number =
				GlobalToSettingValue(3.5f, SourceValueKind::kFloat);

			require(boolean && std::get<bool>(*boolean),
				"nonzero global did not become true");
			require(integer && std::get<int64_t>(*integer) == 12,
				"global did not become a signed integer");
			require(number && std::get<double>(*number) == 3.5,
				"global did not become a double");
		});

		runner.test("MCM globals coerce descriptor writes to floats", [] {
			require(SettingValueToGlobal(
						dmui::SettingValue{ true },
						SourceValueKind::kBool) == 1.0f,
				"true did not become one");
			require(SettingValueToGlobal(
						dmui::SettingValue{ int64_t{ -7 } },
						SourceValueKind::kInt) == -7.0f,
				"signed integer did not become a float");
			require(SettingValueToGlobal(
						dmui::SettingValue{ 2.25 },
						SourceValueKind::kFloat) == 2.25f,
				"double did not become a float");
			require(!SettingValueToGlobal(
						dmui::SettingValue{ std::string{ "7" } },
						SourceValueKind::kFloat),
				"mistyped descriptor value was accepted");
			require(!SettingValueToGlobal(
						dmui::SettingValue{
							(std::numeric_limits<double>::infinity)() },
						SourceValueKind::kFloat),
				"non-finite descriptor value was accepted");
		});

		runner.test("MCM global choices translate only numeric index strings", [] {
			const auto read =
				GlobalToSettingValue(2.0f, SourceValueKind::kString);
			require(read && std::get<std::string>(*read) == "2",
				"global choice did not become an index string");
			require(SettingValueToGlobal(
						dmui::SettingValue{ std::string{ "7" } },
						SourceValueKind::kString) == 7.0f,
				"global choice index did not become a float");
			require(!SettingValueToGlobal(
						dmui::SettingValue{ std::string{ "61 (FX) slot" } },
						SourceValueKind::kString),
				"global choice accepted an option label instead of an index");
			require(!SettingValueToGlobal(
						dmui::SettingValue{ std::string{ "2 trailing" } },
						SourceValueKind::kString),
				"global choice accepted a partially numeric string");
		});

		runner.test("MCM global kind follows the descriptor default", [] {
			require(ResolveSourceValueKind(
						SourceValueKind::kNone,
						dmui::SettingValue{ false }) ==
					SourceValueKind::kBool,
				"switcher default did not select boolean coercion");
			require(ResolveSourceValueKind(
						SourceValueKind::kFloat,
						dmui::SettingValue{ int64_t{} }) ==
					SourceValueKind::kInt,
				"stepper default did not select integer coercion");
		});
	}
}
