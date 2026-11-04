#pragma once

#include <Platform/rendering/ImGuiPlatformTargets.h>

#include <atomic>
#include <cstdint>
#include <string_view>

namespace DearModdingUI::ToggleOutcome
{
	enum class Outcome : uint32_t
	{
		kToggled,
		kInputHooksFailed,
		kEngineInactive,
		kRendererNotAttached,
		kBackendUninitialized,
		kBackendFailed,
		kHostNotReady
	};

	struct Gates
	{
		bool inputHooks{ false };
		bool engineActive{ false };
		PlatformImGui::Readiness renderer{ PlatformImGui::Readiness::kRendererNotAttached };
		bool hostAccepted{ false };
	};

	// Names the first gate that stopped a toggle the host refused.
	[[nodiscard]] constexpr Outcome Classify(const Gates& a_gates) noexcept
	{
		using PlatformImGui::Readiness;
		if (a_gates.hostAccepted)
			return Outcome::kToggled;
		if (!a_gates.inputHooks)
			return Outcome::kInputHooksFailed;
		if (!a_gates.engineActive)
			return Outcome::kEngineInactive;
		switch (a_gates.renderer)
		{
		case Readiness::kRendererNotAttached:
			return Outcome::kRendererNotAttached;
		case Readiness::kBackendUninitialized:
			return Outcome::kBackendUninitialized;
		case Readiness::kBackendFailed:
			return Outcome::kBackendFailed;
		default:
			return Outcome::kHostNotReady;
		}
	}

	[[nodiscard]] constexpr std::string_view Name(Outcome a_outcome) noexcept
	{
		switch (a_outcome)
		{
		case Outcome::kToggled:
			return "toggled";
		case Outcome::kInputHooksFailed:
			return "input-hooks-failed";
		case Outcome::kEngineInactive:
			return "engine-inactive";
		case Outcome::kRendererNotAttached:
			return "renderer-not-attached";
		case Outcome::kBackendUninitialized:
			return "backend-uninitialized";
		case Outcome::kBackendFailed:
			return "backend-failed";
		default:
			return "host-not-ready";
		}
	}

	// Repeated presses with the same cause log once.
	class ChangeFilter
	{
	public:
		[[nodiscard]] bool Changed(Outcome a_outcome) noexcept
		{
			return m_last.exchange(a_outcome, std::memory_order_acq_rel) != a_outcome;
		}

	private:
		// No outcome precedes the first toggle, so it always logs.
		std::atomic<Outcome> m_last{ static_cast<Outcome>(UINT32_MAX) };
	};
}
