#include <DearModdingUI/controls/TextRuns.h>

#include <DearModdingUI/presentation/Theme.h>
#include <Support/Utf8.h>

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace DearModdingUI::TextRuns
{
	namespace
	{
		constexpr uint32_t kKnownFlags{
			DMUI_TEXT_SPAN_BOLD | DMUI_TEXT_SPAN_ITALIC |
			DMUI_TEXT_SPAN_UNDERLINE | DMUI_TEXT_SPAN_STRIKETHROUGH |
			DMUI_TEXT_SPAN_INTERACTIVE | DMUI_TEXT_SPAN_HAS_ROLE |
			DMUI_TEXT_SPAN_CUSTOM_FOREGROUND | DMUI_TEXT_SPAN_CUSTOM_BACKGROUND
		};
		constexpr uint32_t kFaceFlags{ DMUI_TEXT_SPAN_BOLD | DMUI_TEXT_SPAN_ITALIC };
		// Tone backgrounds reuse text colors, so they tint rather than cover.
		constexpr float kToneBackgroundAlpha{ 0.35f };

		[[nodiscard]] constexpr uint32_t ForbiddenFlags(Consumer a_consumer) noexcept
		{
			switch (a_consumer)
			{
			case Consumer::kText:
				return DMUI_TEXT_SPAN_INTERACTIVE;
			case Consumer::kTextView:
				return DMUI_TEXT_SPAN_HAS_ROLE;
			default:
				return DMUI_TEXT_SPAN_INTERACTIVE | DMUI_TEXT_SPAN_HAS_ROLE;
			}
		}

		[[nodiscard]] bool Finite(const DMUI_Vec4& a_value) noexcept
		{
			return std::isfinite(a_value.x) && std::isfinite(a_value.y) &&
				std::isfinite(a_value.z) && std::isfinite(a_value.w);
		}

		[[nodiscard]] Theme::FontStyle FontStyleOf(uint32_t a_flags) noexcept
		{
			const auto bold = (a_flags & DMUI_TEXT_SPAN_BOLD) != 0;
			const auto italic = (a_flags & DMUI_TEXT_SPAN_ITALIC) != 0;
			if (bold && italic)
				return Theme::FontStyle::kBoldItalic;
			if (bold)
				return Theme::FontStyle::kBold;
			return italic ? Theme::FontStyle::kItalic : Theme::FontStyle::kRegular;
		}

		// Other advances would drift from the caret and highlights.
		[[nodiscard]] bool SameAdvances(
			ImFont* a_left, ImFont* a_right, float a_size) noexcept
		{
			constexpr char kProbe[]{ "iIlW0M.," };
			return std::abs(
				a_left->CalcTextSizeA(a_size, FLT_MAX, 0.0f, kProbe).x -
				a_right->CalcTextSizeA(a_size, FLT_MAX, 0.0f, kProbe).x) < 0.01f;
		}

		[[nodiscard]] ImU32 ToneColor(DMUI_TextTone a_tone, float a_alpha) noexcept
		{
			auto color = Theme::TextColor(static_cast<dmui::TextTone>(a_tone));
			color.w *= a_alpha;
			return ImGui::GetColorU32(color);
		}

		[[nodiscard]] ImU32 Packed(const DMUI_Vec4& a_color) noexcept
		{
			return ImGui::GetColorU32(
				ImVec4{ a_color.x, a_color.y, a_color.z, a_color.w });
		}

		void FillRect(
			ImDrawList& a_drawList, const ImVec4& a_clip, float a_x0, float a_y0,
			float a_x1, float a_y1, ImU32 a_color) noexcept
		{
			const auto left = (std::max)(a_x0, a_clip.x);
			const auto top = (std::max)(a_y0, a_clip.y);
			const auto right = (std::min)(a_x1, a_clip.z);
			const auto bottom = (std::min)(a_y1, a_clip.w);
			if (left < right && top < bottom)
				a_drawList.AddRectFilled({ left, top }, { right, bottom }, a_color);
		}
	}

	DMUI_Result Validate(
		Consumer a_consumer,
		std::string_view a_text,
		std::span<const DMUI_TextSpan> a_spans) noexcept
	{
		const auto forbidden = ForbiddenFlags(a_consumer);
		size_t previousEnd{};
		for (const auto& span : a_spans)
		{
			if ((span.flags & ~kKnownFlags) != 0 || (span.flags & forbidden) != 0 ||
				span.foreground >= DMUI_TEXT_TONE_COUNT ||
				span.background >= DMUI_TEXT_TONE_COUNT ||
				((span.flags & DMUI_TEXT_SPAN_HAS_ROLE) &&
					span.role >= DMUI_FONT_ROLE_COUNT) ||
				((span.flags & DMUI_TEXT_SPAN_CUSTOM_FOREGROUND) &&
					!Finite(span.foregroundColor)) ||
				((span.flags & DMUI_TEXT_SPAN_CUSTOM_BACKGROUND) &&
					!Finite(span.backgroundColor)) ||
				span.offset < previousEnd || span.offset > a_text.size() ||
				span.length > a_text.size() - span.offset ||
				!Support::IsUtf8Boundary(a_text.data(), a_text.size(), span.offset) ||
				!Support::IsUtf8Boundary(
					a_text.data(), a_text.size(), span.offset + span.length))
				return DMUI_RESULT_INVALID_ARGUMENT;
			previousEnd = span.offset + span.length;
		}
		return DMUI_RESULT_OK;
	}

	Style Resolve(Consumer a_consumer, const DMUI_TextSpan* a_span) noexcept
	{
		Style style{
			ImGui::GetFont(), ImGui::GetFontSize(), ImGui::GetColorU32(ImGuiCol_Text)
		};
		if (!a_span)
			return style;
		const auto flags = a_span->flags;
		const auto monospace = a_consumer != Consumer::kText;
		// Bold or italic without a role styles the body face.
		const auto role = monospace ? Theme::FontRole::kMonospace :
			(flags & DMUI_TEXT_SPAN_HAS_ROLE) ?
				static_cast<Theme::FontRole>(a_span->role) : Theme::FontRole::kBody;
		const auto restyle = (flags & kFaceFlags) != 0 ||
			(!monospace && (flags & DMUI_TEXT_SPAN_HAS_ROLE) != 0);
		if (restyle)
		{
			const Theme::FontGuard face{ role, FontStyleOf(flags) };
			style.font = ImGui::GetFont();
			style.size = ImGui::GetFontSize();
			if (monospace)
			{
				const Theme::FontGuard regular{ role, Theme::FontStyle::kRegular };
				if (!SameAdvances(style.font, ImGui::GetFont(), style.size))
					style.font = ImGui::GetFont();
			}
		}
		if (flags & DMUI_TEXT_SPAN_CUSTOM_FOREGROUND)
			style.color = Packed(a_span->foregroundColor);
		else if (a_span->foreground != DMUI_TEXT_TONE_INHERIT)
			style.color = ToneColor(a_span->foreground, 1.0f);
		if (flags & DMUI_TEXT_SPAN_CUSTOM_BACKGROUND)
			style.background = Packed(a_span->backgroundColor);
		else if (a_span->background != DMUI_TEXT_TONE_INHERIT)
			style.background = ToneColor(a_span->background, kToneBackgroundAlpha);
		style.underline = (flags & DMUI_TEXT_SPAN_UNDERLINE) != 0;
		style.strikethrough = (flags & DMUI_TEXT_SPAN_STRIKETHROUGH) != 0;
		return style;
	}

	void AppendRuns(
		Consumer a_consumer,
		const char* a_text,
		std::span<const DMUI_TextSpan> a_spans,
		size_t a_begin,
		size_t a_end,
		float a_x,
		const Style& a_normal,
		std::vector<Run>& a_runs)
	{
		auto cursor = a_begin;
		auto x = a_x;
		const auto emit = [&](size_t a_from, size_t a_to, const DMUI_TextSpan* a_span) {
			const auto width = ImGui::CalcTextSize(a_text + a_from, a_text + a_to, false).x;
			a_runs.push_back({
				a_text + a_from, a_text + a_to, x, width, 0.0f,
				a_span ? Resolve(a_consumer, a_span) : a_normal });
			x += width;
			cursor = a_to;
		};
		// Spans are sorted, so the first one ending after the range starts it.
		auto index = static_cast<size_t>(std::partition_point(
			a_spans.begin(), a_spans.end(), [a_begin](const DMUI_TextSpan& a_span) {
				return a_span.offset + a_span.length <= a_begin;
			}) - a_spans.begin());
		for (; index < a_spans.size() && a_spans[index].offset < a_end; ++index)
		{
			const auto& span = a_spans[index];
			const auto from = (std::max)(span.offset, cursor);
			const auto to = (std::min)(span.offset + span.length, a_end);
			if (from > cursor)
				emit(cursor, from, nullptr);
			if (to > from)
				emit(from, to, &span);
		}
		if (cursor < a_end)
			emit(cursor, a_end, nullptr);
	}

	void Paint(
		ImDrawList& a_drawList,
		ImVec2 a_origin,
		float a_lineHeight,
		std::span<const Run> a_runs,
		std::span<const Fill> a_fills,
		const ImVec4& a_clip) noexcept
	{
		const auto bottom = a_origin.y + a_lineHeight;
		for (const auto& run : a_runs)
		{
			if (run.style.background != 0)
				FillRect(
					a_drawList, a_clip, a_origin.x + run.x, a_origin.y,
					a_origin.x + run.x + run.width, bottom, run.style.background);
		}
		for (const auto& fill : a_fills)
			FillRect(
				a_drawList, a_clip, a_origin.x + fill.x0, a_origin.y,
				a_origin.x + fill.x1, bottom, fill.color);
		for (const auto& run : a_runs)
		{
			if (run.begin == run.end)
				continue;
			a_drawList.AddText(
				run.style.font, run.style.size,
				{ a_origin.x + run.x, a_origin.y + run.textY },
				run.style.color, run.begin, run.end, 0.0f, &a_clip);
		}
		for (const auto& run : a_runs)
		{
			if (!run.style.underline && !run.style.strikethrough)
				continue;
			const auto size = run.style.size;
			const auto thickness = (std::max)(1.0f, std::round(size / 14.0f));
			const auto top = a_origin.y + run.textY;
			const auto ascent = run.style.font->GetFontBaked(size)->Ascent;
			const auto left = a_origin.x + run.x;
			const auto right = left + run.width;
			if (run.style.underline)
			{
				const auto y = std::round(top + ascent + thickness);
				FillRect(
					a_drawList, a_clip, left, y, right, y + thickness, run.style.color);
			}
			if (run.style.strikethrough)
			{
				const auto y = std::round(top + ascent * 0.62f);
				FillRect(
					a_drawList, a_clip, left, y, right, y + thickness, run.style.color);
			}
		}
	}
}
