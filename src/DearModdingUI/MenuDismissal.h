#pragma once

#include <cstddef>
#include <cstdint>

namespace DearModdingUI
{
	enum class MenuEscapeTarget : uint32_t
	{
		kNone,
		kInteraction,
		kPopup,
		kDialog,
		kHost
	};

	struct MenuEscapeContext
	{
		bool menuVisible{ false };
		bool activeInteraction{ false };
		bool dialogActive{ false };
		uint32_t topPopupId{ 0 };
		uint32_t dialogPopupId{ 0 };
		size_t popupDepth{ 0 };
	};

	[[nodiscard]] constexpr MenuEscapeTarget DecideMenuEscapeTarget(
		const MenuEscapeContext& a_context) noexcept
	{
		if (!a_context.menuVisible)
			return MenuEscapeTarget::kNone;
		if (a_context.activeInteraction)
			return MenuEscapeTarget::kInteraction;
		if (a_context.popupDepth)
		{
			if (a_context.dialogActive &&
				a_context.dialogPopupId &&
				a_context.topPopupId == a_context.dialogPopupId)
				return MenuEscapeTarget::kDialog;
			return MenuEscapeTarget::kPopup;
		}
		return MenuEscapeTarget::kHost;
	}

	void CaptureMenuEscapePress(
		bool a_menuVisible,
		bool a_dialogActive,
		uint32_t a_dialogPopupId) noexcept;
	[[nodiscard]] bool ConsumeMenuEscapeTarget(
		MenuEscapeTarget a_target) noexcept;
	[[nodiscard]] bool DismissCapturedMenuPopup() noexcept;
	[[nodiscard]] bool DismissCapturedMenuDialog() noexcept;
	void ResetMenuEscapeRequest() noexcept;
}
