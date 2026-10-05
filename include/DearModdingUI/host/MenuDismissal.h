#pragma once

#include <DearModdingUI/host/Hotkeys.h>

#include <cstddef>
#include <cstdint>

namespace DearModdingUI
{
	enum class MenuEscapeTarget : uint32_t
	{
		kNone,
		kKeyCapture,
		kInteraction,
		kPopup,
		kDialog,
		kHost,
		kOverlayFocus
	};

	struct MenuEscapeContext
	{
		HostInputMode inputMode{ HostInputMode::kGameplay };
		bool activeInteraction{ false };
		bool dialogActive{ false };
		uint32_t topPopupId{ 0 };
		uint32_t dialogPopupId{ 0 };
		size_t popupDepth{ 0 };
	};

	[[nodiscard]] constexpr MenuEscapeTarget DecideMenuEscapeTarget(
		const MenuEscapeContext& a_context) noexcept
	{
		if (a_context.inputMode == HostInputMode::kGameplay)
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
		return a_context.inputMode == HostInputMode::kShell ?
			MenuEscapeTarget::kHost : MenuEscapeTarget::kOverlayFocus;
	}

	void CaptureMenuEscapePress(
		HostInputMode a_inputMode,
		bool a_dialogActive,
		uint32_t a_dialogPopupId) noexcept;
	[[nodiscard]] bool ConsumeMenuEscapeTarget(
		MenuEscapeTarget a_target) noexcept;
	[[nodiscard]] bool DismissCapturedMenuPopup() noexcept;
	[[nodiscard]] bool DismissCapturedMenuDialog() noexcept;
	void ResetMenuEscapeRequest() noexcept;
}
