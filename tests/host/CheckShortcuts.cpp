#include "../Harness.h"
#include "../support/ImGuiTestContext.h"

#include <DearModdingUI/UIBindings.generated.h>
#include <DearModdingUI/controls/TextInput.h>
#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/host/UIAdapter.h>

#include <imgui/imgui.h>

#include <string>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;

	namespace
	{
		[[nodiscard]] DMUI_Result AcceptClient(DMUI_ClientHandle) noexcept
		{
			return DMUI_RESULT_OK;
		}

		// Two windows poll one chord; "A" can host an editor.
		class ShortcutHarness
		{
		public:
			struct Pressed
			{
				uint32_t first{};
				uint32_t second{};
			};

			ShortcutHarness() :
				m_imgui({ .disableInputTrickle = true, .disableErrorRecovery = true })
			{}

			Pressed Frame(
				const char* a_chord,
				DMUI_UIShortcutFlags a_flags = 0,
				const char* a_focus = nullptr,
				bool a_editor = false)
			{
				const auto& api = UI::API();
				Pressed pressed;
				m_imgui.BeginWindow(
					"A", { 0.0f, 0.0f }, { 300.0f, 200.0f }, ImGuiWindowFlags_NoFocusOnAppearing);
				if (a_focus)
					ImGui::SetWindowFocus(a_focus);
				if (a_editor)
				{
					std::vector<char> storage(32, '\0');
					DMUI_TextBuffer buffer{ storage.data(), storage.size(), nullptr, nullptr };
					DMUI_TextEditState state{};
					TextFieldSpec spec{ .editFlags = DMUI_UI_TEXT_EDIT_FLAGS_REQUEST_FOCUS };
					(void)DrawTextField("Editor", buffer, spec, state);
				}
				m_result = api.shortcut(1u, a_chord, a_flags, &pressed.first);
				ImGui::End();
				ImGui::SetNextWindowPos({ 320.0f, 0.0f });
				ImGui::SetNextWindowSize({ 300.0f, 200.0f });
				ImGui::Begin("B", nullptr, ImGuiWindowFlags_NoFocusOnAppearing);
				(void)api.shortcut(1u, a_chord, a_flags, &pressed.second);
				m_imgui.EndWindow();
				return pressed;
			}

			[[nodiscard]] DMUI_Result Result() const noexcept { return m_result; }

		private:
			support::ImGuiTestContext m_imgui;
			DMUI_Result m_result{ DMUI_RESULT_OK };
		};

		void Chord(bool a_down, ImGuiKey a_key)
		{
			auto& io = ImGui::GetIO();
			io.AddKeyEvent(ImGuiMod_Ctrl, a_down);
			io.AddKeyEvent(a_key, a_down);
		}
	}

	void run_shortcut_checks(Runner& runner)
	{
		runner.test("shortcuts fire for the focused window only", [] {
			const UI::Testing::ValidationOverride validation{ &AcceptClient };
			ShortcutHarness harness;
			(void)harness.Frame("Ctrl+S", 0, "A");
			(void)harness.Frame("Ctrl+S");
			Chord(true, ImGuiKey_S);
			const auto pressed = harness.Frame("Ctrl+S");
			require(harness.Result() == DMUI_RESULT_OK && pressed.first == 1 && pressed.second == 0,
				"the chord did not reach exactly the focused window: " +
					std::to_string(harness.Result()) + " " + std::to_string(pressed.first) +
					std::to_string(pressed.second));
			Chord(false, ImGuiKey_S);
			(void)harness.Frame("Ctrl+S");

			(void)harness.Frame("Ctrl+S", 0, "B");
			(void)harness.Frame("Ctrl+S");
			Chord(true, ImGuiKey_S);
			const auto moved = harness.Frame("Ctrl+S");
			require(moved.first == 0 && moved.second == 1,
				"focus moving to the other window did not move the chord");
			Chord(false, ImGuiKey_S);
			(void)harness.Frame("Ctrl+S");
		});

		runner.test("an active editor keeps its own keys", [] {
			const UI::Testing::ValidationOverride validation{ &AcceptClient };
			ShortcutHarness harness;
			(void)harness.Frame("A", 0, "A");
			(void)harness.Frame("A");
			ImGui::GetIO().AddKeyEvent(ImGuiKey_A, true);
			const auto free = harness.Frame("A");
			require(free.first == 1, "a plain chord did not fire without an editor");
			ImGui::GetIO().AddKeyEvent(ImGuiKey_A, false);
			(void)harness.Frame("A");

			(void)harness.Frame("A", 0, "A", true);
			(void)harness.Frame("A", 0, "A", true);
			(void)harness.Frame("A", 0, "A", true);
			ImGui::GetIO().AddKeyEvent(ImGuiKey_A, true);
			const auto typing = harness.Frame("A", 0, "A", true);
			require(typing.first == 0 && typing.second == 0,
				"a chord fired while an editor was active");
		});

		runner.test("host hotkey chords never reach pages", [] {
			const UI::Testing::ValidationOverride validation{ &AcceptClient };
			ShortcutHarness harness;
			(void)harness.Frame("Ctrl+S", 0, "A");
			(void)harness.Frame("Ctrl+S");
			Hotkeys::SetReservedChord(HotkeyChord{ 0x1F, kHotkeyModifierControl });
			Chord(true, ImGuiKey_S);
			const auto reserved = harness.Frame("Ctrl+S");
			Hotkeys::SetReservedChord(HotkeyChord{});
			require(harness.Result() == DMUI_RESULT_OK && reserved.first == 0,
				"a chord the host acts on was delivered");
			Chord(false, ImGuiKey_S);
			(void)harness.Frame("Ctrl+S");
			Chord(true, ImGuiKey_S);
			require(harness.Frame("Ctrl+S").first == 1,
				"releasing the host chord did not restore delivery");
			Chord(false, ImGuiKey_S);
			(void)harness.Frame("Ctrl+S");
		});

		runner.test("shortcuts accept keyboard chords only and repeat on request", [] {
			const UI::Testing::ValidationOverride validation{ &AcceptClient };
			ShortcutHarness harness;
			for (const char* rejected : { "PadA", "Mouse1", "none", "", "A+B", "Hyper+Q", "Ctrl+Escape" })
			{
				(void)harness.Frame(rejected);
				require(harness.Result() == DMUI_RESULT_INVALID_ARGUMENT,
					std::string{ "an unsupported chord was accepted: " } + rejected);
			}
			(void)harness.Frame("A", 2);
			require(harness.Result() == DMUI_RESULT_INVALID_ARGUMENT,
				"unknown shortcut flags were accepted");
			uint32_t pressed{};
			require(UI::API().shortcut(1u, nullptr, 0, &pressed) == DMUI_RESULT_INVALID_ARGUMENT &&
					UI::API().shortcut(1u, "A", 0, nullptr) == DMUI_RESULT_INVALID_ARGUMENT,
				"null arguments were accepted");

			const auto count = [&](DMUI_UIShortcutFlags a_flags) {
				ShortcutHarness held;
				(void)held.Frame("Ctrl+S", a_flags, "A");
				(void)held.Frame("Ctrl+S", a_flags);
				Chord(true, ImGuiKey_S);
				uint32_t total{};
				for (int frame = 0; frame < 90; ++frame)
					total += held.Frame("Ctrl+S", a_flags).first;
				Chord(false, ImGuiKey_S);
				(void)held.Frame("Ctrl+S", a_flags);
				return total;
			};
			require(count(0) == 1 && count(DMUI_UI_SHORTCUT_FLAGS_REPEAT) > 3,
				"holding a chord did not fire once, or repeat on request");
		});
	}
}
