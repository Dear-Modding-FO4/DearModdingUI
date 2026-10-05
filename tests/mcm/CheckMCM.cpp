#include "../Harness.h"

namespace vmm_tests
{
	void run_mcm_text_checks(Runner&);
	void run_mcm_mapping_structure_checks(Runner&);
	void run_mcm_parsing_checks(Runner&);
	void run_mcm_condition_checks(Runner&);
	void run_mcm_integration_fixture_checks(Runner&);

	void run_mcm_checks(Runner& runner)
	{
		run_mcm_text_checks(runner);
		run_mcm_mapping_structure_checks(runner);
		run_mcm_parsing_checks(runner);
		run_mcm_condition_checks(runner);
#ifdef DMUI_LOCAL_FIXTURES
		run_mcm_integration_fixture_checks(runner);
#endif
	}
}
