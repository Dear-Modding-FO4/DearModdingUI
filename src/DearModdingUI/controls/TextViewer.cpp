#include <DearModdingUI/controls/TextViewer.h>

#include <DearModdingUI/presentation/Theme.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace DearModdingUI
{
	namespace
	{
		struct TextViewCache
		{
			uint64_t contentRevision{};
			uint64_t matchRevision{};
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

		[[nodiscard]] bool IsUtf8Boundary(
			const char* a_text,
			size_t a_textLength,
			size_t a_offset) noexcept
		{
			return a_offset <= a_textLength &&
				(a_offset == a_textLength ||
					!IsUtf8Continuation(a_text[a_offset]));
		}

		[[nodiscard]] bool IsValidUtf8(
			const char* a_text,
			size_t a_textLength) noexcept
		{
			size_t offset = 0;
			while (offset < a_textLength)
			{
				const auto lead = static_cast<unsigned char>(a_text[offset]);
				size_t length{};
				uint32_t minimum{};
				uint32_t codePoint{};
				if (lead <= 0x7Fu)
				{
					if (lead == 0)
						return false;
					++offset;
					continue;
				}
				if ((lead & 0xE0u) == 0xC0u)
				{
					length = 2;
					minimum = 0x80u;
					codePoint = lead & 0x1Fu;
				}
				else if ((lead & 0xF0u) == 0xE0u)
				{
					length = 3;
					minimum = 0x800u;
					codePoint = lead & 0x0Fu;
				}
				else if ((lead & 0xF8u) == 0xF0u)
				{
					length = 4;
					minimum = 0x10000u;
					codePoint = lead & 0x07u;
				}
				else
				{
					return false;
				}
				if (offset + length > a_textLength)
					return false;
				for (size_t index = 1; index < length; ++index)
				{
					const auto continuation =
						static_cast<unsigned char>(a_text[offset + index]);
					if ((continuation & 0xC0u) != 0x80u)
						return false;
					codePoint =
						(codePoint << 6u) | (continuation & 0x3Fu);
				}
				if (codePoint < minimum ||
					codePoint > 0x10FFFFu ||
					(codePoint >= 0xD800u && codePoint <= 0xDFFFu))
					return false;
				offset += length;
			}
			return true;
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
			if (!IsValidUtf8(text, a_descriptor.textLength))
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
				if (!IsUtf8Boundary(text, a_descriptor.textLength, start) ||
					!IsUtf8Boundary(text, a_descriptor.textLength, end))
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

		void AddHighlight(
			ImDrawList& a_drawList,
			const ImRect& a_clipRect,
			const ImVec2& a_origin,
			float a_x0,
			float a_x1,
			float a_lineHeight,
			ImU32 a_color)
		{
			const auto left =
				(std::max)(a_origin.x + a_x0, a_clipRect.Min.x);
			const auto right =
				(std::min)(a_origin.x + a_x1, a_clipRect.Max.x);
			const auto top =
				(std::max)(a_origin.y, a_clipRect.Min.y);
			const auto bottom =
				(std::min)(a_origin.y + a_lineHeight, a_clipRect.Max.y);
			if (left < right && top < bottom)
			{
				a_drawList.AddRectFilled(
					{ left, top },
					{ right, bottom },
					a_color);
			}
		}

		void DrawInactiveHighlights(
			ImDrawList& a_drawList,
			const ImRect& a_clipRect,
			const ImVec2& a_origin,
			const char* a_text,
			const VisibleText& a_visible,
			const DMUI_TextViewDescriptor& a_descriptor,
			size_t a_lineStart,
			size_t a_lineEnd,
			size_t a_activeMatch,
			float a_lineHeight,
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
				AddHighlight(
					a_drawList,
					a_clipRect,
					a_origin,
					startX,
					x,
					a_lineHeight,
					a_color);
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

		void DrawLine(
			const DMUI_TextViewDescriptor& a_descriptor,
			const DMUI_TextViewState& a_state,
			size_t a_line,
			float a_lineHeight)
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

			auto* drawList = ImGui::GetWindowDrawList();
			DrawInactiveHighlights(
				*drawList,
				clipRect,
				origin,
				text,
				visible,
				a_descriptor,
				lineStart,
				lineEnd,
				a_state.activeMatch,
				a_lineHeight,
				ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
			if (a_state.activeMatch < a_descriptor.matchCount)
			{
				const auto match =
					a_descriptor.matchByteOffsets[a_state.activeMatch];
				auto activeRange = ByteRange{
					(std::max)(match, lineStart),
					(std::min)(
						match + a_descriptor.matchByteLength,
						lineEnd)
				};
				activeRange.begin =
					(std::max)(activeRange.begin, visible.begin);
				activeRange.end =
					(std::min)(activeRange.end, visible.end);
				if (activeRange.begin < activeRange.end)
				{
					const auto startX =
						visible.beginX +
						TextWidth(
							text,
							visible.begin,
							activeRange.begin);
					const auto endX =
						startX +
						TextWidth(
							text,
							activeRange.begin,
							activeRange.end);
					AddHighlight(
						*drawList,
						clipRect,
						origin,
						startX,
						endX,
						a_lineHeight,
						ImGui::GetColorU32(ImGuiCol_HeaderActive));
				}
			}

			const ImVec4 fineClip{
				clipRect.Min.x,
				clipRect.Min.y,
				clipRect.Max.x,
				clipRect.Max.y
			};
			drawList->AddText(
				ImGui::GetFont(),
				ImGui::GetFontSize(),
				{ origin.x + visible.beginX, origin.y },
				ImGui::GetColorU32(ImGuiCol_Text),
				text + visible.begin,
				text + visible.end,
				0.0f,
				&fineClip);
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
			!IsUtf8Boundary(
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

		const ChildScope child{
			a_descriptor.id,
			{ a_descriptor.viewport.x, a_descriptor.viewport.y },
			ImGuiChildFlags_Borders,
			ImGuiWindowFlags_HorizontalScrollbar
		};
		bool revealConsumed{};
		const auto textHeight = ImGui::GetTextLineHeight();
		const auto lineHeight = ImGui::GetTextLineHeightWithSpacing();
		if (contentChanged &&
			nextState.revealByteOffset == DMUI_TEXT_VIEW_NO_OFFSET)
		{
			ImGui::SetScrollX(0.0f);
			ImGui::SetScrollY(0.0f);
		}
		if (nextState.revealByteOffset != DMUI_TEXT_VIEW_NO_OFFSET)
		{
			revealConsumed = RevealTextOffset(
				a_descriptor,
				nextState.revealByteOffset,
				lineHeight,
				textHeight,
				child.IsVisible());
		}
		if (child.IsVisible())
		{
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
						static_cast<size_t>(line),
						textHeight);
					ImGui::Dummy({
						(std::max)(cache.contentWidth, 1.0f),
						textHeight
					});
				}
			}
		}

		if (revealConsumed)
			nextState.revealByteOffset = DMUI_TEXT_VIEW_NO_OFFSET;
		a_state = nextState;
		return DMUI_RESULT_OK;
	}
}
