#include "UIAdapterInternal.h"
#include <DearModdingUI/UIBindings.generated.h>
#include <DearModdingUI/presentation/PresentationServices.h>

namespace DearModdingUI::UI::Bindings
{
	DMUI_Result DMUI_CALL Image(
		DMUI_ClientHandle a_client, DMUI_ImageHandle a_image,
		const DMUI_ImageDrawOptions* a_options, uint32_t* a_drawn) noexcept
	{
		if (!a_drawn)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_drawn = 0;
		const auto validation = AdapterInternal::Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		return PresentationServices::DrawImage(a_client, a_image, a_options, a_drawn);
	}

	DMUI_Result DMUI_CALL PlotAnnotated(
		DMUI_ClientHandle a_client, const char* a_id,
		const DMUI_AnnotatedPlotDescriptor* a_descriptor) noexcept
	{
		const auto validation = AdapterInternal::Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		return PresentationServices::DrawAnnotatedPlot(a_client, a_id, a_descriptor);
	}
}
