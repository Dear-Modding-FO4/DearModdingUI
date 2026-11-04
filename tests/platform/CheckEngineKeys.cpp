#include <Windows.h>

#include "../Harness.h"
#include <Platform/input/EngineKeys.h>
#include <DearModdingUI/host/HeldInput.h>
#include <DearModdingUI/host/MenuToggleChord.h>
#include <Support/KeyCatalog.h>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI;
		using namespace DearModdingUI::EngineKeys;

		void* UsLayout()
		{
			return LoadKeyboardLayoutW(L"00000409", 0);
		}

		// The engine reports sided Ctrl/Alt and numpad-coded navigation.
		uint32_t EngineKeyOf(uint32_t a_virtualKey, bool a_extended)
		{
			constexpr std::array<std::pair<uint32_t, uint32_t>, 11> numpadNavigation{ {
				{ 0x0C, 0x65 }, { 0x21, 0x69 }, { 0x22, 0x63 }, { 0x23, 0x61 },
				{ 0x24, 0x67 }, { 0x25, 0x64 }, { 0x26, 0x68 }, { 0x27, 0x66 },
				{ 0x28, 0x62 }, { 0x2D, 0x60 }, { 0x2E, 0x6E },
			} };
			if (a_virtualKey == VK_CONTROL || a_virtualKey == VK_MENU)
				return (a_virtualKey == VK_CONTROL ? 0xA2u : 0xA4u) + (a_extended ? 1u : 0u);
			if (!a_extended)
				for (const auto [navigation, numpad] : numpadNavigation)
					if (navigation == a_virtualKey)
						return numpad;
			return a_virtualKey;
		}

		bool Contains(const HotkeyCodes& a_codes, uint32_t a_code)
		{
			return std::ranges::find(a_codes, a_code) != a_codes.end();
		}
	}

	void run_engine_key_checks(Runner& runner)
	{
		runner.test("Engine virtual keys round trip to every catalog keyboard code", [] {
			auto* layout = UsLayout();
			require(layout != nullptr, "US keyboard layout is unavailable");
			const auto hkl = static_cast<HKL>(layout);
			size_t checked = 0;
			for (const auto& entry : KeyCatalog::kKeys)
			{
				if (entry.code >= KeyCatalog::kKeyboardKeyCount)
					continue;
				const auto extended = (entry.code & 0x80u) != 0;
				uint32_t virtualKey = entry.code == 0xC5 ? VK_PAUSE :
					entry.code == 0x45 ? VK_NUMLOCK :
					MapVirtualKeyExW((entry.code & 0x7Fu) | (extended ? 0xE000u : 0u), MAPVK_VSC_TO_VK_EX, hkl);
				if (!virtualKey)
					continue;
				const auto engineKey = EngineKeyOf(virtualKey, extended);
				require(Contains(ToHotkeyCode(engineKey, layout), entry.code),
					std::string{ "engine key did not map back to catalog key " } + std::string{ entry.token });
				++checked;
			}
			require(checked > 100, "round trip covered too few catalog keys");
		});

		runner.test("Enter and Numpad Enter share one engine key and both fire", [] {
			const auto codes = ToHotkeyCode(VK_RETURN, UsLayout());
			require(codes.count == 2 && Contains(codes, 0x1C) && Contains(codes, 0x9C),
				"engine Enter lost a candidate");
			require(ToHotkeyCode(VK_END, UsLayout()).count == 1 &&
					Contains(ToHotkeyCode(VK_END, UsLayout()), 0xCF),
				"dedicated End did not keep the default toggle code");
		});

		runner.test("Modifiers folded from a batch apply to earlier keys in the same poll", [] {
			// Poll sorts by key code, so End (0x23) precedes LCtrl (0xA2).
			const std::array batch{
				KeyEdge{ VK_END, Edge::kPress },
				KeyEdge{ VK_LCONTROL, Edge::kPress } };
			ModifierShadow shadow;
			shadow.FoldBatch(batch);
			require(shadow.Mask() == kHotkeyModifierControl, "same-batch Ctrl was not visible to End");

			shadow.Apply(VK_RCONTROL, Edge::kPress);
			shadow.Apply(VK_LCONTROL, Edge::kRelease);
			require(shadow.Mask() == kHotkeyModifierControl, "releasing one Ctrl dropped the other");
			shadow.Apply(VK_LSHIFT, Edge::kHold);
			require(shadow.Mask() == (kHotkeyModifierControl | kHotkeyModifierShift),
				"a held modifier was not tracked");
			shadow.Apply(VK_END, Edge::kPress);
			shadow.Reset();
			require(shadow.Mask() == 0, "reset left a modifier down");
		});

		runner.test("Engine input reset ends an owned toggle chord so the next press toggles", [] {
			const auto end = ToHotkeyCode(VK_END, UsLayout()).values[0];
			Hotkeys::SetReservedChord(kMenuDefaultToggleChord);
			Hotkeys::ReleaseActiveKeys();
			require(Hotkeys::HandleKey(end, 0, true, false) == HotkeyMessageResult::kMenuToggle &&
					Hotkeys::IsToggleChordHeld(),
				"the toggle key did not open its chord");
			// ClearInputState drops the key without a release edge.
			(void)DearModdingUI::ReleaseHeldInput();
			require(!Hotkeys::IsToggleChordHeld(), "the reset kept game input blocked");
			require(Hotkeys::HandleKey(end, 0, true, false) == HotkeyMessageResult::kMenuToggle,
				"a stale hold swallowed the next toggle press");
			(void)DearModdingUI::ReleaseHeldInput();
		});
	}
}
