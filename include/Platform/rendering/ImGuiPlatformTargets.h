#pragma once

#include <cstdint>
#include <Support/KeyCatalog.h>

namespace DearModdingUI::PlatformImGui
{
	enum class Readiness : uint32_t
	{
		kReady,
		kWaitingForUiPass,
		kBackendUninitialized,
		kBackendFailed
	};

	[[nodiscard]] constexpr Readiness ClassifyReadiness(
		bool a_uiPassObserved,
		bool a_backendReady,
		bool a_backendFailed) noexcept
	{
		if (!a_uiPassObserved)
			return Readiness::kWaitingForUiPass;
		if (a_backendFailed)
			return Readiness::kBackendFailed;
		return a_backendReady ? Readiness::kReady : Readiness::kBackendUninitialized;
	}

	[[nodiscard]] constexpr bool ShouldRenderHostFrame(
		bool a_modalVisible,
		bool a_overlayDemanded) noexcept
	{
		return a_modalVisible || a_overlayDemanded;
	}

	struct MousePosition
	{
		float x{ 0.0f };
		float y{ 0.0f };
	};

	[[nodiscard]] constexpr MousePosition MapNativeCursorToTarget(
		MousePosition a_position,
		uint32_t a_clientWidth,
		uint32_t a_clientHeight,
		uint32_t a_targetWidth,
		uint32_t a_targetHeight) noexcept
	{
		return {
			a_position.x * static_cast<float>(a_targetWidth) /
				static_cast<float>(a_clientWidth),
			a_position.y * static_cast<float>(a_targetHeight) /
				static_cast<float>(a_clientHeight)
		};
	}

	[[nodiscard]] constexpr MousePosition MapClientToTarget(
		MousePosition a_position,
		uint32_t a_clientWidth,
		uint32_t a_clientHeight,
		uint32_t a_targetWidth,
		uint32_t a_targetHeight) noexcept
	{
		if (!a_clientWidth ||
			!a_clientHeight ||
			!a_targetWidth ||
			!a_targetHeight ||
			(a_clientWidth == a_targetWidth &&
				a_clientHeight == a_targetHeight) ||
			!(a_position.x >= 0.0f &&
				a_position.y >= 0.0f &&
				a_position.x < static_cast<float>(a_clientWidth) &&
				a_position.y < static_cast<float>(a_clientHeight)))
			return a_position;

		return MapNativeCursorToTarget(
			a_position, a_clientWidth, a_clientHeight, a_targetWidth, a_targetHeight);
	}

	[[nodiscard]] constexpr uint32_t MouseKeyCode(uint32_t a_idCode) noexcept
	{
		if (a_idCode < DearModdingUI::KeyCatalog::kMouseButtonCount)
			return DearModdingUI::KeyCatalog::kMouseButtonOffset + a_idCode;
		if (a_idCode == 0x800)
			return DearModdingUI::KeyCatalog::kMouseWheelOffset;
		if (a_idCode == 0x900)
			return DearModdingUI::KeyCatalog::kMouseWheelOffset + 1;
		return 0;
	}

	[[nodiscard]] constexpr bool IsMouseWheelCode(uint32_t a_code) noexcept
	{
		return a_code == DearModdingUI::KeyCatalog::kMouseWheelOffset ||
			a_code == DearModdingUI::KeyCatalog::kMouseWheelOffset + 1;
	}

	inline constexpr uint32_t kEscapeVirtualKey = 0x1B;
	inline constexpr uint32_t kEscapeKeyCode = 0x01;

	enum class EscapeDecision : uint32_t
	{
		kForward,
		kCapture,
		kConsume,
		kConsumeAndRelease,
		kReleaseAndForward
	};

	// An Escape press the visible host captured is owned until its release.
	[[nodiscard]] constexpr EscapeDecision DecideEscapeEdge(
		bool a_pressed,
		bool a_repeat,
		bool a_menuVisible,
		bool a_pressConsumed) noexcept
	{
		if (!a_pressed)
			return a_pressConsumed ? EscapeDecision::kConsumeAndRelease : EscapeDecision::kForward;
		if (a_repeat)
			return a_pressConsumed ? EscapeDecision::kConsume : EscapeDecision::kForward;
		if (a_menuVisible)
			return EscapeDecision::kCapture;
		return a_pressConsumed ? EscapeDecision::kReleaseAndForward : EscapeDecision::kForward;
	}
}
