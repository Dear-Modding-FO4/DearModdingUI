#include "MenuDismissal.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <atomic>

namespace DearModdingUI
{
	namespace
	{
		std::atomic<MenuEscapeTarget> s_target{ MenuEscapeTarget::kNone };
		std::atomic<uint32_t> s_popupId{ 0 };
		std::atomic<size_t> s_popupDepth{ 0 };

		[[nodiscard]] bool DismissCapturedSurface(
			MenuEscapeTarget a_target) noexcept
		{
			if (!ConsumeMenuEscapeTarget(a_target))
				return false;

			auto* context = ImGui::GetCurrentContext();
			const auto popupDepth =
				s_popupDepth.load(std::memory_order_relaxed);
			if (!context ||
				!popupDepth ||
				static_cast<size_t>(context->OpenPopupStack.Size) !=
					popupDepth ||
				context->OpenPopupStack.back().PopupId !=
					s_popupId.load(std::memory_order_relaxed))
				return true;

			ImGui::ClosePopupToLevel(
				static_cast<int>(popupDepth - 1),
				true);
			return true;
		}
	}

	void CaptureMenuEscapePress(
		bool a_menuVisible,
		bool a_dialogActive,
		uint32_t a_dialogPopupId) noexcept
	{
		uint32_t topPopupId{};
		size_t popupDepth{};
		bool activeInteraction{};
		if (const auto* context = ImGui::GetCurrentContext())
		{
			activeInteraction =
				context->ActiveId != 0 ||
				context->DragDropActive;
			popupDepth = static_cast<size_t>(context->OpenPopupStack.Size);
			if (popupDepth)
				topPopupId = context->OpenPopupStack.back().PopupId;
		}

		s_popupId.store(topPopupId, std::memory_order_relaxed);
		s_popupDepth.store(popupDepth, std::memory_order_relaxed);
		s_target.store(
			DecideMenuEscapeTarget({
				a_menuVisible,
				activeInteraction,
				a_dialogActive,
				topPopupId,
				a_dialogPopupId,
				popupDepth
			}),
			std::memory_order_release);
	}

	bool ConsumeMenuEscapeTarget(MenuEscapeTarget a_target) noexcept
	{
		auto expected = a_target;
		return s_target.compare_exchange_strong(
			expected,
			MenuEscapeTarget::kNone,
			std::memory_order_acq_rel,
			std::memory_order_acquire);
	}

	bool DismissCapturedMenuPopup() noexcept
	{
		return DismissCapturedSurface(MenuEscapeTarget::kPopup);
	}

	bool DismissCapturedMenuDialog() noexcept
	{
		return DismissCapturedSurface(MenuEscapeTarget::kDialog);
	}

	void ResetMenuEscapeRequest() noexcept
	{
		s_target.store(MenuEscapeTarget::kNone, std::memory_order_release);
		s_popupId.store(0, std::memory_order_relaxed);
		s_popupDepth.store(0, std::memory_order_relaxed);
	}
}
