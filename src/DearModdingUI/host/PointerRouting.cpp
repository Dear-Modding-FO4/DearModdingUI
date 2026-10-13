#include <DearModdingUI/host/PointerRouting.h>

#include <imgui/imgui_internal.h>

namespace DearModdingUI::PointerRouting
{
	namespace
	{
		struct State
		{
			DMUI_PageHandle owner{ DMUI_INVALID_PAGE_HANDLE };
			// ImGui keeps its presses until release so drags are not split.
			uint32_t forwardedHeld{};
			uint32_t divertedHeld{};
			uint32_t pendingPressed{};
			uint32_t pendingReleased{};
			DMUI_Vec2 pendingWheel{};
			Frame frame;
		};

		State s_state;

		void Adopt(DMUI_PageHandle a_owner) noexcept
		{
			if (a_owner != s_state.owner)
			{
				s_state = {};
				s_state.owner = a_owner;
			}
		}

		[[nodiscard]] bool ImGuiTakesPointer(DMUI_Vec2 a_position) noexcept
		{
			const auto* context = ImGui::GetCurrentContext();
			if (!context)
				return true;
			if (s_state.forwardedHeld != 0 || context->OpenPopupStack.Size > 0)
				return true;
			ImGuiWindow* hovered{};
			ImGui::FindHoveredWindowEx({ a_position.x, a_position.y }, false, &hovered, nullptr);
			return hovered != nullptr;
		}
	}

	Verdict RoutePress(
		DMUI_PageHandle a_owner, uint32_t a_button, DMUI_Vec2 a_position, bool a_excluded) noexcept
	{
		Adopt(a_owner);
		if (a_owner == DMUI_INVALID_PAGE_HANDLE)
			return Verdict::kForward;
		if (a_excluded || ImGuiTakesPointer(a_position))
		{
			s_state.forwardedHeld |= a_button;
			return Verdict::kForward;
		}
		s_state.divertedHeld |= a_button;
		s_state.pendingPressed |= a_button;
		return Verdict::kDivert;
	}

	Verdict RouteRelease(DMUI_PageHandle a_owner, uint32_t a_button) noexcept
	{
		Adopt(a_owner);
		if (a_owner == DMUI_INVALID_PAGE_HANDLE)
			return Verdict::kForward;
		if ((s_state.divertedHeld & a_button) == 0)
		{
			s_state.forwardedHeld &= ~a_button;
			return Verdict::kForward;
		}
		s_state.divertedHeld &= ~a_button;
		s_state.pendingReleased |= a_button;
		return Verdict::kDivert;
	}

	Verdict RouteWheel(
		DMUI_PageHandle a_owner, DMUI_Vec2 a_notches, DMUI_Vec2 a_position, bool a_excluded) noexcept
	{
		Adopt(a_owner);
		if (a_owner == DMUI_INVALID_PAGE_HANDLE || a_excluded || ImGuiTakesPointer(a_position))
			return Verdict::kForward;
		s_state.pendingWheel.x += a_notches.x;
		s_state.pendingWheel.y += a_notches.y;
		return Verdict::kDivert;
	}

	void BeginFrame(DMUI_PageHandle a_owner) noexcept
	{
		Adopt(a_owner);
		if (a_owner == DMUI_INVALID_PAGE_HANDLE)
			return;
		s_state.frame = {
			s_state.divertedHeld,
			s_state.pendingPressed,
			s_state.pendingReleased,
			s_state.pendingWheel
		};
		s_state.pendingPressed = 0;
		s_state.pendingReleased = 0;
		s_state.pendingWheel = {};
	}

	void LoseCapture() noexcept
	{
		s_state.pendingReleased |= s_state.divertedHeld;
		s_state.divertedHeld = 0;
		s_state.forwardedHeld = 0;
	}

	bool HoldsButtons() noexcept
	{
		return (s_state.forwardedHeld | s_state.divertedHeld) != 0;
	}

	void Reset() noexcept
	{
		s_state = {};
	}

	std::optional<Frame> FrameFor(DMUI_PageHandle a_page) noexcept
	{
		if (a_page == DMUI_INVALID_PAGE_HANDLE || a_page != s_state.owner)
			return std::nullopt;
		return s_state.frame;
	}
}
