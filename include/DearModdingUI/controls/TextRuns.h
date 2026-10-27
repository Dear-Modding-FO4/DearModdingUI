#pragma once

#include <DearModdingUI/API.h>

#include <imgui/imgui.h>

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace DearModdingUI::TextRuns
{
	enum class Consumer : uint8_t
	{
		// Proportional layout in the active font; accepts roles, not INTERACTIVE.
		kText,
		// Monospace role with its bold and italic faces; accepts INTERACTIVE.
		kTextView,
		// Monospace role; accepts neither roles nor INTERACTIVE.
		kTextField
	};

	[[nodiscard]] DMUI_Result Validate(
		Consumer a_consumer,
		std::string_view a_text,
		std::span<const DMUI_TextSpan> a_spans) noexcept;

	struct Style
	{
		ImFont* font{};
		float size{};
		ImU32 color{};
		// Zero for no background.
		ImU32 background{};
		bool underline{};
		bool strikethrough{};
	};

	// The caller has already set a monospace consumer's font.
	[[nodiscard]] Style Resolve(
		Consumer a_consumer,
		const DMUI_TextSpan* a_span) noexcept;

	struct Run
	{
		const char* begin;
		const char* end;
		// From the line origin.
		float x;
		float width;
		// Text top below the line top.
		float textY;
		Style style;
	};

	// Widths come from the active font, so monospace measures its regular face.
	void AppendRuns(
		Consumer a_consumer,
		const char* a_text,
		std::span<const DMUI_TextSpan> a_spans,
		size_t a_begin,
		size_t a_end,
		float a_x,
		const Style& a_normal,
		std::vector<Run>& a_runs);

	// A highlight under the glyphs, such as a search match or a selection.
	struct Fill
	{
		float x0;
		float x1;
		ImU32 color;
	};

	// a_clip is min x, min y, max x, max y.
	void Paint(
		ImDrawList& a_drawList,
		ImVec2 a_origin,
		float a_lineHeight,
		std::span<const Run> a_runs,
		std::span<const Fill> a_fills,
		const ImVec4& a_clip) noexcept;
}
