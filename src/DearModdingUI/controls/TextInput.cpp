#include <DearModdingUI/controls/TextInput.h>
#include <DearModdingUI/host/MenuDismissal.h>
#include <DearModdingUI/host/RenderExecution.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace ImStb
{
#include <imgui/imstb_textedit.h>
}

#include <algorithm>
#include <climits>
#include <cstring>
#include <new>
#include <vector>

namespace DearModdingUI
{
	namespace
	{
		struct InputSnapshot
		{
			std::vector<char> text;
			ImStb::STB_TexteditState stb{};
			ImVec2 scroll{};
			int textLength{};
			int bufferCapacity{};
			float cursorAnimation{};
			bool cursorFollow{};
			bool cursorCenterY{};
			bool selectedAllMouseLock{};
			bool editedBefore{};
			bool editedThisFrame{};
			bool wantReloadUserBuffer{};
			ImS8 lastMoveDirection{};
			int reloadSelectionStart{};
			int reloadSelectionEnd{};
			bool captured{};
		};

		struct InputCall
		{
			DMUI_TextBuffer& buffer;
			char* originalData{};
			size_t originalCapacity{};
			size_t originalLength{};
			std::vector<char> original;
			InputSnapshot snapshot;
			ImGuiID id{};
			char* deactivatedText{};
			size_t deactivatedTextSize{};
			DMUI_Result failure{ DMUI_RESULT_OK };
			uint32_t keyEvents{};
			bool activeEditedBefore{};
			bool activeEditedThisFrame{};
			bool anyEditedThisFrame{};
			ImGuiDeactivatedItemData deactivatedItem{};
		};

		[[nodiscard]] bool CaptureState(
			InputCall& a_call,
			ImGuiInputTextState& a_state) noexcept
		{
			if (a_call.snapshot.captured)
				return true;
			if (a_state.ID != a_call.id || a_state.TextLen < 0 ||
				!a_state.Stb || !a_state.TextA.Data)
				return false;
			const auto length = static_cast<size_t>(a_state.TextLen);
			if (length + 1 > a_call.snapshot.text.size() ||
				length + 1 > static_cast<size_t>(a_state.TextA.Size))
				return false;

			std::memcpy(
				a_call.snapshot.text.data(),
				a_state.TextA.Data,
				length + 1);
			a_call.snapshot.stb = *a_state.Stb;
			a_call.snapshot.scroll = a_state.Scroll;
			a_call.snapshot.textLength = a_state.TextLen;
			a_call.snapshot.bufferCapacity = a_state.BufCapacity;
			a_call.snapshot.cursorAnimation = a_state.CursorAnim;
			a_call.snapshot.cursorFollow = a_state.CursorFollow;
			a_call.snapshot.cursorCenterY = a_state.CursorCenterY;
			a_call.snapshot.selectedAllMouseLock =
				a_state.SelectedAllMouseLock;
			a_call.snapshot.editedBefore = a_state.EditedBefore;
			a_call.snapshot.editedThisFrame = a_state.EditedThisFrame;
			a_call.snapshot.wantReloadUserBuffer =
				a_state.WantReloadUserBuf;
			a_call.snapshot.lastMoveDirection =
				a_state.LastMoveDirectionLR;
			a_call.snapshot.reloadSelectionStart =
				a_state.ReloadSelectionStart;
			a_call.snapshot.reloadSelectionEnd =
				a_state.ReloadSelectionEnd;
			a_call.snapshot.captured = true;
			return true;
		}

		void RestoreState(InputCall& a_call) noexcept
		{
			auto* state = ImGui::GetInputTextState(a_call.id);
			if (state && a_call.snapshot.captured && state->Stb &&
				state->TextA.Data &&
				static_cast<size_t>(a_call.snapshot.textLength) + 1 <=
					static_cast<size_t>(state->TextA.Size))
			{
				std::memcpy(
					state->TextA.Data,
					a_call.snapshot.text.data(),
					static_cast<size_t>(a_call.snapshot.textLength) + 1);
				*state->Stb = a_call.snapshot.stb;
				state->TextLen = a_call.snapshot.textLength;
				state->TextSrc = state->TextA.Data;
				state->BufCapacity = a_call.snapshot.bufferCapacity;
				state->Scroll = a_call.snapshot.scroll;
				state->CursorAnim = a_call.snapshot.cursorAnimation;
				state->CursorFollow = a_call.snapshot.cursorFollow;
				state->CursorCenterY = a_call.snapshot.cursorCenterY;
				state->SelectedAllMouseLock =
					a_call.snapshot.selectedAllMouseLock;
				state->EditedBefore = a_call.snapshot.editedBefore;
				state->EditedThisFrame = a_call.snapshot.editedThisFrame;
				state->WantReloadUserBuf =
					a_call.snapshot.wantReloadUserBuffer;
				state->LastMoveDirectionLR =
					a_call.snapshot.lastMoveDirection;
				state->ReloadSelectionStart =
					a_call.snapshot.reloadSelectionStart;
				state->ReloadSelectionEnd =
					a_call.snapshot.reloadSelectionEnd;
			}

			if (a_call.deactivatedText &&
				a_call.originalLength + 1 <=
					a_call.deactivatedTextSize)
			{
				std::memcpy(
					a_call.deactivatedText,
					a_call.original.data(),
					a_call.originalLength + 1);
			}

			std::memcpy(
				a_call.originalData,
				a_call.original.data(),
				a_call.originalLength + 1);
		}

		void RejectResize(
			InputCall& a_call,
			ImGuiInputTextCallbackData& a_data,
			DMUI_Result a_result) noexcept
		{
			a_call.failure = a_result;
			RestoreState(a_call);
			a_data.Buf = a_call.originalData;
			a_data.BufSize = static_cast<int>(a_call.originalCapacity);
			a_data.BufTextLen = static_cast<int>(a_call.originalLength);
		}

		int InputCallback(ImGuiInputTextCallbackData* a_data) noexcept
		{
			auto& call = *static_cast<InputCall*>(a_data->UserData);
			if (a_data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
			{
				call.keyEvents |= a_data->EventKey == ImGuiKey_UpArrow ?
					DMUI_UI_TEXT_EDIT_EVENTS_HISTORY_PREVIOUS :
					DMUI_UI_TEXT_EDIT_EVENTS_HISTORY_NEXT;
				return 0;
			}
			if (a_data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
			{
				call.keyEvents |= DMUI_UI_TEXT_EDIT_EVENTS_COMPLETION;
				return 0;
			}
			if (a_data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter)
			{
				auto* state = ImGui::GetInputTextState(call.id);
				if (!state || !CaptureState(call, *state))
				{
					call.failure = DMUI_RESULT_RESOURCE_EXHAUSTED;
					return 1;
				}
				return 0;
			}

			if (a_data->EventFlag != ImGuiInputTextFlags_CallbackResize)
				return 0;
			if (call.failure != DMUI_RESULT_OK)
			{
				RejectResize(call, *a_data, call.failure);
				return 0;
			}

			const auto minimumCapacity =
				static_cast<size_t>(a_data->BufSize);
			if (minimumCapacity == 0 ||
				minimumCapacity > static_cast<size_t>(INT_MAX))
			{
				RejectResize(
					call,
					*a_data,
					DMUI_RESULT_BUFFER_TOO_SMALL);
				return 0;
			}
			if (minimumCapacity <= call.buffer.capacity)
			{
				a_data->Buf = call.buffer.data;
				a_data->BufSize =
					static_cast<int>(call.buffer.capacity);
				return 0;
			}

			char* resizedData = call.buffer.data;
			size_t resizedCapacity = call.buffer.capacity;
			DMUI_Result result{};
			{
				const RenderExecution::ClientGuard guard{
					DMUI_INVALID_CLIENT_HANDLE,
					false
				};
				result = call.buffer.resize(
					call.buffer.userData,
					minimumCapacity,
					&resizedData,
					&resizedCapacity);
			}
			if (result != DMUI_RESULT_OK)
			{
				RejectResize(call, *a_data, result);
				return 0;
			}
			if (!resizedData || resizedCapacity < minimumCapacity ||
				resizedCapacity > static_cast<size_t>(INT_MAX) ||
				std::memcmp(
					resizedData,
					call.original.data(),
					call.originalLength + 1) != 0)
			{
				RejectResize(
					call,
					*a_data,
					DMUI_RESULT_CALLBACK_FAILED);
				return 0;
			}

			call.buffer.data = resizedData;
			call.buffer.capacity = resizedCapacity;
			a_data->Buf = resizedData;
			a_data->BufSize = static_cast<int>(resizedCapacity);
			return 0;
		}

		struct InputOutcome
		{
			bool returned{};
			uint32_t keyEvents{};
		};

		[[nodiscard]] DMUI_Result DrawTextInputImpl(
			const char* a_label,
			const char* a_hint,
			DMUI_TextBuffer& a_buffer,
			ImGuiInputTextFlags a_flags,
			InputOutcome& a_outcome)
		{
			a_outcome = {};
			if (!a_label || !a_hint)
				return DMUI_RESULT_INVALID_ARGUMENT;
			if (!a_buffer.data || a_buffer.capacity == 0 ||
				a_buffer.capacity > static_cast<size_t>(INT_MAX))
				return DMUI_RESULT_INVALID_ARGUMENT;
			if (!ImGui::GetCurrentContext())
				return DMUI_RESULT_HOST_NOT_READY;

			const auto* terminator = static_cast<const char*>(
				std::memchr(a_buffer.data, '\0', a_buffer.capacity));
			if (!terminator)
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto length =
				static_cast<size_t>(terminator - a_buffer.data);

			auto& context = *ImGui::GetCurrentContext();
			const auto id = ImGui::GetID(a_label);
			auto* state = ImGui::GetInputTextState(id);
			const auto stateLength =
				state && state->TextLen >= 0 ?
					static_cast<size_t>(state->TextLen) :
					0;
			const auto snapshotCapacity =
				(std::max)(length, stateLength) + 1;
			InputCall call{
				.buffer = a_buffer,
				.originalData = a_buffer.data,
				.originalCapacity = a_buffer.capacity,
				.originalLength = length,
				.original = std::vector<char>{
					a_buffer.data,
					a_buffer.data + length + 1
				},
				.snapshot = {
					.text = std::vector<char>(snapshotCapacity)
				},
				.id = id,
				.deactivatedText =
					context.InputTextDeactivatedState.ID == id ?
						context.InputTextDeactivatedState.TextA.Data :
						nullptr,
				.deactivatedTextSize =
					context.InputTextDeactivatedState.ID == id ?
						static_cast<size_t>(
							context.InputTextDeactivatedState.TextA.Size) :
						0,
				.activeEditedBefore =
					context.ActiveIdHasBeenEditedBefore,
				.activeEditedThisFrame =
					context.ActiveIdHasBeenEditedThisFrame,
				.anyEditedThisFrame =
					context.AnyIdHasBeenEditedThisFrame,
				.deactivatedItem = context.DeactivatedItemData
			};
			if (state && !CaptureState(call, *state))
				return DMUI_RESULT_INVALID_ARGUMENT;

			auto flags = a_flags;
			if (a_buffer.resize)
			{
				flags |=
					ImGuiInputTextFlags_CallbackResize |
					ImGuiInputTextFlags_CallbackCharFilter;
			}
			constexpr ImGuiInputTextFlags callbackFlags =
				ImGuiInputTextFlags_CallbackResize |
				ImGuiInputTextFlags_CallbackHistory |
				ImGuiInputTextFlags_CallbackCompletion;
			const auto callback =
				(flags & callbackFlags) != 0 ? InputCallback : nullptr;
			void* const userData = callback ? &call : nullptr;

			const auto changed = ImGui::InputTextWithHint(
				a_label,
				a_hint,
				a_buffer.data,
				a_buffer.capacity,
				flags,
				callback,
				userData);
			if (call.failure != DMUI_RESULT_OK)
			{
				RestoreState(call);
				a_buffer.data = call.originalData;
				a_buffer.capacity = call.originalCapacity;
				context.LastItemData.StatusFlags &=
					~(ImGuiItemStatusFlags_Edited |
						ImGuiItemStatusFlags_EditedInternal);
				context.AnyIdHasBeenEditedThisFrame =
					call.anyEditedThisFrame;
				context.DeactivatedItemData = call.deactivatedItem;
				if (context.ActiveId == id)
				{
					context.ActiveIdHasBeenEditedBefore =
						call.activeEditedBefore;
					context.ActiveIdHasBeenEditedThisFrame =
						call.activeEditedThisFrame;
				}
				return call.failure;
			}

			a_outcome = { changed, call.keyEvents };
			return DMUI_RESULT_OK;
		}

		constexpr uint32_t kKnownEditFlags{
			DMUI_UI_TEXT_EDIT_FLAGS_HISTORY_KEYS |
			DMUI_UI_TEXT_EDIT_FLAGS_COMPLETION_KEY |
			DMUI_UI_TEXT_EDIT_FLAGS_CAPTURE_CANCEL |
			DMUI_UI_TEXT_EDIT_FLAGS_KEEP_FOCUS_ON_SUBMIT |
			DMUI_UI_TEXT_EDIT_FLAGS_REQUEST_FOCUS |
			DMUI_UI_TEXT_EDIT_FLAGS_RELOAD
		};

		[[nodiscard]] bool IsCharacterBoundary(
			const DMUI_TextBuffer& a_buffer,
			size_t a_offset) noexcept
		{
			const auto* terminator = static_cast<const char*>(
				std::memchr(a_buffer.data, '\0', a_buffer.capacity));
			const auto length = static_cast<size_t>(terminator - a_buffer.data);
			return a_offset >= length ||
				(static_cast<unsigned char>(a_buffer.data[a_offset]) & 0xC0u) != 0x80u;
		}

		[[nodiscard]] DMUI_Result DrawTextEditorImpl(
			const char* a_label,
			const char* a_hint,
			DMUI_TextBuffer& a_buffer,
			ImGuiInputTextFlags a_flags,
			DMUI_UITextEditFlags a_editFlags,
			size_t a_cursor,
			DMUI_TextEditState& a_state)
		{
			a_state = {};
			const auto has = [a_editFlags](uint32_t a_flag) {
				return (a_editFlags & a_flag) != 0;
			};
			if (!a_label || !a_hint || (a_editFlags & ~kKnownEditFlags) != 0 ||
				(has(DMUI_UI_TEXT_EDIT_FLAGS_COMPLETION_KEY) &&
					(a_flags & ImGuiInputTextFlags_AllowTabInput) != 0))
				return DMUI_RESULT_INVALID_ARGUMENT;
			if (!a_buffer.data || a_buffer.capacity == 0 ||
				a_buffer.capacity > static_cast<size_t>(INT_MAX) ||
				!std::memchr(a_buffer.data, '\0', a_buffer.capacity))
				return DMUI_RESULT_INVALID_ARGUMENT;
			if (has(DMUI_UI_TEXT_EDIT_FLAGS_RELOAD) &&
				!IsCharacterBoundary(a_buffer, a_cursor))
				return DMUI_RESULT_INVALID_ARGUMENT;
			if (!ImGui::GetCurrentContext())
				return DMUI_RESULT_HOST_NOT_READY;

			auto& context = *ImGui::GetCurrentContext();
			const auto id = ImGui::GetID(a_label);
			const auto wasActive = context.ActiveId == id;
			if (has(DMUI_UI_TEXT_EDIT_FLAGS_REQUEST_FOCUS) && !wasActive)
				ImGui::SetKeyboardFocusHere();
			if (has(DMUI_UI_TEXT_EDIT_FLAGS_RELOAD) && wasActive)
			{
				if (auto* const state = ImGui::GetInputTextState(id))
				{
					// ImGui reloads before applying keys, so typing lands at the new cursor.
					const auto cursor = static_cast<int>(
						(std::min)(a_cursor, static_cast<size_t>(INT_MAX)));
					state->WantReloadUserBuf = true;
					state->ReloadSelectionStart = cursor;
					state->ReloadSelectionEnd = cursor;
				}
			}

			uint32_t events{};
			if (has(DMUI_UI_TEXT_EDIT_FLAGS_CAPTURE_CANCEL) && wasActive &&
				ImGui::IsKeyPressed(ImGuiKey_Escape, false))
			{
				events |= DMUI_UI_TEXT_EDIT_EVENTS_CANCELED;
				// The host would otherwise clear the active field after this frame.
				(void)ConsumeMenuEscapeTarget(MenuEscapeTarget::kInteraction);
				// A foreign lock keeps InputText from reverting or deactivating on Escape.
				ImGui::SetKeyOwner(
					ImGuiKey_Escape,
					ImHashStr("DMUI_TextEditCancel"),
					ImGuiInputFlags_LockThisFrame);
			}

			auto flags = a_flags | ImGuiInputTextFlags_EnterReturnsTrue;
			if (has(DMUI_UI_TEXT_EDIT_FLAGS_HISTORY_KEYS))
				flags |= ImGuiInputTextFlags_CallbackHistory;
			if (has(DMUI_UI_TEXT_EDIT_FLAGS_COMPLETION_KEY))
				flags |= ImGuiInputTextFlags_CallbackCompletion;

			// The setting is process-wide, so it must be restored even when drawing throws.
			struct KeepActiveScope
			{
				bool& value;
				bool saved;
				~KeepActiveScope() { value = saved; }
			};
			auto& io = ImGui::GetIO();
			const KeepActiveScope keepActive{
				io.ConfigInputTextEnterKeepActive,
				io.ConfigInputTextEnterKeepActive
			};
			io.ConfigInputTextEnterKeepActive =
				has(DMUI_UI_TEXT_EDIT_FLAGS_KEEP_FOCUS_ON_SUBMIT);
			InputOutcome outcome;
			const auto result = DrawTextInputImpl(
				a_label, a_hint, a_buffer, flags, outcome);
			if (result != DMUI_RESULT_OK)
				return result;

			events |= outcome.keyEvents;
			if (outcome.returned)
				events |= DMUI_UI_TEXT_EDIT_EVENTS_SUBMITTED;
			if (ImGui::IsItemEdited())
				events |= DMUI_UI_TEXT_EDIT_EVENTS_EDITED;
			a_state.events = events;
			if (context.ActiveId != id)
				return DMUI_RESULT_OK;

			// Owning Tab keeps keyboard tabbing from leaving the field on the next press.
			if (has(DMUI_UI_TEXT_EDIT_FLAGS_COMPLETION_KEY))
				ImGui::SetKeyOwner(ImGuiKey_Tab, id);
			a_state.active = 1u;
			if (const auto* state = ImGui::GetInputTextState(id); state && state->Stb)
			{
				const auto& stb = *state->Stb;
				// stb leaves stale equal bounds when nothing is selected.
				const auto selected = stb.select_start != stb.select_end;
				const auto first = selected ? (std::min)(stb.select_start, stb.select_end) : stb.cursor;
				const auto last = selected ? (std::max)(stb.select_start, stb.select_end) : stb.cursor;
				a_state.cursor = static_cast<size_t>(stb.cursor);
				a_state.selectionStart = static_cast<size_t>(first);
				a_state.selectionEnd = static_cast<size_t>(last);
			}
			if (context.PlatformImeData.WantVisible)
			{
				// InputText reports the IME anchor one pixel left of the caret.
				const auto& ime = context.PlatformImeData;
				a_state.caretPosition = { ime.InputPos.x + 1.0f, ime.InputPos.y };
				a_state.lineHeight = ime.InputLineHeight;
			}
			return DMUI_RESULT_OK;
		}
	}

	DMUI_Result DrawTextInput(
		const char* a_label,
		const char* a_hint,
		DMUI_TextBuffer& a_buffer,
		bool& a_changed) noexcept
	{
		try
		{
			InputOutcome outcome;
			const auto result = DrawTextInputImpl(
				a_label,
				a_hint,
				a_buffer,
				ImGuiInputTextFlags_None,
				outcome);
			a_changed = outcome.returned;
			return result;
		}
		catch (const std::bad_alloc&)
		{
			a_changed = false;
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	DMUI_Result DrawTextEditor(
		const char* a_label,
		const char* a_hint,
		DMUI_TextBuffer& a_buffer,
		ImGuiInputTextFlags a_flags,
		DMUI_UITextEditFlags a_editFlags,
		size_t a_cursor,
		DMUI_TextEditState& a_state) noexcept
	{
		try
		{
			return DrawTextEditorImpl(
				a_label,
				a_hint,
				a_buffer,
				a_flags,
				a_editFlags,
				a_cursor,
				a_state);
		}
		catch (const std::bad_alloc&)
		{
			a_state = {};
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}
}
