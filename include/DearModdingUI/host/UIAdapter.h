#pragma once

#include <DearModdingUI/CUIAPI.h>

namespace DearModdingUI::UI
{
	[[nodiscard]] const DMUI_UIAPI& API() noexcept;
	[[nodiscard]] DMUI_Result Query(
		uint32_t a_requestedUIAbi,
		uint32_t a_minimumRevision,
		uint32_t a_minimumTableSize,
		DMUI_UIAPIInfo* a_info) noexcept;

#if defined(DMUI_UI_TESTING)
	namespace Testing
	{
		using ValidationHook = DMUI_Result (*)(DMUI_ClientHandle) noexcept;

		class ValidationOverride
		{
		public:
			explicit ValidationOverride(ValidationHook a_hook) noexcept;
			~ValidationOverride() noexcept;

			ValidationOverride(const ValidationOverride&) = delete;
			ValidationOverride(ValidationOverride&&) = delete;
			ValidationOverride& operator=(const ValidationOverride&) = delete;
			ValidationOverride& operator=(ValidationOverride&&) = delete;

		private:
			ValidationHook m_previous;
		};
	}
#endif
}
