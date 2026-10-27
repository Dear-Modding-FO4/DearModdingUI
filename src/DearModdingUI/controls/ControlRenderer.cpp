#include <DearModdingUI/controls/ControlRenderer.h>

#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/controls/ChromeGeometry.h>
#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/presentation/Theme.h>
#include <DearModdingUI/presentation/VisualDecisions.h>

#include <algorithm>
#include <cmath>

namespace DearModdingUI
{
	namespace
	{
		inline constexpr float kPrimaryFillAlpha{ 0.6f };
		inline constexpr float kPrimaryHoveredFillAlpha{ 0.75f };
		inline constexpr float kPrimaryPressedFillAlpha{ 0.9f };

		struct ButtonLayout
		{
			InlineIconLayout inner;
			ImVec2 size;
		};

		[[nodiscard]] ButtonLayout LayoutButton(
			const ButtonContent& a_content,
			dmui::ButtonStyle a_style) noexcept
		{
			const auto& style = ImGui::GetStyle();
			const auto* label = a_content.label ? a_content.label : "";
			const auto hasLabel = label[0] != '\0';
			const auto hasGlyph =
				a_content.glyph != 0 && HasIconGlyph(a_content.glyph);
			const auto glyphSize = a_content.glyphSize > 0.0f ?
				a_content.glyphSize :
				ImGui::GetFontSize();
			const auto text = ImGui::CalcTextSize(label);
			const auto inner = DecideInlineIconLayout(
				hasGlyph,
				text.x,
				text.y,
				glyphSize,
				hasLabel ? style.ItemSpacing.x : 0.0f);
			const auto verticalPadding = a_style == dmui::ButtonStyle::kSubtle ?
				TitleBarButtonPadding() :
				style.FramePadding.y;
			const auto height = inner.contentHeight + verticalPadding * 2.0f;
			return {
				inner,
				{
					hasGlyph && !hasLabel ?
						height :
						inner.contentWidth + style.FramePadding.x * 2.0f,
					height
				}
			};
		}

		[[nodiscard]] ImU32 FillColor(
			dmui::ButtonStyle a_style,
			const ButtonVisualState& a_state) noexcept
		{
			const auto engaged = a_state.hovered || a_state.active;
			if (a_style == dmui::ButtonStyle::kPrimary)
			{
				auto accent = Theme::Current().tokens.accent;
				accent.w *= a_state.pressed ? kPrimaryPressedFillAlpha :
					engaged ? kPrimaryHoveredFillAlpha : kPrimaryFillAlpha;
				return ImGui::GetColorU32(accent);
			}
			return ImGui::GetColorU32(
				a_state.pressed ? ImGuiCol_ButtonActive :
				engaged ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
		}

		void DrawSectionRule(
			const HorizontalRuleSegment& a_segment,
			float a_y,
			const ImVec4& a_color,
			SectionRuleStyle a_style) noexcept
		{
			auto color = a_color;
			auto thickness = 0.0f;
			if (a_style == SectionRuleStyle::kSubordinate)
			{
				const auto minimizedFactor =
					Theme::Current().chrome.headingMinimizedFactor;
				color.w *= minimizedFactor;
				thickness = SeparatorThickness() * (1.0f - minimizedFactor);
			}
			if (a_segment.maxX <= a_segment.minX)
				return;
			ImGui::GetWindowDrawList()->AddLine(
				{ a_segment.minX, a_y },
				{ a_segment.maxX, a_y },
				ImGui::GetColorU32(color),
				thickness > 0.0f ? thickness : 1.0f);
		}

		[[nodiscard]] InlineIconLayout LayoutSectionHeader(
			const SectionHeaderContent& a_content) noexcept
		{
			const auto textSize = ImGui::CalcTextSize(a_content.title);
			return DecideInlineIconLayout(
				HasIconGlyph(a_content.glyph),
				textSize.x,
				textSize.y,
				ImGui::GetFontSize(),
				ImGui::GetStyle().ItemSpacing.x);
		}

		class DefaultControlRenderer final : public ControlRenderer
		{
		public:
			ImVec2 MeasureButton(
				const ButtonContent& a_content,
				dmui::ButtonStyle a_style) const noexcept override
			{
				return LayoutButton(a_content, a_style).size;
			}

			void DrawButton(
				const ImRect& a_bounds,
				const ButtonContent& a_content,
				dmui::ButtonStyle a_style,
				const ButtonVisualState& a_state) const noexcept override
			{
				if (a_style == dmui::ButtonStyle::kSubtle)
				{
					(void)DrawRoundedHighlight(
						a_bounds.Min,
						a_bounds.Max,
						a_state.hovered || a_state.active || a_state.focused,
						a_state.pressed,
						ImGui::GetWindowDrawList());
				}
				else
				{
					ImGui::RenderFrame(
						a_bounds.Min,
						a_bounds.Max,
						FillColor(a_style, a_state),
						true,
						ImGui::GetStyle().FrameRounding);
					if (a_state.focused)
						ImGui::RenderNavCursor(a_bounds, a_state.id);
				}

				const auto layout = LayoutButton(a_content, a_style);
				const ImVec4 clip{
					a_bounds.Min.x,
					a_bounds.Min.y,
					a_bounds.Max.x,
					a_bounds.Max.y
				};
				(void)DrawIconText(
					{
						(std::max)(
							a_bounds.Min.x,
							a_bounds.GetCenter().x -
								layout.inner.contentWidth * 0.5f),
						a_bounds.Min.y
					},
					a_bounds.GetHeight(),
					a_content.glyph,
					a_content.label,
					ImGui::GetColorU32(ImGuiCol_Text),
					&clip,
					a_content.glyphSize);
			}

			float MeasureFieldLabel(
				const char* a_label,
				const char* a_description,
				float a_wrapWidth) const noexcept override
			{
				// An empty label is still one line tall in ImGui; label-less rows reserve no header.
				if ((!a_label || a_label[0] == '\0') &&
					(!a_description || a_description[0] == '\0'))
					return 0.0f;
				float labelHeight{};
				{
					const Theme::FontGuard font{ Theme::FontRole::kSubheading };
					labelHeight = ImGui::CalcTextSize(
						a_label,
						nullptr,
						false,
						a_wrapWidth).y;
				}
				if (!a_description || a_description[0] == '\0')
					return labelHeight;

				float descriptionHeight{};
				{
					const Theme::FontGuard font{ Theme::FontRole::kSubtext };
					descriptionHeight = ImGui::CalcTextSize(
						a_description,
						nullptr,
						false,
						a_wrapWidth).y;
				}
				return labelHeight +
					ImGui::GetStyle().ItemSpacing.y +
					descriptionHeight;
			}

			void DrawFieldLabel(
				const ImVec2& a_position,
				float a_width,
				const char* a_label,
				const char* a_description) const noexcept override
			{
				PlaceRowContent(a_position);
				// Wrap positions are window-local, unlike the content rectangle.
				ImGui::PushTextWrapPos(
					ImGui::GetCursorPosX() +
						(std::max)(a_width, ImGui::GetFontSize()));
				{
					const Theme::FontGuard font{ Theme::FontRole::kSubheading };
					ImGui::TextUnformatted(a_label);
				}
				if (a_description && a_description[0] != '\0')
				{
					const Theme::FontGuard font{ Theme::FontRole::kSubtext };
					ImGui::TextDisabled("%s", a_description);
				}
				ImGui::PopTextWrapPos();
			}

			TextFieldFrame BeginTextField(
				DMUI_TextFieldStyle a_style) const noexcept override
			{
				TextFieldFrame frame{
					{},
					ImGui::GetCursorScreenPos(),
					ImGui::GetFrameHeight()
				};
				if (a_style != DMUI_TEXT_FIELD_STYLE_SEARCH)
					return frame;
				const auto scale = Theme::SearchScale();
				const auto& theme = Theme::Current();
				const auto iconSpace =
					theme.chrome.searchIconSize * scale +
					theme.chrome.searchInputPaddingExtra * scale;
				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4());
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4());
				ImGui::PushStyleColor(
					ImGuiCol_FrameBgActive,
					theme.tokens.searchFrameActive);
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4());
				ImGui::PushStyleColor(ImGuiCol_Text, theme.tokens.text);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
				ImGui::PushStyleVar(
					ImGuiStyleVar_FramePadding,
					ImVec2(
						iconSpace,
						theme.chrome.searchInputFramePaddingY * scale));
				frame.style = { 5, 2 };
				return frame;
			}

			void EndTextField(
				const TextFieldFrame& a_frame,
				DMUI_TextFieldStyle a_style) const noexcept override
			{
				if (a_style == DMUI_TEXT_FIELD_STYLE_SEARCH)
				{
					const auto scale = Theme::SearchScale();
					const auto& theme = Theme::Current();
					const auto iconSize = theme.chrome.searchIconSize * scale;
					const ImVec2 iconPosition{
						a_frame.origin.x + theme.chrome.searchIconOffsetX * scale,
						RowContentY(a_frame.origin.y, a_frame.height, iconSize)
					};
					DrawCenteredIcon(
						ImGui::GetWindowDrawList(),
						dmui::PhosphorGlyph::kMagnifyingGlass,
						{
							iconPosition,
							{ iconPosition.x + iconSize, iconPosition.y + iconSize }
						},
						iconSize,
						IconColor(
							ImGui::GetColorU32(ImGuiCol_Text),
							theme.chrome.searchIconAlpha));
				}
				PopStyle(a_frame.style);
			}

			SectionPalette SectionColors(
				bool a_minimized) const noexcept override
			{
				const auto& theme = Theme::Current();
				SectionPalette palette{
					theme.tokens.heading,
					theme.tokens.headingHovered
				};
				if (a_minimized)
				{
					palette.text.w *= theme.chrome.headingMinimizedFactor;
					palette.hovered.w *= theme.chrome.headingMinimizedFactor;
				}
				return palette;
			}

			SectionHeaderMetrics MeasureSectionHeader(
				const SectionHeaderContent& a_content) const noexcept override
			{
				const auto layout = LayoutSectionHeader(a_content);
				return { layout.contentWidth, layout.contentHeight };
			}

			void DrawSectionHeader(
				const ImVec2& a_position,
				float a_width,
				const SectionHeaderContent& a_content,
				bool a_hovered) const noexcept override
			{
				const auto palette = SectionColors(a_content.minimized);
				const auto color = a_hovered ? palette.hovered : palette.text;
				const auto layout = LayoutSectionHeader(a_content);
				const auto gap = ImGui::GetStyle().ItemSpacing.x;
				if (layout.contentWidth <= 0.0f)
				{
					DrawSectionRule(
						{ a_position.x, a_position.x + a_width },
						a_position.y + layout.contentHeight * 0.5f,
						color,
						a_content.ruleStyle);
					return;
				}
				const auto lineLength = (std::max)(
					(a_width - layout.contentWidth - gap * 2.0f) * 0.5f,
					0.0f);
				const auto contentMinX = a_position.x + lineLength + gap;
				const auto lineY = DrawIconText(
					{ contentMinX, a_position.y },
					layout.contentHeight,
					a_content.glyph,
					a_content.title,
					ImGui::GetColorU32(color));
				const auto extents = ResolveRuledHeadingRuleExtents(
					a_position.x,
					a_position.x + a_width,
					contentMinX,
					contentMinX + layout.contentWidth,
					gap);
				DrawSectionRule(extents.left, lineY, color, a_content.ruleStyle);
				DrawSectionRule(extents.right, lineY, color, a_content.ruleStyle);
			}

			void DrawSectionRules(
				const ImRect& a_row,
				float a_textMinX,
				float a_textMaxX,
				const SectionHeaderContent& a_content,
				bool a_hovered) const noexcept override
			{
				const auto palette = SectionColors(a_content.minimized);
				const auto extents = ResolveRuledHeadingRuleExtents(
					a_row.Min.x,
					a_row.Max.x,
					a_textMinX,
					a_textMaxX,
					ImGui::GetStyle().ItemSpacing.x);
				const auto color = a_hovered ? palette.hovered : palette.text;
				const auto y = a_row.GetCenter().y;
				DrawSectionRule(extents.left, y, color, a_content.ruleStyle);
				DrawSectionRule(extents.right, y, color, a_content.ruleStyle);
			}

			StyleScope PushBoxStyle(bool a_background) const noexcept override
			{
				ImGui::PushStyleVar(
					a_background ?
						ImGuiStyleVar_FramePadding :
						ImGuiStyleVar_WindowPadding,
					Theme::Layout().panelPadding);
				if (!a_background)
					return { 0, 1 };
				ImGui::PushStyleColor(
					ImGuiCol_FrameBg,
					Theme::Current().tokens.rowAlt);
				return { 1, 1 };
			}

			bool BeginChoice(
				const char* a_id,
				const char* a_preview) const noexcept override
			{
				return ImGui::BeginCombo(a_id, a_preview);
			}

			void EndChoice() const noexcept override
			{
				// Commits the cursor restored by the last row before the popup ends.
				ImGui::Dummy({ 0.0f, 0.0f });
				ImGui::EndCombo();
			}

			bool DrawChoiceRow(
				const char* a_label,
				bool a_selected) const noexcept override
			{
				// Drawn apart from the item so "##" in a label shows verbatim.
				const auto start = ImGui::GetCursorScreenPos();
				const auto pressed = ImGui::Selectable("##Option", a_selected);
				if (a_selected)
					ImGui::SetItemDefaultFocus();
				const auto item = GImGui->LastItemData;
				const auto after = ImGui::GetCursorScreenPos();
				ImGui::SetCursorScreenPos(start);
				ImGui::TextUnformatted(a_label);
				ImGui::SetCursorScreenPos(after);
				// Tooltips and focus checks still address the selectable.
				GImGui->LastItemData = item;
				return pressed;
			}

			bool DrawToggle(bool& a_value, const char* a_label) const noexcept override
			{
				auto changed = ImGui::Checkbox("##Toggle", &a_value);
				if (!a_label || a_label[0] == '\0')
					return changed;
				const auto& style = ImGui::GetStyle();
				ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
				const auto text = ImGui::CalcTextSize(a_label);
				const auto origin = ImGui::GetCursorScreenPos();
				if (ImGui::InvisibleButton("##ToggleLabel", { text.x, ImGui::GetFrameHeight() }))
				{
					a_value = !a_value;
					changed = true;
				}
				ImGui::GetWindowDrawList()->AddText(
					{ origin.x, origin.y + style.FramePadding.y },
					ImGui::GetColorU32(ImGuiCol_Text),
					a_label);
				return changed;
			}

			bool DrawColorEdit(ImVec4& a_color, bool a_alpha) const noexcept override
			{
				constexpr auto flags =
					ImGuiColorEditFlags_DisplayRGB |
					ImGuiColorEditFlags_InputRGB |
					ImGuiColorEditFlags_PickerHueBar;
				return a_alpha ?
					ImGui::ColorEdit4(
						"##Color",
						&a_color.x,
						flags | ImGuiColorEditFlags_AlphaBar) :
					ImGui::ColorEdit3(
						"##Color",
						&a_color.x,
						flags | ImGuiColorEditFlags_NoAlpha);
			}

			bool DrawColorSwatch(
				const char* a_id,
				const ImVec4& a_color,
				bool a_alpha,
				const ImVec2& a_size) const noexcept override
			{
				return ImGui::ColorButton(
					a_id,
					a_color,
					a_alpha ? ImGuiColorEditFlags_None : ImGuiColorEditFlags_NoAlpha,
					a_size);
			}

			StyleScope PushKeyCaptureStyle(
				const KeyCaptureVisual& a_visual) const noexcept override
			{
				ImGui::PushStyleColor(
					ImGuiCol_Button,
					ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
				ImGui::PushStyleColor(
					ImGuiCol_Text,
					a_visual.warned ?
						Theme::StatusTextColor(DMUI_SEVERITY_WARNING) :
						ImGui::GetStyleColorVec4(
							a_visual.strong ? ImGuiCol_Text : ImGuiCol_TextDisabled));
				return { 2, 0 };
			}

			void DrawKeyCaptureCue(const ImRect& a_bounds) const noexcept override
			{
				auto color = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
				color.w *= 0.65f +
					0.25f * std::sin(static_cast<float>(ImGui::GetTime()) * 4.0f);
				ImGui::GetWindowDrawList()->AddRect(
					a_bounds.Min,
					a_bounds.Max,
					ImGui::GetColorU32(color),
					ImGui::GetStyle().FrameRounding,
					0,
					1.5f);
			}

			StyleScope PushKeyClearStyle() const noexcept override
			{
				ImGui::PushStyleColor(
					ImGuiCol_Text,
					ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
				return { 1, 0 };
			}

			void DrawSpinner(
				const ImRect& a_bounds,
				double a_time) const noexcept override
			{
				constexpr float kTurnSeconds{ 0.9f };
				constexpr float kArc{ IM_PI * 1.5f };
				const auto size = a_bounds.GetWidth();
				const auto thickness = (std::max)(2.0f, size * 0.12f);
				const auto start = static_cast<float>(std::fmod(a_time, kTurnSeconds)) /
					kTurnSeconds * IM_PI * 2.0f;
				auto* draw = ImGui::GetWindowDrawList();
				draw->PathArcTo(
					a_bounds.GetCenter(), (size - thickness) * 0.5f, start, start + kArc);
				draw->PathStroke(
					ImGui::GetColorU32(Theme::Current().tokens.accent),
					ImDrawFlags_None,
					thickness);
			}

			bool DrawNumeric(const NumericContent& a_content) const noexcept override
			{
				const auto flags = a_content.clamp ?
					ImGuiSliderFlags_AlwaysClamp :
					ImGuiSliderFlags_None;
				switch (a_content.presentation)
				{
				case DMUI_NUMERIC_PRESENTATION_SLIDER:
					return ImGui::SliderScalar(
						"##Numeric",
						a_content.dataType,
						a_content.value,
						a_content.minimum,
						a_content.maximum,
						a_content.format,
						flags);
				case DMUI_NUMERIC_PRESENTATION_DRAG:
					return ImGui::DragScalar(
						"##Numeric",
						a_content.dataType,
						a_content.value,
						a_content.speed,
						a_content.minimum,
						a_content.maximum,
						a_content.format,
						flags);
				default:
					return ImGui::InputScalar(
						"##Numeric",
						a_content.dataType,
						a_content.value,
						nullptr,
						nullptr,
						a_content.format);
				}
			}
		};
	}

	const ControlRenderer& ControlRendererFor(
		Theme::ControlRendererKind a_kind) noexcept
	{
		static const DefaultControlRenderer renderer;
		switch (a_kind)
		{
		case Theme::ControlRendererKind::kDefault:
			break;
		}
		return renderer;
	}

	const ControlRenderer& CurrentControlRenderer() noexcept
	{
		return ControlRendererFor(Theme::Current().controlRenderer);
	}
}
