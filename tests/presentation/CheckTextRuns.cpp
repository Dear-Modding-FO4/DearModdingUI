#include "../Harness.h"
#include "../support/ImGuiTestContext.h"

#include <DearModdingUI/UI.h>
#include <DearModdingUI/controls/TextInput.h>
#include <DearModdingUI/controls/TextRuns.h>
#include <DearModdingUI/controls/TextSpans.h>
#include <DearModdingUI/controls/TextViewer.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;

	namespace
	{
		constexpr DMUI_Vec4 kRed{ 1.0f, 0.0f, 0.0f, 1.0f };
		constexpr DMUI_Vec4 kGreen{ 0.0f, 1.0f, 0.0f, 1.0f };

		[[nodiscard]] DMUI_TextSpan Span(size_t a_offset, size_t a_length, uint32_t a_flags = 0)
		{
			DMUI_TextSpan span{};
			span.offset = a_offset;
			span.length = a_length;
			span.flags = a_flags;
			return span;
		}

		// Green glyphs, optionally with a red background and both decorations.
		[[nodiscard]] DMUI_TextSpan Marked(size_t a_offset, size_t a_length, bool a_decorated)
		{
			auto span = Span(a_offset, a_length, DMUI_TEXT_SPAN_CUSTOM_FOREGROUND);
			span.foregroundColor = kGreen;
			if (a_decorated)
			{
				span.flags |= DMUI_TEXT_SPAN_CUSTOM_BACKGROUND | DMUI_TEXT_SPAN_UNDERLINE |
					DMUI_TEXT_SPAN_STRIKETHROUGH;
				span.backgroundColor = kRed;
			}
			return span;
		}

		[[nodiscard]] size_t Vertices(const ImDrawList& a_list, DMUI_Vec4 a_color)
		{
			const auto packed = ImGui::ColorConvertFloat4ToU32(
				{ a_color.x, a_color.y, a_color.z, a_color.w });
			return static_cast<size_t>(std::ranges::count(
				std::span{ a_list.VtxBuffer.Data, static_cast<size_t>(a_list.VtxBuffer.Size) },
				packed, &ImDrawVert::col));
		}

		[[nodiscard]] ImGuiWindow* FindWindowContaining(std::string_view a_name)
		{
			for (auto* window : ImGui::GetCurrentContext()->Windows)
			{
				if (std::string_view{ window->Name }.contains(a_name))
					return window;
			}
			return nullptr;
		}

		// A decoration adds quads beyond the glyphs.
		void RequirePass(
			size_t a_plainGreen, size_t a_decoratedGreen, size_t a_red, const char* a_what)
		{
			require(
				a_red == 4 && a_decoratedGreen == a_plainGreen + 8 && a_plainGreen != 0,
				std::string{ a_what } + " did not draw its background, underline, and strikethrough");
		}

		struct ViewData
		{
			std::string text{ "ab cd\nef" };
			std::vector<size_t> lines{ 0, 6 };
			std::vector<DMUI_TextSpan> spans;
			DMUI_TextViewDescriptor descriptor{};
			DMUI_TextViewState state{};

			void Prepare(const char* a_id, uint64_t a_spanRevision)
			{
				descriptor = {
					.id = a_id,
					.text = text.data(),
					.textLength = text.size(),
					.lineOffsets = lines.data(),
					.lineCount = lines.size(),
					.spans = spans.data(),
					.spanCount = spans.size(),
					.contentRevision = 1,
					.spanRevision = a_spanRevision,
					.viewport = { 300.0f, 100.0f }
				};
				state = { .activeMatch = DMUI_TEXT_VIEW_NO_OFFSET,
					.revealByteOffset = DMUI_TEXT_VIEW_NO_OFFSET };
				state.contentRevision = 1;
			}
		};

		struct StyleScript
		{
			std::vector<DMUI_TextSpan> spans;
			DMUI_Result result{ DMUI_RESULT_OK };
			std::string seen;

			static DMUI_Result DMUI_CALL Call(
				void* a_user, const char* a_text, size_t a_length,
				const DMUI_TextSpan** a_spans, size_t* a_count) noexcept
			{
				auto& script = *static_cast<StyleScript*>(a_user);
				script.seen.assign(a_text, a_length);
				*a_spans = script.spans.data();
				*a_count = script.spans.size();
				return script.result;
			}
		};
	}

	void run_text_runs_checks(Runner& runner)
	{
		runner.test("span validation follows each consumer's rules", [] {
			using TextRuns::Consumer;
			// "h", an accented e of two bytes, "llo world".
			const std::string_view text{ "h\xC3\xA9llo world" };
			const auto check = [&](Consumer a_consumer, std::vector<DMUI_TextSpan> a_spans) {
				return TextRuns::Validate(a_consumer, text, a_spans);
			};
			const std::vector<DMUI_TextSpan> good{ Span(0, 1), Span(1, 2), Span(4, 3) };
			for (const auto consumer : { Consumer::kText, Consumer::kTextView, Consumer::kTextField })
			{
				require(check(consumer, good) == DMUI_RESULT_OK &&
						check(consumer, {}) == DMUI_RESULT_OK,
					"sorted spans on boundaries were rejected");
				require(check(consumer, { Span(4, 3), Span(0, 1) }) == DMUI_RESULT_INVALID_ARGUMENT &&
						check(consumer, { Span(0, 5), Span(3, 2) }) == DMUI_RESULT_INVALID_ARGUMENT,
					"unsorted or overlapping spans were accepted");
				require(check(consumer, { Span(2, 1) }) == DMUI_RESULT_INVALID_ARGUMENT &&
						check(consumer, { Span(0, 2) }) == DMUI_RESULT_INVALID_ARGUMENT,
					"a span inside a UTF-8 sequence was accepted");
				require(check(consumer, { Span(8, 5) }) == DMUI_RESULT_INVALID_ARGUMENT &&
						check(consumer, { Span(text.size() + 1, 0) }) == DMUI_RESULT_INVALID_ARGUMENT,
					"a span outside the text was accepted");
				auto tone = Span(0, 1);
				tone.background = DMUI_TEXT_TONE_COUNT;
				auto color = Span(0, 1, DMUI_TEXT_SPAN_CUSTOM_BACKGROUND);
				color.backgroundColor.w = std::numeric_limits<float>::quiet_NaN();
				require(check(consumer, { Span(0, 1, 0x1000u) }) == DMUI_RESULT_INVALID_ARGUMENT &&
						check(consumer, { tone }) == DMUI_RESULT_INVALID_ARGUMENT &&
						check(consumer, { color }) == DMUI_RESULT_INVALID_ARGUMENT,
					"unknown flags, tones, or non-finite colors were accepted");
			}
			auto role = Span(0, 1, DMUI_TEXT_SPAN_HAS_ROLE);
			const auto interactive = Span(0, 1, DMUI_TEXT_SPAN_INTERACTIVE);
			require(check(Consumer::kText, { role }) == DMUI_RESULT_OK &&
					check(Consumer::kTextView, { role }) == DMUI_RESULT_INVALID_ARGUMENT &&
					check(Consumer::kTextField, { role }) == DMUI_RESULT_INVALID_ARGUMENT,
				"font roles are legal in text only");
			require(check(Consumer::kText, { interactive }) == DMUI_RESULT_INVALID_ARGUMENT &&
					check(Consumer::kTextView, { interactive }) == DMUI_RESULT_OK &&
					check(Consumer::kTextField, { interactive }) == DMUI_RESULT_INVALID_ARGUMENT,
				"interactive spans are legal in the text view only");
			role.role = DMUI_FONT_ROLE_COUNT;
			require(check(Consumer::kText, { role }) == DMUI_RESULT_INVALID_ARGUMENT,
				"an unknown font role was accepted");
		});

		runner.test("the shared pass draws backgrounds and decorations for text, views, and fields", [] {
			support::ImGuiTestContext imgui{ { .disableErrorRecovery = true } };

			// Text operation.
			imgui.BeginWindow("##RunsText", { 0.0f, 0.0f }, { 400.0f, 200.0f });
			const auto* textList = ImGui::GetWindowDrawList();
			const std::vector<DMUI_TextSpan> plainText{ Marked(0, 2, false) };
			const std::vector<DMUI_TextSpan> markedText{ Marked(0, 2, true) };
			require(ValidateTextSpans("ab", plainText, {}) == DMUI_RESULT_OK,
				"the plain span was rejected");
			(void)DrawTextSpans("ab", plainText, {});
			const auto textPlain = Vertices(*textList, kGreen);
			(void)DrawTextSpans("ab", markedText, {});
			RequirePass(
				textPlain, Vertices(*textList, kGreen) - textPlain, Vertices(*textList, kRed),
				"the text operation");
			imgui.EndWindow();

			// Text view: the same runs on a line of a bordered child.
			uint64_t spanRevision{};
			const auto drawView = [&](std::vector<DMUI_TextSpan> a_spans) {
				ViewData data;
				data.spans = std::move(a_spans);
				data.Prepare("RunsView", ++spanRevision);
				for (int frame = 0; frame < 2; ++frame)
				{
					imgui.BeginWindow("##ViewHost", { 0.0f, 0.0f }, { 400.0f, 200.0f });
					require(DrawTextView(1, data.descriptor, data.state) == DMUI_RESULT_OK,
						"the span view failed to draw");
					imgui.EndWindow();
				}
				return std::pair{
					Vertices(*FindWindowContaining("RunsView")->DrawList, kGreen),
					Vertices(*FindWindowContaining("RunsView")->DrawList, kRed) };
			};
			const auto viewPlain = drawView({ Marked(0, 2, false) });
			const auto viewMarked = drawView({ Marked(0, 2, true) });
			RequirePass(viewPlain.first, viewMarked.first, viewMarked.second, "the text view");

			// Text field overlay.
			const auto drawField = [&](bool a_decorated) {
				std::vector<char> storage(64, '\0');
				std::memcpy(storage.data(), "ab cd", 5);
				DMUI_TextBuffer buffer{ storage.data(), storage.size(), nullptr, nullptr };
				StyleScript script;
				script.spans = { Marked(0, 2, a_decorated) };
				TextFieldSpec spec{ .styleSpans = &StyleScript::Call, .styleUserData = &script };
				DMUI_TextEditState state{};
				imgui.BeginWindow("##RunsFieldHost", { 0.0f, 0.0f }, { 400.0f, 200.0f });
				require(DrawTextField("RunsField", buffer, spec, state) == DMUI_RESULT_OK,
					"the styled field failed to draw");
				const auto* list = ImGui::GetWindowDrawList();
				const auto result = std::pair{ Vertices(*list, kGreen), Vertices(*list, kRed) };
				imgui.EndWindow();
				return result;
			};
			const auto fieldPlain = drawField(false);
			const auto fieldMarked = drawField(true);
			RequirePass(fieldPlain.first, fieldMarked.first, fieldMarked.second, "the text field");
		});

		runner.test("an exception in the client styler fails the field call", [] {
			dmui::ui::TextStyler throwing = [](std::string_view) -> std::span<const dmui::TextSpan> {
				throw std::runtime_error("styler");
			};
			const dmui::ui::TextFieldOptions options{ .styler = &throwing };
			const auto abi = options.Abi();
			const DMUI_TextSpan* spans{};
			size_t count{};
			require(abi.styleSpans &&
					abi.styleSpans(abi.styleUserData, "a", 1, &spans, &count) ==
						DMUI_RESULT_CALLBACK_FAILED,
				"a throwing styler crossed the ABI");

			support::ImGuiTestContext imgui{ { .disableErrorRecovery = true } };
			std::vector<char> storage(16, '\0');
			DMUI_TextBuffer buffer{ storage.data(), storage.size(), nullptr, nullptr };
			TextFieldSpec spec{ .styleSpans = abi.styleSpans, .styleUserData = abi.styleUserData };
			DMUI_TextEditState state{};
			imgui.BeginWindow("##RunsThrow", { 0.0f, 0.0f }, { 400.0f, 200.0f });
			require(DrawTextField("Throw", buffer, spec, state) == DMUI_RESULT_CALLBACK_FAILED,
				"the field hid the styler failure");
			imgui.EndWindow();
		});

		runner.test("a styled field is single line and unmasked", [] {
			support::ImGuiTestContext imgui{ { .disableErrorRecovery = true } };
			std::vector<char> storage(16, '\0');
			DMUI_TextBuffer buffer{ storage.data(), storage.size(), nullptr, nullptr };
			StyleScript script;
			TextFieldSpec spec{ .styleSpans = &StyleScript::Call, .styleUserData = &script };
			DMUI_TextEditState state{};
			imgui.BeginWindow("##RunsReject", { 0.0f, 0.0f }, { 400.0f, 200.0f });
			spec.lines = 3;
			require(DrawTextField("Multi", buffer, spec, state) == DMUI_RESULT_INVALID_ARGUMENT,
				"a multiline field accepted a styler");
			spec.lines = 0;
			spec.flags = ImGuiInputTextFlags_Password;
			require(DrawTextField("Secret", buffer, spec, state) == DMUI_RESULT_INVALID_ARGUMENT,
				"a masked field accepted a styler");
			spec.flags = 0;
			script.spans = { Span(0, 1, DMUI_TEXT_SPAN_INTERACTIVE) };
			require(DrawTextField("Click", buffer, spec, state) == DMUI_RESULT_INVALID_ARGUMENT,
				"an interactive span reached the field");
			script.spans = { Span(0, 1, DMUI_TEXT_SPAN_HAS_ROLE) };
			require(DrawTextField("Role", buffer, spec, state) == DMUI_RESULT_INVALID_ARGUMENT,
				"a font role reached the field");
			imgui.EndWindow();
		});
	}
}
