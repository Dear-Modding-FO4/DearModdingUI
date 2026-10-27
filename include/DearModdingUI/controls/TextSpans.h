#pragma once

#include <DearModdingUI/API.h>
#include <DearModdingUI/StyledText.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace DearModdingUI
{
	inline constexpr uint32_t kMaxTextSpans{ 4096u };
	inline constexpr size_t kMaxStyledTextBytes{ 1u << 20 };

	[[nodiscard]] bool ValidTextOptions(const DMUI_TextOptions& a_options) noexcept;

	// Checks the options, text, and spans against the text operation.
	[[nodiscard]] DMUI_Result ValidateTextSpans(
		std::string_view a_text,
		std::span<const DMUI_TextSpan> a_spans,
		const DMUI_TextOptions& a_options) noexcept;

	// Lays the text out inline as one ImGui item. The input must already be valid.
	DMUI_Result DrawTextSpans(
		std::string_view a_text,
		std::span<const DMUI_TextSpan> a_spans,
		const DMUI_TextOptions& a_options) noexcept;

	// One style over the whole text.
	inline DMUI_Result DrawTextSpans(
		std::string_view a_text,
		const dmui::TextStyle& a_style,
		dmui::TextOptions a_options = {}) noexcept
	{
		const auto span = a_style.Span(0, a_text.size());
		return DrawTextSpans(a_text, std::span{ &span, 1 }, a_options.Abi());
	}

	inline DMUI_Result DrawBulletText(
		std::string_view a_text,
		dmui::TextTone a_tone = dmui::TextTone::kInherit,
		bool a_wrap = true) noexcept
	{
		return DrawTextSpans(
			a_text, dmui::TextStyle{}.Tone(a_tone), { .wrap = a_wrap, .bullet = true });
	}
}
