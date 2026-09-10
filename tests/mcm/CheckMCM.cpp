#include "../Harness.h"

namespace vmm_tests
{
	void run_mcm_text_checks(Runner&);
	void run_mcm_mapping_structure_checks(Runner&);
	void run_mcm_parsing_checks(Runner&);
	void run_mcm_value_source_checks(Runner&);
	void run_mcm_condition_checks(Runner&);

	void run_mcm_checks(Runner& runner)
	{
		run_mcm_text_checks(runner);
		run_mcm_mapping_structure_checks(runner);
		run_mcm_parsing_checks(runner);
		run_mcm_value_source_checks(runner);
		run_mcm_condition_checks(runner);
	}
}
