#include "HostAPIEntries.h"

#include <DearModdingUI/IconGlyphs.h>
#include <Support/BoundedString.h>

#include <new>

namespace DearModdingUI::HostAPIInternal
{
	DMUI_Result ApiResolveIconGlyph(
		const DMUI_IconResolutionRequest* a_request,
		uint32_t* a_glyph) noexcept
	{
		if (!a_glyph)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_glyph = 0;
		if (!a_request)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_request->structSize < DMUI_ICON_RESOLUTION_REQUEST_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;

		const auto explicitName =
			Internal::ReadBoundedString(a_request->explicitName, 128, true);
		const auto primaryMetadata =
			Internal::ReadBoundedString(a_request->primaryMetadata, 256, true);
		const auto secondaryMetadata =
			Internal::ReadBoundedString(a_request->secondaryMetadata, 256, true);
		if (!explicitName || !primaryMetadata || !secondaryMetadata ||
			!Internal::ValidText(*explicitName, true) ||
			!Internal::ValidText(*primaryMetadata, true) ||
			!Internal::ValidText(*secondaryMetadata, true))
			return DMUI_RESULT_INVALID_ARGUMENT;

		try
		{
			*a_glyph = static_cast<uint32_t>(
				ResolveIconSelection(
					*explicitName,
					*primaryMetadata,
					*secondaryMetadata)
					.GlyphOr({}));
			return DMUI_RESULT_OK;
		}
		catch (const std::bad_alloc&)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
		catch (...)
		{
			return DMUI_RESULT_CALLBACK_FAILED;
		}
	}
}
