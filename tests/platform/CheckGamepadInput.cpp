#include <cstdint>
#include <array>

#include "../Harness.h"
#include <Support/KeyCatalog.h>
#include <Platform/input/GameInput.h>
#include <DearModdingUI/host/ControllerNavigation.h>
#include <DearModdingUI/host/MenuToggleChord.h>
#include <DearModdingUI/host/MenuDismissal.h>
#include "../support/ImGuiTestContext.h"
#include <imgui/imgui_internal.h>

// Compile the linked production mapper against a test-owned ControlMap singleton.
#include "../../Depends/commonlibf4/src/F4SE/InputMap.cpp"

namespace vmm_tests
{
	using namespace DearModdingUI::KeyCatalog;

	namespace
	{
		void RequireHeldOpeningChordIsSuppressed()
		{
			using namespace DearModdingUI;
			using namespace ControllerNavigation;
			support::ImGuiTestContext imgui{ { .disableInputTrickle = true } };
			Reset();
			ResetMenuEscapeRequest();
			Hotkeys::ReleaseActiveKeys();
			Hotkeys::SetReservedChord(kMenuDefaultGamepadToggleChord, HotkeySlot::kGamepad);
			auto& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
			io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
			bool visible = false;
			const auto route = [&](uint32_t a_code, bool a_pressed, bool a_repeat = false) {
				const auto result = RouteButton(a_code, a_pressed, a_repeat, a_pressed ? 1.0f : 0.0f,
					{ visible ? HostInputMode::kShell : HostInputMode::kGameplay, false, !visible });
				if (result == HotkeyMessageResult::kMenuToggle)
					visible = !visible;
				return result;
			};
			constexpr std::array chord{ kPadB, kPadStart, kPadLB, kPadRB, kPadBack };
			for (const auto code : chord)
				(void)route(code, true);
			require(visible && IsNavigating(), "opening chord did not toggle into navigation");
			for (int frame = 0; frame < 3; ++frame)
			{
				for (const auto code : chord)
					(void)route(code, true, true);
				if (PrepareFrame(visible ? HostInputMode::kShell : HostInputMode::kGameplay))
					visible = !visible;
				imgui.BeginWindow("routing-sidebar");
				require(!ConsumeMenuEscapeTarget(MenuEscapeTarget::kHost),
					"held opening chord dismissed the menu");
				for (const auto key : kButtonKeys)
					require(!ImGui::GetKeyData(key)->Down, "suppressed button reached ImGui as down");
				imgui.EndWindow();
			}
			for (const auto code : chord)
				(void)route(code, false);
			Reset();
			Hotkeys::ReleaseActiveKeys();
			ResetMenuEscapeRequest();
		}
	}

	void run_gamepad_input_checks(Runner& runner)
	{
		runner.test("toggle queues stay blocked through chord release and recover on focus loss", [] {
			using namespace DearModdingUI;
			using namespace Addictol::GameInput;
			HotkeyRegistry registry;
			registry.SetReservedChord(kMenuDefaultGamepadToggleChord, HotkeySlot::kGamepad);
			bool blocked = false;
			auto frame = InputQueueDecision::kForward;
			const auto decide = [&](InputQueueDecision a_previous = InputQueueDecision::kForward) {
				return DecideInputQueue(
					blocked, kMenuInputSuppression, registry.IsToggleChordHeld(), a_previous);
			};
			const auto route = [&](uint32_t a_code, bool a_pressed, bool a_repeat = false) {
				if (registry.HandleKey(a_code, 0, a_pressed, a_repeat) == HotkeyMessageResult::kMenuToggle)
					blocked = !blocked;
				frame = decide(frame);
			};
			frame = decide();
			for (const auto code : { kPadLB, kPadRB, kPadBack })
				route(code, true);
			require(blocked && frame == InputQueueDecision::kDiscard,
				"opening chord leaked its queue to later receivers");
			for (const auto code : { kPadLB, kPadRB, kPadBack })
				route(code, false);

			frame = decide();
			for (const auto code : { kPadLB, kPadRB, kPadBack })
				route(code, true);
			require(!blocked && registry.IsToggleChordHeld() &&
					frame == InputQueueDecision::kDiscard && decide() == InputQueueDecision::kDiscard,
				"closing chord leaked the closing queue or following held frame");
			frame = decide();
			route(kPadBack, false);
			route(kPadLB, true, true);
			route(kPadRB, false);
			require(registry.IsToggleChordHeld() && decide() == InputQueueDecision::kDiscard,
				"trigger release allowed a remaining shoulder hold into gameplay");
			route(kPadLB, false);
			require(!registry.IsToggleChordHeld() && frame == InputQueueDecision::kDiscard &&
					decide() == InputQueueDecision::kForward,
				"final release escaped its queue or kept the next queue blocked");

			frame = decide();
			route(kPadLB, true);
			require(frame == InputQueueDecision::kForward,
				"an old toggle activation blocked a fresh partial chord");
			route(kPadRB, true);
			route(kPadBack, true);
			require(blocked && registry.IsToggleChordHeld(),
				"fresh chord did not reopen the menu");
			registry.ReleaseActiveKeys();
			blocked = false;
			require(!registry.IsToggleChordHeld() && decide() == InputQueueDecision::kForward,
				"focus-loss or disconnect reconciliation retained the toggle hold");

			registry.SetReservedChord(ParseHotkeyChord("Mouse3+Mouse4").chord);
			blocked = true;
			frame = decide();
			route(kMouseButtonOffset + 2, true);
			route(kMouseButtonOffset + 3, true);
			require(!blocked && decide() == InputQueueDecision::kDiscard,
				"keyboard-slot mouse toggle did not retain its hold");
			route(kMouseButtonOffset + 3, false);
			route(kMouseButtonOffset + 2, false);
			require(frame == InputQueueDecision::kDiscard && decide() == InputQueueDecision::kForward,
				"mouse toggle release did not preserve the frame boundary");
			RequireHeldOpeningChordIsSuppressed();
		});
		runner.test("engine gamepad masks and Orbis ids retain catalog identity", [] {
			constexpr std::array masks{
				0x1u, 0x2u, 0x4u, 0x8u, 0x10u, 0x20u, 0x40u, 0x80u,
				0x100u, 0x200u, 0x1000u, 0x2000u, 0x4000u, 0x8000u, 0x9u, 0xAu
			};
			constexpr std::array tokens{
				"PadUp", "PadDown", "PadLeft", "PadRight", "PadStart", "PadBack",
				"PadLS", "PadRS", "PadLB", "PadRB", "PadA", "PadB", "PadX", "PadY",
				"PadLT", "PadRT"
			};
			constexpr std::array orbis{
				0x10u, 0x40u, 0x80u, 0x20u, 0x8u, 0x100000u, 0x2u, 0x4u,
				0x400u, 0x800u, 0x4000u, 0x2000u, 0x8000u, 0x1000u, 0x9u, 0xAu
			};
			auto& mapType = RE::ControlMap::GetSingleton()->pcGamePadMapType;
			for (const auto type : { RE::PC_GAMEPAD_TYPE::kDirectX, RE::PC_GAMEPAD_TYPE::kOrbis })
			{
				mapType = type;
				const auto& ids = type == RE::PC_GAMEPAD_TYPE::kOrbis ? orbis : masks;
				for (size_t index = 0; index < ids.size(); ++index)
				{
					const auto code = F4SE::InputMap::GamepadMaskToKeycode(ids[index]);
					require(DearModdingUI::KeyCatalog::Token(code) == tokens[index],
						"engine gamepad id changed token identity");
				}
				require(F4SE::InputMap::GamepadMaskToKeycode(0) == F4SE::InputMap::kMaxMacros,
					"unknown gamepad id was bindable");
			}
			mapType = RE::PC_GAMEPAD_TYPE::kDirectX;
		});
	}
}
