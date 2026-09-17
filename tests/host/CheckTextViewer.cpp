#include "../Harness.h"
#include "../support/ImGuiTestContext.h"

#include <DearModdingUI/Client.h>
#include <DearModdingUI/controls/TextViewer.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
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
				sizeof(DMUI_TextViewDescriptor),
				a_id,
				a_text.data(),
				a_text.size(),
				a_lines.data(),
				a_lines.size(),
				a_matches.data(),
				a_matches.size(),
				a_matchLength,
				a_contentRevision,
				a_matchRevision,
				{ 360.0f, 140.0f }
			};
		}

		[[nodiscard]] DMUI_TextViewState State(
			uint64_t a_contentRevision = 1,
			uint64_t a_matchRevision = 1)
		{
			return {
				sizeof(DMUI_TextViewState),
				a_contentRevision,
				a_matchRevision,
				DMUI_TEXT_VIEW_NO_OFFSET,
				DMUI_TEXT_VIEW_NO_OFFSET
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
			const auto activeColor =
				ImGui::GetColorU32(ImGuiCol_HeaderActive);
			const auto inactiveColor =
				ImGui::GetColorU32(ImGuiCol_TextSelectedBg);
			const auto* child = FindChild("LiteralText");
			require(child, "text viewer child window was not created");
			const auto& vertices = child->DrawList->VtxBuffer;
			require(
				std::ranges::any_of(
					vertices,
					[&](const ImDrawVert& a_vertex) {
						return a_vertex.col == activeColor;
					}) &&
					std::ranges::any_of(
						vertices,
						[&](const ImDrawVert& a_vertex) {
							return a_vertex.col == inactiveColor;
						}),
				"active and overlapping match highlights were not rendered");
			const auto activeVertex = std::ranges::find_if(
				vertices,
				[&](const ImDrawVert& a_vertex) {
						return a_vertex.col == activeColor;
				});
			const auto reversedVertices = vertices | std::views::reverse;
			const auto lastInactiveVertex = std::ranges::find_if(
				reversedVertices,
				[&](const ImDrawVert& a_vertex) {
						return a_vertex.col == inactiveColor;
				});
			require(
				activeVertex != vertices.end() &&
						lastInactiveVertex != reversedVertices.end() &&
						&*activeVertex > &*lastInactiveVertex,
				"an overlapping inactive highlight covered the active match");
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

		runner.test("text reveal waits for measured and visible extents", [] {
			std::string text;
			std::vector<size_t> lines{ 0 };
			for (size_t line = 0; line < 240; ++line)
			{
				text.append("line ");
				text.append(std::to_string(line));
				text.push_back('\n');
				lines.push_back(text.size());
			}
			const auto revealLine = size_t{ 210 };
			auto descriptor =
				Descriptor("InitialReveal", text, lines, {}, 0, 41, 43);
			descriptor.viewport = { 260.0f, 90.0f };
			auto state = State(41, 43);
			state.revealByteOffset = lines[revealLine];
			support::ImGuiTestContext imgui{
				{
					.displaySize = { 640.0f, 360.0f },
					.disableErrorRecovery = true
				}
			};

			imgui.BeginWindow(
				"##InitialRevealParent",
				{ 0.0f, 0.0f },
				{ 420.0f, 180.0f });
			require(
				DrawTextView(7, descriptor, state) == DMUI_RESULT_OK &&
					state.revealByteOffset == lines[revealLine],
				"initial reveal was consumed before content extent existed");
			imgui.EndWindow(true);

			imgui.BeginWindow(
				"##InitialRevealParent",
				{ 0.0f, 0.0f },
				{ 420.0f, 180.0f });
			require(
				DrawTextView(7, descriptor, state) == DMUI_RESULT_OK,
				"initial reveal failed on its follow-up frame");
			const auto* child = FindChild("InitialReveal");
			require(child, "initial-reveal child window was not created");
			const auto lineHeight = ImGui::GetTextLineHeightWithSpacing();
			const auto targetY =
				child->DC.CursorStartPos.y +
				static_cast<float>(revealLine) * lineHeight;
			require(
				child->Scroll.y > 0.0f &&
					targetY >= child->InnerRect.Min.y &&
					targetY + ImGui::GetTextLineHeight() <=
						child->InnerRect.Max.y &&
					state.revealByteOffset == DMUI_TEXT_VIEW_NO_OFFSET,
				"revealed line was not actually visible before consumption");
			imgui.EndWindow(true);

			auto hiddenReplacement = descriptor;
			hiddenReplacement.contentRevision = 44;
			hiddenReplacement.matchRevision = 45;
			auto hiddenState = State(44, 45);
			imgui.BeginWindow(
				"##InitialRevealParent",
				{ 0.0f, 0.0f },
				{ 420.0f, 180.0f });
			auto* hiddenParent = ImGui::GetCurrentWindow();
			hiddenParent->SkipItems = true;
			require(
				DrawTextView(7, hiddenReplacement, hiddenState) ==
					DMUI_RESULT_OK,
				"hidden replacement content failed validation");
			hiddenParent->SkipItems = false;
			imgui.EndWindow(true);

			imgui.BeginWindow(
				"##InitialRevealParent",
				{ 0.0f, 0.0f },
				{ 420.0f, 180.0f });
			require(
				DrawTextView(7, hiddenReplacement, hiddenState) ==
					DMUI_RESULT_OK,
				"hidden replacement content failed when shown");
			const auto* resetChild = FindChild("InitialReveal");
			require(
				resetChild && resetChild->Scroll.y == 0.0f,
				"hidden content replacement retained stale scroll");
			imgui.EndWindow(true);

			const std::string shortText{ "short\n" };
			const auto shortLines = LineOffsets(shortText);
			auto changingDescriptor = Descriptor(
				"ChangedReveal",
				shortText,
				shortLines,
				{},
				0,
				47,
				53);
			changingDescriptor.viewport = descriptor.viewport;
			auto changingState = State(47, 53);
			imgui.BeginWindow("##ChangedRevealParent");
			require(
				DrawTextView(7, changingDescriptor, changingState) ==
					DMUI_RESULT_OK,
				"short precursor content failed to draw");
			imgui.EndWindow(true);

			changingDescriptor = Descriptor(
				"ChangedReveal",
				text,
				lines,
				{},
				0,
				48,
				54);
			changingDescriptor.viewport = descriptor.viewport;
			changingState = State(48, 54);
			changingState.revealByteOffset = lines[revealLine];
			imgui.BeginWindow("##ChangedRevealParent");
			require(
				DrawTextView(7, changingDescriptor, changingState) ==
						DMUI_RESULT_OK &&
					changingState.revealByteOffset == lines[revealLine],
				"new-content reveal was consumed against the old extent");
			imgui.EndWindow(true);

			imgui.BeginWindow("##ChangedRevealParent");
			require(
				DrawTextView(7, changingDescriptor, changingState) ==
						DMUI_RESULT_OK &&
					changingState.revealByteOffset ==
						DMUI_TEXT_VIEW_NO_OFFSET,
				"new-content reveal was not completed on the measured frame");
			const auto* changedChild = FindChild("ChangedReveal");
			require(
				changedChild && changedChild->Scroll.y > 0.0f,
				"new-content reveal did not change the actual scroll");
			imgui.EndWindow(true);
		});

		runner.test("offscreen text reveal scrolls its parent and survives hiding", [] {
			std::string text;
			std::vector<size_t> lines{ 0 };
			for (size_t line = 0; line < 160; ++line)
			{
				text.append("nested line\n");
				lines.push_back(text.size());
			}
			auto descriptor =
				Descriptor("NestedReveal", text, lines, {}, 0, 59, 61);
			descriptor.viewport = { 240.0f, 80.0f };
			auto state = State(59, 61);
			state.revealByteOffset = lines[140];
			support::ImGuiTestContext imgui{
				{
					.displaySize = { 640.0f, 360.0f },
					.disableErrorRecovery = true
				}
			};

			const auto drawFrame = [&] {
				imgui.BeginWindow(
					"##NestedRevealRoot",
					{ 0.0f, 0.0f },
					{ 420.0f, 180.0f });
				if (ImGui::BeginChild(
						"NestedRevealOuter",
						{ 320.0f, 110.0f },
						ImGuiChildFlags_Borders))
				{
					ImGui::Dummy({ 1.0f, 260.0f });
					require(
						DrawTextView(7, descriptor, state) ==
							DMUI_RESULT_OK,
						"nested text view failed to draw");
				}
				ImGui::EndChild();
				imgui.EndWindow(true);
			};

			drawFrame();
			require(
				state.revealByteOffset == lines[140],
				"offscreen child consumed its reveal request");
			drawFrame();
			const auto* outer = FindChild("NestedRevealOuter");
			require(
				outer && outer->Scroll.y > 0.0f,
				"offscreen text view did not reveal its parent viewport");
			require(
				state.revealByteOffset == lines[140],
				"newly visible child consumed an unapplied text reveal");
			drawFrame();
			require(
				state.revealByteOffset == DMUI_TEXT_VIEW_NO_OFFSET,
				"nested reveal was not consumed after both scrolls applied");

			auto partialDescriptor = Descriptor(
				"PartialReveal",
				text,
				lines,
				{},
				0,
				63,
				65);
			partialDescriptor.viewport = { 240.0f, 80.0f };
			auto partialState = State(63, 65);
			const auto partialLine = size_t{ 1 };
			partialState.revealByteOffset = lines[partialLine];
			const auto drawPartialFrame = [&] {
				imgui.BeginWindow(
					"##PartialRevealRoot",
					{ 0.0f, 0.0f },
					{ 420.0f, 180.0f });
				if (ImGui::BeginChild(
						"PartialRevealOuter",
						{ 320.0f, 110.0f },
						ImGuiChildFlags_Borders))
				{
					ImGui::Dummy({ 1.0f, 75.0f });
					require(
						DrawTextView(7, partialDescriptor, partialState) ==
							DMUI_RESULT_OK,
						"partially clipped text view failed to draw");
				}
				ImGui::EndChild();
				imgui.EndWindow(true);
			};

			drawPartialFrame();
			const auto* partialOuter = FindChild("/PartialRevealOuter_");
			const auto* partialChild = FindChild("/PartialReveal_");
			require(
				partialOuter && partialChild &&
					partialOuter != partialChild &&
					partialOuter->ClipRect.Overlaps(partialChild->Rect()) &&
					!partialOuter->ClipRect.Contains(partialChild->Rect()),
				"partial-reveal fixture did not clip the text viewport");
			require(
				partialState.revealByteOffset == lines[partialLine],
				"partially clipped target was consumed while invisible");

			drawPartialFrame();
			partialChild = FindChild("/PartialReveal_");
			require(partialChild, "partial-reveal child window disappeared");
			const auto targetY =
				partialChild->DC.CursorStartPos.y +
				static_cast<float>(partialLine) *
					ImGui::GetTextLineHeightWithSpacing();
			const ImRect target{
				{ partialChild->DC.CursorStartPos.x, targetY },
				{
					partialChild->DC.CursorStartPos.x + 1.0f,
					targetY + ImGui::GetTextLineHeight()
				}
			};
			require(
				partialChild->ClipRect.Contains(target) &&
					partialState.revealByteOffset ==
						DMUI_TEXT_VIEW_NO_OFFSET,
				"partial reveal cleared before the target became visible");
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
	}
}
