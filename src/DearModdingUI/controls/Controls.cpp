#include <DearModdingUI/controls/Controls.h>

#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/controls/SettingsTable.h>
#include <DearModdingUI/VisualDecisions.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numbers>

namespace DearModdingUI
{
	namespace
	{
		inline constexpr ImVec4 kTransparentButtonChrome{ 0, 0, 0, 0 };
		inline constexpr float kCloseCrossDiagonalScale{
			0.5f / std::numbers::sqrt2_v<float>
		};

		[[nodiscard]] float ContentOffsetY(
			float a_height,
			float a_contentHeight) noexcept
		{
			return (std::max)((a_height - a_contentHeight) * 0.5f, 0.0f);
		}

		void DrawIcon(
			ImDrawList* a_drawList,
			char32_t a_glyph,
			const ImVec2& a_position,
			float a_size,
			ImU32 a_color,
			const ImVec4* a_clip) noexcept
		{
			auto* font = ImGui::GetFont();
			if (!font || !HasIconGlyph(a_glyph) || a_size <= 0.0f)
				return;
			font->RenderChar(
				a_drawList,
				a_size,
				a_position,
				a_color,
				static_cast<ImWchar>(a_glyph),
				a_clip);
		}

		[[nodiscard]] float PillRounding(
			const ImVec2& a_min,
			const ImVec2& a_max) noexcept
		{
			return ImMin(a_max.x - a_min.x, a_max.y - a_min.y) * 0.5f;
		}

		[[nodiscard]] bool DrawRoundedHighlight(
			const ImVec2& a_min,
			const ImVec2& a_max,
			bool a_hovered,
			bool a_active,
			ImDrawList* a_drawList) noexcept
		{
			if (!a_hovered && !a_active)
				return false;
			const auto rounding = ImMin(
				ImMax(ImGui::GetStyle().FrameRounding, 0.0f),
				PillRounding(a_min, a_max));
			a_drawList->AddRectFilled(
				a_min,
				a_max,
				ImGui::GetColorU32(
					a_active ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered),
				rounding);
			return true;
		}

		[[nodiscard]] ImRect RightTitleButtonRect(
			ImGuiWindow* a_window,
			float a_fontSize) noexcept
		{
			const auto padding = TitleBarButtonPadding();
			const auto extent = a_fontSize + padding * 2.0f;
			const auto& style = ImGui::GetStyle();
			const ImVec2 minimum{
				RightTitleBarButtonOriginX(
					a_window->Rect().Max.x,
					a_window->WindowBorderSize,
					style.FramePadding.x,
					a_fontSize,
					0.0f,
					padding),
				a_window->Rect().Min.y + style.FramePadding.y - padding
			};
			return { minimum, { minimum.x + extent, minimum.y + extent } };
		}

		class NativeTitleButtonGuard
		{
		public:
			NativeTitleButtonGuard() noexcept
			{
				ImGui::PushStyleColor(
					ImGuiCol_ButtonHovered, kTransparentButtonChrome);
				ImGui::PushStyleColor(
					ImGuiCol_ButtonActive, kTransparentButtonChrome);
			}

			~NativeTitleButtonGuard()
			{
				ImGui::PopStyleColor(2);
			}

			NativeTitleButtonGuard(const NativeTitleButtonGuard&) = delete;
			NativeTitleButtonGuard& operator=(
				const NativeTitleButtonGuard&) = delete;
		};

		void DrawRoundedCloseHighlight(ImGuiWindow* a_window) noexcept
		{
			if (!a_window || (a_window->Flags & ImGuiWindowFlags_NoTitleBar))
				return;
			const auto bounds =
				RightTitleButtonRect(a_window, ImGui::GetFontSize());
			auto& context = *ImGui::GetCurrentContext();
			const auto hovered =
				context.HoveredWindow == a_window &&
				ImGui::IsMouseHoveringRect(bounds.Min, bounds.Max, false);
			const auto held =
				hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
			a_window->DrawList->PushClipRect(
				a_window->Rect().Min,
				a_window->Rect().Max);
			if (DrawRoundedHighlight(
					bounds.Min,
					bounds.Max,
					hovered,
					held,
					a_window->DrawList))
			{
				const auto center = bounds.GetCenter();
				const auto diagonal =
					ImGui::GetFontSize() * kCloseCrossDiagonalScale -
					Theme::Scale();
				const auto color = ImGui::GetColorU32(ImGuiCol_Text);
				a_window->DrawList->AddLine(
					{ center.x - diagonal, center.y - diagonal },
					{ center.x + diagonal, center.y + diagonal },
					color);
				a_window->DrawList->AddLine(
					{ center.x + diagonal, center.y - diagonal },
					{ center.x - diagonal, center.y + diagonal },
					color);
			}
			a_window->DrawList->PopClipRect();
		}

		template <class Draw>
		[[nodiscard]] bool DrawEnabled(bool a_enabled, Draw a_draw) noexcept
		{
			ImGui::BeginDisabled(!a_enabled);
			const auto pressed = a_draw();
			ImGui::EndDisabled();
			return pressed && a_enabled;
		}

		void DrawRuledHeadingRules(
			const RuledHeadingRuleExtents& a_extents,
			float a_y,
			ImVec4 a_color,
			RuledHeadingRuleStyle a_style) noexcept
		{
			auto thickness = 0.0f;
			if (a_style == RuledHeadingRuleStyle::kSubordinate)
			{
				a_color.w *= Theme::kFeatureHeadingDefaults.minimizedFactor;
				thickness =
					SeparatorThickness() *
					(1.0f - Theme::kFeatureHeadingDefaults.minimizedFactor);
			}
			const auto packed = ImGui::GetColorU32(a_color);
			const auto draw = [&](const HorizontalRuleSegment& a_segment) {
				if (a_segment.maxX <= a_segment.minX)
					return;
				ImGui::GetWindowDrawList()->AddLine(
					{ a_segment.minX, a_y },
					{ a_segment.maxX, a_y },
					packed,
					thickness > 0.0f ? thickness : 1.0f);
			};
			draw(a_extents.left);
			draw(a_extents.right);
		}
	}

	bool HasIconGlyph(char32_t a_glyph) noexcept
	{
		if (!IsRepresentableIconGlyph<ImWchar>(a_glyph))
			return false;
		auto* font = ImGui::GetFont();
		return font && font->IsGlyphInFont(static_cast<ImWchar>(a_glyph));
	}

	ImU32 IconColor(ImU32 a_textColor, float a_alpha) noexcept
	{
		auto tint = Theme::IconTint();
		tint.w *= ImGui::ColorConvertU32ToFloat4(a_textColor).w * a_alpha;
		return ImGui::ColorConvertFloat4ToU32(tint);
	}

	void DrawCenteredIcon(
		ImDrawList* a_drawList,
		char32_t a_glyph,
		const ImRect& a_bounds,
		float a_size,
		ImU32 a_color,
		const ImVec4* a_clip) noexcept
	{
		auto* font = ImGui::GetFont();
		if (!font || !HasIconGlyph(a_glyph) || a_size <= 0.0f)
			return;
		const auto center = a_bounds.GetCenter();
		ImVec2 position{
			center.x - a_size * 0.5f,
			center.y - a_size * 0.5f
		};
		if (auto* baked = font->GetFontBaked(a_size);
			baked && baked->Size > 0.0f)
		{
			if (const auto* glyph = baked->FindGlyphNoFallback(
					static_cast<ImWchar>(a_glyph)))
			{
				const auto origin = ResolveCenteredGlyphOrigin(
					center.x,
					center.y,
					glyph->X0,
					glyph->Y0,
					glyph->X1,
					glyph->Y1,
					a_size / baked->Size);
				position = { origin.x, origin.y };
			}
		}
		DrawIcon(
			a_drawList,
			a_glyph,
			position,
			a_size,
			a_color,
			a_clip);
	}

	float DrawIconText(
		const ImVec2& a_position,
		float a_height,
		char32_t a_glyph,
		const char* a_text,
		ImU32 a_color,
		const ImVec4* a_clip) noexcept
	{
		a_text = a_text ? a_text : "";
		const auto textSize = ImGui::CalcTextSize(a_text);
		const auto layout = DecideInlineIconLayout(
			HasIconGlyph(a_glyph),
			textSize.x,
			textSize.y,
			ImGui::GetFontSize(),
			ImGui::GetStyle().ItemSpacing.x);
		const auto contentY =
			a_position.y + ContentOffsetY(a_height, layout.contentHeight);
		const auto textY =
			contentY + ContentOffsetY(layout.contentHeight, textSize.y);
		if (layout.drawIcon)
		{
			DrawCenteredIcon(
				ImGui::GetWindowDrawList(),
				a_glyph,
				{
					{ a_position.x, contentY },
					{ a_position.x + layout.iconSize,
						contentY + layout.contentHeight }
				},
				layout.iconSize,
				IconColor(a_color),
				a_clip);
		}
		ImGui::GetWindowDrawList()->AddText(
			ImGui::GetFont(),
			ImGui::GetFontSize(),
			{ a_position.x + layout.textOffset, textY },
			a_color,
			a_text,
			nullptr,
			0.0f,
			a_clip);
		return contentY + layout.contentHeight * 0.5f;
	}

	float TitleBarButtonPadding() noexcept
	{
		return ResolveTitleBarButtonPadding(
			ImGui::GetStyle().FramePadding.y);
	}

	float SeparatorThickness() noexcept
	{
		return Theme::kSeparatorThickness * Theme::Scale();
	}

	bool BeginWithRoundedTitleBarButtons(
		const char* a_name,
		bool* a_open,
		ImGuiWindowFlags a_flags) noexcept
	{
		bool visible{};
		{
			const NativeTitleButtonGuard guard;
			visible = ImGui::Begin(a_name, a_open, a_flags);
		}
		DrawRoundedCloseHighlight(ImGui::GetCurrentWindowRead());
		return visible;
	}

	bool BeginPopupModalWithRoundedTitleBarButtons(
		const char* a_name,
		bool* a_open,
		ImGuiWindowFlags a_flags) noexcept
	{
		bool visible{};
		{
			const NativeTitleButtonGuard guard;
			visible = ImGui::BeginPopupModal(a_name, a_open, a_flags);
		}
		if (visible)
			DrawRoundedCloseHighlight(ImGui::GetCurrentWindowRead());
		return visible;
	}

	bool DrawCompactChromeButton(
		const char* a_id,
		const ImVec2& a_origin,
		const ImVec2& a_size,
		char32_t a_glyph,
		const char* a_text,
		const char* a_tooltip,
		ImU32 a_color,
		bool a_active,
		float a_glyphSize) noexcept
	{
		auto* window = ImGui::GetCurrentWindow();
		if (!window)
			return false;
		const auto restore = ImGui::GetCursorScreenPos();
		ImGui::SetCursorScreenPos(a_origin);
		const auto pressed = ImGui::InvisibleButton(a_id, a_size);
		const auto hovered = ImGui::IsItemHovered(
			ImGuiHoveredFlags_AllowWhenDisabled);
		const ImRect bounds{
			a_origin,
			{ a_origin.x + a_size.x, a_origin.y + a_size.y }
		};
		(void)DrawRoundedHighlight(
			bounds.Min,
			bounds.Max,
			hovered || a_active,
			ImGui::IsItemActive(),
			window->DrawList);
		if (a_glyph)
		{
			DrawCenteredIcon(
				window->DrawList,
				a_glyph,
				bounds,
				a_glyphSize > 0.0f ? a_glyphSize : ImGui::GetFontSize(),
				a_color);
		}
		else if (a_text)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, a_color);
			ImGui::RenderTextClipped(
				bounds.Min,
				bounds.Max,
				a_text,
				nullptr,
				nullptr,
				{ 0.5f, 0.5f },
				&bounds);
			ImGui::PopStyleColor();
		}
		if (hovered && a_tooltip)
			ImGui::SetTooltip("%s", a_tooltip);
		ImGui::SetCursorScreenPos(restore);
		ImGui::Dummy({ 0.0f, 0.0f });
		return pressed;
	}

	RowResult DrawSelectableRow(const RowOptions& a_options) noexcept
	{
		auto* window = ImGui::GetCurrentWindow();
		if (!window || window->SkipItems || !a_options.id || !a_options.label)
			return {};
		const auto height =
			a_options.height > 0.0f ? a_options.height : ImGui::GetFrameHeight();
		const auto splitArrow =
			a_options.leadingAffordance == RowLeadingAffordance::kArrow &&
			a_options.clickBehavior == RowClickBehavior::kSelect;
		ImGui::PushID(a_options.id);
		if (a_options.flushHorizontalHighlight)
		{
			ImGui::PushStyleVar(
				ImGuiStyleVar_ItemSpacing,
				{ 0.0f, ImGui::GetStyle().ItemSpacing.y });
		}
		const ImVec2 size{
			(std::max)(ImGui::GetContentRegionAvail().x, 0.0f),
			height
		};
		auto pressed =
			a_options.highlightStyle == RowHighlightStyle::kSelectable ?
			ImGui::Selectable(
				"##Row",
				a_options.selected,
				splitArrow ?
					ImGuiSelectableFlags_NoAutoClosePopups :
					ImGuiSelectableFlags_None,
				size) :
			ImGui::InvisibleButton("##Row", size);
		const auto hovered = ImGui::IsItemHovered();
		if (a_options.flushHorizontalHighlight)
			ImGui::PopStyleVar();
		const ImRect rect{ ImGui::GetItemRectMin(), ImGui::GetItemRectMax() };
		auto arrowPressed = false;
		if (splitArrow)
		{
			const auto rowID = ImGui::GetItemID();
			const auto pressOriginID = window->GetID("##RowPressOrigin");
			constexpr auto kPressOriginNone = 0;
			constexpr auto kPressOriginArrow = 1;
			constexpr auto kPressOriginLabel = 2;
			const auto slotWidth =
				ImGui::GetFontSize() +
				ImGui::GetStyle().FramePadding.x * 2.0f;
			auto* storage = ImGui::GetStateStorage();
			if (ImGui::IsItemActivated())
			{
				const auto& context = *ImGui::GetCurrentContext();
				const auto pressOrigin =
					context.ActiveId == rowID &&
						context.ActiveIdSource == ImGuiInputSource_Mouse &&
						context.ActiveIdClickOffset.x < slotWidth ?
					kPressOriginArrow :
					context.ActiveIdSource == ImGuiInputSource_Mouse ?
						kPressOriginLabel :
						kPressOriginNone;
				storage->SetInt(pressOriginID, pressOrigin);
			}
			if (pressed)
			{
				const auto releaseOverArrow =
					ImGui::GetIO().MousePos.x < rect.Min.x + slotWidth;
				switch (storage->GetInt(pressOriginID))
				{
				case kPressOriginArrow:
					arrowPressed = releaseOverArrow;
					pressed = false;
					break;
				case kPressOriginLabel:
					pressed = !releaseOverArrow;
					break;
				default:
					break;
				}
			}
			if (ImGui::IsItemDeactivated())
				storage->SetInt(pressOriginID, kPressOriginNone);
			if (pressed &&
				a_options.highlightStyle ==
					RowHighlightStyle::kSelectable &&
				(window->Flags & ImGuiWindowFlags_Popup) &&
				(ImGui::GetItemFlags() & ImGuiItemFlags_AutoClosePopups))
			{
				ImGui::CloseCurrentPopup();
			}
		}
		auto* drawList = ImGui::GetWindowDrawList();
		if (a_options.highlightStyle == RowHighlightStyle::kRoundedFill &&
			(hovered || a_options.selected))
		{
			drawList->AddRectFilled(
				rect.Min,
				rect.Max,
				ImGui::GetColorU32(
					hovered ? ImGuiCol_HeaderHovered : ImGuiCol_Header),
				ImGui::GetStyle().FrameRounding);
		}

		const auto textColor = a_options.disabledColor.value_or(
			hovered ? a_options.hoveredTextColor : a_options.textColor);
		const auto fontSize = ImGui::GetFontSize();
		auto cursorX = rect.Min.x + ImGui::GetStyle().FramePadding.x;
		if (a_options.leadingAffordance == RowLeadingAffordance::kArrow ||
			a_options.leadingAffordance == RowLeadingAffordance::kBack)
		{
			ImGui::RenderArrow(
				drawList,
				{
					cursorX,
					rect.Min.y + ContentOffsetY(rect.GetHeight(), fontSize)
				},
				textColor,
				a_options.leadingAffordance == RowLeadingAffordance::kBack ?
					ImGuiDir_Left :
					(a_options.expanded && *a_options.expanded ?
						ImGuiDir_Down :
						ImGuiDir_Right));
			cursorX += fontSize + ImGui::GetStyle().ItemInnerSpacing.x;
		}
		if (HasIconGlyph(a_options.glyph))
		{
			const ImRect iconBounds = a_options.centerGlyph ?
				rect :
				ImRect{
					{ cursorX, rect.Min.y },
					{ cursorX + fontSize, rect.Max.y }
				};
			DrawCenteredIcon(
				drawList,
				a_options.glyph,
				iconBounds,
				fontSize,
				IconColor(textColor));
			if (!a_options.centerGlyph)
				cursorX += fontSize + ImGui::GetStyle().ItemInnerSpacing.x;
		}
		if (!a_options.centerGlyph)
		{
			const auto clipMax =
				rect.Max.x - ImGui::GetStyle().FramePadding.x -
				a_options.trailingWidth;
			const ImVec4 clip{ rect.Min.x, rect.Min.y, clipMax, rect.Max.y };
			drawList->AddText(
				ImGui::GetFont(),
				fontSize,
				{
					cursorX,
					rect.Min.y + ContentOffsetY(
						rect.GetHeight(),
						ImGui::CalcTextSize(a_options.label).y)
				},
				textColor,
				a_options.label,
				ImGui::FindRenderedTextEnd(a_options.label),
				0.0f,
				&clip);
		}
		if (a_options.expanded &&
			(arrowPressed ||
				(pressed &&
					a_options.clickBehavior == RowClickBehavior::kToggle)))
			*a_options.expanded = !*a_options.expanded;
		ImGui::PopID();
		return { pressed, rect };
	}

	void DrawRuledHeading(const RuledHeadingOptions& a_options) noexcept
	{
		char countedText[256]{};
		const auto* text = a_options.text ? a_options.text : "";
		if (a_options.count)
		{
			std::snprintf(
				countedText,
				sizeof(countedText),
				"%s (%zu)",
				text,
				*a_options.count);
			text = countedText;
		}
		auto color = Theme::kFeatureHeadingDefaults.colorDefault;
		auto hoveredColor = Theme::kFeatureHeadingDefaults.colorHovered;
		if (a_options.expanded && !*a_options.expanded)
		{
			color.w *= Theme::kFeatureHeadingDefaults.minimizedFactor;
			hoveredColor.w *= Theme::kFeatureHeadingDefaults.minimizedFactor;
		}
		if (a_options.layout == RuledHeadingLayout::kLeadingRow)
		{
			const auto row = DrawSelectableRow({
				.id = a_options.key,
				.label = text,
				.leadingAffordance = RowLeadingAffordance::kArrow,
				.expanded = a_options.expanded,
				.glyph = a_options.glyph,
				.textColor = ImGui::GetColorU32(color),
				.hoveredTextColor = ImGui::GetColorU32(hoveredColor),
				.highlightStyle = RowHighlightStyle::kRoundedFill,
				.clickBehavior = RowClickBehavior::kToggle
			});
			const auto textMin =
				row.rect.Min.x + ImGui::GetStyle().FramePadding.x +
				ImGui::GetFontSize() * 2.0f +
				ImGui::GetStyle().ItemInnerSpacing.x * 2.0f;
			const auto textMax = (std::min)(
				textMin + ImGui::CalcTextSize(text).x,
				row.rect.Max.x);
			DrawRuledHeadingRules(
				ResolveRuledHeadingRuleExtents(
					row.rect.Min.x,
					row.rect.Max.x,
					row.rect.Min.x + ImGui::GetStyle().FramePadding.x,
					textMax,
					ImGui::GetStyle().ItemSpacing.x),
				row.rect.GetCenter().y,
				ImGui::IsItemHovered() ? hoveredColor : color,
				a_options.ruleStyle);
			return;
		}

		const auto position = ImGui::GetCursorScreenPos();
		const auto width = ImGui::GetContentRegionAvail().x;
		const auto textSize = ImGui::CalcTextSize(text);
		const auto layout = DecideInlineIconLayout(
			HasIconGlyph(a_options.glyph),
			textSize.x,
			textSize.y,
			ImGui::GetFontSize(),
			ImGui::GetStyle().ItemSpacing.x);
		const auto gap = ImGui::GetStyle().ItemSpacing.x;
		const auto lineLength =
			(std::max)((width - layout.contentWidth - gap * 2.0f) * 0.5f, 0.0f);
		auto clicked = false;
		auto hovered = false;
		if (a_options.expanded)
		{
			ImGui::PushID(a_options.key);
			clicked = ImGui::InvisibleButton(
				"##DearModdingUI.RuledHeading",
				{ width, layout.contentHeight });
			hovered = ImGui::IsItemHovered();
		}
		else
			ImGui::Dummy({ width, layout.contentHeight });
		if (hovered)
			color = hoveredColor;
		const auto contentMinX = position.x + lineLength + gap;
		const auto lineY = DrawIconText(
			{ contentMinX, position.y },
			layout.contentHeight,
			a_options.glyph,
			text,
			ImGui::GetColorU32(color));
		DrawRuledHeadingRules(
			ResolveRuledHeadingRuleExtents(
				position.x,
				position.x + width,
				contentMinX,
				contentMinX + layout.contentWidth,
				gap),
			lineY,
			color,
			a_options.ruleStyle);
		if (a_options.expanded)
		{
			if (clicked)
				*a_options.expanded = !*a_options.expanded;
			ImGui::PopID();
		}
	}

	std::optional<size_t> DrawTitleRow(
		const TitleRowOptions& a_options) noexcept
	{
		const auto start = ImGui::GetCursorScreenPos();
		const auto maxX = start.x + ImGui::GetContentRegionAvail().x;
		const auto bodyFontSize = ImGui::GetFontSize();
		const auto buttonExtent = ResolveTitleRowButtonExtent(
			a_options.buttonExtentPolicy,
			bodyFontSize,
			TitleBarButtonPadding());
		const auto glyphSize =
			a_options.buttonExtentPolicy ==
					TitleRowButtonExtentPolicy::kHostChrome ?
				HostChromeIconSize(bodyFontSize) :
				bodyFontSize;
		float totalWidth{};
		for (const auto& button : a_options.buttons)
			totalWidth += (std::max)(button.width, 0.0f);
		const auto layout = ResolvePageActionRowLayout(
			start.x,
			maxX,
			totalWidth,
			a_options.buttons.size(),
			ImGui::GetStyle().ItemSpacing.x);
		ImVec2 titleSize{};
		{
			const Theme::FontGuard font{
				a_options.titleFont,
				a_options.titleScale
			};
			titleSize = ImGui::CalcTextSize(a_options.title);
			const auto rowHeight = (std::max)(
				titleSize.y,
				a_options.buttons.empty() ? 0.0f : buttonExtent);
			const ImVec2 titlePosition{
				start.x + (std::max)(a_options.titleInsetX, 0.0f),
				start.y + ContentOffsetY(rowHeight, titleSize.y)
			};
			ImGui::RenderTextEllipsis(
				ImGui::GetWindowDrawList(),
				titlePosition,
				{ layout.titleMaxX, start.y + rowHeight },
				layout.titleMaxX,
				a_options.title,
				nullptr,
				&titleSize);
			ImGui::Dummy({ maxX - start.x, rowHeight });
		}
		const auto rowBottom = ImGui::GetItemRectMax().y;
		std::optional<size_t> pressed;
		auto positionX = layout.actionsMinX;
		for (size_t index = 0; index < a_options.buttons.size(); ++index)
		{
			const auto& button = a_options.buttons[index];
			ImGui::PushID(button.id);
			if (DrawEnabled(
					button.enabled,
					[&]() noexcept {
						return DrawCompactChromeButton(
							"##DearModdingUI.TitleRowButton",
							{
								positionX,
								start.y + ContentOffsetY(
									rowBottom - start.y,
									buttonExtent)
							},
							{ button.width, buttonExtent },
							button.glyph,
							button.glyph ? nullptr : button.fallbackLabel,
							button.tooltip,
							button.glyph ?
								IconColor(ImGui::GetColorU32(ImGuiCol_Text)) :
								ImGui::GetColorU32(ImGuiCol_Text),
							button.active,
							glyphSize);
					}))
				pressed = index;
			ImGui::PopID();
			positionX += button.width + ImGui::GetStyle().ItemSpacing.x;
		}
		auto contentBottom = rowBottom;
		if (a_options.summary && *a_options.summary)
		{
			ImGui::SetCursorScreenPos({
				start.x,
				contentBottom + ImGui::GetStyle().ItemInnerSpacing.y
			});
			auto color = Theme::kFullPalette[ImGuiCol_Text];
			color.w *= Theme::kVersionTextOpacity;
			const Theme::FontGuard font{ Theme::FontRole::kSubtext };
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::TextWrapped("%s", a_options.summary);
			ImGui::PopStyleColor();
			contentBottom = ImGui::GetItemRectMax().y;
		}
		if (a_options.drawSeparator)
		{
			ImGui::SetCursorScreenPos({
				start.x,
				contentBottom + ImGui::GetStyle().ItemSpacing.y
			});
			ImGui::SeparatorEx(
				ImGuiSeparatorFlags_Horizontal,
				SeparatorThickness());
			ImGui::Spacing();
		}
		return pressed;
	}

	float SettingsActionButtonExtent() noexcept
	{
		return TitleBarButtonExtent(
			ImGui::GetFontSize(),
			TitleBarButtonPadding());
	}

	void DrawSearchInput(
		const char* a_id,
		const char* a_hint,
		std::string& a_search) noexcept
	{
		ImGui::PushID(a_id);
		const auto scale = Theme::SearchScale();
		const auto iconSize = Theme::kSearchIconSize * scale;
		const auto iconSpace =
			iconSize + Theme::kSearchInputPaddingExtra * scale;
		const auto cursor = ImGui::GetCursorScreenPos();
		const auto width = ImGui::GetContentRegionAvail().x;
		const auto frameHeight = ImGui::GetFrameHeight();
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4());
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4());
		ImGui::PushStyleColor(
			ImGuiCol_FrameBgActive,
			ImVec4(0.3f, 0.3f, 0.3f, 0.9f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4());
		ImGui::PushStyleColor(ImGuiCol_Text, Theme::kFullPalette[ImGuiCol_Text]);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		ImGui::PushStyleVar(
			ImGuiStyleVar_FramePadding,
			ImVec2(iconSpace, Theme::kSearchInputFramePaddingY * scale));
		ImGui::SetNextItemWidth(width);
		char buffer[256]{};
		strncpy_s(buffer, a_search.c_str(), sizeof(buffer) - 1);
		if (ImGui::InputTextWithHint(
				"##search",
				a_hint,
				buffer,
				sizeof(buffer)))
			a_search = buffer;
		const ImVec2 iconPosition{
			cursor.x + Theme::kSearchIconOffsetX * scale,
			cursor.y + ContentOffsetY(frameHeight, iconSize)
		};
		DrawCenteredIcon(
			ImGui::GetWindowDrawList(),
			PhosphorGlyph::kMagnifyingGlass,
			{
				iconPosition,
				{ iconPosition.x + iconSize, iconPosition.y + iconSize }
			},
			iconSize,
			IconColor(
				ImGui::GetColorU32(ImGuiCol_Text),
				Theme::kSearchIconAlpha));
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(5);
		ImGui::PopID();
	}

	float SettingsActionButtonWidth(
		SettingsAction a_action,
		const char* a_fallbackLabel,
		float a_buttonExtent) noexcept
	{
		const auto presentation = ResolveSettingsActionButtonPresentation(
			a_action,
			HasIconGlyph(SettingsActionGlyph(a_action)));
		const auto labelWidth = a_fallbackLabel ?
			ImGui::CalcTextSize(a_fallbackLabel).x :
			0.0f;
		return a_action == SettingsAction::kReset ?
			SettingsTable::ResolveResetColumnWidth(
				!presentation.useTextFallback,
				labelWidth,
				a_buttonExtent,
				ImGui::GetStyle().FramePadding.x) :
			ActionButtonWidth(
				!presentation.useTextFallback,
				labelWidth,
				a_buttonExtent,
				ImGui::GetStyle().FramePadding.x);
	}

	bool DrawSettingsActionButton(
		const char* a_id,
		const ImVec2& a_origin,
		const ImVec2& a_size,
		SettingsAction a_action,
		const char* a_fallbackLabel,
		const char* a_tooltip,
		bool a_enabled) noexcept
	{
		const auto presentation = ResolveSettingsActionButtonPresentation(
			a_action,
			HasIconGlyph(SettingsActionGlyph(a_action)));
		return DrawEnabled(
			a_enabled,
			[&]() noexcept {
				return DrawCompactChromeButton(
					a_id,
					a_origin,
					a_size,
					presentation.glyph,
					presentation.useTextFallback ? a_fallbackLabel : nullptr,
					a_tooltip,
					presentation.useTextFallback ?
						ImGui::GetColorU32(ImGuiCol_Text) :
						IconColor(ImGui::GetColorU32(ImGuiCol_Text)));
			});
	}

	void DrawCollapsingSectionHeader(
		const char* a_key,
		const char* a_text,
		char32_t a_glyph,
		bool& a_expanded,
		size_t a_count) noexcept
	{
		DrawRuledHeading({
			.key = a_key,
			.text = a_text,
			.glyph = a_glyph,
			.count = a_count,
			.expanded = &a_expanded
		});
	}

	void DrawBulletText(const char* a_text) noexcept
	{
		ImGui::Bullet();
		ImGui::TextWrapped("%s", a_text ? a_text : "");
	}

	void DrawSectionHeader(const char* a_text)
	{
		DrawSectionHeader(
			a_text,
			ResolveIconGlyph(IconKind::kCategory, {}, a_text ? a_text : ""));
	}

	void DrawSectionHeader(const char* a_text, char32_t a_glyph) noexcept
	{
		DrawRuledHeading({
			.text = a_text,
			.glyph = a_glyph
		});
	}
}
