#include <DearModdingUI/controls/TextInput.h>

#include <DearModdingUI/TextInput.h>
#include <DearModdingUI/controls/ControlRenderer.h>
#include <DearModdingUI/controls/TextRuns.h>
#include <DearModdingUI/presentation/Theme.h>
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
#include <cmath>
#include <cstring>
#include <new>
#include <optional>
#include <string>
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

		// Asks the client to grow its buffer; its contents must survive unchanged.
		[[nodiscard]] DMUI_Result GrowTextBuffer(
			DMUI_TextBuffer& a_buffer,
			size_t a_minimumCapacity,
			const char* a_original,
			size_t a_originalLength) noexcept
		{
			if (a_minimumCapacity == 0 ||
				a_minimumCapacity > static_cast<size_t>(INT_MAX) ||
				!a_buffer.resize)
				return DMUI_RESULT_BUFFER_TOO_SMALL;
			char* data = a_buffer.data;
			size_t capacity = a_buffer.capacity;
			DMUI_Result result{};
			{
				const RenderExecution::ClientGuard guard{
					DMUI_INVALID_CLIENT_HANDLE,
					false
				};
				result = a_buffer.resize(
					a_buffer.userData,
					a_minimumCapacity,
					&data,
					&capacity);
			}
			if (result != DMUI_RESULT_OK)
				return result;
			if (!data || capacity < a_minimumCapacity ||
				capacity > static_cast<size_t>(INT_MAX) ||
				std::memcmp(data, a_original, a_originalLength + 1) != 0)
				return DMUI_RESULT_CALLBACK_FAILED;
			a_buffer.data = data;
			a_buffer.capacity = capacity;
			return DMUI_RESULT_OK;
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

			const auto result = GrowTextBuffer(
				call.buffer,
				minimumCapacity,
				call.original.data(),
				call.originalLength);
			if (result != DMUI_RESULT_OK)
			{
				RejectResize(call, *a_data, result);
				return 0;
			}
			a_data->Buf = call.buffer.data;
			a_data->BufSize = static_cast<int>(call.buffer.capacity);
			return 0;
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

		[[nodiscard]] bool IsMultiline(const TextFieldSpec& a_spec) noexcept
		{
			return a_spec.lines != 0 || a_spec.size.y > 0.0f;
		}

		[[nodiscard]] DMUI_Result ValidateTextField(
			const char* a_id,
			const DMUI_TextBuffer& a_buffer,
			const TextFieldSpec& a_spec) noexcept
		{
			const auto editFlags = a_spec.editFlags;
			const auto has = [editFlags](uint32_t a_flag) {
				return (editFlags & a_flag) != 0;
			};
			if (!a_id || a_id[0] == '\0' || (editFlags & ~kKnownEditFlags) != 0 ||
				(a_spec.style != DMUI_TEXT_FIELD_STYLE_PLAIN &&
					a_spec.style != DMUI_TEXT_FIELD_STYLE_SEARCH) ||
				!std::isfinite(a_spec.size.x) || !std::isfinite(a_spec.size.y) ||
				a_spec.size.y < 0.0f ||
				a_spec.maxBytes >= static_cast<uint32_t>(INT_MAX) ||
				(has(DMUI_UI_TEXT_EDIT_FLAGS_COMPLETION_KEY) &&
					(a_spec.flags & ImGuiInputTextFlags_AllowTabInput) != 0))
				return DMUI_RESULT_INVALID_ARGUMENT;
			// Overlaid runs follow a single, unmasked, left-aligned line.
			if (a_spec.styleSpans &&
				(IsMultiline(a_spec) ||
					(a_spec.flags &
						(ImGuiInputTextFlags_Password | ImGuiInputTextFlags_ElideLeft)) != 0))
				return DMUI_RESULT_INVALID_ARGUMENT;
			// ImGui has no history or completion callbacks for multiline fields.
			if (IsMultiline(a_spec) &&
				(a_spec.style == DMUI_TEXT_FIELD_STYLE_SEARCH ||
					has(DMUI_UI_TEXT_EDIT_FLAGS_HISTORY_KEYS) ||
					has(DMUI_UI_TEXT_EDIT_FLAGS_COMPLETION_KEY)))
				return DMUI_RESULT_INVALID_ARGUMENT;
			if (!a_buffer.data || a_buffer.capacity == 0 ||
				a_buffer.capacity > static_cast<size_t>(INT_MAX) ||
				!std::memchr(a_buffer.data, '\0', a_buffer.capacity))
				return DMUI_RESULT_INVALID_ARGUMENT;
			if (has(DMUI_UI_TEXT_EDIT_FLAGS_RELOAD) &&
				!IsCharacterBoundary(a_buffer, a_spec.cursor))
				return DMUI_RESULT_INVALID_ARGUMENT;
			return DMUI_RESULT_OK;
		}

		struct FieldFrameScope
		{
			~FieldFrameScope()
			{
				CurrentControlRenderer().EndTextField(frame, style);
				ImGui::PopID();
			}

			TextFieldFrame frame;
			DMUI_TextFieldStyle style;
		};

		struct InputOutcome
		{
			bool edited{};
			bool returned{};
			uint32_t keyEvents{};
		};

		// A failing callback leaves the text unstyled.
		[[nodiscard]] DMUI_Result DrawStyledText(
			const DMUI_TextBuffer& a_buffer,
			const TextFieldSpec& a_spec,
			ImGuiID a_id)
		{
			const auto length = static_cast<size_t>(
				static_cast<const char*>(std::memchr(a_buffer.data, '\0', a_buffer.capacity)) -
				a_buffer.data);
			std::span<const DMUI_TextSpan> spans;
			const DMUI_TextSpan* returned{};
			size_t count{};
			DMUI_Result result{};
			{
				const RenderExecution::ClientGuard guard{
					DMUI_INVALID_CLIENT_HANDLE,
					false
				};
				result = a_spec.styleSpans(
					a_spec.styleUserData, a_buffer.data, length, &returned, &count);
			}
			if (result == DMUI_RESULT_OK && count != 0 && !returned)
				result = DMUI_RESULT_INVALID_ARGUMENT;
			if (result == DMUI_RESULT_OK)
			{
				spans = { returned, count };
				result = TextRuns::Validate(
					TextRuns::Consumer::kTextField, { a_buffer.data, length }, spans);
				if (result != DMUI_RESULT_OK)
					spans = {};
			}

			thread_local std::vector<TextRuns::Run> runs;
			runs.clear();
			TextRuns::AppendRuns(
				TextRuns::Consumer::kTextField,
				a_buffer.data,
				spans,
				0,
				length,
				0.0f,
				TextRuns::Resolve(TextRuns::Consumer::kTextField, nullptr),
				runs);

			// The field's horizontal scroll only exists while it is active.
			const auto* state = ImGui::GetInputTextState(a_id);
			const auto scroll = state && ImGui::GetCurrentContext()->ActiveId == a_id ?
				state->Scroll.x : 0.0f;
			const auto min = ImGui::GetItemRectMin();
			const auto max = ImGui::GetItemRectMax();
			const auto& padding = ImGui::GetStyle().FramePadding;
			TextRuns::Paint(
				*ImGui::GetWindowDrawList(),
				{ min.x + padding.x - scroll, min.y + padding.y },
				ImGui::GetFontSize(),
				runs,
				{},
				{ min.x, min.y, max.x, max.y });
			return result;
		}

		// Runs ImGui over the buffer with the growth limit applied.
		[[nodiscard]] DMUI_Result DrawTextInputImpl(
			DMUI_TextBuffer& a_buffer,
			const TextFieldSpec& a_spec,
			InputOutcome& a_outcome)
		{
			a_outcome = {};
			const auto* terminator = static_cast<const char*>(
				std::memchr(a_buffer.data, '\0', a_buffer.capacity));
			const auto length =
				static_cast<size_t>(terminator - a_buffer.data);

			// A limited field is sized once up front so ImGui clips input at the
			// limit instead of asking the client to grow past it.
			const auto limit = a_spec.maxBytes ?
				(std::max)(static_cast<size_t>(a_spec.maxBytes), length) + 1 :
				size_t{};
			if (limit && a_buffer.resize && a_buffer.capacity < limit)
			{
				const std::vector<char> original{
					a_buffer.data,
					a_buffer.data + length + 1
				};
				const auto grown = GrowTextBuffer(
					a_buffer, limit, original.data(), length);
				if (grown != DMUI_RESULT_OK)
					return grown;
			}
			const auto capacity = limit ?
				(std::min)(a_buffer.capacity, limit) :
				a_buffer.capacity;
			const auto resizable = !limit && a_buffer.resize;

			auto& context = *ImGui::GetCurrentContext();
			const auto id = ImGui::GetID("##Text");
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

			auto flags = a_spec.flags | ImGuiInputTextFlags_EnterReturnsTrue;
			if (resizable)
			{
				flags |=
					ImGuiInputTextFlags_CallbackResize |
					ImGuiInputTextFlags_CallbackCharFilter;
			}
			if ((a_spec.editFlags & DMUI_UI_TEXT_EDIT_FLAGS_HISTORY_KEYS) != 0)
				flags |= ImGuiInputTextFlags_CallbackHistory;
			if ((a_spec.editFlags & DMUI_UI_TEXT_EDIT_FLAGS_COMPLETION_KEY) != 0)
				flags |= ImGuiInputTextFlags_CallbackCompletion;
			constexpr ImGuiInputTextFlags callbackFlags =
				ImGuiInputTextFlags_CallbackResize |
				ImGuiInputTextFlags_CallbackHistory |
				ImGuiInputTextFlags_CallbackCompletion;
			const auto callback =
				(flags & callbackFlags) != 0 ? InputCallback : nullptr;
			void* const userData = callback ? &call : nullptr;

			const auto multiline = IsMultiline(a_spec);
			ImVec2 size{ a_spec.size.x, 0.0f };
			if (multiline)
			{
				flags |= ImGuiInputTextFlags_Multiline;
				size.y = a_spec.size.y > 0.0f ?
					a_spec.size.y :
					ImGui::GetTextLineHeightWithSpacing() *
						static_cast<float>(a_spec.lines);
			}
			else if (a_spec.size.x != 0.0f)
			{
				ImGui::SetNextItemWidth(a_spec.size.x);
				size.x = 0.0f;
			}
			else if (a_spec.style == DMUI_TEXT_FIELD_STYLE_SEARCH &&
				(context.NextItemData.HasFlags & ImGuiNextItemDataFlags_HasWidth) == 0)
			{
				// Search fills the row unless the caller set a width.
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			}

			// The styled runs replace the glyphs, so ImGui keeps only caret and selection.
			if (a_spec.styleSpans)
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 0));
			// EnterReturnsTrue makes the return value the submit signal.
			const auto returned = ImGui::InputTextEx(
				"##Text",
				multiline ? nullptr : (a_spec.hint ? a_spec.hint : ""),
				a_buffer.data,
				static_cast<int>(capacity),
				size,
				flags,
				callback,
				userData);
			if (a_spec.styleSpans)
				ImGui::PopStyleColor();
			const auto edited = ImGui::IsItemEdited();
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

			a_outcome = { edited, returned, call.keyEvents };
			return a_spec.styleSpans ? DrawStyledText(a_buffer, a_spec, id) : DMUI_RESULT_OK;
		}

		[[nodiscard]] DMUI_Result DrawTextFieldImpl(
			const char* a_id,
			DMUI_TextBuffer& a_buffer,
			const TextFieldSpec& a_spec,
			DMUI_TextEditState& a_state)
		{
			a_state = {};
			const auto valid = ValidateTextField(a_id, a_buffer, a_spec);
			if (valid != DMUI_RESULT_OK)
				return valid;
			if (!ImGui::GetCurrentContext())
				return DMUI_RESULT_HOST_NOT_READY;
			const auto has = [&a_spec](uint32_t a_flag) {
				return (a_spec.editFlags & a_flag) != 0;
			};

			ImGui::PushID(a_id);
			// Overlaid runs and the caret must share advances.
			std::optional<Theme::FontGuard> monospace;
			if (a_spec.styleSpans)
				monospace.emplace(Theme::FontRole::kMonospace);
			auto& context = *ImGui::GetCurrentContext();
			const auto id = ImGui::GetID("##Text");
			const auto wasActive = context.ActiveId == id;
			if (has(DMUI_UI_TEXT_EDIT_FLAGS_REQUEST_FOCUS) && !wasActive)
				ImGui::SetKeyboardFocusHere();
			if (has(DMUI_UI_TEXT_EDIT_FLAGS_RELOAD) && wasActive)
			{
				if (auto* const state = ImGui::GetInputTextState(id))
				{
					// ImGui reloads before applying keys, so typing lands at the new cursor.
					const auto cursor = static_cast<int>(
						(std::min)(a_spec.cursor, static_cast<size_t>(INT_MAX)));
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
			DMUI_Result result{};
			{
				const FieldFrameScope frame{
					CurrentControlRenderer().BeginTextField(a_spec.style),
					a_spec.style
				};
				result = DrawTextInputImpl(a_buffer, a_spec, outcome);
			}
			if (result != DMUI_RESULT_OK)
				return result;

			events |= outcome.keyEvents;
			if (outcome.returned)
				events |= DMUI_UI_TEXT_EDIT_EVENTS_SUBMITTED;
			if (outcome.edited)
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

	DMUI_Result DrawTextField(
		const char* a_id,
		DMUI_TextBuffer& a_buffer,
		const TextFieldSpec& a_spec,
		DMUI_TextEditState& a_state) noexcept
	{
		try
		{
			return DrawTextFieldImpl(a_id, a_buffer, a_spec, a_state);
		}
		catch (const std::bad_alloc&)
		{
			a_state = {};
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	DMUI_Result DrawTextField(
		const char* a_id,
		std::string& a_text,
		const TextFieldSpec& a_spec,
		bool& a_edited) noexcept
	{
		a_edited = false;
		dmui::TextInputBuffer buffer{ a_text };
		if (buffer.Result() != DMUI_RESULT_OK)
			return buffer.Result();
		DMUI_TextEditState state{};
		const auto result = DrawTextField(a_id, *buffer.Get(), a_spec, state);
		if (result != DMUI_RESULT_OK)
			return result;
		a_edited = (state.events & DMUI_UI_TEXT_EDIT_EVENTS_EDITED) != 0;
		return buffer.CommitTo(a_text);
	}
}
