#include "Harness.h"

#include <iostream>

int main()
{
	using namespace vmm_tests;
	std::cout.setf(std::ios::unitbuf);

	Runner runner;
	run_subsystem_health_checks(runner);
	run_imgui_platform_checks(runner);
	run_dear_modding_ui_checks(runner);
	run_settings_table_checks(runner);
	run_hotkey_checks(runner);
	run_mcm_checks(runner);
	run_mcm_availability_checks(runner);
	run_mcm_binding_checks(runner);
	run_mcm_global_value_checks(runner);
	run_mcm_settings_ini_checks(runner);
	run_mcm_keybind_checks(runner);
	run_mcm_runtime_checks(runner);
	run_mcm_action_checks(runner);

	std::cout << '\n' << runner.tests() - runner.failures() << '/' << runner.tests() << " checks passed\n";
	return runner.failures() == 0 ? 0 : 1;
}
