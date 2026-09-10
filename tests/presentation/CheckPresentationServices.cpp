#include "../Harness.h"

namespace vmm_tests
{
	void run_presentation_render_execution_checks(Runner&);
	void run_presentation_blur_pipeline_checks(Runner&);
	void run_presentation_image_resource_checks(Runner&);
	void run_presentation_overlay_notification_plot_checks(Runner&);
	void run_presentation_dialog_interaction_checks(Runner&);

	void run_presentation_service_checks(Runner& runner)
	{
		run_presentation_render_execution_checks(runner);
		run_presentation_blur_pipeline_checks(runner);
		run_presentation_image_resource_checks(runner);
		run_presentation_overlay_notification_plot_checks(runner);
		run_presentation_dialog_interaction_checks(runner);
	}
}
