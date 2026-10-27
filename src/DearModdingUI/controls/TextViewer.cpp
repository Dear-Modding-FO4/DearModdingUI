#include <DearModdingUI/controls/TextViewer.h>
#include <Support/Utf8.h>

#include <DearModdingUI/controls/TextRuns.h>
#include <DearModdingUI/host/ShortcutChord.h>
#include <DearModdingUI/presentation/Theme.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace DearModdingUI
{
	namespace
	{
		// A line and byte column that survive front trims of the text.
		struct TextPosition
		{
			uint64_t line{};
			size_t column{};
			[[nodiscard]] bool operator==(const TextPosition&) const noexcept = default;
		};

		struct Selection
		{
			TextPosition anchor;
			TextPosition focus;
			bool dragging{};
		};

		struct TextViewCache
		{
			uint64_t contentRevision{};
			uint64_t matchRevision{};
			uint64_t spanRevision{};
			uint64_t firstLineNumber{};
			size_t spanCount{};
			Selection selection;
			size_t pressedSpan{ DMUI_TEXT_VIEW_NO_OFFSET };
			bool spansInitialized{};
			size_t textLength{};
			size_t lineCount{};
			size_t matchCount{};
			size_t matchByteLength{};
			const ImFont* font{};
			float fontSize{};
			float contentWidth{};
			bool contentInitialized{};
			bool matchesInitialized{};
		};

		struct TextViewCacheStorage
		{
			~TextViewCacheStorage() noexcept
			{
				for (auto& entry : entries.Data)
					delete static_cast<TextViewCache*>(entry.val_p);
			}

			ImGuiStorage entries;
		};

		constexpr uint32_t kKnownViewFlags{
			DMUI_TEXT_VIEW_FOLLOW_TAIL | DMUI_TEXT_VIEW_SELECTABLE
		};

		struct ByteRange
		{
			size_t begin{};
			size_t end{};
		};

		struct VisibleText
		{
			size_t begin{};
			size_t end{};
			float beginX{};
		};

		class ClientIdScope
		{
		public:
			explicit ClientIdScope(DMUI_ClientHandle a_client) noexcept
			{
				auto* window = ImGui::GetCurrentWindow();
				ImGui::PushOverrideID(
					ImHashData(
						&a_client,
						sizeof(a_client),
						window->IDStack.back()));
			}

			~ClientIdScope() noexcept
			{
				ImGui::PopID();
			}

			ClientIdScope(const ClientIdScope&) = delete;
			ClientIdScope& operator=(const ClientIdScope&) = delete;
		};

		class ChildScope
		{
		public:
			ChildScope(
				const char* a_id,
				const ImVec2& a_size,
				ImGuiChildFlags a_childFlags,
				ImGuiWindowFlags a_windowFlags) :
				m_visible{ ImGui::BeginChild(
					a_id,
					a_size,
					a_childFlags,
					a_windowFlags) }
			{}

			~ChildScope() noexcept
			{
				ImGui::EndChild();
			}

			ChildScope(const ChildScope&) = delete;
			ChildScope& operator=(const ChildScope&) = delete;

			[[nodiscard]] bool IsVisible() const noexcept
			{
				return m_visible;
			}

		private:
			bool m_visible{};
		};

		[[nodiscard]] constexpr bool IsUtf8Continuation(char a_value) noexcept
		{
			return (static_cast<unsigned char>(a_value) & 0xC0u) == 0x80u;
		}

		void DestroyTextViewCaches(
			ImGuiContext*,
			ImGuiContextHook* a_hook) noexcept
		{
			auto* storage =
				static_cast<TextViewCacheStorage*>(a_hook->UserData);
			a_hook->UserData = nullptr;
			delete storage;
		}

		[[nodiscard]] TextViewCacheStorage& ContextCaches()
		{
			auto* context = ImGui::GetCurrentContext();
			const auto owner =
				ImHashStr("DearModdingUI.TextViewer.Cache");
			for (auto& hook : context->Hooks)
			{
				if (hook.Owner == owner &&
					hook.Type == ImGuiContextHookType_Shutdown)
					return *static_cast<TextViewCacheStorage*>(
						hook.UserData);
			}

			auto storage = std::make_unique<TextViewCacheStorage>();
			ImGuiContextHook hook;
			hook.Type = ImGuiContextHookType_Shutdown;
			hook.Owner = owner;
			hook.Callback = &DestroyTextViewCaches;
			hook.UserData = storage.get();
			(void)ImGui::AddContextHook(context, &hook);
			return *storage.release();
		}

		[[nodiscard]] TextViewCache& CacheFor(ImGuiID a_id)
		{
			auto& storage = ContextCaches().entries;
			if (auto* cached =
					static_cast<TextViewCache*>(storage.GetVoidPtr(a_id)))
				return *cached;

			auto cache = std::make_unique<TextViewCache>();
			auto* result = cache.get();
			storage.SetVoidPtr(a_id, result);
			(void)cache.release();
			return *result;
		}

		[[nodiscard]] DMUI_Result ValidateBasic(
			const DMUI_TextViewDescriptor& a_descriptor) noexcept
		{
			constexpr auto maxDifference =
				static_cast<size_t>(
					(std::numeric_limits<std::ptrdiff_t>::max)());
			constexpr auto maxMatchCount =
				maxDifference / sizeof(size_t);
			if (!a_descriptor.id || !a_descriptor.id[0] ||
				(!a_descriptor.text && a_descriptor.textLength != 0) ||
				!a_descriptor.lineOffsets ||
				a_descriptor.lineCount == 0 ||
				a_descriptor.lineCount >
					static_cast<size_t>(
						(std::numeric_limits<int>::max)()) ||
				a_descriptor.textLength > maxDifference ||
				a_descriptor.lineCount > maxDifference ||
				a_descriptor.matchCount > maxMatchCount ||
				(a_descriptor.matchCount != 0 &&
					(!a_descriptor.matchByteOffsets ||
						a_descriptor.matchByteLength == 0 ||
						a_descriptor.matchByteLength > maxDifference ||
						a_descriptor.matchByteLength >
							a_descriptor.textLength)) ||
				a_descriptor.lineOffsets[0] != 0 ||
				(a_descriptor.flags & ~kKnownViewFlags) != 0 ||
				(!a_descriptor.spans && a_descriptor.spanCount != 0) ||
				a_descriptor.spanCount > maxDifference / sizeof(DMUI_TextSpan) ||
				a_descriptor.firstLineNumber >
					(std::numeric_limits<uint64_t>::max)() - a_descriptor.lineCount ||
				!std::isfinite(a_descriptor.viewport.x) ||
				!std::isfinite(a_descriptor.viewport.y) ||
				a_descriptor.viewport.x < 0.0f ||
				a_descriptor.viewport.y < 0.0f)
				return DMUI_RESULT_INVALID_ARGUMENT;
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] size_t LineEnd(
			const DMUI_TextViewDescriptor& a_descriptor,
			size_t a_line) noexcept
		{
			auto end = a_line + 1 < a_descriptor.lineCount ?
				a_descriptor.lineOffsets[a_line + 1] - 1 :
				a_descriptor.textLength;
			if (end > a_descriptor.lineOffsets[a_line] &&
				a_descriptor.text[end - 1] == '\r')
				--end;
			return end;
		}

		[[nodiscard]] DMUI_Result ValidateContent(
			const DMUI_TextViewDescriptor& a_descriptor) noexcept
		{
			const auto* text = a_descriptor.text ?
				a_descriptor.text :
				"";
			if (!Support::IsValidUtf8(text, a_descriptor.textLength))
				return DMUI_RESULT_INVALID_ARGUMENT;

			size_t expectedLine = 1;
			for (size_t offset = 0; offset < a_descriptor.textLength; ++offset)
			{
				if (text[offset] != '\n')
					continue;
				if (expectedLine >= a_descriptor.lineCount ||
					a_descriptor.lineOffsets[expectedLine] != offset + 1)
					return DMUI_RESULT_INVALID_ARGUMENT;
				++expectedLine;
			}
			return expectedLine == a_descriptor.lineCount ?
				DMUI_RESULT_OK :
				DMUI_RESULT_INVALID_ARGUMENT;
		}

		[[nodiscard]] float MeasureContentWidth(
			const DMUI_TextViewDescriptor& a_descriptor)
		{
			const auto* text = a_descriptor.text ?
				a_descriptor.text :
				"";
			float contentWidth = 0.0f;
			for (size_t line = 0; line < a_descriptor.lineCount; ++line)
			{
				contentWidth = (std::max)(
					contentWidth,
					ImGui::CalcTextSize(
						text + a_descriptor.lineOffsets[line],
						text + LineEnd(a_descriptor, line),
						false).x);
			}
			return contentWidth;
		}

		[[nodiscard]] DMUI_Result ValidateMatches(
			const DMUI_TextViewDescriptor& a_descriptor) noexcept
		{
			if (a_descriptor.matchCount == 0)
				return DMUI_RESULT_OK;

			const auto* text = a_descriptor.text ?
				a_descriptor.text :
				"";
			size_t previous{};
			for (size_t index = 0; index < a_descriptor.matchCount; ++index)
			{
				const auto start = a_descriptor.matchByteOffsets[index];
				if ((index > 0 && start < previous) ||
					start > a_descriptor.textLength ||
					a_descriptor.matchByteLength >
						a_descriptor.textLength - start)
					return DMUI_RESULT_INVALID_ARGUMENT;
				const auto end = start + a_descriptor.matchByteLength;
				if (!Support::IsUtf8Boundary(text, a_descriptor.textLength, start) ||
					!Support::IsUtf8Boundary(text, a_descriptor.textLength, end))
					return DMUI_RESULT_INVALID_ARGUMENT;
				previous = start;
			}
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] size_t LineForOffset(
			const DMUI_TextViewDescriptor& a_descriptor,
			size_t a_offset) noexcept
		{
			const auto* begin = a_descriptor.lineOffsets;
			const auto* end = begin + a_descriptor.lineCount;
			const auto* upper = std::upper_bound(begin, end, a_offset);
			return upper == begin ?
				0 :
				static_cast<size_t>((upper - begin) - 1);
		}

		[[nodiscard]] float TextWidth(
			const char* a_text,
			size_t a_begin,
			size_t a_end)
		{
			return a_begin == a_end ?
				0.0f :
				ImGui::CalcTextSize(
					a_text + a_begin,
					a_text + a_end,
					false).x;
		}

		[[nodiscard]] VisibleText ClipTextHorizontally(
			const char* a_text,
			size_t a_lineStart,
			size_t a_lineEnd,
			float a_originX,
			const ImRect& a_clipRect)
		{
			auto* font = ImGui::GetFont();
			const auto fontSize = ImGui::GetFontSize();
			const auto leftWidth =
				(std::max)(a_clipRect.Min.x - a_originX, 0.0f);
			const char* visibleBegin = a_text + a_lineStart;
			const auto prefix = font->CalcTextSizeA(
				fontSize,
				leftWidth,
				0.0f,
				a_text + a_lineStart,
				a_text + a_lineEnd,
				&visibleBegin);
			if (visibleBegin == a_text + a_lineEnd)
			{
				return {
					a_lineEnd,
					a_lineEnd,
					prefix.x
				};
			}

			const auto visibleWidth = (std::max)(
				a_clipRect.Max.x - (a_originX + prefix.x) +
					fontSize * 2.0f,
				fontSize * 2.0f);
			const char* visibleEnd = visibleBegin;
			(void)font->CalcTextSizeA(
				fontSize,
				visibleWidth,
				0.0f,
				visibleBegin,
				a_text + a_lineEnd,
				&visibleEnd);
			if (visibleEnd == visibleBegin)
			{
				++visibleEnd;
				while (visibleEnd < a_text + a_lineEnd &&
					   IsUtf8Continuation(*visibleEnd))
					++visibleEnd;
			}
			return {
				static_cast<size_t>(visibleBegin - a_text),
				static_cast<size_t>(visibleEnd - a_text),
				prefix.x
			};
		}

		using Fills = std::vector<TextRuns::Fill>;
		using Runs = std::vector<TextRuns::Run>;

		void CollectInactiveMatches(
			Fills& a_fills,
			const char* a_text,
			const VisibleText& a_visible,
			const DMUI_TextViewDescriptor& a_descriptor,
			size_t a_lineStart,
			size_t a_lineEnd,
			size_t a_activeMatch,
			ImU32 a_color)
		{
			if (a_descriptor.matchCount == 0)
				return;

			auto byte = a_visible.begin;
			auto x = a_visible.beginX;
			ByteRange pending;
			bool hasPending{};
			const auto flush = [&] {
				x += TextWidth(a_text, byte, pending.begin);
				const auto startX = x;
				x += TextWidth(a_text, pending.begin, pending.end);
				a_fills.push_back({ startX, x, a_color });
				byte = pending.end;
			};

			const auto* matches = a_descriptor.matchByteOffsets;
			const auto firstRelevant = (std::max)(
				a_lineStart > a_descriptor.matchByteLength ?
					a_lineStart - a_descriptor.matchByteLength :
					0,
				a_visible.begin > a_descriptor.matchByteLength ?
					a_visible.begin - a_descriptor.matchByteLength :
					0);
			const auto* first = std::lower_bound(
				matches,
				matches + a_descriptor.matchCount,
				firstRelevant);
			for (auto* match = first;
				 match != matches + a_descriptor.matchCount &&
				 *match < a_lineEnd &&
				 *match < a_visible.end;
				 ++match)
			{
				if (static_cast<size_t>(match - matches) == a_activeMatch)
					continue;
				auto range = ByteRange{
					(std::max)(*match, a_lineStart),
					(std::min)(
						*match + a_descriptor.matchByteLength,
						a_lineEnd)
				};
				range.begin = (std::max)(range.begin, a_visible.begin);
				range.end = (std::min)(range.end, a_visible.end);
				if (range.begin >= range.end)
					continue;
				if (hasPending && range.begin <= pending.end)
				{
					pending.end = (std::max)(pending.end, range.end);
					continue;
				}
				if (hasPending)
					flush();
				pending = range;
				hasPending = true;
			}
			if (hasPending)
				flush();
		}

		// The first span that ends after a_offset.
		[[nodiscard]] size_t FirstSpanEndingAfter(
			const DMUI_TextViewDescriptor& a_descriptor,
			size_t a_offset) noexcept
		{
			const auto* begin = a_descriptor.spans;
			const auto* end = begin + a_descriptor.spanCount;
			return static_cast<size_t>(std::partition_point(
				begin, end, [a_offset](const DMUI_TextSpan& a_span) {
					return a_span.offset + a_span.length <= a_offset;
				}) - begin);
		}

		[[nodiscard]] std::optional<size_t> SpanAt(
			const DMUI_TextViewDescriptor& a_descriptor,
			size_t a_offset) noexcept
		{
			const auto index = FirstSpanEndingAfter(a_descriptor, a_offset);
			if (index < a_descriptor.spanCount &&
				a_descriptor.spans[index].offset <= a_offset)
				return index;
			return std::nullopt;
		}

		// Per-frame scratch shared by every visible line.
		struct LineScratch
		{
			TextRuns::Style normal;
			Runs runs;
			Fills fills;
		};

		void DrawLine(
			const DMUI_TextViewDescriptor& a_descriptor,
			const DMUI_TextViewState& a_state,
			const std::optional<ByteRange>& a_selection,
			size_t a_line,
			float a_lineHeight,
			LineScratch& a_scratch)
		{
			const auto* text = a_descriptor.text ?
				a_descriptor.text :
				"";
			const auto lineStart = a_descriptor.lineOffsets[a_line];
			const auto lineEnd = LineEnd(a_descriptor, a_line);
			if (lineStart == lineEnd)
				return;

			const auto origin = ImGui::GetCursorScreenPos();
			auto* window = ImGui::GetCurrentWindow();
			const auto clipRect = window->ClipRect;
			const auto visible = ClipTextHorizontally(
				text,
				lineStart,
				lineEnd,
				origin.x,
				clipRect);
			if (visible.begin == visible.end)
				return;

			auto& runs = a_scratch.runs;
			auto& fills = a_scratch.fills;
			runs.clear();
			fills.clear();

			TextRuns::AppendRuns(
				TextRuns::Consumer::kTextView,
				text,
				{ a_descriptor.spans, a_descriptor.spanCount },
				visible.begin,
				visible.end,
				visible.beginX,
				a_scratch.normal,
				runs);

			CollectInactiveMatches(
				fills,
				text,
				visible,
				a_descriptor,
				lineStart,
				lineEnd,
				a_state.activeMatch,
				ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
			const auto addRange = [&](ByteRange a_range, ImU32 a_color) {
				a_range.begin = (std::max)({ a_range.begin, lineStart, visible.begin });
				a_range.end = (std::min)({ a_range.end, lineEnd, visible.end });
				if (a_range.begin >= a_range.end)
					return;
				const auto startX =
					visible.beginX + TextWidth(text, visible.begin, a_range.begin);
				fills.push_back({
					startX, startX + TextWidth(text, a_range.begin, a_range.end),
					a_color });
			};
			if (a_state.activeMatch < a_descriptor.matchCount)
			{
				const auto match =
					a_descriptor.matchByteOffsets[a_state.activeMatch];
				addRange(
					{ match, match + a_descriptor.matchByteLength },
					ImGui::GetColorU32(ImGuiCol_HeaderActive));
			}
			if (a_selection)
				addRange(*a_selection, ImGui::GetColorU32(ImGuiCol_TextSelectedBg));

			const ImVec4 fineClip{
				clipRect.Min.x,
				clipRect.Min.y,
				clipRect.Max.x,
				clipRect.Max.y
			};
			TextRuns::Paint(
				*ImGui::GetWindowDrawList(), origin, a_lineHeight, runs, fills, fineClip);
		}

		[[nodiscard]] size_t NextBoundary(
			const char* a_text, size_t a_end, size_t a_offset) noexcept
		{
			auto next = (std::min)(a_offset + 1, a_end);
			while (next < a_end && IsUtf8Continuation(a_text[next]))
				++next;
			return next;
		}

		struct Hit
		{
			size_t line{};
			// The nearest character boundary, for placing a selection end.
			size_t caret{};
			// The character under the pointer, absent past the text.
			std::optional<size_t> under;
		};

		[[nodiscard]] Hit HitTest(
			const DMUI_TextViewDescriptor& a_descriptor,
			const ImVec2& a_origin,
			float a_lineHeight,
			const ImVec2& a_mouse)
		{
			const auto* text = a_descriptor.text ?
				a_descriptor.text :
				"";
			const auto relativeY = a_mouse.y - a_origin.y;
			const auto row = relativeY <= 0.0f ?
				size_t{} :
				static_cast<size_t>(relativeY / a_lineHeight);
			Hit hit;
			hit.line = (std::min)(row, a_descriptor.lineCount - 1);
			const auto start = a_descriptor.lineOffsets[hit.line];
			const auto end = LineEnd(a_descriptor, hit.line);
			const auto x = (std::max)(a_mouse.x - a_origin.x, 0.0f);
			const char* remaining = text + start;
			const auto prefix = ImGui::GetFont()->CalcTextSizeA(
				ImGui::GetFontSize(), x, 0.0f, text + start, text + end, &remaining);
			if (remaining >= text + end)
			{
				hit.caret = end;
				return hit;
			}
			const auto at = static_cast<size_t>(remaining - text);
			const auto next = NextBoundary(text, end, at);
			hit.caret = x - prefix.x > TextWidth(text, at, next) * 0.5f ? next : at;
			if (relativeY >= 0.0f && row < a_descriptor.lineCount)
				hit.under = at;
			return hit;
		}

		[[nodiscard]] size_t OffsetOf(
			const DMUI_TextViewDescriptor& a_descriptor,
			const TextPosition& a_position) noexcept
		{
			if (a_position.line < a_descriptor.firstLineNumber)
				return 0;
			const auto line = a_position.line - a_descriptor.firstLineNumber;
			if (line >= a_descriptor.lineCount)
				return a_descriptor.textLength;
			const auto start = a_descriptor.lineOffsets[line];
			const auto end = LineEnd(a_descriptor, line);
			auto offset = start + (std::min)(a_position.column, end - start);
			while (offset < end && IsUtf8Continuation(a_descriptor.text[offset]))
				++offset;
			return offset;
		}

		[[nodiscard]] TextPosition PositionOf(
			const DMUI_TextViewDescriptor& a_descriptor,
			size_t a_offset) noexcept
		{
			const auto line = LineForOffset(a_descriptor, a_offset);
			return {
				a_descriptor.firstLineNumber + line,
				a_offset - a_descriptor.lineOffsets[line]
			};
		}

		// The child window drawn last frame, if any.
		[[nodiscard]] ImGuiWindow* FindViewWindow(const char* a_name, ImGuiID a_id)
		{
			const char* name{};
			ImFormatStringToTempBuffer(
				&name, nullptr, "%s/%s_%08X", ImGui::GetCurrentWindow()->Name, a_name, a_id);
			return ImGui::FindWindowByName(name);
		}

		[[nodiscard]] bool RevealTextOffset(
			const DMUI_TextViewDescriptor& a_descriptor,
			size_t a_offset,
			float a_lineHeight,
			float a_textHeight,
			bool a_childVisible)
		{
			const auto* text = a_descriptor.text ?
				a_descriptor.text :
				"";
			const auto line = LineForOffset(a_descriptor, a_offset);
			const auto lineStart = a_descriptor.lineOffsets[line];
			const auto horizontal =
				TextWidth(text, lineStart, a_offset);
			auto* child = ImGui::GetCurrentWindow();
			const auto origin = ImGui::GetCursorScreenPos();
			const ImRect target{
				{
					origin.x + horizontal,
					origin.y + static_cast<float>(line) * a_lineHeight
				},
				{
					origin.x + horizontal + 1.0f,
					origin.y + static_cast<float>(line) * a_lineHeight +
						a_textHeight
				}
			};
			const auto targetInViewport =
				a_childVisible && child->InnerRect.Contains(target);
			const auto targetActuallyVisible =
				targetInViewport && child->ClipRect.Contains(target);
			if (targetActuallyVisible)
				return true;

			if (a_childVisible && !targetInViewport)
			{
				(void)ImGui::ScrollToRectEx(
						child,
						target,
						ImGuiScrollFlags_KeepVisibleEdgeX |
							ImGuiScrollFlags_KeepVisibleEdgeY |
							ImGuiScrollFlags_NoScrollParent);
			}

			if (auto* parent = child->ParentWindow)
			{
				const auto parentTarget =
						targetInViewport ? target : child->Rect();
				if (!parent->ClipRect.Contains(parentTarget))
				{
						(void)ImGui::ScrollToRectEx(
							parent,
							parentTarget,
							ImGuiScrollFlags_KeepVisibleEdgeX |
								ImGuiScrollFlags_KeepVisibleEdgeY);
				}
			}
			return false;
		}
	}

	DMUI_Result DrawTextView(
		DMUI_ClientHandle a_client,
		const DMUI_TextViewDescriptor& a_descriptor,
		DMUI_TextViewState& a_state)
	{
		if (const auto validation = ValidateBasic(a_descriptor);
			validation != DMUI_RESULT_OK)
			return validation;
		if (a_state.scrollCommand >= DMUI_TEXT_VIEW_SCROLL_COUNT)
			return DMUI_RESULT_INVALID_ARGUMENT;

		auto nextState = a_state;
		if (nextState.contentRevision != a_descriptor.contentRevision ||
			nextState.matchRevision != a_descriptor.matchRevision)
		{
			nextState.contentRevision = a_descriptor.contentRevision;
			nextState.matchRevision = a_descriptor.matchRevision;
			nextState.activeMatch = DMUI_TEXT_VIEW_NO_OFFSET;
			nextState.revealByteOffset = DMUI_TEXT_VIEW_NO_OFFSET;
		}
		if (nextState.activeMatch != DMUI_TEXT_VIEW_NO_OFFSET &&
			nextState.activeMatch >= a_descriptor.matchCount)
			return DMUI_RESULT_INVALID_ARGUMENT;

		const auto* text = a_descriptor.text ?
			a_descriptor.text :
			"";
		if (nextState.revealByteOffset != DMUI_TEXT_VIEW_NO_OFFSET &&
			!Support::IsUtf8Boundary(
				text,
				a_descriptor.textLength,
				nextState.revealByteOffset))
			return DMUI_RESULT_INVALID_ARGUMENT;

		const Theme::FontGuard font{ Theme::FontRole::kMonospace };
		const ClientIdScope clientScope{ a_client };
		const auto viewerId = ImGui::GetID(a_descriptor.id);
		auto& cache = CacheFor(viewerId);
		const auto contentChanged =
			!cache.contentInitialized ||
			cache.contentRevision != a_descriptor.contentRevision ||
			cache.textLength != a_descriptor.textLength ||
			cache.lineCount != a_descriptor.lineCount;
		const auto fontChanged =
			!cache.contentInitialized ||
			cache.font != ImGui::GetFont() ||
			cache.fontSize != ImGui::GetFontSize();
		const auto matchesChanged =
			!cache.matchesInitialized ||
			contentChanged ||
			cache.matchRevision != a_descriptor.matchRevision ||
			cache.matchCount != a_descriptor.matchCount ||
			cache.matchByteLength != a_descriptor.matchByteLength;
		const auto spansChanged =
			!cache.spansInitialized ||
			contentChanged ||
			cache.spanRevision != a_descriptor.spanRevision ||
			cache.spanCount != a_descriptor.spanCount;

		if (contentChanged)
		{
			if (const auto validation = ValidateContent(a_descriptor);
				validation != DMUI_RESULT_OK)
				return validation;
		}
		auto contentWidth = cache.contentWidth;
		if (contentChanged || fontChanged)
			contentWidth = MeasureContentWidth(a_descriptor);
		if (matchesChanged)
		{
			if (const auto validation = ValidateMatches(a_descriptor);
				validation != DMUI_RESULT_OK)
				return validation;
		}
		if (spansChanged)
		{
			if (const auto validation = TextRuns::Validate(
					TextRuns::Consumer::kTextView,
					{ text, a_descriptor.textLength },
					{ a_descriptor.spans, a_descriptor.spanCount });
				validation != DMUI_RESULT_OK)
				return validation;
		}

		if (contentChanged || fontChanged)
		{
			cache.contentRevision = a_descriptor.contentRevision;
			cache.textLength = a_descriptor.textLength;
			cache.lineCount = a_descriptor.lineCount;
			cache.font = ImGui::GetFont();
			cache.fontSize = ImGui::GetFontSize();
			cache.contentWidth = contentWidth;
			cache.contentInitialized = true;
		}
		if (matchesChanged)
		{
			cache.matchRevision = a_descriptor.matchRevision;
			cache.matchCount = a_descriptor.matchCount;
			cache.matchByteLength = a_descriptor.matchByteLength;
			cache.matchesInitialized = true;
		}
		if (spansChanged)
		{
			cache.spanRevision = a_descriptor.spanRevision;
			cache.spanCount = a_descriptor.spanCount;
			cache.spansInitialized = true;
		}

		const auto textHeight = ImGui::GetTextLineHeight();
		const auto lineHeight = ImGui::GetTextLineHeightWithSpacing();
		const auto followTail =
			(a_descriptor.flags & DMUI_TEXT_VIEW_FOLLOW_TAIL) != 0;
		const auto selectable =
			(a_descriptor.flags & DMUI_TEXT_VIEW_SELECTABLE) != 0;

		// A pending scroll target (wheel, scrollbar) is where the reader is.
		const auto* previous = FindViewWindow(a_descriptor.id, viewerId);
		const auto position = !previous ? 0.0f :
			previous->ScrollTarget.y != FLT_MAX ?
				previous->ScrollTarget.y : previous->Scroll.y;
		const auto atEnd = !previous || position >= previous->ScrollMax.y - 1.0f;
		const auto command = nextState.scrollCommand;
		// An explicit command or a pending reveal outranks following.
		const auto pinEnd = command == DMUI_TEXT_VIEW_SCROLL_BOTTOM ||
			(followTail && atEnd && command == DMUI_TEXT_VIEW_SCROLL_NONE &&
				nextState.revealByteOffset == DMUI_TEXT_VIEW_NO_OFFSET);
		if (previous)
		{
			std::optional<float> target;
			if (a_descriptor.firstLineNumber != cache.firstLineNumber)
			{
				// Trimmed or prepended lines move the reader's line, not the reader.
				const auto shift =
					static_cast<double>(a_descriptor.firstLineNumber) -
					static_cast<double>(cache.firstLineNumber);
				target = static_cast<float>((std::max)(
					0.0, position - shift * lineHeight));
			}
			const auto page = (std::max)(
				previous->InnerRect.GetHeight() - lineHeight, lineHeight);
			if (command == DMUI_TEXT_VIEW_SCROLL_TOP)
				target = 0.0f;
			else if (command == DMUI_TEXT_VIEW_SCROLL_PAGE_UP)
				target = (std::max)(position - page, 0.0f);
			else if (command == DMUI_TEXT_VIEW_SCROLL_PAGE_DOWN)
				target = (std::min)(position + page, previous->ScrollMax.y);
			if (target)
				ImGui::SetNextWindowScroll({ -1.0f, *target });
		}
		cache.firstLineNumber = a_descriptor.firstLineNumber;

		const ChildScope child{
			a_descriptor.id,
			{ a_descriptor.viewport.x, a_descriptor.viewport.y },
			ImGuiChildFlags_Borders,
			ImGuiWindowFlags_HorizontalScrollbar
		};
		bool revealConsumed{};
		if (nextState.revealByteOffset != DMUI_TEXT_VIEW_NO_OFFSET)
		{
			revealConsumed = RevealTextOffset(
				a_descriptor,
				nextState.revealByteOffset,
				lineHeight,
				textHeight,
				child.IsVisible());
		}

		uint32_t events{};
		size_t eventSpan{ DMUI_TEXT_VIEW_NO_OFFSET };
		bool atBottom{ atEnd };
		if (child.IsVisible())
		{
			const auto origin = ImGui::GetCursorScreenPos();
			std::optional<ByteRange> selection;
			auto& selected = cache.selection;
			if (selectable || a_descriptor.spanCount != 0)
			{
				// One item over the content claims the pointer and the focus.
				ImGui::InvisibleButton(
					"##TextViewInput",
					{ (std::max)(cache.contentWidth, 1.0f),
						static_cast<float>(a_descriptor.lineCount) * lineHeight },
					ImGuiButtonFlags_MouseButtonLeft);
				const auto input = ImGui::GetItemID();
				const auto hovered = ImGui::IsItemHovered();
				const auto held = ImGui::IsItemActive();
				const auto released = ImGui::IsItemDeactivated();
				ImGui::SetCursorScreenPos(origin);

				std::optional<size_t> hoveredSpan;
				std::optional<Hit> hit;
				if (hovered || held)
				{
					hit = HitTest(a_descriptor, origin, lineHeight, ImGui::GetMousePos());
					if (hovered && hit->under)
					{
						hoveredSpan = SpanAt(a_descriptor, *hit->under);
						if (hoveredSpan &&
							(a_descriptor.spans[*hoveredSpan].flags &
								DMUI_TEXT_SPAN_INTERACTIVE) == 0)
							hoveredSpan.reset();
					}
				}
				if (hoveredSpan)
				{
					events |= DMUI_TEXT_VIEW_EVENT_SPAN_HOVERED;
					eventSpan = *hoveredSpan;
					ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				}
				if (ImGui::IsItemActivated())
				{
					cache.pressedSpan = hoveredSpan.value_or(DMUI_TEXT_VIEW_NO_OFFSET);
					if (selectable && hit)
					{
						selected.anchor = selected.focus =
							PositionOf(a_descriptor, hit->caret);
						selected.dragging = true;
					}
				}
				if (held && selected.dragging && hit)
				{
					selected.focus = PositionOf(a_descriptor, hit->caret);
					const auto& window = *ImGui::GetCurrentWindow();
					const auto mouseY = ImGui::GetMousePos().y;
					const auto overshoot = mouseY < window.InnerRect.Min.y ?
						mouseY - window.InnerRect.Min.y :
						(std::max)(mouseY - window.InnerRect.Max.y, 0.0f);
					if (overshoot != 0.0f)
						ImGui::SetScrollY(
							window.Scroll.y + overshoot * ImGui::GetIO().DeltaTime * 10.0f);
				}
				if (released)
				{
					selected.dragging = false;
					const auto& io = ImGui::GetIO();
					const auto moved = io.MouseDragMaxDistanceSqr[0] >=
						io.MouseDragThreshold * io.MouseDragThreshold;
					if (!moved && hoveredSpan &&
						*hoveredSpan == cache.pressedSpan)
					{
						events |= DMUI_TEXT_VIEW_EVENT_SPAN_CLICKED;
						eventSpan = *hoveredSpan;
					}
					cache.pressedSpan = DMUI_TEXT_VIEW_NO_OFFSET;
				}
				if (selectable && selected.anchor != selected.focus)
				{
					auto begin = OffsetOf(a_descriptor, selected.anchor);
					auto end = OffsetOf(a_descriptor, selected.focus);
					if (begin > end)
						std::swap(begin, end);
					if (begin < end)
						selection = ByteRange{ begin, end };
				}
				static const auto copyChord = ParseShortcutChord("Ctrl+C");
				static const auto copyAlt = ParseShortcutChord("Ctrl+Insert");
				if (selection &&
					(ShortcutPressed(*copyChord, 0, input) ||
						ShortcutPressed(*copyAlt, 0, input)))
					ImGui::SetClipboardText(
						std::string{ text + selection->begin, text + selection->end }.c_str());
			}

			thread_local LineScratch scratch;
			scratch.normal = TextRuns::Resolve(TextRuns::Consumer::kTextView, nullptr);
			ImGuiListClipper clipper;
			clipper.Begin(
				static_cast<int>(a_descriptor.lineCount),
				lineHeight);
			while (clipper.Step())
			{
				for (int line = clipper.DisplayStart;
					 line < clipper.DisplayEnd;
					 ++line)
				{
					DrawLine(
						a_descriptor,
						nextState,
						selection,
						static_cast<size_t>(line),
						textHeight,
						scratch);
					ImGui::Dummy({
						(std::max)(cache.contentWidth, 1.0f),
						textHeight
					});
				}
			}

			auto& window = *ImGui::GetCurrentWindow();
			const auto contentHeight =
				window.DC.CursorMaxPos.y - window.DC.CursorStartPos.y;
			const auto scrollMax = (std::max)(
				contentHeight + window.WindowPadding.y * 2.0f -
					window.InnerRect.GetHeight(),
				0.0f);
			atBottom = window.Scroll.y >= scrollMax - 1.0f;
			if (pinEnd)
			{
				ImGui::SetScrollY(scrollMax);
				atBottom = true;
			}
		}

		if (revealConsumed)
			nextState.revealByteOffset = DMUI_TEXT_VIEW_NO_OFFSET;
		nextState.scrollCommand = DMUI_TEXT_VIEW_SCROLL_NONE;
		nextState.atBottom = atBottom ? 1u : 0u;
		nextState.events = events;
		nextState.spanIndex = eventSpan;
		nextState.userId = eventSpan == DMUI_TEXT_VIEW_NO_OFFSET ?
			0 : a_descriptor.spans[eventSpan].userId;
		a_state = nextState;
		return DMUI_RESULT_OK;
	}
}
