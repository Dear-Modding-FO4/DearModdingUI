#include <cstdint>
#include <array>

#include "../Harness.h"
#include <Support/KeyCatalog.h>
#include <Platform/input/GameInput.h>
#include <DearModdingUI/host/ControllerNavigation.h>
#include <DearModdingUI/host/MenuToggleChord.h>
#include <DearModdingUI/host/MenuDismissal.h>
#include <DearModdingUI/controls/SettingsTable.h>
#include <DearModdingUI/controls/Controls.h>
#include "../support/ImGuiTestContext.h"
#include <imgui/imgui_internal.h>

// Compile the linked production mapper against a test-owned ControlMap singleton.
#include "../../Depends/commonlibf4/src/F4SE/InputMap.cpp"

namespace vmm_tests
{
	using namespace DearModdingUI::KeyCatalog;

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
		});
		runner.test("controller arbitration preserves cursor A and edit commands", [] {
			using namespace DearModdingUI::ControllerNavigation;
			auto mode = ControllerMode::kCursor;
			require(DecideControllerMode(mode, false, kPadA) == mode &&
				DecideControllerMode(mode, false, kPadB) == mode &&
				DecideControllerAction(kPadA, mode, false, false) == ControllerAction::kCursorClick,
				"cursor A switched modes or activated focus");
			mode = DecideControllerMode(mode, false, kPadUp);
			require(mode == ControllerMode::kNavigation &&
				DecideControllerAction(kPadA, mode, false, false) == ControllerAction::kNative,
				"D-pad did not hand control to navigation");
			require(DecideControllerMode(mode, true, 0) == ControllerMode::kCursor,
				"cursor movement did not win arbitration");
			require(DecideControllerMode(mode, false, 0) == mode &&
				DecideControllerMode(mode, false, kPadLS) == ControllerMode::kCursor &&
				DecideControllerMode(ControllerMode::kCursor, false, kPadLS) == mode &&
				DecideControllerAction(kPadLS, mode, true, false) == ControllerAction::kToggleMode,
				"LS did not toggle independently of editing or idle input changed modes");
			require(DecideControllerAction(kPadB, mode, true, false) == ControllerAction::kBack &&
				DecideControllerAction(kPadLB, mode, true, false) == ControllerAction::kNative &&
				DecideControllerAction(kPadRB, mode, false, false) == ControllerAction::kContent,
				"editing stole Back or shoulder tweak semantics");
			require(DecideControllerAction(kPadX, mode, false, false) != ControllerAction::kReset &&
				DecideControllerAction(kPadX, mode, true, true) != ControllerAction::kReset &&
				DecideControllerAction(kPadX, mode, false, true) == ControllerAction::kReset,
				"X reset an absent target or an active edit");
		});
		runner.test("left stick moves focus with ImGui repeat and stays out of cursor navigation", [] {
			using namespace DearModdingUI;
			using namespace ControllerNavigation;
			support::ImGuiTestContext imgui{ { .deltaTime = 0.1f, .disableInputTrickle = true } };
			Reset();
			auto& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
			io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
			UseNavigation();
			std::array<ImGuiID, 3> items{};
			const auto frame = [&](bool a_initialize = false) {
				PrepareFrame(true);
				imgui.BeginWindow("stick-navigation", { 20, 20 }, { 500, 500 });
				for (size_t i = 0; i < items.size(); ++i)
				{
					ImGui::PushID(static_cast<int>(i));
					ImGui::Button("Item");
					items[i] = ImGui::GetItemID();
					if (a_initialize && i == 0)
					{
						ImGui::SetFocusID(items[i], ImGui::GetCurrentWindow());
						ImGui::FocusWindow(ImGui::GetCurrentWindow());
					}
					ImGui::PopID();
				}
				imgui.EndWindow();
			};
			frame(true);
			QueueStick(true, 0.0f, -0.6f);
			frame();
			require(IsNavigating() &&
				std::abs(ImGui::GetKeyData(ImGuiKey_GamepadDpadDown)->AnalogValue - 0.5f) < 0.001f,
				"left stick selected cursor mode or lost its analog navigation value");
			frame();
			require(imgui.Get()->NavId == items[1], "left stick did not move focus to the next item");
			for (int i = 0; i < 4; ++i)
				frame();
			require(imgui.Get()->NavId == items[2], "held left stick did not repeat through ImGui navigation");
			UseCursor();
			frame();
			require(!ImGui::GetKeyData(ImGuiKey_GamepadDpadDown)->Down,
				"held stick continued navigating in cursor mode");
			UseNavigation();
			QueueButton(kPadDown, 1.0f);
			frame();
			QueueStick(true, 0.0f, 0.0f);
			frame();
			require(ImGui::IsKeyDown(ImGuiKey_GamepadDpadDown), "stick release canceled physical D-pad");
			QueueButton(kPadDown, 0.0f);
			frame();
			require(!ImGui::IsKeyDown(ImGuiKey_GamepadDpadDown), "D-pad release stayed latched");
			Reset();
		});
		runner.test("LS routing toggles once and warps to focused items only", [] {
			using namespace DearModdingUI;
			using namespace ControllerNavigation;
			support::ImGuiTestContext imgui{ { .disableInputTrickle = true } };
			Reset();
			Hotkeys::ReleaseActiveKeys();
			auto& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
			io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
			UseNavigation();
			ImVec2 center{};
			const auto frame = [&](bool a_desktop = false) {
				PrepareFrame(true, a_desktop);
				imgui.BeginWindow("cursor-warp", { 80, 90 }, { 500, 500 });
				ImGui::Button("Target", { 150, 40 });
				center = ImRect{ ImGui::GetItemRectMin(), ImGui::GetItemRectMax() }.GetCenter();
				ImGui::SetFocusID(ImGui::GetItemID(), ImGui::GetCurrentWindow());
				ImGui::FocusWindow(ImGui::GetCurrentWindow());
				imgui.EndWindow();
			};
			const auto route = [&](bool a_down, bool a_repeat = false) {
				(void)RouteButton(kPadLS, a_down, a_repeat, a_down ? 1.0f : 0.0f,
					{ true, false, true, false });
			};
			frame();
			io.AddMousePosEvent(400.0f, 400.0f);
			frame(true);
			Hotkeys::BeginCapture(HotkeySlot::kGamepad);
			route(true);
			frame();
			require(IsNavigating() && !TakeCursorWarp(), "captured LS toggled or warped");
			(void)Hotkeys::CancelCapture();
			route(true, true);
			frame();
			require(IsNavigating(), "captured hold escaped the forwarded-bit guard");
			route(false);
			frame();
			route(true);
			QueueButton(kPadA, 1.0f);
			PrepareFrame(true);
			const auto warp = TakeCursorWarp();
			require(!IsNavigating() && warp && warp->x == center.x && warp->y == center.y &&
				!TakeCursorWarp(), "LS did not produce one focused-item warp");
			frame();
			require(io.MouseDown[0], "A in the warp frame did not retain cursor click semantics");
			QueueButton(kPadA, 0.0f);
			route(true, true);
			frame();
			require(!IsNavigating() && !TakeCursorWarp(), "held LS toggled twice");
			route(false);
			frame();
			route(true);
			frame();
			require(IsNavigating() && !TakeCursorWarp(), "LS did not return to navigation without a warp");
			route(false);
			frame();
			BeginDesktopInput();
			QueueButton(kPadLS, 1.0f);
			frame(true);
			require(!IsNavigating() && io.MousePos.x == center.x && io.MousePos.y == center.y &&
				!TakeCursorWarp(), "desktop warp did not update the same frame's mouse position");
			BeginDesktopInput();
			QueueButton(kPadLS, 0.0f);
			io.AddMousePosEvent(400.0f, 400.0f);
			frame(true);
			require(io.MousePos.x == center.x && io.MousePos.y == center.y,
				"stationary desktop mouse polling undid the warp");
			BeginDesktopInput();
			io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, true, 0.75f);
			frame(true);
			const auto moved = io.MousePos;
			require(moved.x > center.x && moved.y == center.y &&
				!ImGui::IsKeyDown(ImGuiKey_GamepadDpadRight),
				"desktop stick did not move the warped cursor exclusively");
			BeginDesktopInput();
			io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, false, 0.0f);
			io.AddMousePosEvent(400.0f, 400.0f);
			frame(true);
			require(io.MousePos.x == moved.x && io.MousePos.y == moved.y,
				"desktop stick release or stationary mouse polling lost the virtual position");
			UseNavigation();
			io.AddMousePosEvent(430.0f, 450.0f);
			frame(true);
			require(!IsNavigating() && io.MousePos.x == 430.0f && io.MousePos.y == 450.0f,
				"real desktop mouse movement did not reclaim the cursor");
			Reset();
			Hotkeys::ReleaseActiveKeys();
			UseNavigation();
			imgui.Get()->NavId = 0;
			route(true);
			PrepareFrame(true);
			require(!IsNavigating() && !TakeCursorWarp(), "LS invented a target without a focused item");
			Reset();
			Hotkeys::ReleaseActiveKeys();
		});
		runner.test("controller input preserves analog triggers and owns cursor clicks and Back", [] {
			using namespace DearModdingUI;
			using namespace ControllerNavigation;
			support::ImGuiTestContext imgui{ { .disableInputTrickle = true } };
			Reset();
			auto& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
			io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
			QueueButton(kPadLT, AnalogValue(0.55f));
			QueueButton(kPadRT, AnalogValue(0.05f));
			PrepareFrame(true);
			imgui.BeginWindow("controller");
			require(ImGui::IsKeyDown(ImGuiKey_GamepadL2) &&
				std::abs(ImGui::GetKeyData(ImGuiKey_GamepadL2)->AnalogValue - 0.5f) < 0.001f &&
				!ImGui::IsKeyDown(ImGuiKey_GamepadR2),
				"trigger deadzone or analog range was lost");
			imgui.EndWindow();
			Reset();
			QueueButton(kPadA, 1.0f);
			PrepareFrame(true);
			imgui.BeginWindow("controller");
			require(io.MouseDown[0] &&
				!ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown),
				"cursor A did not click exclusively");
			imgui.EndWindow();
			QueueButton(kPadUp, 1.0f);
			QueueButton(kPadA, 0.0f);
			PrepareFrame(true);
			imgui.BeginWindow("controller");
			require(!io.MouseDown[0], "A release was lost after mode changed");
			ImGui::Button("edit");
			ImGui::SetActiveID(ImGui::GetItemID(), ImGui::GetCurrentWindow());
			imgui.EndWindow();
			(void)RouteButton(kPadB, true, false, 1.0f, { true, false, true, false });
			PrepareFrame(true);
			imgui.BeginWindow("controller");
			require(ConsumeMenuEscapeTarget(MenuEscapeTarget::kInteraction) &&
				!ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight),
				"B bypassed the dismissal chain or also reached ImGui cancel");
			ImGui::ClearActiveID();
			imgui.EndWindow();
			Reset();
			ResetMenuEscapeRequest();
		});
		runner.test("controller focus and X use the selected sidebar and existing row reset", [] {
			using namespace DearModdingUI;
			using namespace ControllerNavigation;
			support::ImGuiTestContext imgui{ { .disableInputTrickle = true } };
			Reset();
			auto& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
			io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
			UseNavigation();
			PrepareFrame(true);
			imgui.BeginWindow("controller", { 20, 20 }, { 800, 500 });
			BeginPane(Pane::kSidebar);
			(void)DrawSelectableRow({ .id = "first", .label = "First" });
			(void)DrawSelectableRow({ .id = "selected", .label = "Selected", .selected = true });
			const auto selected = ImGui::GetItemID();
			imgui.EndWindow();
			PrepareFrame(true);
			imgui.BeginWindow("controller", { 20, 20 }, { 800, 500 });
			require(imgui.Get()->NavId == selected,
				"pending first-item initialization displaced the selected sidebar row");
			(void)SettingsTable::Begin(0, "reset-table");
			(void)SettingsTable::BeginRow(0, "value", "Value", nullptr);
			ImGui::Button("setting");
			ImGui::SetFocusID(ImGui::GetItemID(), ImGui::GetCurrentWindow());
			bool reset{};
			(void)SettingsTable::EndRow(0, { true, true }, reset);
			(void)SettingsTable::End(0);
			imgui.EndWindow();
			QueueButton(kPadX, 1.0f);
			PrepareFrame(true);
			imgui.BeginWindow("controller", { 20, 20 }, { 800, 500 });
			(void)SettingsTable::Begin(0, "reset-table");
			(void)SettingsTable::BeginRow(0, "value", "Value", nullptr);
			ImGui::Button("setting");
			const auto setting = ImGui::GetItemID();
			require(imgui.Get()->NavId == setting, "row focus was not retained");
			(void)SettingsTable::EndRow(0, { true, true }, reset);
			require(!ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft), "X also reached ImGui");
			require(reset, "X did not reset the focused row");
			(void)SettingsTable::End(0);
			imgui.EndWindow();
			Reset();
		});
		runner.test("controller routing suppresses opening chords and captured holds until release", [] {
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
					{ visible, false, visible, !visible });
				if (result == HotkeyMessageResult::kMenuToggle)
					visible = !visible;
				return result;
			};
			const auto quietFrame = [&](bool a_desktop = false) {
				if (PrepareFrame(visible, a_desktop))
					visible = !visible;
				imgui.BeginWindow("routing-sidebar");
				BeginPane(Pane::kSidebar);
				ImGui::Button("selected");
				FocusSelectedItem(true);
				auto* sidebar = ImGui::GetCurrentWindow();
				ImGui::Begin("routing-content", nullptr, ImGuiWindowFlags_NoFocusOnAppearing);
				BeginPane(Pane::kContent);
				ImGui::Button("content");
				ImGui::End();
				require(imgui.Get()->NavWindow == sidebar && !TakeClose() && !TakeSearch() &&
					!ConsumeMenuEscapeTarget(MenuEscapeTarget::kHost),
					"held opening chord switched panes or dismissed the menu");
				for (const auto key : kButtonKeys)
					require(!ImGui::GetKeyData(key)->Down, "suppressed button reached ImGui as down");
				imgui.EndWindow();
			};
			(void)route(kPadB, true);
			(void)route(kPadStart, true);
			(void)route(kPadLB, true);
			(void)route(kPadRB, true);
			require(route(kPadBack, true) == HotkeyMessageResult::kMenuToggle && visible && IsNavigating(),
				"opening chord did not toggle into navigation");
			for (int frame = 0; frame < 3; ++frame)
			{
				for (const auto code : { kPadLB, kPadRB, kPadBack, kPadB, kPadStart })
					(void)route(code, true, true);
				quietFrame();
			}
			for (const auto code : { kPadLB, kPadRB, kPadBack, kPadB, kPadStart })
				(void)route(code, false);
			quietFrame();
			(void)route(kPadB, true);
			require(ConsumeMenuEscapeTarget(MenuEscapeTarget::kHost),
				"fresh B no longer dismisses after its held press was released");
			(void)route(kPadB, false);
			Hotkeys::BeginCapture(HotkeySlot::kGamepad);
			(void)route(kPadA, true);
			(void)route(kPadY, true);
			(void)route(kPadY, false);
			require(Hotkeys::TakeCapture().has_value() && !Hotkeys::IsCapturing(),
				"capture did not finish on companion release");
			(void)route(kPadA, true, true);
			quietFrame();
			(void)route(kPadA, false);
			(void)route(kPadDown, true);
			PrepareFrame(true);
			imgui.BeginWindow("routing-sidebar");
			require(ImGui::IsKeyDown(ImGuiKey_GamepadDpadDown), "fresh navigation press was dropped");
			imgui.EndWindow();
			(void)route(kPadDown, true, true);
			PrepareFrame(true);
			imgui.BeginWindow("routing-sidebar");
			require(ImGui::IsKeyDown(ImGuiKey_GamepadDpadDown), "forwarded hold was dropped");
			imgui.EndWindow();
			(void)route(kPadDown, false);
			PrepareFrame(true);
			imgui.BeginWindow("routing-sidebar");
			require(!ImGui::IsKeyDown(ImGuiKey_GamepadDpadDown), "forwarded release was dropped");
			imgui.EndWindow();
			Reset();
			Hotkeys::ReleaseActiveKeys();
			visible = false;
			Hotkeys::SetReservedChord({ kPadB, 0 }, HotkeySlot::kGamepad);
			require(route(kPadB, true) == HotkeyMessageResult::kMenuToggle, "B toggle was not owned");
			(void)route(kPadB, true, true);
			quietFrame();
			(void)route(kPadB, false);
			Hotkeys::BeginCapture(HotkeySlot::kGamepad);
			(void)route(kPadB, true);
			require(ConsumeMenuEscapeTarget(MenuEscapeTarget::kKeyCapture) && Hotkeys::CancelCapture(),
				"fresh B did not retain capture dismissal");
			(void)route(kPadB, false);
			Reset();
			Hotkeys::ReleaseActiveKeys();
			Hotkeys::SetReservedChord(kMenuDefaultGamepadToggleChord, HotkeySlot::kGamepad);
			visible = false;
			BeginDesktopInput();
			for (const auto code : { kPadLB, kPadRB, kPadBack })
				QueueButton(code, 1.0f);
			quietFrame(true);
			require(visible && IsNavigating(), "desktop opening chord did not use the toggle result");
			for (int frame = 0; frame < 3; ++frame)
			{
				BeginDesktopInput();
				for (const auto code : { kPadLB, kPadRB, kPadBack })
					QueueButton(code, 1.0f);
				quietFrame(true);
			}
			BeginDesktopInput();
			for (const auto code : { kPadLB, kPadRB, kPadBack })
				QueueButton(code, 0.0f);
			quietFrame(true);
			BeginDesktopInput();
			QueueButton(kPadRB, 1.0f);
			PrepareFrame(true, true);
			imgui.BeginWindow("routing-content");
			BeginPane(Pane::kContent);
			require(imgui.Get()->NavWindow == ImGui::GetCurrentWindow(),
				"desktop release was lost and blocked the next fresh pane command");
			imgui.EndWindow();
			BeginDesktopInput();
			QueueButton(kPadRB, 0.0f);
			PrepareFrame(true, true);
			imgui.BeginWindow("routing-content");
			imgui.EndWindow();
			Reset();
			Hotkeys::ReleaseActiveKeys();
			ResetMenuEscapeRequest();
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
