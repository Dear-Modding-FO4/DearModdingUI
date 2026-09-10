#include "../Harness.h"

namespace vmm_tests
{
	void run_host_api_compatibility_checks(Runner&);
	void run_page_activity_checks(Runner&);
	void run_declarative_settings_checks(Runner&);
	void run_status_checks(Runner&);
	void run_shell_geometry_checks(Runner&);
	void run_client_preflight_checks(Runner&);
	void run_health_diagnostics_checks(Runner&);
	void run_link_row_checks(Runner&);
	void run_external_open_checks(Runner&);
	void run_faq_checks(Runner&);
	void run_diagnostic_api_checks(Runner&);
	void run_registry_registration_checks(Runner&);
	void run_navigation_checks(Runner&);
	void run_client_status_checks(Runner&);
	void run_asset_contract_checks(Runner&);
	void run_host_control_checks(Runner&);
	void run_host_settings_checks(Runner&);
	void run_navigation_presentation_checks(Runner&);
	void run_carrier_menu_checks(Runner&);
	void run_registry_lifecycle_checks(Runner&);

	void run_dear_modding_ui_checks(Runner& runner)
	{
		run_host_api_compatibility_checks(runner);
		run_page_activity_checks(runner);
		run_declarative_settings_checks(runner);
		run_status_checks(runner);
		run_shell_geometry_checks(runner);
		run_client_preflight_checks(runner);
		run_health_diagnostics_checks(runner);
		run_link_row_checks(runner);
		run_external_open_checks(runner);
		run_faq_checks(runner);
		run_diagnostic_api_checks(runner);
		run_registry_registration_checks(runner);
		run_navigation_checks(runner);
		run_client_status_checks(runner);
		run_asset_contract_checks(runner);
		run_host_control_checks(runner);
		run_host_settings_checks(runner);
		run_navigation_presentation_checks(runner);
		run_carrier_menu_checks(runner);
		run_registry_lifecycle_checks(runner);
	}
}
