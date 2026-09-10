#pragma once

#include <cstdint>

namespace DearModdingUI
{
	enum class HostPageKind : uint32_t;

#if defined(DMUI_PREVIEW)
	void ConfigurePreviewHostPage(HostPageKind a_page) noexcept;
#endif
	void DrawShell() noexcept;
	void ApplyMenuEscapeDismissal() noexcept;
}
