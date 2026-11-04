#pragma once

#include <DearModdingUI/host/Hotkeys.h>

#include <imgui/imgui.h>

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <span>

namespace DearModdingUI::EngineKeys
{
	inline constexpr uint32_t kVirtualKeyLeftShift = 0xA0;
	inline constexpr uint32_t kVirtualKeyRightMenu = 0xA5;

	[[nodiscard]] constexpr bool IsModifier(uint32_t a_virtualKey) noexcept
	{
		return a_virtualKey >= kVirtualKeyLeftShift && a_virtualKey <= kVirtualKeyRightMenu;
	}

	// Hotkey codes for one engine key; Enter maps to two.
	struct HotkeyCodes
	{
		std::array<uint32_t, 2> values{};
		size_t count{ 0 };

		[[nodiscard]] const uint32_t* begin() const noexcept { return values.data(); }
		[[nodiscard]] const uint32_t* end() const noexcept { return values.data() + count; }
	};

	// A null layout selects the calling thread's layout.
	[[nodiscard]] HotkeyCodes ToHotkeyCode(uint32_t a_virtualKey, void* a_layout = nullptr) noexcept;
	[[nodiscard]] ImGuiKey ToImGuiKey(uint32_t a_virtualKey, void* a_layout = nullptr) noexcept;

	enum class Edge : uint8_t
	{
		kPress,
		kHold,
		kRelease
	};

	// Zero value releases; zero held time is the first press.
	[[nodiscard]] constexpr Edge ClassifyEdge(float a_value, float a_heldDownSecs) noexcept
	{
		if (a_value == 0.0F)
			return Edge::kRelease;
		return a_heldDownSecs == 0.0F ? Edge::kPress : Edge::kHold;
	}

	struct KeyEdge
	{
		uint32_t virtualKey;
		Edge edge;
	};

	// Modifier state from the engine queue, independent of any window message.
	class ModifierShadow
	{
	public:
		void Apply(uint32_t a_virtualKey, Edge a_edge) noexcept;
		// Poll sorts by key code, so modifiers must land first.
		void FoldBatch(std::span<const KeyEdge> a_batch) noexcept;
		void Reset() noexcept { m_down.reset(); }

		[[nodiscard]] uint32_t Mask() const noexcept;

	private:
		std::bitset<kVirtualKeyRightMenu - kVirtualKeyLeftShift + 1> m_down;
	};
}
