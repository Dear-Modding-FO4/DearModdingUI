#pragma once

#include <DearModdingUI/CUIAPI.h>

#include <cstddef>

namespace DearModdingUI::UI
{
	class DrawListClipScope
	{
	public:
		DrawListClipScope() noexcept;
		~DrawListClipScope() noexcept;
		DrawListClipScope(const DrawListClipScope&) = delete;
		DrawListClipScope& operator=(const DrawListClipScope&) = delete;
		[[nodiscard]] bool Balanced() const noexcept;

	private:
		size_t m_depth;
		size_t m_previousFloor;
	};

	class ListClipperScope
	{
	public:
		ListClipperScope() noexcept;
		~ListClipperScope() noexcept;

		ListClipperScope(const ListClipperScope&) = delete;
		ListClipperScope(ListClipperScope&&) = delete;
		ListClipperScope& operator=(const ListClipperScope&) = delete;
		ListClipperScope& operator=(ListClipperScope&&) = delete;

	private:
		size_t m_depth;
	};

	[[nodiscard]] const DMUI_UIAPI& API() noexcept;

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
