#include <Platform/input/EngineKeys.h>

#include <Windows.h>

namespace DearModdingUI::EngineKeys
{
	namespace
	{
		[[nodiscard]] HKL ResolveLayout(void* a_layout) noexcept
		{
			return a_layout ? static_cast<HKL>(a_layout) : GetKeyboardLayout(0);
		}

		[[nodiscard]] ImGuiKey ScanCodeToImGuiKey(uint32_t a_scanCode) noexcept
		{
			switch (a_scanCode)
			{
			case 41: return ImGuiKey_GraveAccent;
			case 12: return ImGuiKey_Minus;
			case 13: return ImGuiKey_Equal;
			case 26: return ImGuiKey_LeftBracket;
			case 27: return ImGuiKey_RightBracket;
			case 86: return ImGuiKey_Oem102;
			case 43: return ImGuiKey_Backslash;
			case 39: return ImGuiKey_Semicolon;
			case 40: return ImGuiKey_Apostrophe;
			case 51: return ImGuiKey_Comma;
			case 52: return ImGuiKey_Period;
			case 53: return ImGuiKey_Slash;
			default: return ImGuiKey_None;
			}
		}

		[[nodiscard]] ImGuiKey VirtualKeyToImGuiKey(uint32_t a_virtualKey) noexcept
		{
			if (a_virtualKey >= '0' && a_virtualKey <= '9')
				return static_cast<ImGuiKey>(ImGuiKey_0 + (a_virtualKey - '0'));
			if (a_virtualKey >= 'A' && a_virtualKey <= 'Z')
				return static_cast<ImGuiKey>(ImGuiKey_A + (a_virtualKey - 'A'));
			if (a_virtualKey >= VK_F1 && a_virtualKey <= VK_F12)
				return static_cast<ImGuiKey>(ImGuiKey_F1 + (a_virtualKey - VK_F1));
			if (a_virtualKey >= VK_F13 && a_virtualKey <= VK_F24)
				return static_cast<ImGuiKey>(ImGuiKey_F13 + (a_virtualKey - VK_F13));
			if (a_virtualKey >= VK_NUMPAD0 && a_virtualKey <= VK_NUMPAD9)
				return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + (a_virtualKey - VK_NUMPAD0));

			switch (a_virtualKey)
			{
			case VK_TAB: return ImGuiKey_Tab;
			case VK_LEFT: return ImGuiKey_LeftArrow;
			case VK_RIGHT: return ImGuiKey_RightArrow;
			case VK_UP: return ImGuiKey_UpArrow;
			case VK_DOWN: return ImGuiKey_DownArrow;
			case VK_PRIOR: return ImGuiKey_PageUp;
			case VK_NEXT: return ImGuiKey_PageDown;
			case VK_HOME: return ImGuiKey_Home;
			case VK_END: return ImGuiKey_End;
			case VK_INSERT: return ImGuiKey_Insert;
			case VK_DELETE: return ImGuiKey_Delete;
			case VK_BACK: return ImGuiKey_Backspace;
			case VK_SPACE: return ImGuiKey_Space;
			case VK_RETURN: return ImGuiKey_Enter;
			case VK_ESCAPE: return ImGuiKey_Escape;
			case VK_CAPITAL: return ImGuiKey_CapsLock;
			case VK_SCROLL: return ImGuiKey_ScrollLock;
			case VK_NUMLOCK: return ImGuiKey_NumLock;
			case VK_SNAPSHOT: return ImGuiKey_PrintScreen;
			case VK_PAUSE: return ImGuiKey_Pause;
			case VK_DECIMAL: return ImGuiKey_KeypadDecimal;
			case VK_DIVIDE: return ImGuiKey_KeypadDivide;
			case VK_MULTIPLY: return ImGuiKey_KeypadMultiply;
			case VK_SUBTRACT: return ImGuiKey_KeypadSubtract;
			case VK_ADD: return ImGuiKey_KeypadAdd;
			case VK_LSHIFT: return ImGuiKey_LeftShift;
			case VK_LCONTROL: return ImGuiKey_LeftCtrl;
			case VK_LMENU: return ImGuiKey_LeftAlt;
			case VK_LWIN: return ImGuiKey_LeftSuper;
			case VK_RSHIFT: return ImGuiKey_RightShift;
			case VK_RCONTROL: return ImGuiKey_RightCtrl;
			case VK_RMENU: return ImGuiKey_RightAlt;
			case VK_RWIN: return ImGuiKey_RightSuper;
			case VK_APPS: return ImGuiKey_Menu;
			case VK_BROWSER_BACK: return ImGuiKey_AppBack;
			case VK_BROWSER_FORWARD: return ImGuiKey_AppForward;
			default: return ImGuiKey_None;
			}
		}

		[[nodiscard]] constexpr bool IsOemKey(uint32_t a_virtualKey) noexcept
		{
			return (a_virtualKey >= VK_OEM_1 && a_virtualKey <= VK_OEM_3) ||
				(a_virtualKey >= VK_OEM_4 && a_virtualKey <= VK_OEM_8) ||
				a_virtualKey == VK_OEM_102 ||
				a_virtualKey == VK_OEM_COMMA || a_virtualKey == VK_OEM_PERIOD;
		}
	}

	namespace
	{
		// The engine only reports these VKs for the extended cluster.
		[[nodiscard]] constexpr bool IsExtendedNavigation(uint32_t a_virtualKey) noexcept
		{
			return (a_virtualKey >= VK_PRIOR && a_virtualKey <= VK_DOWN) ||
				a_virtualKey == VK_INSERT || a_virtualKey == VK_DELETE;
		}
	}

	HotkeyCodes ToHotkeyCode(uint32_t a_virtualKey, void* a_layout) noexcept
	{
		// Scan codes collapse or misreport these; the catalog keys them by VK.
		if (a_virtualKey == VK_PAUSE)
			return { { 0xC5 }, 1 };
		if (a_virtualKey == VK_NUMLOCK)
			return { { 0x45 }, 1 };
		if (a_virtualKey == VK_SNAPSHOT)
			return { { 0xB7 }, 1 };
		if (a_virtualKey == VK_RETURN)
			return { { 0x1C, 0x9C }, 2 };

		const auto mapped = MapVirtualKeyExW(a_virtualKey, MAPVK_VK_TO_VSC_EX, ResolveLayout(a_layout));
		const auto scanCode = mapped & 0xFFu;
		if (!scanCode)
			return {};
		const auto prefix = (mapped >> 8) & 0xFFu;
		const auto extended = prefix == 0xE0 || prefix == 0xE1 || IsExtendedNavigation(a_virtualKey);
		return { { scanCode | (extended ? 0x80u : 0u) }, 1 };
	}

	ImGuiKey ToImGuiKey(uint32_t a_virtualKey, void* a_layout) noexcept
	{
		if (const auto key = VirtualKeyToImGuiKey(a_virtualKey); key != ImGuiKey_None)
			return key;
		// OEM virtual keys follow the layout; the physical position does not.
		return IsOemKey(a_virtualKey) ?
			ScanCodeToImGuiKey(MapVirtualKeyExW(a_virtualKey, MAPVK_VK_TO_VSC, ResolveLayout(a_layout))) :
			ImGuiKey_None;
	}

	void ModifierShadow::Apply(uint32_t a_virtualKey, Edge a_edge) noexcept
	{
		if (IsModifier(a_virtualKey))
			m_down.set(a_virtualKey - kVirtualKeyLeftShift, a_edge != Edge::kRelease);
	}

	void ModifierShadow::FoldBatch(std::span<const KeyEdge> a_batch) noexcept
	{
		for (const auto& key : a_batch)
			Apply(key.virtualKey, key.edge);
	}

	uint32_t ModifierShadow::Mask() const noexcept
	{
		// Bits follow VK order: LShift, RShift, LCtrl, RCtrl, LAlt, RAlt.
		uint32_t mask{};
		if (m_down.test(0) || m_down.test(1))
			mask |= kHotkeyModifierShift;
		if (m_down.test(2) || m_down.test(3))
			mask |= kHotkeyModifierControl;
		if (m_down.test(4) || m_down.test(5))
			mask |= kHotkeyModifierAlt;
		return mask;
	}
}
