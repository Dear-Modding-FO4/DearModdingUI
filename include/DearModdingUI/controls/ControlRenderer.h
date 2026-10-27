#pragma once

#include <DearModdingUI/API.h>
#include <DearModdingUI/Button.h>
#include <DearModdingUI/presentation/ThemeSpec.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace DearModdingUI
{
	// What a button shows; label is plain UTF-8, glyph a Phosphor codepoint.
	struct ButtonContent
	{
		const char* label{};
		char32_t glyph{};
		// Zero draws the glyph at the font size.
		float glyphSize{};
	};

	struct ButtonVisualState
	{
		ImGuiID id{};
		bool hovered{};
		bool pressed{};
		bool focused{};
		// A toggled-on button, such as the current page's entry.
		bool active{};
	};

	// Style entries a renderer pushed; the primitive pops them with PopStyle.
	struct StyleScope
	{
		int colors{};
		int vars{};
	};

	inline void PopStyle(const StyleScope& a_scope) noexcept
	{
		ImGui::PopStyleVar(a_scope.vars);
		ImGui::PopStyleColor(a_scope.colors);
	}

	struct TextFieldFrame
	{
		StyleScope style;
		// Where the field started and the frame height before the style applied.
		ImVec2 origin;
		float height{};
	};

	enum class SectionRuleStyle : uint32_t
	{
		kSection,
		kSubordinate
	};

	struct SectionHeaderContent
	{
		const char* title{};
		// Zero draws no icon.
		char32_t glyph{};
		// A collapsed section dims its header.
		bool minimized{};
		SectionRuleStyle ruleStyle{ SectionRuleStyle::kSection };
	};

	struct SectionPalette
	{
		ImVec4 text;
		ImVec4 hovered;
	};

	struct SectionHeaderMetrics
	{
		float contentWidth{};
		float height{};
	};

	// What a numeric input edits; the primitive owns bounds, steps, and the result.
	struct NumericContent
	{
		DMUI_NumericPresentation presentation{};
		ImGuiDataType dataType{};
		// Native storage of dataType, edited in place.
		void* value{};
		// Null where unbounded.
		const void* minimum{};
		const void* maximum{};
		const char* format{};
		float speed{};
		// Holds typed values inside the bounds too.
		bool clamp{};
	};

	struct KeyCaptureVisual
	{
		// A binding or an active capture reads at full strength, otherwise muted.
		bool strong{};
		bool capturing{};
		// A conflict or invalid binding tints the text with the warning severity.
		bool warned{};
	};

	// Owns how the primitives look; the primitives own ids, input, and state.
	// New looks implement this and are chosen by Theme::ControlRendererKind.
	class ControlRenderer
	{
	public:
		virtual ~ControlRenderer() = default;

		[[nodiscard]] virtual ImVec2 MeasureButton(
			const ButtonContent& a_content,
			dmui::ButtonStyle a_style) const noexcept = 0;
		// Paints into the current window at a_bounds.
		virtual void DrawButton(
			const ImRect& a_bounds,
			const ButtonContent& a_content,
			dmui::ButtonStyle a_style,
			const ButtonVisualState& a_state) const noexcept = 0;

		[[nodiscard]] virtual float MeasureFieldLabel(
			const char* a_label,
			const char* a_description,
			float a_wrapWidth) const noexcept = 0;
		// Draws at the cursor and leaves it below the block.
		virtual void DrawFieldLabel(
			const ImVec2& a_position,
			float a_width,
			const char* a_label,
			const char* a_description) const noexcept = 0;

		// Pushes the frame look of a text field; pair with EndTextField.
		[[nodiscard]] virtual TextFieldFrame BeginTextField(
			DMUI_TextFieldStyle a_style) const noexcept = 0;
		// Pops the frame look and paints affordances such as the search icon.
		virtual void EndTextField(
			const TextFieldFrame& a_frame,
			DMUI_TextFieldStyle a_style) const noexcept = 0;

		[[nodiscard]] virtual SectionPalette SectionColors(
			bool a_minimized) const noexcept = 0;
		[[nodiscard]] virtual SectionHeaderMetrics MeasureSectionHeader(
			const SectionHeaderContent& a_content) const noexcept = 0;
		// Paints the icon and title centered between two rules; a header without
		// either is a single rule.
		virtual void DrawSectionHeader(
			const ImVec2& a_position,
			float a_width,
			const SectionHeaderContent& a_content,
			bool a_hovered) const noexcept = 0;
		// Paints the rules beside a header row whose text another control drew.
		virtual void DrawSectionRules(
			const ImRect& a_row,
			float a_textMinX,
			float a_textMaxX,
			const SectionHeaderContent& a_content,
			bool a_hovered) const noexcept = 0;
		// Pushes the look of a boxed region; pair with PopStyle.
		[[nodiscard]] virtual StyleScope PushBoxStyle(
			bool a_background) const noexcept = 0;

		// The closed frame with its arrow; true while the popup is open.
		[[nodiscard]] virtual bool BeginChoice(
			const char* a_id,
			const char* a_preview) const noexcept = 0;
		virtual void EndChoice() const noexcept = 0;
		// One option row inside the popup; true when it is pressed.
		[[nodiscard]] virtual bool DrawChoiceRow(
			const char* a_label,
			bool a_selected) const noexcept = 0;

		// The checkbox frame and mark; true on the frame it is toggled.
		// A non-empty label is drawn verbatim and toggles the box too.
		[[nodiscard]] virtual bool DrawToggle(
			bool& a_value,
			const char* a_label) const noexcept = 0;

		// Draws one item for the presentation; true when it edited the value.
		[[nodiscard]] virtual bool DrawNumeric(
			const NumericContent& a_content) const noexcept = 0;

		// The inline color fields with their picker; true when the color changed.
		[[nodiscard]] virtual bool DrawColorEdit(
			ImVec4& a_color,
			bool a_alpha) const noexcept = 0;
		// One preset swatch; true when it is pressed.
		[[nodiscard]] virtual bool DrawColorSwatch(
			const char* a_id,
			const ImVec4& a_color,
			bool a_alpha,
			const ImVec2& a_size) const noexcept = 0;

		// Pushes the frame look of the binding button; pair with PopStyle.
		[[nodiscard]] virtual StyleScope PushKeyCaptureStyle(
			const KeyCaptureVisual& a_visual) const noexcept = 0;
		// Marks a field that is waiting for input.
		virtual void DrawKeyCaptureCue(const ImRect& a_bounds) const noexcept = 0;
		// Pushes the muted look of the unbind button; pair with PopStyle.
		[[nodiscard]] virtual StyleScope PushKeyClearStyle() const noexcept = 0;

		// Paints an indeterminate busy indicator into a_bounds; a_time is the
		// frame clock in seconds.
		virtual void DrawSpinner(
			const ImRect& a_bounds,
			double a_time) const noexcept = 0;
	};

	[[nodiscard]] const ControlRenderer& ControlRendererFor(
		Theme::ControlRendererKind a_kind) noexcept;
	[[nodiscard]] const ControlRenderer& CurrentControlRenderer() noexcept;
}
