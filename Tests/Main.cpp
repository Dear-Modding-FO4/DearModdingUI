#include "Harness.h"

#include <iostream>

int main()
{
	using namespace vmm_tests;
	std::cout.setf(std::ios::unitbuf);

	Runner runner;
	run_imgui_platform_checks(runner);
	run_dear_modding_ui_checks(runner);
	run_settings_table_checks(runner);
	run_hotkey_checks(runner);
	run_mcm_checks(runner);
	run_mcm_binding_checks(runner);
	run_mcm_global_value_checks(runner);

	std::cout << '\n' << runner.tests() - runner.failures() << '/' << runner.tests() << " checks passed\n";
	return runner.failures() == 0 ? 0 : 1;
}
