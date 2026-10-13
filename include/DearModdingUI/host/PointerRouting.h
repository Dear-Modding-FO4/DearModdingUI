#pragma once

#include <DearModdingUI/API.h>

#include <cstdint>
#include <optional>

namespace DearModdingUI::PointerRouting
{
	enum class Verdict : uint32_t
	{
		kForward,
		kDivert
	};

	// Pointer input this frame that DMUI did not take.
	struct Frame
	{
		uint32_t down{};
		uint32_t pressed{};
		uint32_t released{};
		DMUI_Vec2 wheel{};
	};

	// Message thread with the ImGui context current; kDivert must skip ImGui.
	[[nodiscard]] Verdict RoutePress(
		DMUI_PageHandle a_owner, uint32_t a_button, DMUI_Vec2 a_position, bool a_excluded) noexcept;
	[[nodiscard]] Verdict RouteRelease(DMUI_PageHandle a_owner, uint32_t a_button) noexcept;
	[[nodiscard]] Verdict RouteWheel(
		DMUI_PageHandle a_owner, DMUI_Vec2 a_notches, DMUI_Vec2 a_position, bool a_excluded) noexcept;
	// Mouse capture was lost, so no release will arrive: held buttons end now.
	void LoseCapture() noexcept;
	[[nodiscard]] bool HoldsButtons() noexcept;
	// Render thread, before NewFrame: pending edges become this frame's input.
	void BeginFrame(DMUI_PageHandle a_owner) noexcept;
	void Reset() noexcept;
	[[nodiscard]] std::optional<Frame> FrameFor(DMUI_PageHandle a_page) noexcept;
}
