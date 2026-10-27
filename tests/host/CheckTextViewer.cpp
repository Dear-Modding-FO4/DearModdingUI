#include "../Harness.h"
#include "../support/ImGuiTestContext.h"

#include <DearModdingUI/Client.h>
#include <DearModdingUI/controls/TextViewer.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;

	namespace
	{
		[[nodiscard]] std::vector<size_t> LineOffsets(
			const std::string& a_text)
		{
			std::vector<size_t> result{ 0 };
			for (size_t offset = 0; offset < a_text.size(); ++offset)
			{
				if (a_text[offset] == '\n')
					result.push_back(offset + 1);
			}
			return result;
		}

		[[nodiscard]] DMUI_TextViewDescriptor Descriptor(
			const char* a_id,
			const std::string& a_text,
			const std::vector<size_t>& a_lines,
			const std::vector<size_t>& a_matches = {},
			size_t a_matchLength = 0,
			uint64_t a_contentRevision = 1,
			uint64_t a_matchRevision = 1)
		{
			return {
				.id = a_id,
				.text = a_text.data(),
				.textLength = a_text.size(),
				.lineOffsets = a_lines.data(),
				.lineCount = a_lines.size(),
				.matchByteOffsets = a_matches.data(),
				.matchCount = a_matches.size(),
				.matchByteLength = a_matchLength,
				.contentRevision = a_contentRevision,
				.matchRevision = a_matchRevision,
				.viewport = { 360.0f, 140.0f }
			};
		}

		[[nodiscard]] DMUI_TextViewState State(
			uint64_t a_contentRevision = 1,
			uint64_t a_matchRevision = 1)
		{
			return {
				.contentRevision = a_contentRevision,
				.matchRevision = a_matchRevision,
				.activeMatch = DMUI_TEXT_VIEW_NO_OFFSET,
				.revealByteOffset = DMUI_TEXT_VIEW_NO_OFFSET,
				.spanIndex = DMUI_TEXT_VIEW_NO_OFFSET
			};
		}

		[[nodiscard]] ImGuiWindow* FindChild(std::string_view a_id)
		{
			auto* context = ImGui::GetCurrentContext();
			for (auto* window : context->Windows)
			{
				if ((window->Flags & ImGuiWindowFlags_ChildWindow) != 0 &&
					std::string_view{ window->Name }.contains(a_id))
					return window;
			}
			return nullptr;
		}

		[[nodiscard]] std::vector<ImGuiWindow*> FindChildren(
			std::string_view a_id)
		{
			std::vector<ImGuiWindow*> result;
			for (auto* window : ImGui::GetCurrentContext()->Windows)
			{
				if ((window->Flags & ImGuiWindowFlags_ChildWindow) != 0 &&
					std::string_view{ window->Name }.contains(a_id))
					result.push_back(window);
			}
			return result;
		}
	}

	void run_text_viewer_checks(Runner& runner)
	{
		runner.test("text viewer renders literal UTF8 and overlapping matches", [] {
			const std::string text{
				"head\r\n\r\n100% ## literal\r\ncaf\xC3\xA9 ababa\n"
			};
			const auto lines = LineOffsets(text);
			const auto first = text.find("aba");
			const std::vector<size_t> matches{ first, first + 2 };
			auto descriptor =
				Descriptor("LiteralText", text, lines, matches, 3);
			auto state = State();

			const dmui::TextViewRequest request{
				.text = text,
				.lineOffsets = lines,
				.matchByteOffsets = matches,
				.matchByteLength = 3,
				.contentRevision = 1,
				.matchRevision = 1,
				.viewport = descriptor.viewport
			};
			dmui::TextViewState navigation;
			require(
				dmui::SelectPreviousTextMatch(request, navigation) &&
					navigation.activeMatch == 1 &&
					navigation.revealByteOffset == matches[1],
				"previous navigation did not wrap to the final match");
			require(
				dmui::SelectNextTextMatch(request, navigation) &&
					navigation.activeMatch == 0 &&
					navigation.revealByteOffset == matches[0],
				"next navigation did not wrap to the first match");
			state.activeMatch = navigation.activeMatch;
			state.revealByteOffset = navigation.revealByteOffset;

			support::ImGuiTestContext imgui{
				{ .disableErrorRecovery = true }
			};
			imgui.BeginWindow(
				"##TextViewerLiteral",
				{ 0.0f, 0.0f },
				{ 500.0f, 260.0f });
			require(
				DrawTextView(7, descriptor, state) == DMUI_RESULT_OK,
				"valid CRLF and UTF-8 text failed to draw");
			require(
				state.revealByteOffset == DMUI_TEXT_VIEW_NO_OFFSET,
				"successful draw did not consume the reveal request");
			imgui.EndWindow();

			descriptor =
				Descriptor("CachedValidation", text, lines, {}, 7, 8, 9);
			state = State(8, 9);
			imgui.BeginWindow("##CachedValidation");
			require(
				DrawTextView(7, descriptor, state) == DMUI_RESULT_OK,
				"zero-match descriptor rejected a nonzero match length");
			imgui.EndWindow(true);

			const auto cachedAccented = text.find("\xC3\xA9");
			const std::vector<size_t> cachedInvalidMatch{
				cachedAccented + 1
			};
			descriptor.matchByteOffsets = cachedInvalidMatch.data();
			descriptor.matchCount = cachedInvalidMatch.size();
			descriptor.matchByteLength = 1;
			descriptor.matchRevision = 10;
			state.matchRevision = 10;
			imgui.BeginWindow("##CachedValidation");
			require(
				DrawTextView(7, descriptor, state) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"query revision skipped cached match validation");
			imgui.EndWindow();

			descriptor.matchByteOffsets = nullptr;
			descriptor.matchCount = 0;
			descriptor.matchByteLength = 7;
			descriptor.matchRevision = 9;
			state.matchRevision = 9;
			descriptor.lineOffsets = nullptr;
			imgui.BeginWindow("##CachedValidation");
			require(
				DrawTextView(7, descriptor, state) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"cached descriptor accepted a null line index");
			imgui.EndWindow();

			descriptor.lineOffsets = lines.data();
			descriptor.textLength =
				static_cast<size_t>(
					(std::numeric_limits<std::ptrdiff_t>::max)()) +
				1;
			imgui.BeginWindow("##CachedValidation");
			require(
				DrawTextView(7, descriptor, state) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"cached descriptor accepted an unrepresentable text length");
			imgui.EndWindow();

			descriptor.textLength = text.size();
			descriptor.matchByteOffsets = cachedInvalidMatch.data();
			descriptor.matchCount =
				static_cast<size_t>(
					(std::numeric_limits<std::ptrdiff_t>::max)()) /
					sizeof(size_t) +
				1;
			descriptor.matchByteLength = 1;
			imgui.BeginWindow("##CachedValidation");
			require(
				DrawTextView(7, descriptor, state) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"cached descriptor accepted an overflowing match array");
			imgui.EndWindow();
		});

		runner.test("text viewer rejects invalid byte indexes and stale state", [] {
			const std::string text{ "caf\xC3\xA9\nnext" };
			auto lines = LineOffsets(text);
			auto descriptor = Descriptor("InvalidOffsets", text, lines);
			auto state = State();
			support::ImGuiTestContext imgui{
				{ .disableErrorRecovery = true }
			};

			lines[1] = 2;
			descriptor.lineOffsets = lines.data();
			imgui.BeginWindow("##InvalidLineOffsets");
			require(
				DrawTextView(7, descriptor, state) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"invalid line start was accepted");
			imgui.EndWindow();

			lines = LineOffsets(text);
			const auto accented = text.find("\xC3\xA9");
			const dmui::TextViewRequest request{
				.text = text,
				.lineOffsets = lines,
				.contentRevision = 2,
				.matchRevision = 2
			};
			dmui::TextViewState navigation;
			require(
				!dmui::RevealTextOffset(
					request,
					navigation,
					accented + 1) &&
					navigation.revealByteOffset == dmui::kNoTextOffset,
				"C++ reveal helper accepted a UTF-8 continuation byte");
			const std::vector<size_t> invalidMatch{ accented + 1 };
			descriptor = Descriptor(
				"InvalidMatchOffsets",
				text,
				lines,
				invalidMatch,
				1,
				2,
				2);
			state = State(2, 2);
			imgui.BeginWindow("##InvalidMatchOffsets");
			require(
				DrawTextView(7, descriptor, state) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"match starting inside a UTF-8 sequence was accepted");
			imgui.EndWindow();

			descriptor = Descriptor(
				"RevisionReset",
				text,
				lines,
				{},
				0,
				3,
				4);
			state = State(1, 1);
			state.activeMatch = 0;
			state.revealByteOffset = 0;
			imgui.BeginWindow("##RevisionReset");
			require(
				DrawTextView(7, descriptor, state) == DMUI_RESULT_OK &&
					state.contentRevision == 3 &&
					state.matchRevision == 4 &&
					state.activeMatch == DMUI_TEXT_VIEW_NO_OFFSET &&
					state.revealByteOffset == DMUI_TEXT_VIEW_NO_OFFSET,
				"revision change retained stale match or reveal state");
			imgui.EndWindow();
		});

		runner.test("large text view draws only visible lines", [] {
			std::string text;
			std::vector<size_t> lines;
			constexpr size_t lineCount{ 50000 };
			text.reserve(lineCount * 42);
			lines.reserve(lineCount + 1);
			lines.push_back(0);
			for (size_t line = 0; line < lineCount; ++line)
			{
				text.append("0123456789012345678901234567890123456789\n");
				lines.push_back(text.size());
			}
			auto descriptor =
				Descriptor("LargeText", text, lines, {}, 0, 17, 23);
			descriptor.viewport = { 320.0f, 120.0f };
			auto state = State(17, 23);
			support::ImGuiTestContext imgui{
				{
					.displaySize = { 640.0f, 360.0f },
					.disableErrorRecovery = true
				}
			};

			imgui.BeginWindow(
				"##LargeTextParent",
				{ 0.0f, 0.0f },
				{ 500.0f, 260.0f });
			require(
				DrawTextView(7, descriptor, state) == DMUI_RESULT_OK,
				"large pre-indexed text failed initial validation");
			imgui.EndWindow(true);

			imgui.BeginWindow(
				"##LargeTextParent",
				{ 0.0f, 0.0f },
				{ 500.0f, 260.0f });
			require(
				DrawTextView(7, descriptor, state) == DMUI_RESULT_OK,
				"cached large text failed to draw");
			const auto* child = FindChild("LargeText");
			require(child, "large text viewer child window was not created");
			const auto vertexCount = child->DrawList->VtxBuffer.Size;
			require(
				vertexCount < 10000,
				"large text renderer emitted geometry for off-screen lines");
			imgui.EndWindow(true);

			auto borrowedText =
				std::make_unique<std::string>(250000, 'W');
			const std::vector<size_t> oneLine{ 0 };
			std::vector<size_t> denseMatches;
			denseMatches.reserve(borrowedText->size() / 2);
			for (size_t offset = 0; offset + 4 <= borrowedText->size();
				 offset += 2)
				denseMatches.push_back(offset);
			auto longDescriptor = Descriptor(
				"LongClippedLine",
				*borrowedText,
				oneLine,
				denseMatches,
				4,
				31,
				37);
			longDescriptor.viewport = { 220.0f, 80.0f };
			auto longState = State(31, 37);

			imgui.BeginWindow(
				"##LongClippedParent",
				{ 0.0f, 0.0f },
				{ 400.0f, 180.0f });
			require(
				DrawTextView(7, longDescriptor, longState) ==
					DMUI_RESULT_OK,
				"long matched line failed to draw");
			const auto* longChild = FindChild("LongClippedLine");
			require(longChild, "long-line child window was not created");
			require(
				longChild->DrawList->VtxBuffer.Size < 2000,
				"horizontal clipping emitted geometry for the full line");
			imgui.EndWindow(true);

			auto relocatedText =
				std::make_unique<std::string>(*borrowedText);
			borrowedText.reset();
			longDescriptor.text = relocatedText->data();
			imgui.BeginWindow(
				"##LongClippedParent",
				{ 0.0f, 0.0f },
				{ 400.0f, 180.0f });
			require(
				DrawTextView(7, longDescriptor, longState) ==
					DMUI_RESULT_OK,
				"cached geometry retained the expired text buffer");
			imgui.EndWindow();
		});

		runner.test("text geometry cache follows ImGui scope and context", [] {
			const std::string narrowText =
				std::string(100, 'i') + "\n" + std::string(99, 'i');
			const std::string wideText = std::string(199, 'W') + "\n";
			const auto narrowLines = LineOffsets(narrowText);
			const auto wideLines = LineOffsets(wideText);
			auto narrow = Descriptor(
				"ScopedText",
				narrowText,
				narrowLines,
				{},
				0,
				67,
				71);
			auto wide = Descriptor(
				"ScopedText",
				wideText,
				wideLines,
				{},
				0,
				67,
				71);
			narrow.viewport = { 120.0f, 55.0f };
			wide.viewport = narrow.viewport;
			auto narrowState = State(67, 71);
			auto wideState = State(67, 71);
			auto otherClientState = State(67, 71);

			{
				support::ImGuiTestContext imgui{
					{
						.displaySize = { 640.0f, 360.0f },
						.disableErrorRecovery = true
					}
				};
				for (int frame = 0; frame < 2; ++frame)
				{
					imgui.BeginWindow(
						"##ScopedTextParent",
						{ 0.0f, 0.0f },
						{ 420.0f, 260.0f });
					ImGui::PushID("NarrowScope");
					require(
						DrawTextView(11, narrow, narrowState) ==
							DMUI_RESULT_OK,
						"narrow scoped text failed to draw");
					ImGui::PopID();
					ImGui::PushID("WideScope");
					require(
						DrawTextView(11, wide, wideState) ==
							DMUI_RESULT_OK,
						"wide scoped text failed to draw");
					ImGui::PopID();
					ImGui::PushID("NarrowScope");
					require(
						DrawTextView(12, wide, otherClientState) ==
							DMUI_RESULT_OK,
						"second client's scoped text failed to draw");
					ImGui::PopID();
					imgui.EndWindow(true);
				}

				const auto children = FindChildren("ScopedText");
				require(
					children.size() == 3,
					"same-label text views did not retain client and ImGui scopes");
				const auto [narrowest, widest] = std::ranges::minmax_element(
					children,
					{},
					[](const ImGuiWindow* a_window) {
						return a_window->ScrollMax.x;
					});
				require(
					(*widest)->ScrollMax.x >
						(*narrowest)->ScrollMax.x + 100.0f,
					"scoped text views shared cached content geometry");
			}

			auto recreatedState = State(67, 71);
			support::ImGuiTestContext recreated{
				{
					.displaySize = { 640.0f, 360.0f },
					.disableErrorRecovery = true
				}
			};
			for (int frame = 0; frame < 2; ++frame)
			{
				recreated.BeginWindow(
					"##ScopedTextParent",
					{ 0.0f, 0.0f },
					{ 420.0f, 140.0f });
				ImGui::PushID("WideScope");
				require(
					DrawTextView(11, narrow, recreatedState) ==
						DMUI_RESULT_OK,
					"context recreation retained stale text geometry");
				ImGui::PopID();
				recreated.EndWindow(true);
			}
			const auto* recreatedChild = FindChild("ScopedText");
			require(
				recreatedChild && recreatedChild->ScrollMax.x > 0.0f,
				"recreated context did not establish its own text cache");
		});

		runner.test("text view keeps the reader's line across front trims and appends", [] {
			const auto build = [](int a_first, int a_count) {
				std::string text;
				for (int line = a_first; line < a_first + a_count; ++line)
					text += "line " + std::to_string(line) + "\n";
				return text;
			};
			auto text = build(0, 200);
			auto lines = LineOffsets(text);
			auto descriptor = Descriptor("Anchor", text, lines);
			auto state = State();
			support::ImGuiTestContext imgui{ { .disableErrorRecovery = true } };
			const auto frame = [&](int a_count = 3) {
				for (int index = 0; index < a_count; ++index)
				{
					imgui.BeginWindow("##Anchor", { 0.0f, 0.0f }, { 500.0f, 260.0f });
					require(DrawTextView(1, descriptor, state) == DMUI_RESULT_OK,
						"the anchored view failed to draw");
					imgui.EndWindow();
				}
			};
			const auto scroll = [] { return FindChild("Anchor")->Scroll.y; };
			frame();
			state.scrollCommand = DMUI_TEXT_VIEW_SCROLL_PAGE_DOWN;
			frame();
			require(state.scrollCommand == DMUI_TEXT_VIEW_SCROLL_NONE && scroll() > 50.0f,
				"a page command was kept or did not scroll");
			state.scrollCommand = DMUI_TEXT_VIEW_SCROLL_PAGE_DOWN;
			frame();
			const auto before = scroll();

			// Ten lines leave the front: the same line stays under the reader.
			const auto lineHeight = ImGui::GetTextLineHeightWithSpacing();
			text = build(10, 190);
			lines = LineOffsets(text);
			descriptor = Descriptor("Anchor", text, lines, {}, 0, 2);
			descriptor.firstLineNumber = 10;
			state.contentRevision = 2;
			frame();
			require(std::abs(scroll() - (before - 10.0f * lineHeight)) < 1.0f,
				"trimming front lines moved the reader's place");

			// Appending keeps the position; so does replacing without a trim.
			text += build(200, 20);
			lines = LineOffsets(text);
			descriptor = Descriptor("Anchor", text, lines, {}, 0, 3);
			descriptor.firstLineNumber = 10;
			state.contentRevision = 3;
			frame();
			require(std::abs(scroll() - (before - 10.0f * lineHeight)) < 1.0f,
				"a content change reset the scroll position");

			state.scrollCommand = DMUI_TEXT_VIEW_SCROLL_TOP;
			frame();
			require(scroll() == 0.0f, "the top command did not scroll to the top");
			state.scrollCommand = DMUI_TEXT_VIEW_SCROLL_COUNT;
			imgui.BeginWindow("##Anchor", { 0.0f, 0.0f }, { 500.0f, 260.0f });
			require(DrawTextView(1, descriptor, state) == DMUI_RESULT_INVALID_ARGUMENT,
				"an unknown scroll command was accepted");
			imgui.EndWindow();
		});

		runner.test("text view follows the tail only while it is at the bottom", [] {
			const auto build = [](int a_count) {
				std::string text;
				for (int line = 0; line < a_count; ++line)
					text += "entry " + std::to_string(line) + "\n";
				return text;
			};
			auto text = build(100);
			auto lines = LineOffsets(text);
			auto descriptor = Descriptor("Tail", text, lines);
			descriptor.flags = DMUI_TEXT_VIEW_FOLLOW_TAIL;
			auto state = State();
			support::ImGuiTestContext imgui{ { .disableErrorRecovery = true } };
			const auto frame = [&](int a_count = 4) {
				for (int index = 0; index < a_count; ++index)
				{
					imgui.BeginWindow("##Tail", { 0.0f, 0.0f }, { 500.0f, 260.0f });
					require(DrawTextView(1, descriptor, state) == DMUI_RESULT_OK,
						"the tail view failed to draw");
					imgui.EndWindow();
				}
			};
			const auto append = [&](uint64_t a_revision) {
				text += build(20);
				lines = LineOffsets(text);
				const auto flags = descriptor.flags;
				descriptor = Descriptor("Tail", text, lines, {}, 0, a_revision);
				descriptor.flags = flags;
				state.contentRevision = a_revision;
			};
			const auto atBottom = [] {
				const auto* child = FindChild("Tail");
				return child->ScrollMax.y > 0.0f && child->Scroll.y >= child->ScrollMax.y - 1.0f;
			};
			frame();
			require(atBottom() && state.atBottom != 0, "a new tail view did not start at the end");
			append(2);
			frame();
			require(atBottom() && state.atBottom != 0, "appended lines did not stay pinned");

			state.scrollCommand = DMUI_TEXT_VIEW_SCROLL_TOP;
			frame();
			require(FindChild("Tail")->Scroll.y == 0.0f && state.atBottom == 0,
				"scrolling up did not leave the end");
			append(3);
			frame();
			require(FindChild("Tail")->Scroll.y == 0.0f && state.atBottom == 0,
				"appended lines pulled a scrolled-up reader down");

			state.scrollCommand = DMUI_TEXT_VIEW_SCROLL_BOTTOM;
			frame();
			require(atBottom() && state.atBottom != 0, "the bottom command did not reach the end");
			append(4);
			frame();
			require(atBottom(), "following did not resume at the end");
		});

		runner.test("text view lets a reveal or paging leave the pinned tail", [] {
			std::string text;
			for (int line = 0; line < 100; ++line)
				text += "entry " + std::to_string(line) + "\n";
			const auto lines = LineOffsets(text);
			auto descriptor = Descriptor("Pinned", text, lines);
			descriptor.flags = DMUI_TEXT_VIEW_FOLLOW_TAIL;
			auto state = State();
			support::ImGuiTestContext imgui{ { .disableErrorRecovery = true } };
			const auto frame = [&](int a_count = 4) {
				for (int index = 0; index < a_count; ++index)
				{
					imgui.BeginWindow("##Pinned", { 0.0f, 0.0f }, { 500.0f, 260.0f });
					require(DrawTextView(1, descriptor, state) == DMUI_RESULT_OK,
						"the pinned view failed to draw");
					imgui.EndWindow();
				}
			};
			const auto bottom = [] {
				const auto* child = FindChild("Pinned");
				return child->Scroll.y >= child->ScrollMax.y - 1.0f;
			};
			frame();
			require(bottom(), "the view did not start pinned");

			state.revealByteOffset = lines[10];
			frame();
			require(!bottom() && state.revealByteOffset == DMUI_TEXT_VIEW_NO_OFFSET,
				"a reveal did not move the pinned view and clear");

			state.scrollCommand = DMUI_TEXT_VIEW_SCROLL_BOTTOM;
			frame();
			require(bottom(), "the bottom command did not re-pin");
			const auto before = FindChild("Pinned")->Scroll.y;
			state.scrollCommand = DMUI_TEXT_VIEW_SCROLL_PAGE_UP;
			frame();
			const auto after = FindChild("Pinned")->Scroll.y;
			require(!bottom() && after < before - 50.0f && state.atBottom == 0,
				"paging up from the pinned tail did not stay released");
		});

		runner.test("text view reports span hover and click with the caller's id", [] {
			const std::string text{ "hello world\nsecond line" };
			const auto lines = LineOffsets(text);
			std::vector<DMUI_TextSpan> spans(2);
			spans[0].offset = 0;
			spans[0].length = 5;
			spans[0].userId = 7;
			spans[1].offset = 6;
			spans[1].length = 5;
			spans[1].flags = DMUI_TEXT_SPAN_INTERACTIVE;
			spans[1].userId = 42;
			auto descriptor = Descriptor("Spans", text, lines);
			descriptor.spans = spans.data();
			descriptor.spanCount = spans.size();
			auto state = State();
			support::ImGuiTestContext imgui{
				{ .disableInputTrickle = true, .disableErrorRecovery = true }
			};
			const auto frame = [&] {
				imgui.BeginWindow("##Spans", { 0.0f, 0.0f }, { 500.0f, 260.0f });
				require(DrawTextView(1, descriptor, state) == DMUI_RESULT_OK,
					"the span view failed to draw");
				imgui.EndWindow();
			};
			auto& io = ImGui::GetIO();
			io.AddMousePosEvent(480.0f, 240.0f);
			frame();
			frame();
			const auto origin = FindChild("Spans")->DC.CursorStartPos;
			const auto textHeight = ImGui::GetTextLineHeight();
			const ImVec2 plain{ origin.x + 3.0f, origin.y + textHeight * 0.5f };
			const ImVec2 target{
				origin.x + ImGui::CalcTextSize("hello ").x + 3.0f, origin.y + textHeight * 0.5f };

			io.AddMousePosEvent(plain.x, plain.y);
			frame();
			require(state.events == 0 && state.spanIndex == DMUI_TEXT_VIEW_NO_OFFSET,
				"a non-interactive span reported events");
			io.AddMousePosEvent(target.x, target.y);
			frame();
			require(state.events == DMUI_TEXT_VIEW_EVENT_SPAN_HOVERED &&
					state.spanIndex == 1 && state.userId == 42,
				"hovering an interactive span was not reported");
			io.AddMouseButtonEvent(0, true);
			frame();
			io.AddMouseButtonEvent(0, false);
			frame();
			require((state.events & DMUI_TEXT_VIEW_EVENT_SPAN_CLICKED) != 0 &&
					state.spanIndex == 1 && state.userId == 42,
				"clicking an interactive span was not reported");
			frame();
			require((state.events & DMUI_TEXT_VIEW_EVENT_SPAN_CLICKED) == 0,
				"a click event outlived its frame");

			// A drag that starts on the span is a selection, not a click.
			io.AddMouseButtonEvent(0, true);
			frame();
			io.AddMousePosEvent(target.x + 60.0f, target.y);
			frame();
			io.AddMouseButtonEvent(0, false);
			frame();
			require((state.events & DMUI_TEXT_VIEW_EVENT_SPAN_CLICKED) == 0,
				"a drag was reported as a click");
		});

		runner.test("text view copies the dragged selection with Ctrl+C", [] {
			const std::string text{ "alpha\nbravo\ncharlie" };
			const auto lines = LineOffsets(text);
			auto descriptor = Descriptor("Select", text, lines);
			descriptor.flags = DMUI_TEXT_VIEW_SELECTABLE;
			auto state = State();
			support::ImGuiTestContext imgui{
				{ .disableInputTrickle = true, .disableErrorRecovery = true }
			};
			const auto frame = [&] {
				imgui.BeginWindow("##Select", { 0.0f, 0.0f }, { 500.0f, 260.0f });
				require(DrawTextView(1, descriptor, state) == DMUI_RESULT_OK,
					"the selectable view failed to draw");
				imgui.EndWindow();
			};
			auto& io = ImGui::GetIO();
			io.AddMousePosEvent(480.0f, 240.0f);
			frame();
			frame();
			const auto origin = FindChild("Select")->DC.CursorStartPos;
			const auto lineHeight = ImGui::GetTextLineHeightWithSpacing();
			const auto half = ImGui::GetTextLineHeight() * 0.5f;
			io.AddMousePosEvent(origin.x + 1.0f, origin.y + half);
			frame();
			io.AddMouseButtonEvent(0, true);
			frame();
			io.AddMousePosEvent(origin.x + ImGui::CalcTextSize("brav").x + 1.0f,
				origin.y + lineHeight + half);
			frame();
			io.AddMouseButtonEvent(0, false);
			frame();
			ImGui::SetClipboardText("untouched");
			io.AddKeyEvent(ImGuiMod_Ctrl, true);
			io.AddKeyEvent(ImGuiKey_C, true);
			frame();
			require(std::string_view{ ImGui::GetClipboardText() } == "alpha\nbrav",
				"Ctrl+C did not copy the dragged selection");
			io.AddKeyEvent(ImGuiKey_C, false);
			io.AddKeyEvent(ImGuiMod_Ctrl, false);
			frame();

			// Without the flag the view offers no selection.
			descriptor.flags = 0;
			io.AddMousePosEvent(origin.x + 1.0f, origin.y + half);
			frame();
			io.AddMouseButtonEvent(0, true);
			frame();
			io.AddMousePosEvent(origin.x + 60.0f, origin.y + half);
			frame();
			io.AddMouseButtonEvent(0, false);
			frame();
			ImGui::SetClipboardText("untouched");
			io.AddKeyEvent(ImGuiMod_Ctrl, true);
			io.AddKeyEvent(ImGuiKey_C, true);
			frame();
			require(std::string_view{ ImGui::GetClipboardText() } == "untouched",
				"a view without SELECTABLE copied text");
		});
	}
}
