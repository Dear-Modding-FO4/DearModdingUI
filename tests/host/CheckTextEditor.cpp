#include "../Harness.h"
#include "../support/ImGuiTestContext.h"

#include <DearModdingUI/controls/TextInput.h>
#include <DearModdingUI/host/MenuDismissal.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;

	namespace
	{
		constexpr ImVec2 kFieldPosition{ 20.0f, 20.0f };

		class EditorHarness
		{
		public:
			explicit EditorHarness(std::string_view a_text) :
				m_storage(256, '\0')
			{
				Assign(a_text);
			}

			void Assign(std::string_view a_text)
			{
				std::ranges::fill(m_storage, '\0');
				std::memcpy(m_storage.data(), a_text.data(), a_text.size());
			}

			[[nodiscard]] std::string Text() const
			{
				return m_storage.data();
			}

			DMUI_TextEditState Frame(
				DMUI_UITextEditFlags a_flags,
				size_t a_cursor = 0,
				ImGuiInputTextFlags a_inputFlags = ImGuiInputTextFlags_None)
			{
				m_imgui.BeginWindow(
					"##TextEditor",
					{ 0.0f, 0.0f },
					{ 640.0f, 240.0f },
					ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings,
					ImGuiCond_Always);
				ImGui::SetCursorScreenPos(kFieldPosition);
				ImGui::SetNextItemWidth(400.0f);
				DMUI_TextBuffer buffer{ m_storage.data(), m_storage.size(), nullptr, nullptr };
				DMUI_TextEditState state{};
				m_result = DrawTextField(
					"Console",
					buffer,
					{
						.flags = a_inputFlags,
						.editFlags = a_flags,
						.cursor = a_cursor,
						.styleSpans = m_style,
						.styleUserData = m_styleData
					},
					state);
				m_imgui.EndWindow(true);
				return state;
			}

			DMUI_TextEditState Press(ImGuiKey a_key, DMUI_UITextEditFlags a_flags)
			{
				auto& io = ImGui::GetIO();
				io.AddKeyEvent(a_key, true);
				const auto state = Frame(a_flags);
				io.AddKeyEvent(a_key, false);
				(void)Frame(a_flags);
				return state;
			}

			[[nodiscard]] DMUI_Result Result() const noexcept { return m_result; }

			void Style(DMUI_TextStyleFn a_style, void* a_data) noexcept
			{
				m_style = a_style;
				m_styleData = a_data;
			}

			[[nodiscard]] const ImDrawList& DrawList() const
			{
				return *ImGui::FindWindowByName("##TextEditor")->DrawList;
			}

		private:
			DMUI_TextStyleFn m_style{};
			void* m_styleData{};
			support::ImGuiTestContext m_imgui;
			std::vector<char> m_storage;
			DMUI_Result m_result{ DMUI_RESULT_OK };
		};

		[[nodiscard]] bool Has(const DMUI_TextEditState& a_state, uint32_t a_event) noexcept
		{
			return (a_state.events & a_event) != 0;
		}

		[[nodiscard]] DMUI_TextEditState Focus(EditorHarness& a_editor, DMUI_UITextEditFlags a_flags)
		{
			(void)a_editor.Frame(a_flags | DMUI_UI_TEXT_EDIT_FLAGS_REQUEST_FOCUS);
			(void)a_editor.Frame(a_flags | DMUI_UI_TEXT_EDIT_FLAGS_REQUEST_FOCUS);
			return a_editor.Frame(a_flags);
		}
	}

	void run_text_editor_checks(Runner& runner)
	{
		runner.test("text editor reload applies client text before typed input", [] {
			EditorHarness editor{ "abc" };
			(void)Focus(editor, DMUI_UI_TEXT_EDIT_FLAGS_NONE);
			editor.Assign("hello world");
			ImGui::GetIO().AddInputCharactersUTF8("X");
			const auto reloaded = editor.Frame(DMUI_UI_TEXT_EDIT_FLAGS_RELOAD, 5);
			require(editor.Result() == DMUI_RESULT_OK && editor.Text() == "helloX world",
				"reload did not precede this frame's typed input");
			require(Has(reloaded, DMUI_UI_TEXT_EDIT_EVENTS_EDITED) &&
					reloaded.cursor == 6 && reloaded.selectionStart == 6 &&
					reloaded.selectionEnd == 6,
				"reload did not report the edit or the placed cursor");

			editor.Assign("\xC3\xA9");
			(void)editor.Frame(DMUI_UI_TEXT_EDIT_FLAGS_RELOAD, 1);
			require(editor.Result() == DMUI_RESULT_INVALID_ARGUMENT,
				"reload accepted a cursor inside a UTF-8 sequence");
		});

		runner.test("text editor reports keys, submit, and cancel without losing focus", [] {
			EditorHarness editor{ "" };
			constexpr DMUI_UITextEditFlags flags{
				DMUI_UI_TEXT_EDIT_FLAGS_HISTORY_KEYS | DMUI_UI_TEXT_EDIT_FLAGS_COMPLETION_KEY |
				DMUI_UI_TEXT_EDIT_FLAGS_KEEP_FOCUS_ON_SUBMIT | DMUI_UI_TEXT_EDIT_FLAGS_CAPTURE_CANCEL
			};
			(void)Focus(editor, flags);
			ImGui::GetIO().AddInputCharactersUTF8("tgm");
			(void)editor.Frame(flags);
			const auto up = editor.Press(ImGuiKey_UpArrow, flags);
			const auto tab = editor.Press(ImGuiKey_Tab, flags);
			require(Has(up, DMUI_UI_TEXT_EDIT_EVENTS_HISTORY_PREVIOUS) &&
					Has(tab, DMUI_UI_TEXT_EDIT_EVENTS_COMPLETION),
				"history or completion keys were not reported");

			const auto submitted = editor.Press(ImGuiKey_Enter, flags);
			const auto refocused = editor.Frame(flags);
			require(Has(submitted, DMUI_UI_TEXT_EDIT_EVENTS_SUBMITTED) && refocused.active != 0,
				"submit was not reported or the editor lost focus");

			CaptureMenuEscapePress(HostInputMode::kShell, false, 0);
			const auto canceled = editor.Press(ImGuiKey_Escape, flags);
			require(!ConsumeMenuEscapeTarget(MenuEscapeTarget::kInteraction),
				"captured Escape still reached the host interaction cancel");
			const auto after = editor.Frame(flags);
			require(Has(canceled, DMUI_UI_TEXT_EDIT_EVENTS_CANCELED) && after.active != 0 &&
					editor.Text() == "tgm",
				"captured Escape reverted or deactivated the editor");
			require(ImGui::GetIO().ConfigInputTextEnterKeepActive == false,
				"editor leaked the keep-active setting");
		});

		runner.test("text editor styles the text it holds after this frame's edits", [] {
			struct Script
			{
				std::string seen;
				size_t length{};

				static DMUI_Result DMUI_CALL Call(
					void* a_user, const char* a_text, size_t a_length,
					const DMUI_TextSpan** a_spans, size_t* a_count) noexcept
				{
					auto& script = *static_cast<Script*>(a_user);
					script.seen.assign(a_text, a_length);
					static DMUI_TextSpan span{};
					span.offset = 0;
					span.length = a_length;
					span.flags = DMUI_TEXT_SPAN_CUSTOM_BACKGROUND;
					span.backgroundColor = { 1.0f, 0.0f, 0.0f, 1.0f };
					*a_spans = &span;
					*a_count = 1;
					return DMUI_RESULT_OK;
				}
			} script;
			EditorHarness editor{ "ab" };
			editor.Style(&Script::Call, &script);
			(void)Focus(editor, DMUI_UI_TEXT_EDIT_FLAGS_NONE);
			ImGui::GetIO().AddInputCharactersUTF8("X");
			(void)editor.Frame(DMUI_UI_TEXT_EDIT_FLAGS_NONE);
			require(editor.Result() == DMUI_RESULT_OK && script.seen == editor.Text() &&
					script.seen.find('X') != std::string::npos,
				"the styler did not see the text after this frame's edit");
		});

		runner.test("text editor rejects conflicting and unknown flags", [] {
			EditorHarness editor{ "" };
			(void)editor.Frame(DMUI_UI_TEXT_EDIT_FLAGS_COMPLETION_KEY, 0, ImGuiInputTextFlags_AllowTabInput);
			require(editor.Result() == DMUI_RESULT_INVALID_ARGUMENT,
				"completion key accepted Tab input");
			(void)editor.Frame(0x80000000u);
			require(editor.Result() == DMUI_RESULT_INVALID_ARGUMENT,
				"unknown edit flags were accepted");
		});
	}
}
