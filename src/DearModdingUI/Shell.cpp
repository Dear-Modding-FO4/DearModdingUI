#include <DearModdingUI/Shell.h>
#include <DearModdingUI/BackgroundBlur.h>
#include <DearModdingUI/Host.h>
#include <DearModdingUI/HostSettings.h>
#include <DearModdingUI/HostSettingsView.h>
#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/SettingsTable.h>
#include <DearModdingUI/Status.h>
#include <DearModdingUI/Theme.h>
#include <DearModdingUI/VisualDecisions.h>
#include <DearModdingUI/SidebarComparison.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <map>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace DearModdingUI
{
	namespace
	{
		inline constexpr char kCommandPalettePopupId[] =
			"Search mods, pages, and actions###DearModdingPalette";
		inline constexpr float kSidebarModFontScale{ 0.8f };
		inline constexpr size_t kMinimumVisiblePageRows{ 3 };

		struct ShellState : ClientSelectionState
		{
			std::map<std::string, bool> categoryExpansion;
			std::map<std::string, bool> modExpansion;
			std::optional<std::vector<std::string>> previewExpandedClients;
			std::optional<SidebarLayoutKind> previewSidebarLayoutOverride;
			std::string paletteQuery;
			size_t paletteSelection{ 0 };
			SidebarLayoutKind sidebarLayout{ DEFAULT_SIDEBAR_LAYOUT };
			DrillDownState drillDown;
			bool drillDownInitialized{ false };
			bool paletteOpenRequested{ false };
			bool paletteFocusRequested{ false };
			bool paletteVisible{ false };
		};

		[[nodiscard]] ShellState& State() noexcept
		{
			static ShellState state;
			return state;
		}

		[[nodiscard]] bool HasIconGlyph(char32_t a_glyph) noexcept
		{
			if (!a_glyph)
				return false;
			auto* font = ImGui::GetFont();
			return font &&
				font->IsGlyphInFont(static_cast<ImWchar>(a_glyph));
		}

		[[nodiscard]] ImU32 IconColor(
			ImU32 a_textColor,
			float a_alpha = 1.0f) noexcept
		{
			auto tint = Theme::IconTint();
			tint.w *=
				ImGui::ColorConvertU32ToFloat4(a_textColor).w * a_alpha;
			return ImGui::ColorConvertFloat4ToU32(tint);
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

		[[nodiscard]] RowContentMetrics CurrentFontRowContentMetrics(
			float a_fontSize,
			const char* a_text = nullptr,
			const char* a_textEnd = nullptr) noexcept
		{
			RowContentMetrics metrics{ a_fontSize };
			auto* font = ImGui::GetFont();
			if (!font || a_fontSize <= 0.0f)
				return metrics;
			if (auto* baked = font->GetFontBaked(a_fontSize);
				baked && baked->Size > 0.0f)
			{
				const ImFontGlyph* reference = nullptr;
				if (a_text)
				{
					const auto* textEnd = a_textEnd ?
						a_textEnd :
						a_text + std::strlen(a_text);
					auto minY = FLT_MAX;
					auto maxY = -FLT_MAX;
					for (auto* cursor = a_text;
						cursor < textEnd && *cursor;)
					{
						unsigned int character{};
						const auto length = ImTextCharFromUtf8(
							&character,
							cursor,
							textEnd);
						if (length <= 0)
							break;
						cursor += length;
						const auto* glyph = baked->FindGlyphNoFallback(
							static_cast<ImWchar>(character));
						if (!glyph || !glyph->Visible)
							continue;
						minY = (std::min)(minY, glyph->Y0);
						maxY = (std::max)(maxY, glyph->Y1);
					}
					if (maxY > minY)
					{
						metrics.opticalMinY = minY;
						metrics.opticalMaxY = maxY;
					}
				}
				else
				{
					reference = baked->FindGlyphNoFallback(
						static_cast<ImWchar>('H'));
				}
				if (reference)
				{
					metrics.opticalMinY = reference->Y0;
					metrics.opticalMaxY = reference->Y1;
				}
				if (metrics.opticalMaxY > metrics.opticalMinY)
					metrics.opticalScale = a_fontSize / baked->Size;
			}
			return metrics;
		}

		[[nodiscard]] float DrawIconText(
			const ImVec2& a_position,
			float a_height,
			char32_t a_glyph,
			const char* a_text,
			ImU32 a_color,
			const ImVec4* a_clip = nullptr) noexcept
		{
			const auto textSize = ImGui::CalcTextSize(a_text);
			const auto layout = DecideInlineIconLayout(
				HasIconGlyph(a_glyph),
				textSize.x,
				textSize.y,
				ImGui::GetFontSize(),
				ImGui::GetStyle().ItemSpacing.x);
			const auto contentY = a_position.y + RowContentOffsetY(
				a_height,
				{ layout.contentHeight },
				RowContentMetric::kBox);
			const auto textOffsetY = RowContentOffsetY(
				layout.contentHeight,
				{ textSize.y },
				RowContentMetric::kBox);
			const auto textMetrics = CurrentFontRowContentMetrics(
				ImGui::GetFontSize(),
				a_text);
			const auto textCenterOffsetY =
				textMetrics.opticalMaxY > textMetrics.opticalMinY &&
						textMetrics.opticalScale > 0.0f ?
					(textMetrics.opticalMinY + textMetrics.opticalMaxY) *
						textMetrics.opticalScale * 0.5f :
					textSize.y * 0.5f;
			const auto textCenterY =
				contentY + textOffsetY + textCenterOffsetY;
			if (layout.drawIcon)
			{
				DrawCenteredIcon(
					ImGui::GetWindowDrawList(),
					a_glyph,
					ImRect{
						{
							a_position.x,
							textCenterY - layout.iconSize * 0.5f
						},
						{
							a_position.x + layout.iconSize,
							textCenterY + layout.iconSize * 0.5f
						}
					},
					layout.iconSize,
					IconColor(a_color),
					a_clip);
			}
			ImGui::GetWindowDrawList()->AddText(
				ImGui::GetFont(),
				ImGui::GetFontSize(),
				{
					a_position.x + layout.textOffset,
					contentY + textOffsetY
				},
				a_color,
				a_text,
				nullptr,
				0.0f,
				a_clip);
			return textCenterY;
		}

		[[nodiscard]] float GetPillRounding(
			const ImVec2& a_min,
			const ImVec2& a_max) noexcept
		{
			return ImMin(a_max.x - a_min.x, a_max.y - a_min.y) * 0.5f;
		}

		[[nodiscard]] bool DrawRoundedButtonHighlight(
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
				GetPillRounding(a_min, a_max));
			a_drawList->AddRectFilled(
				a_min,
				a_max,
				ImGui::GetColorU32(
					a_active ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered),
				rounding);
			return true;
		}

		enum class RowLeadingAffordance : uint32_t
		{
			kNone,
			kArrow,
			kBack,
			kIcon
		};

		enum class RowHighlightStyle : uint32_t
		{
			kSelectable,
			kRoundedFill
		};

		enum class RowClickBehavior : uint32_t
		{
			kSelect,
			kToggle
		};

		struct RowOptions
		{
			const char* id{ nullptr };
			const char* label{ nullptr };
			bool selected{ false };
			float height{ 0.0f };
			RowLeadingAffordance leadingAffordance{
				RowLeadingAffordance::kNone
			};
			bool* expanded{ nullptr };
			char32_t glyph{ 0 };
			ImU32 textColor{ 0 };
			ImU32 hoveredTextColor{ 0 };
			std::optional<ImU32> disabledColor;
			float trailingWidth{ 0.0f };
			bool flushHorizontalHighlight{ false };
			RowHighlightStyle highlightStyle{
				RowHighlightStyle::kSelectable
			};
			RowClickBehavior clickBehavior{ RowClickBehavior::kSelect };
			bool centerGlyph{ false };
		};

		struct RowResult
		{
			bool pressed{ false };
			ImRect rect;
		};

		struct RowInteraction
		{
			bool pressed{ false };
			bool hovered{ false };
			bool arrowPressed{ false };
			bool expanded{ false };
			bool splitArrow{ false };
			ImRect arrowRect;
		};

		[[nodiscard]] ImRect SelectableRowContentRect() noexcept
		{
			const auto min = ImGui::GetItemRectMin();
			const auto max = ImGui::GetItemRectMax();
			const auto content = ResolveRowContentRect(
				RowContentRectKind::kSelectable,
				{ min.x, min.y, max.x, max.y });
			return {
				{ content.minX, content.minY },
				{ content.maxX, content.maxY }
			};
		}

		[[nodiscard]] RowResult DrawRow(
			const RowOptions& a_options,
			const ImRect& a_contentRect,
			const RowInteraction& a_interaction) noexcept
		{
			auto* drawList = ImGui::GetWindowDrawList();
			if (a_options.highlightStyle == RowHighlightStyle::kRoundedFill &&
				(a_interaction.hovered || a_options.selected))
			{
				drawList->AddRectFilled(
					a_contentRect.Min,
					a_contentRect.Max,
					ImGui::GetColorU32(
						a_interaction.hovered ?
							ImGuiCol_HeaderHovered :
							ImGuiCol_Header),
					ImGui::GetStyle().FrameRounding);
			}

			const auto textColor = a_options.disabledColor.value_or(
				a_interaction.hovered ?
					a_options.hoveredTextColor :
					a_options.textColor);
			const auto fontSize = ImGui::GetFontSize();
			const auto hasGlyph = HasIconGlyph(a_options.glyph);
			const auto hasLeadingSlot =
				(a_options.leadingAffordance == RowLeadingAffordance::kArrow &&
					!a_interaction.splitArrow) ||
				a_options.leadingAffordance == RowLeadingAffordance::kBack;
			const auto hasIconSlot =
				hasGlyph ||
				(a_options.leadingAffordance == RowLeadingAffordance::kIcon &&
					!a_options.glyph);
			const auto content = ResolveRowContentLayout(
				a_contentRect.Min.x,
				a_contentRect.Max.x,
				ImGui::GetStyle().FramePadding.x,
				fontSize,
				ImGui::GetStyle().ItemInnerSpacing.x,
				ImGui::GetStyle().ItemSpacing.x,
				hasLeadingSlot,
				hasIconSlot,
				a_options.trailingWidth);
			const ImVec4 clip{
				a_contentRect.Min.x,
				a_contentRect.Min.y,
				content.clipMaxX,
				a_contentRect.Max.y
			};

			if (a_interaction.splitArrow)
			{
				ImGui::RenderArrow(
					drawList,
					{
						a_interaction.arrowRect.Min.x + (std::max)(
							(a_interaction.arrowRect.GetWidth() - fontSize) *
								0.5f,
							0.0f),
						a_contentRect.Min.y + RowContentOffsetY(
							a_contentRect.GetHeight(),
							{ fontSize },
							RowContentMetric::kBox)
					},
					textColor,
					a_interaction.expanded ?
						ImGuiDir_Down :
						ImGuiDir_Right);
			}
			else if (hasLeadingSlot)
			{
				ImGui::RenderArrow(
					drawList,
					{
						content.leadingMinX,
						a_contentRect.Min.y + RowContentOffsetY(
							a_contentRect.GetHeight(),
							{ fontSize },
							RowContentMetric::kBox)
					},
					textColor,
					a_options.leadingAffordance == RowLeadingAffordance::kBack ?
						ImGuiDir_Left :
						(a_interaction.expanded ?
							ImGuiDir_Down :
							ImGuiDir_Right));
			}

			if (hasGlyph)
			{
				DrawCenteredIcon(
					drawList,
					a_options.glyph,
					a_options.centerGlyph ?
						a_contentRect :
						ImRect{
							{ content.iconMinX, a_contentRect.Min.y },
							{
								content.iconMinX + fontSize,
								a_contentRect.Max.y
							}
						},
					fontSize,
					IconColor(textColor),
					&clip);
			}

			const auto* labelEnd = ImGui::FindRenderedTextEnd(a_options.label);
			if (!a_options.centerGlyph &&
				content.textMinX < content.clipMaxX)
			{
				drawList->AddText(
					ImGui::GetFont(),
					fontSize,
					{
						content.textMinX,
						a_contentRect.Min.y + RowContentOffsetY(
							a_contentRect.GetHeight(),
							CurrentFontRowContentMetrics(
								fontSize,
								a_options.label,
								labelEnd),
							RowContentMetric::kOptical)
					},
					textColor,
					a_options.label,
					labelEnd,
					0.0f,
					&clip);
			}

			if (a_options.expanded &&
				(a_interaction.arrowPressed ||
					(a_interaction.pressed &&
						a_options.clickBehavior == RowClickBehavior::kToggle)))
				*a_options.expanded = !*a_options.expanded;
			return { a_interaction.pressed, a_contentRect };
		}

		[[nodiscard]] RowResult DrawSelectableRow(
			const RowOptions& a_options) noexcept
		{
			auto* window = ImGui::GetCurrentWindow();
			if (!window || window->SkipItems || !a_options.id || !a_options.label)
				return {};

			const auto height = a_options.height > 0.0f ?
				a_options.height :
				ImGui::GetFrameHeight();
			const auto expanded = a_options.expanded && *a_options.expanded;
			const auto splitArrow =
				a_options.leadingAffordance == RowLeadingAffordance::kArrow &&
				a_options.clickBehavior == RowClickBehavior::kSelect;
			auto arrowPressed = false;
			ImRect arrowRect;

			ImGui::PushID(a_options.id);
			if (splitArrow)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4());
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4());
				ImGui::PushStyleColor(
					ImGuiCol_ButtonHovered,
					Theme::kFullPalette[ImGuiCol_HeaderHovered]);
				ImGui::PushStyleColor(
					ImGuiCol_ButtonActive,
					Theme::kFullPalette[ImGuiCol_HeaderActive]);
				arrowPressed = ImGui::ArrowButtonEx(
					"##LeadingAffordance",
					expanded ? ImGuiDir_Down : ImGuiDir_Right,
					{ height, height },
					ImGuiButtonFlags_None);
				ImGui::PopStyleColor(4);
				arrowRect = {
					ImGui::GetItemRectMin(),
					ImGui::GetItemRectMax()
				};
				ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			}

			const ImVec2 size{
				(std::max)(ImGui::GetContentRegionAvail().x, 0.0f),
				height
			};
			if (a_options.flushHorizontalHighlight)
			{
				ImGui::PushStyleVar(
					ImGuiStyleVar_ItemSpacing,
					{ 0.0f, ImGui::GetStyle().ItemSpacing.y });
			}
			auto pressed = false;
			if (a_options.highlightStyle == RowHighlightStyle::kSelectable)
			{
				pressed = ImGui::Selectable(
					"##Row",
					a_options.selected,
					ImGuiSelectableFlags_None,
					size);
			}
			else
			{
				pressed = ImGui::InvisibleButton("##Row", size);
			}
			const auto hovered = ImGui::IsItemHovered();
			const auto contentRect = SelectableRowContentRect();
			if (a_options.flushHorizontalHighlight)
				ImGui::PopStyleVar();
			const auto result = DrawRow(
				a_options,
				contentRect,
				{
					.pressed = pressed,
					.hovered = hovered,
					.arrowPressed = arrowPressed,
					.expanded = expanded,
					.splitArrow = splitArrow,
					.arrowRect = arrowRect
				});
			ImGui::PopID();
			return result;
		}

		inline constexpr float kCloseCrossDiagonalScale{
			0.5f / std::numbers::sqrt2_v<float>
		};
		inline constexpr float kCloseCrossInsetAtBaseline{ 1.0f };
		inline constexpr ImVec4 kTransparentButtonChrome{ 0, 0, 0, 0 };
		[[nodiscard]] float TitleBarButtonPadding() noexcept
		{
			return ResolveTitleBarButtonPadding(
				ImGui::GetStyle().FramePadding.y);
		}

		[[nodiscard]] float SeparatorThickness() noexcept
		{
			return Theme::kSeparatorThickness * Theme::Scale();
		}

		[[nodiscard]] ImRect TitleBarButtonRect(
			const ImVec2& a_origin,
			float a_fontSize) noexcept
		{
			const auto full =
				a_fontSize + TitleBarButtonPadding() * 2.0f;
			return { a_origin, { a_origin.x + full, a_origin.y + full } };
		}

		[[nodiscard]] ImVec2 RightTitleBarButtonOrigin(
			ImGuiWindow* a_window,
			float a_fontSize,
			float a_offset = 0.0f) noexcept
		{
			const auto& style = ImGui::GetStyle();
			const auto buttonPadding = TitleBarButtonPadding();
			return {
				RightTitleBarButtonOriginX(
					a_window->Rect().Max.x,
					a_window->WindowBorderSize,
					style.FramePadding.x,
					a_fontSize,
					a_offset,
					buttonPadding),
				a_window->Rect().Min.y +
					style.FramePadding.y -
					buttonPadding
			};
		}

		[[nodiscard]] bool IsTitleBarButtonHovered(
			ImGuiWindow* a_window,
			const ImRect& a_bounds) noexcept
		{
			auto& context = *ImGui::GetCurrentContext();
			return context.HoveredWindow == a_window &&
				ImGui::IsMouseHoveringRect(
					a_bounds.Min, a_bounds.Max, false);
		}

		class NativeTitleBarButtonHighlightGuard
		{
		public:
			NativeTitleBarButtonHighlightGuard() noexcept
			{
				ImGui::PushStyleColor(
					ImGuiCol_ButtonHovered, kTransparentButtonChrome);
				ImGui::PushStyleColor(
					ImGuiCol_ButtonActive, kTransparentButtonChrome);
			}

			~NativeTitleBarButtonHighlightGuard() noexcept
			{
				ImGui::PopStyleColor(2);
			}

			NativeTitleBarButtonHighlightGuard(
				const NativeTitleBarButtonHighlightGuard&) = delete;
			NativeTitleBarButtonHighlightGuard& operator=(
				const NativeTitleBarButtonHighlightGuard&) = delete;
		};

		void DrawRoundedCloseHighlight(ImGuiWindow* a_window) noexcept
		{
			if (!a_window ||
				(a_window->Flags & ImGuiWindowFlags_NoTitleBar))
				return;

			const auto size = ImGui::GetFontSize();
			const auto position = RightTitleBarButtonOrigin(a_window, size);
			const auto bounds = TitleBarButtonRect(position, size);
			const auto hovered = IsTitleBarButtonHovered(a_window, bounds);
			const auto held = hovered &&
				ImGui::IsMouseDown(ImGuiMouseButton_Left);

			a_window->DrawList->PushClipRect(
				a_window->Rect().Min, a_window->Rect().Max);
			if (DrawRoundedButtonHighlight(
					bounds.Min,
					bounds.Max,
					hovered,
					held,
					a_window->DrawList))
			{
				const auto center = bounds.GetCenter();
				const auto diagonal =
					size * kCloseCrossDiagonalScale -
					kCloseCrossInsetAtBaseline * Theme::Scale();
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

		[[nodiscard]] bool BeginWithRoundedTitleBarButtons(
			const char* a_name,
			bool* a_open,
			ImGuiWindowFlags a_flags) noexcept
		{
			bool visible = false;
			{
				const NativeTitleBarButtonHighlightGuard guard;
				visible = ImGui::Begin(a_name, a_open, a_flags);
			}
			auto* window = ImGui::GetCurrentWindowRead();
			DrawRoundedCloseHighlight(window);
			return visible;
		}

		[[nodiscard]] bool BeginPopupModalWithRoundedTitleBarButtons(
			const char* a_name,
			bool* a_open,
			ImGuiWindowFlags a_flags) noexcept
		{
			bool visible = false;
			{
				const NativeTitleBarButtonHighlightGuard guard;
				visible = ImGui::BeginPopupModal(a_name, a_open, a_flags);
			}
			if (visible)
				DrawRoundedCloseHighlight(ImGui::GetCurrentWindowRead());
			return visible;
		}

		[[nodiscard]] bool DrawCompactChromeButton(
			const char* a_id,
			const ImVec2& a_origin,
			const ImVec2& a_size,
			char32_t a_glyph,
			const char* a_text,
			const char* a_tooltip,
			ImU32 a_color,
			bool a_active = false,
			float a_glyphSize = 0.0f) noexcept
		{
			auto* window = ImGui::GetCurrentWindow();
			if (!window)
				return false;

			const auto restore = ImGui::GetCursorScreenPos();
			ImGui::SetCursorScreenPos(a_origin);
			const auto pressed = ImGui::InvisibleButton(a_id, a_size);
			const auto hovered = ImGui::IsItemHovered(
				ImGuiHoveredFlags_AllowWhenDisabled);
			const auto held = ImGui::IsItemActive();
			const ImRect bounds{
				a_origin, { a_origin.x + a_size.x, a_origin.y + a_size.y }
			};

			(void)DrawRoundedButtonHighlight(
				bounds.Min,
				bounds.Max,
				hovered || a_active,
				held,
				window->DrawList);
			if (a_glyph)
			{
				const auto glyphSize = a_glyphSize > 0.0f ?
					a_glyphSize :
					ImGui::GetFontSize();
				DrawCenteredIcon(
					window->DrawList,
					a_glyph,
					bounds,
					glyphSize,
					a_color,
					nullptr);
			}
			else if (a_text)
			{
				const auto textSize = ImGui::CalcTextSize(a_text);
				const ImVec2 textOrigin{
					bounds.Min.x,
					bounds.Min.y + RowContentOffsetY(
						a_size.y,
						CurrentFontRowContentMetrics(ImGui::GetFontSize()),
						RowContentMetric::kOptical)
				};
				ImGui::PushStyleColor(ImGuiCol_Text, a_color);
				ImGui::RenderTextClipped(
					textOrigin,
					bounds.Max,
					a_text,
					nullptr,
					&textSize,
					{ 0.5f, 0.0f },
					&bounds);
				ImGui::PopStyleColor();
			}
			if (hovered && a_tooltip)
				ImGui::SetTooltip("%s", a_tooltip);
			ImGui::SetCursorScreenPos(restore);
			// A restored cursor must be followed by an item or ImGui reports an unbounded SetCursorPos.
			ImGui::Dummy({ 0.0f, 0.0f });
			return pressed;
		}

		struct HostSettingsTitleButton
		{
			SettingsAction action;
			const char* id;
			const char* label;
			const char* tooltip;
			float width;
		};

		struct TitleRowButton
		{
			const char* id;
			float width;
			char32_t glyph;
			const char* fallbackLabel;
			const char* tooltip;
			bool enabled{ true };
			bool active{ false };
		};

		struct TitleRowOptions
		{
			const char* title;
			Theme::FontRole titleFont{ Theme::FontRole::kTitle };
			float titleScale{ 1.0f };
			float titleInsetX{};
			std::span<const TitleRowButton> buttons;
			TitleRowButtonExtentPolicy buttonExtentPolicy{
				TitleRowButtonExtentPolicy::kTitleBar
			};
			bool drawSeparator{ true };
			const char* summary{};
		};

		template<class Draw>
		[[nodiscard]] bool DrawEnabledControl(
			bool a_enabled,
			Draw a_draw) noexcept
		{
			// Disabled controls render normally but never report a press.
			ImGui::BeginDisabled(!a_enabled);
			const auto pressed = a_draw();
			ImGui::EndDisabled();
			return pressed && a_enabled;
		}

		[[nodiscard]] std::optional<size_t> DrawTitleRow(
			const TitleRowOptions& a_options) noexcept
		{
			const auto start = ImGui::GetCursorScreenPos();
			const auto contentMaxX =
				start.x + ImGui::GetContentRegionAvail().x;
			const auto bodyFontSize = ImGui::GetFontSize();
			const auto buttonExtent = ResolveTitleRowButtonExtent(
				a_options.buttonExtentPolicy,
				bodyFontSize,
				TitleBarButtonPadding());
			const auto buttonGlyphSize =
				a_options.buttonExtentPolicy ==
						TitleRowButtonExtentPolicy::kHostChrome ?
					HostChromeIconSize(bodyFontSize) :
					bodyFontSize;
			float buttonWidthSum{};
			for (const auto& button : a_options.buttons)
				buttonWidthSum += (std::max)(button.width, 0.0f);
			const auto layout = ResolvePageActionRowLayout(
				start.x,
				contentMaxX,
				buttonWidthSum,
				a_options.buttons.size(),
				ImGui::GetStyle().ItemSpacing.x);
			const auto titleMinX =
				start.x + (std::max)(a_options.titleInsetX, 0.0f);
			ImVec2 titleSize{};
			float rowHeight{};
			{
				const Theme::FontGuard font{
					a_options.titleFont,
					a_options.titleScale
				};
				titleSize = ImGui::CalcTextSize(a_options.title);
				const auto titleMetrics = CurrentFontRowContentMetrics(
					ImGui::GetFontSize(),
					a_options.title);
				rowHeight = (std::max)(
					titleSize.y,
					a_options.buttons.empty() ? 0.0f : buttonExtent);
				// Grow when centered ink would otherwise need a negative origin.
				if (titleMetrics.opticalMaxY > titleMetrics.opticalMinY &&
					titleMetrics.opticalScale > 0.0f)
				{
					rowHeight = (std::max)(
						rowHeight,
						(titleMetrics.opticalMinY +
							titleMetrics.opticalMaxY) *
							titleMetrics.opticalScale);
				}
				const ImVec2 titlePosition{
					titleMinX,
					start.y + RowContentOffsetY(
						rowHeight,
						titleMetrics,
						RowContentMetric::kOptical)
				};
				if (layout.titleMaxX > titleMinX)
				{
					ImGui::RenderTextEllipsis(
						ImGui::GetWindowDrawList(),
						titlePosition,
						{ layout.titleMaxX, start.y + rowHeight },
						layout.titleMaxX,
						a_options.title,
						nullptr,
						&titleSize);
				}
				ImGui::Dummy({
					contentMaxX - start.x,
					rowHeight
				});
			}

			std::optional<size_t> pressedIndex;
			auto positionX = layout.actionsMinX;
			for (size_t index = 0;
				index < a_options.buttons.size();
				++index)
			{
				const auto& button = a_options.buttons[index];
				ImGui::PushID(button.id);
				const auto pressed = DrawEnabledControl(
					button.enabled,
					[&]() noexcept {
						return DrawCompactChromeButton(
							"##DearModdingUI.TitleRowButton",
							{
								positionX,
								start.y + RowContentOffsetY(
									rowHeight,
									{ buttonExtent },
									RowContentMetric::kBox)
							},
							{ button.width, buttonExtent },
							button.glyph,
							button.glyph ? nullptr : button.fallbackLabel,
							button.tooltip,
							button.glyph ?
								IconColor(ImGui::GetColorU32(ImGuiCol_Text)) :
								ImGui::GetColorU32(ImGuiCol_Text),
							button.active,
							buttonGlyphSize);
					});
				ImGui::PopID();
				if (pressed)
					pressedIndex = index;
				positionX +=
					button.width + ImGui::GetStyle().ItemSpacing.x;
			}

			auto contentBottomY = start.y + rowHeight;
			if (a_options.summary && *a_options.summary)
			{
				ImGui::SetCursorScreenPos({
					start.x,
					contentBottomY + ImGui::GetStyle().ItemInnerSpacing.y
				});
				auto color = Theme::kFullPalette[ImGuiCol_Text];
				color.w *= Theme::kVersionTextOpacity;
				const Theme::FontGuard font{ Theme::FontRole::kSubtext };
				ImGui::PushStyleColor(ImGuiCol_Text, color);
				ImGui::TextWrapped("%s", a_options.summary);
				ImGui::PopStyleColor();
				contentBottomY = ImGui::GetItemRectMax().y;
			}
			if (a_options.drawSeparator)
			{
				ImGui::SetCursorScreenPos({
					start.x,
					contentBottomY + ImGui::GetStyle().ItemSpacing.y
				});
				ImGui::SeparatorEx(
					ImGuiSeparatorFlags_Horizontal,
					SeparatorThickness());
				ImGui::Spacing();
			}
			return pressedIndex;
		}

		[[nodiscard]] bool DrawHeader(
			const NavigationModel& a_model,
			const ShellState& a_state,
			bool a_drawClose) noexcept
		{
			const auto* client = a_model.FindClient(a_state.activeClient);
			const auto breadcrumb = BuildHostBreadcrumb(
				"Evil Modding",
				HostSettings::IsPanelOpen() ?
					std::string_view{ "Interface Settings" } :
					(a_state.activeHostPage == HostPageKind::kHome ?
							kHostHomePage.displayName :
						client ?
							std::string_view{ client->displayName } :
							std::string_view{}));
			const auto textScale = Theme::kHeaderFallbackTextScale;
			constexpr auto extentPolicy =
				TitleRowButtonExtentPolicy::kHostChrome;
			const auto buttonExtent = ResolveTitleRowButtonExtent(
				extentPolicy,
				ImGui::GetFontSize(),
				TitleBarButtonPadding());
			const auto hasCloseGlyph = HasIconGlyph(PhosphorGlyph::kX);
			constexpr const char* closeLabel{ "Close" };
			const auto buttonWidth = ActionButtonWidth(
				hasCloseGlyph,
				ImGui::CalcTextSize(closeLabel).x,
				buttonExtent,
				ImGui::GetStyle().FramePadding.x);
			const TitleRowButton closeButton{
				"##DearModdingUI.HostCloseButton",
				buttonWidth,
				hasCloseGlyph ? PhosphorGlyph::kX : char32_t{},
				closeLabel,
				"Close menu"
			};
			const std::span<const TitleRowButton> buttons{
				&closeButton,
				a_drawClose ? size_t{ 1 } : size_t{}
			};
			return DrawTitleRow({
				.title = breadcrumb.c_str(),
				.titleScale = textScale,
				.titleInsetX = BulletRunContentInset(
					ImGui::GetStyle().FramePadding.x,
					ImGui::GetFontSize()),
				.buttons = buttons,
				.buttonExtentPolicy = extentPolicy
			}).has_value();
		}

		void DrawCategoryHeader(
			const char* a_key,
			const NavigationClient& a_client,
			const char* a_name,
			bool& a_expanded,
			size_t a_count) noexcept
		{
			char text[256]{};
			std::snprintf(text, sizeof(text), "%s (%zu)", a_name, a_count);
			const auto glyph = ResolveCategoryIconGlyph(
				a_name,
				a_client.displayName,
				a_client.id,
				a_client.iconName);
			auto color = Theme::kFeatureHeadingDefaults.colorDefault;
			auto hoveredColor =
				Theme::kFeatureHeadingDefaults.colorHovered;
			if (!a_expanded)
			{
				color.w *= Theme::kFeatureHeadingDefaults.minimizedFactor;
				hoveredColor.w *=
					Theme::kFeatureHeadingDefaults.minimizedFactor;
			}
			(void)DrawSelectableRow({
				.id = a_key,
				.label = text,
				.leadingAffordance = RowLeadingAffordance::kArrow,
				.expanded = &a_expanded,
				.glyph = glyph,
				.textColor = ImGui::GetColorU32(color),
				.hoveredTextColor = ImGui::GetColorU32(hoveredColor),
				.highlightStyle = RowHighlightStyle::kRoundedFill,
				.clickBehavior = RowClickBehavior::kToggle
			});
		}

		void DrawPaletteAffordance(ShellState& a_state) noexcept
		{
			const auto position = ImGui::GetCursorScreenPos();
			const auto size = ImVec2{
				ImGui::GetContentRegionAvail().x,
				ImGui::GetFrameHeight()
			};
			const auto pressed = ImGui::InvisibleButton(
				"##OpenNavigationPalette",
				size);
			const auto hovered = ImGui::IsItemHovered();
			const auto held = ImGui::IsItemActive();
			const auto color = ImGui::GetColorU32(
				held ?
					ImGuiCol_FrameBgActive :
					(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg));
			ImGui::RenderFrame(
				position,
				{ position.x + size.x, position.y + size.y },
				color,
				true,
				ImGui::GetStyle().FrameRounding);

			const auto scale = Theme::SearchScale();
			const auto iconSize = Theme::kSearchIconSize * scale;
			const auto iconPosition = ImVec2{
				position.x + ImGui::GetStyle().FramePadding.x,
				position.y + RowContentOffsetY(
					size.y,
					{ iconSize },
					RowContentMetric::kBox)
			};
			DrawCenteredIcon(
				ImGui::GetWindowDrawList(),
				PhosphorGlyph::kMagnifyingGlass,
				{
					iconPosition,
					{
						iconPosition.x + iconSize,
						iconPosition.y + iconSize
					}
				},
				iconSize,
				IconColor(
					ImGui::GetColorU32(ImGuiCol_Text),
					Theme::kSearchIconAlpha),
				nullptr);

			constexpr auto hint = "Search mods, pages, and actions...";
			const auto textSize = ImGui::CalcTextSize(hint);
			const auto textPosition = ImVec2{
				iconPosition.x + iconSize + ImGui::GetStyle().ItemInnerSpacing.x,
				position.y + RowContentOffsetY(
					size.y,
					{ textSize.y },
					RowContentMetric::kBox)
			};
			ImGui::RenderTextEllipsis(
				ImGui::GetWindowDrawList(),
				textPosition,
				{
					position.x + size.x - ImGui::GetStyle().FramePadding.x,
					position.y + size.y
				},
				position.x + size.x - ImGui::GetStyle().FramePadding.x,
				hint,
				nullptr,
				&textSize);
			if (pressed)
				a_state.paletteOpenRequested = true;
		}

		[[nodiscard]] std::string CategoryKey(
			const NavigationClient& a_client,
			std::string_view a_category)
		{
			return a_client.id + "/" + std::string{ a_category };
		}

		void ExpandPageAncestors(
			const NavigationModel& a_model,
			DMUI_PageHandle a_page,
			ShellState& a_state)
		{
			const auto* page = a_model.FindPage(a_page);
			const auto* client = page ?
				a_model.FindClient(page->client) :
				nullptr;
			if (!page || !client)
				return;
			a_state.modExpansion[client->id] = true;
			a_state.categoryExpansion[CategoryKey(*client, page->category)] = true;
		}

		void NavigateToPage(
			const NavigationModel& a_model,
			DMUI_PageHandle a_page,
			ShellState& a_state) noexcept;

		void ApplyPreviewExpandedClients(
			const NavigationModel& a_model,
			ShellState& a_state)
		{
			if (!a_state.previewExpandedClients)
				return;
			for (const auto& client : a_model.clients)
			{
				a_state.modExpansion[client.id] =
					std::ranges::find(
						*a_state.previewExpandedClients,
						client.id) != a_state.previewExpandedClients->end();
			}
			if (a_state.sidebarLayout == SidebarLayoutKind::DrillDown)
			{
				a_state.drillDownInitialized = true;
				if (a_state.previewExpandedClients->empty())
				{
					a_state.drillDown = TransitionDrillDown(
						a_state.drillDown,
						DrillDownEvent::Back);
				}
				else
				{
					const auto client = std::ranges::find(
						a_model.clients,
						a_state.previewExpandedClients->front(),
						&NavigationClient::id);
					if (client != a_model.clients.end())
					{
						a_state.drillDown = TransitionDrillDown(
							a_state.drillDown,
							DrillDownEvent::SelectClient,
							client->handle);
						NavigateToPage(
							a_model,
							ResolveLandingPage(*client),
							a_state);
					}
				}
			}
			a_state.previewExpandedClients.reset();
		}

		void NavigateToPage(
			const NavigationModel& a_model,
			DMUI_PageHandle a_page,
			ShellState& a_state) noexcept
		{
			const auto* page = a_model.FindPage(a_page);
			if (!page)
				return;
			HostSettings::NotifyModSelected();
			a_state.activeClient = page->client;
			a_state.activePage = page->handle;
			a_state.activeHostPage.reset();
			RecordRecentPage(a_model, page->handle, a_state);
			ExpandPageAncestors(a_model, page->handle, a_state);
			if (a_state.sidebarLayout == SidebarLayoutKind::DrillDown &&
				a_state.drillDownInitialized)
			{
				a_state.drillDown = TransitionDrillDown(
					a_state.drillDown,
					DrillDownEvent::SelectClient,
					page->client);
			}
		}

		void NavigateToHostPage(
			HostPageKind a_page,
			ShellState& a_state) noexcept
		{
			HostSettings::NotifyModSelected();
			SelectHostPage(a_page, a_state);
			if (a_state.sidebarLayout == SidebarLayoutKind::DrillDown &&
				a_state.drillDownInitialized)
			{
				a_state.drillDown = TransitionDrillDown(
					a_state.drillDown,
					DrillDownEvent::Back);
			}
		}

		[[nodiscard]] const ClientStatus* FindClientStatus(
			const std::vector<ClientStatus>& a_statuses,
			DMUI_ClientHandle a_client) noexcept
		{
			const auto found = std::ranges::find(
				a_statuses,
				a_client,
				&ClientStatus::client);
			return found != a_statuses.end() ? &*found : nullptr;
		}

		void DrawClientStatusDot(
			const ImRect& a_bounds,
			const ClientStatus* a_status,
			bool a_corner = false) noexcept
		{
			if (!a_status || !IsPersistentStatus(a_status->severity))
				return;
			const auto color =
				a_status->severity == DMUI_STATUS_SEVERITY_WARNING ?
					Theme::kStatusPaletteDefaults.warning :
					Theme::kStatusPaletteDefaults.error;
			const auto radius = ImGui::GetFontSize() *
				Theme::kSearchIconStrokeRatio;
			const auto& padding = ImGui::GetStyle().FramePadding;
			ImGui::GetWindowDrawList()->AddCircleFilled(
				a_corner ?
					ImVec2{
						a_bounds.Max.x - (std::max)(padding.x * 0.5f, radius),
						a_bounds.Min.y + (std::max)(padding.y * 0.5f, radius)
					} :
					ImVec2{
						a_bounds.Max.x - padding.x - radius,
						a_bounds.GetCenter().y
					},
				radius,
				ImGui::GetColorU32(color));
		}

		[[nodiscard]] float ClientStatusTrailingWidth(
			const ClientStatus* a_status) noexcept
		{
			return IsPersistentStatus(
				a_status ? a_status->severity : DMUI_STATUS_SEVERITY_INFO) ?
				ImGui::GetFontSize() :
				0.0f;
		}

		enum class ClientRowKind : uint32_t
		{
			Tree,
			List,
			Rail
		};

		[[nodiscard]] RowResult DrawClientNavigationRow(
			const NavigationClient& a_client,
			const ClientStatus* a_status,
			ShellState& a_state,
			ClientRowKind a_kind,
			bool* a_expanded = nullptr) noexcept
		{
			const Theme::FontGuard font{
				Theme::FontRole::kTitle,
				kSidebarModFontScale
			};
			const auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
			const auto row = DrawSelectableRow({
				.id = a_client.id.c_str(),
				.label = a_client.displayName.c_str(),
				.selected = a_client.handle == a_state.activeClient,
				.leadingAffordance =
					a_kind == ClientRowKind::Tree ?
						RowLeadingAffordance::kArrow :
						RowLeadingAffordance::kIcon,
				.expanded =
					a_kind == ClientRowKind::Tree ? a_expanded : nullptr,
				.glyph = ResolveIconGlyph(
					IconKind::kClient,
					a_client.iconName,
					a_client.id),
				.textColor = textColor,
				.hoveredTextColor = textColor,
				.trailingWidth =
					a_kind == ClientRowKind::Rail ?
						0.0f :
						ClientStatusTrailingWidth(a_status),
				.flushHorizontalHighlight =
					a_kind == ClientRowKind::Rail,
				.centerGlyph = a_kind == ClientRowKind::Rail
			});
			DrawClientStatusDot(
				row.rect,
				a_status,
				a_kind == ClientRowKind::Rail);
			if (a_kind == ClientRowKind::Rail &&
				ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
				ImGui::SetTooltip("%s", a_client.displayName.c_str());
			return row;
		}

		void DrawHostNavigationRow(ShellState& a_state) noexcept
		{
			const Theme::FontGuard font{
				Theme::FontRole::kTitle,
				kSidebarModFontScale
			};
			const auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
			const auto row = DrawSelectableRow({
				.id = "##DearModdingHostHome",
				.label = kHostHomePage.displayName.data(),
				.selected = a_state.activeHostPage == HostPageKind::kHome,
				.leadingAffordance = RowLeadingAffordance::kIcon,
				.glyph = PhosphorGlyph::kAppWindow,
				.textColor = textColor,
				.hoveredTextColor = textColor
			});
			if (row.pressed)
				NavigateToHostPage(HostPageKind::kHome, a_state);
		}

		void DrawPageRows(
			const NavigationModel& a_model,
			const NavigationClient& a_client,
			const NavigationCategory& a_category,
			ShellState& a_state) noexcept
		{
			const Theme::FontGuard font{ Theme::FontRole::kSubtext };
			for (const auto& page : a_category.pages)
			{
				const auto failed = PageFailed(page.handle);
				const auto selected = page.handle == a_state.activePage;
				const auto id = PageRowLabel(a_client, page);
				const auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
				const auto row = DrawSelectableRow({
					.id = id.c_str(),
					.label = page.displayName.c_str(),
					.selected = selected,
					.leadingAffordance = RowLeadingAffordance::kIcon,
					.textColor = textColor,
					.hoveredTextColor = textColor,
					.disabledColor = failed ?
						std::optional<ImU32>{ ImGui::GetColorU32(
							Theme::kStatusPaletteDefaults.error) } :
						std::nullopt
				});
				if (row.pressed)
					NavigateToPage(a_model, page.handle, a_state);
			}
		}

		void DrawPageList(
			const NavigationModel& a_model,
			const NavigationClient& a_client,
			ShellState& a_state) noexcept
		{
			if (a_client.categories.size() == 1)
			{
				DrawPageRows(
					a_model,
					a_client,
					a_client.categories.front(),
					a_state);
				return;
			}

			const auto pageIndent =
				ImGui::GetFontSize() + ImGui::GetStyle().ItemInnerSpacing.x;
			for (const auto& category : a_client.categories)
			{
				const auto key = CategoryKey(a_client, category.displayName);
				auto state =
					a_state.categoryExpansion.try_emplace(key, true).first;
				{
					const Theme::FontGuard font{ Theme::FontRole::kHeading };
					DrawCategoryHeader(
						key.c_str(),
						a_client,
						category.displayName.c_str(),
						state->second,
						category.pages.size());
				}
				if (!state->second)
					continue;

				ImGui::Indent(pageIndent);
				DrawPageRows(a_model, a_client, category, a_state);
				ImGui::Unindent(pageIndent);
			}
		}

		void DrawExpandedClientPages(
			const NavigationModel& a_model,
			const NavigationClient& a_client,
			ShellState& a_state) noexcept
		{
			const auto start = ImGui::GetCursorScreenPos();
			const auto& style = ImGui::GetStyle();
			const auto contentIndent =
				ImGui::GetFrameHeight() + style.ItemInnerSpacing.x;
			ImGui::Indent(contentIndent);
			DrawPageList(a_model, a_client, a_state);
			ImGui::Unindent(contentIndent);
			const auto end = ImGui::GetCursorScreenPos();
			const auto railBottom = end.y - style.ItemSpacing.y;
			const auto railThickness = style.WindowBorderSize * 0.5f;
			if (railBottom > start.y && railThickness > 0.0f)
			{
				auto railColor = style.Colors[ImGuiCol_Border];
				railColor.w *= Theme::kFeatureHeadingDefaults.minimizedFactor;
				ImGui::GetWindowDrawList()->AddLine(
					{
						start.x + style.FramePadding.x,
						start.y
					},
					{
						start.x + style.FramePadding.x,
						railBottom
					},
					ImGui::GetColorU32(railColor),
					railThickness);
			}
		}

		void DrawTreeNavigation(
			const NavigationModel& a_model,
			const std::vector<ClientStatus>& a_statuses,
			ShellState& a_state) noexcept
		{
			for (const auto& client : a_model.clients)
			{
				auto expansion =
					a_state.modExpansion.try_emplace(client.id, false).first;
				const auto* status =
					FindClientStatus(a_statuses, client.handle);
				const auto row = DrawClientNavigationRow(
					client,
					status,
					a_state,
					ClientRowKind::Tree,
					&expansion->second);
				if (row.pressed)
				{
					expansion->second = true;
					NavigateToPage(
						a_model,
						ResolveLandingPage(client),
						a_state);
				}
				if (expansion->second)
					DrawExpandedClientPages(a_model, client, a_state);
			}
		}

		void DrawTwoPaneNavigation(
			const NavigationModel& a_model,
			const std::vector<ClientStatus>& a_statuses,
			ShellState& a_state) noexcept
		{
			const auto& style = ImGui::GetStyle();
			const auto compactSpacing = style.ItemSpacing.y * 0.5f;
			float rowStride{};
			{
				const Theme::FontGuard font{
					Theme::FontRole::kTitle,
					kSidebarModFontScale
				};
				rowStride = ImGui::GetFrameHeight() + compactSpacing;
			}
			const auto dividerReserve =
				style.ItemSpacing.y + style.WindowBorderSize;
			const auto availableHeight = (std::max)(
				ImGui::GetContentRegionAvail().y - dividerReserve,
				0.0f);
			const auto minimumPagesHeight =
				ImGui::GetTextLineHeightWithSpacing() +
				ImGui::GetFrameHeightWithSpacing() *
					static_cast<float>(kMinimumVisiblePageRows);
			const auto panes = ResolveSidebarPaneHeights(
				availableHeight,
				rowStride,
				a_model.clients.size(),
				minimumPagesHeight);

			if (panes.mods > 0.0f)
			{
				if (ImGui::BeginChild(
						"##DearModdingModsPane",
						{ 0.0f, panes.mods }))
				{
					ImGui::PushStyleVar(
						ImGuiStyleVar_ItemSpacing,
						{ style.ItemSpacing.x, compactSpacing });
					for (const auto& client : a_model.clients)
					{
						const auto* status =
							FindClientStatus(a_statuses, client.handle);
						const auto row = DrawClientNavigationRow(
							client,
							status,
							a_state,
							ClientRowKind::List);
						if (row.pressed)
						{
							NavigateToPage(
								a_model,
								ResolveLandingPage(client),
								a_state);
						}
					}
					ImGui::PopStyleVar();
				}
				ImGui::EndChild();
			}

			ImGui::Separator();
			if (ImGui::BeginChild(
					"##DearModdingPagesPane",
					{ 0.0f, -FLT_MIN }))
			{
				DrawSectionHeader("Pages", PhosphorGlyph::kFiles);
				ImGui::Spacing();
				if (const auto* client =
						a_model.FindClient(a_state.activeClient))
					DrawPageList(a_model, *client, a_state);
				else
					ImGui::TextDisabled("Select a mod to browse its pages.");
			}
			ImGui::EndChild();
		}

		void DrawDrillDownNavigation(
			const NavigationModel& a_model,
			const std::vector<ClientStatus>& a_statuses,
			ShellState& a_state) noexcept
		{
			const auto* selectedClient =
				a_state.drillDown.level == DrillDownLevel::Pages ?
					a_model.FindClient(a_state.drillDown.client) :
					nullptr;
			if (!selectedClient)
			{
				a_state.drillDown = TransitionDrillDown(
					a_state.drillDown,
					DrillDownEvent::Back);
				for (const auto& client : a_model.clients)
				{
					const auto* status =
						FindClientStatus(a_statuses, client.handle);
					const auto row = DrawClientNavigationRow(
						client,
						status,
						a_state,
						ClientRowKind::List);
					if (row.pressed)
					{
						a_state.drillDown = TransitionDrillDown(
							a_state.drillDown,
							DrillDownEvent::SelectClient,
							client.handle);
						NavigateToPage(
							a_model,
							ResolveLandingPage(client),
							a_state);
					}
				}
				return;
			}

			{
				const Theme::FontGuard font{
					Theme::FontRole::kTitle,
					kSidebarModFontScale
				};
				const auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
				const auto row = DrawSelectableRow({
					.id = "##DearModdingDrillDownBack",
					.label = "All Mods",
					.leadingAffordance = RowLeadingAffordance::kBack,
					.textColor = textColor,
					.hoveredTextColor = textColor
				});
				if (row.pressed)
				{
					a_state.drillDown = TransitionDrillDown(
						a_state.drillDown,
						DrillDownEvent::Back);
					return;
				}
			}
			ImGui::Spacing();
			DrawSectionHeader(
				selectedClient->displayName.c_str(),
				ResolveIconGlyph(
					IconKind::kClient,
					selectedClient->iconName,
					selectedClient->id));
			ImGui::Spacing();
			DrawPageList(a_model, *selectedClient, a_state);
		}

		void DrawIconRailNavigation(
			const NavigationModel& a_model,
			const std::vector<ClientStatus>& a_statuses,
			ShellState& a_state) noexcept
		{
			const auto& style = ImGui::GetStyle();
			float iconFontSize{};
			{
				const Theme::FontGuard font{
					Theme::FontRole::kTitle,
					kSidebarModFontScale
				};
				iconFontSize = ImGui::GetFontSize();
			}
			const auto geometry = ResolveIconRailGeometry(
				ImGui::GetContentRegionAvail().x,
				iconFontSize,
				style.FramePadding.x,
				style.ItemSpacing.x);
			if (geometry.railWidth > 0.0f)
			{
				if (ImGui::BeginChild(
						"##DearModdingIconRail",
						{ geometry.railWidth, -FLT_MIN }))
				{
					for (const auto& client : a_model.clients)
					{
						const auto* status =
							FindClientStatus(a_statuses, client.handle);
						const auto row = DrawClientNavigationRow(
							client,
							status,
							a_state,
							ClientRowKind::Rail);
						if (row.pressed)
						{
							NavigateToPage(
								a_model,
								ResolveLandingPage(client),
								a_state);
						}
					}
				}
				ImGui::EndChild();
			}

			if (geometry.panelWidth <= 0.0f)
				return;
			ImGui::SameLine(0.0f, geometry.gap);
			if (ImGui::BeginChild(
					"##DearModdingIconRailPages",
					{ geometry.panelWidth, -FLT_MIN }))
			{
				if (const auto* client =
						a_model.FindClient(a_state.activeClient))
				{
					DrawSectionHeader(
						client->displayName.c_str(),
						ResolveIconGlyph(
							IconKind::kClient,
							client->iconName,
							client->id));
					ImGui::Spacing();
					DrawPageList(a_model, *client, a_state);
				}
				else
					ImGui::TextDisabled("Select a mod to browse its pages.");
			}
			ImGui::EndChild();
		}

		struct SidebarLayoutContext
		{
			const NavigationModel& model;
			const std::vector<ClientStatus>& statuses;
			ShellState& state;
		};

		struct TreeSidebarLayout
		{
			inline static constexpr auto kind = SidebarLayoutKind::Tree;

			static void Draw(const SidebarLayoutContext& a_context) noexcept
			{
				DrawTreeNavigation(
					a_context.model,
					a_context.statuses,
					a_context.state);
			}
		};

		struct TwoPaneSidebarLayout
		{
			inline static constexpr auto kind = SidebarLayoutKind::TwoPane;

			static void Draw(const SidebarLayoutContext& a_context) noexcept
			{
				DrawTwoPaneNavigation(
					a_context.model,
					a_context.statuses,
					a_context.state);
			}
		};

		struct DrillDownSidebarLayout
		{
			inline static constexpr auto kind = SidebarLayoutKind::DrillDown;

			static void Draw(const SidebarLayoutContext& a_context) noexcept
			{
				DrawDrillDownNavigation(
					a_context.model,
					a_context.statuses,
					a_context.state);
			}
		};

		struct IconRailSidebarLayout
		{
			inline static constexpr auto kind = SidebarLayoutKind::IconRail;

			static void Draw(const SidebarLayoutContext& a_context) noexcept
			{
				DrawIconRailNavigation(
					a_context.model,
					a_context.statuses,
					a_context.state);
			}
		};

		template<class F>
		decltype(auto) VisitSidebarLayout(
			SidebarLayoutKind a_kind,
			F&& a_fn)
		{
			auto fn = std::forward<F>(a_fn);
			switch (a_kind)
			{
			case SidebarLayoutKind::TwoPane:
				return fn.template operator()<TwoPaneSidebarLayout>();
			case SidebarLayoutKind::DrillDown:
				return fn.template operator()<DrillDownSidebarLayout>();
			case SidebarLayoutKind::IconRail:
				return fn.template operator()<IconRailSidebarLayout>();
			default:
				return fn.template operator()<TreeSidebarLayout>();
			}
		}

		void DrawNavigation(
			const NavigationModel& a_model,
			ShellState& a_state) noexcept
		{
			ImGui::TableNextColumn();
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4());
			if (ImGui::BeginListBox(
					"##DearModdingMenusList",
					{ -FLT_MIN, -FLT_MIN }))
			{
				DrawSectionHeader("Host", PhosphorGlyph::kAppWindow);
				DrawHostNavigationRow(a_state);
				ImGui::Spacing();
				DrawSectionHeader("Mods", PhosphorGlyph::kSquaresFour);
				DrawPaletteAffordance(a_state);
				ImGui::Spacing();
				const auto statusSnapshot = CurrentClientStatuses();
				const auto statuses = RollupClientStatuses(statusSnapshot);
				VisitSidebarLayout(
					a_state.sidebarLayout,
					[&]<class Layout>() noexcept {
						Layout::Draw({ a_model, statuses, a_state });
					});
				ImGui::EndListBox();
			}
			ImGui::PopStyleVar();
			ImGui::PopStyleColor();
		}

		[[nodiscard]] std::vector<NavigationSearchEntry> BuildPaletteResults(
			const NavigationModel& a_model,
			const ShellState& a_state)
		{
			std::vector<NavigationSearchEntry> results;
			if (!a_state.paletteQuery.empty())
			{
				auto hits = SearchNavigation(
					a_model,
					OrderedActions(),
					a_state.paletteQuery);
				results.reserve(hits.size());
				for (auto& hit : hits)
					results.push_back(std::move(hit.entry));
				return results;
			}

			const auto index =
				BuildNavigationSearchIndex(a_model, OrderedActions());
			results.reserve(a_state.recentPages.size());
			for (const auto page : a_state.recentPages)
			{
				const auto found = std::ranges::find_if(
					index,
					[&](const auto& a_entry) {
						return a_entry.kind == NavigationItemKind::kPage &&
							a_entry.page == page;
					});
				if (found != index.end())
					results.push_back(*found);
			}
			return results;
		}

		[[nodiscard]] std::string PaletteRowText(
			const NavigationSearchEntry& a_entry)
		{
			std::string label{ a_entry.displayName };
			if (a_entry.kind == NavigationItemKind::kClient)
			{
				label.append(" \xE2\x80\x94 Mod");
				return label;
			}
			label.append(" \xE2\x80\x94 ");
			label.append(a_entry.clientDisplayName);
			label.append(" \xE2\x80\xBA ");
			label.append(
				a_entry.category.empty() ? "Actions" : a_entry.category);
			return label;
		}

		[[nodiscard]] std::string PaletteRowLabel(
			const NavigationSearchEntry& a_entry)
		{
			std::string label{ "###DearModdingPalette/" };
			switch (a_entry.kind)
			{
			case NavigationItemKind::kClient:
				label.append("client/");
				break;
			case NavigationItemKind::kAction:
				label.append("action/");
				break;
			default:
				label.append("page/");
				break;
			}
			label.append(a_entry.clientId);
			label.push_back('/');
			label.append(a_entry.id);
			return label;
		}

		[[nodiscard]] char32_t PaletteEntryGlyph(
			const NavigationSearchEntry& a_entry) noexcept
		{
			switch (a_entry.kind)
			{
			case NavigationItemKind::kClient:
				return ResolveIconGlyph(
					IconKind::kClient,
					a_entry.iconName,
					a_entry.clientId);
			case NavigationItemKind::kAction:
				if (const auto glyph = ResolveActionIconGlyph(a_entry.iconName))
					return glyph;
				return PhosphorGlyph::kTerminalWindow;
			default:
				return PhosphorGlyph::kFiles;
			}
		}

		void ActivatePaletteEntry(
			const NavigationModel& a_model,
			const NavigationSearchEntry& a_entry,
			ShellState& a_state) noexcept
		{
			if (a_entry.kind == NavigationItemKind::kClient)
			{
				if (const auto* client = a_model.FindClient(a_entry.client))
				NavigateToPage(
					a_model,
					ResolveLandingPage(*client),
					a_state);
			}
			else if (a_entry.kind == NavigationItemKind::kPage)
				NavigateToPage(a_model, a_entry.page, a_state);
			else
				(void)InvokeAction(a_entry.action);
		}

		void DrawCommandPalette(
			const NavigationModel& a_model,
			ShellState& a_state) noexcept
		{
			if (a_state.paletteOpenRequested)
			{
				a_state.paletteQuery.clear();
				a_state.paletteSelection = 0;
				a_state.paletteFocusRequested = true;
				ImGui::OpenPopup(kCommandPalettePopupId);
				a_state.paletteOpenRequested = false;
			}

			const auto& style = ImGui::GetStyle();
			const auto paletteWidth =
				ImGui::GetWindowSize().x - style.WindowPadding.x * 2.0f;
			const auto paletteHeight =
				ImGui::GetFrameHeightWithSpacing() * 2.0f +
				ImGui::GetTextLineHeightWithSpacing() *
					static_cast<float>(kRecentPageCapacity + 1) +
				style.WindowPadding.y * 2.0f;
			ImGui::SetNextWindowSize(
				{ paletteWidth, paletteHeight },
				ImGuiCond_Appearing);
			auto paletteOpen = true;
			if (!BeginPopupModalWithRoundedTitleBarButtons(
					kCommandPalettePopupId,
					&paletteOpen,
					ImGuiWindowFlags_NoSavedSettings))
			{
				a_state.paletteVisible = false;
				return;
			}
			a_state.paletteVisible = true;

			if (a_state.paletteFocusRequested)
			{
				ImGui::SetKeyboardFocusHere();
				a_state.paletteFocusRequested = false;
			}
			const auto previousQuery = a_state.paletteQuery;
			DrawSearchInput(
				"NavigationPaletteSearch",
				"Search mods, pages, and actions...",
				a_state.paletteQuery);
			const auto queryChanged = previousQuery != a_state.paletteQuery;
			auto results = BuildPaletteResults(a_model, a_state);
			a_state.paletteSelection = ResolvePaletteSelectionIndex(
				a_state.paletteSelection,
				results.size(),
				queryChanged);

			auto keyboardMoved = false;
			if (!results.empty() &&
				ImGui::IsKeyPressed(ImGuiKey_UpArrow, false))
			{
				if (a_state.paletteSelection > 0)
					--a_state.paletteSelection;
				keyboardMoved = true;
			}
			if (!results.empty() &&
				ImGui::IsKeyPressed(ImGuiKey_DownArrow, false))
			{
				a_state.paletteSelection = (std::min)(
					a_state.paletteSelection + 1,
					results.size() - 1);
				keyboardMoved = true;
			}

			const auto escapePressed =
				ImGui::IsKeyPressed(ImGuiKey_Escape, false);
			auto activatedIndex = results.size();
			if (!escapePressed && !results.empty() &&
				ImGui::IsKeyPressed(ImGuiKey_Enter, false))
				activatedIndex = a_state.paletteSelection;

			ImGui::Spacing();
			{
				const Theme::FontGuard font{ Theme::FontRole::kHeading };
				ImGui::TextUnformatted(
					a_state.paletteQuery.empty() ?
						"Recent pages" :
						"Results");
			}
			ImGui::Separator();
			if (results.empty())
			{
				ImGui::TextDisabled(
					"%s",
					a_state.paletteQuery.empty() ?
						"No recent pages yet." :
						"No matching mods, pages, or actions.");
			}
			for (size_t index = 0; index < results.size(); ++index)
			{
				const auto label = PaletteRowLabel(results[index]);
				const auto text = PaletteRowText(results[index]);
				const auto selected = index == a_state.paletteSelection;
				const Theme::FontGuard font{
					Theme::FontRole::kSubheading
				};
				const auto rowHeight = ImGui::GetTextLineHeight();
				// Match the fill to the separator without changing vertical row packing.
				const auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
				const auto row = DrawSelectableRow({
					.id = label.c_str(),
					.label = text.c_str(),
					.selected = selected,
					.height = rowHeight,
					.leadingAffordance = RowLeadingAffordance::kIcon,
					.glyph = PaletteEntryGlyph(results[index]),
					.textColor = textColor,
					.hoveredTextColor = textColor,
					.trailingWidth = ImGui::GetStyle().FramePadding.x,
					.flushHorizontalHighlight = true
				});
				if (row.pressed)
					activatedIndex = index;
				if (selected && keyboardMoved)
					ImGui::SetScrollHereY();
			}

			if (activatedIndex < results.size())
			{
				ActivatePaletteEntry(
					a_model,
					results[activatedIndex],
					a_state);
				ImGui::CloseCurrentPopup();
				a_state.paletteVisible = false;
			}
			else if (escapePressed)
			{
				ImGui::CloseCurrentPopup();
				a_state.paletteVisible = false;
			}
			ImGui::EndPopup();
		}

		void DrawFailure(const NavigationPage& a_page) noexcept
		{
			{
				const Theme::FontGuard font{ Theme::FontRole::kHeading };
				ImGui::TextColored(
					Theme::kStatusPaletteDefaults.error,
					"%s could not be displayed",
					a_page.displayName.c_str());
			}
			ImGui::Spacing();
			ImGui::TextWrapped(
				"The mod's page callback failed and has been disabled for this session. "
				"Other pages remain available.");
		}

		[[nodiscard]] bool ActionHasGlyph(
			const RegisteredAction& a_action,
			char32_t& a_glyph) noexcept
		{
			a_glyph = ResolveActionIconGlyph(a_action.iconName);
			return HasIconGlyph(a_glyph);
		}

		[[nodiscard]] float ActionButtonWidthFor(
			const RegisteredAction& a_action,
			float a_buttonExtent) noexcept
		{
			char32_t glyph{};
			const auto hasGlyph = ActionHasGlyph(a_action, glyph);
			return ActionButtonWidth(
				hasGlyph,
				ImGui::CalcTextSize(a_action.displayLabel.c_str()).x,
				a_buttonExtent,
				ImGui::GetStyle().FramePadding.x);
		}

		void DrawPageHeader(const NavigationPage& a_page) noexcept
		{
			constexpr auto extentPolicy =
				TitleRowButtonExtentPolicy::kTitleBar;
			const auto buttonExtent = ResolveTitleRowButtonExtent(
				extentPolicy,
				ImGui::GetFontSize(),
				TitleBarButtonPadding());
			std::vector<TitleRowButton> buttons;
			std::vector<DMUI_ActionHandle> actions;
			for (const auto& action : OrderedActions())
			{
				if (action.client != a_page.client)
					continue;

				char32_t glyph{};
				if (!ActionHasGlyph(action, glyph))
					glyph = {};
				const auto failed = ActionFailed(action.handle);
				buttons.push_back({
					action.id.c_str(),
					ActionButtonWidthFor(action, buttonExtent),
					glyph,
					action.displayLabel.c_str(),
					failed ?
						"Action disabled after its callback failed." :
						(action.tooltip.empty() ?
								action.displayLabel.c_str() :
								action.tooltip.c_str()),
					!failed
				});
				actions.push_back(action.handle);
			}
			const auto pressed = DrawTitleRow({
				.title = a_page.displayName.c_str(),
				.titleScale = Theme::kFeatureTitleScale,
				.buttons = buttons,
				.buttonExtentPolicy = extentPolicy,
				.summary = a_page.summary.empty() ?
					nullptr :
					a_page.summary.c_str()
			});
			if (pressed)
				(void)InvokeAction(actions[*pressed]);
		}

		template <class DrawValue>
		void DrawHostHomeRow(
			const char* a_id,
			const char* a_label,
			const char* a_description,
			DrawValue&& a_drawValue) noexcept
		{
			const auto result = SettingsTable::BeginRow(
				DMUI_INVALID_CLIENT_HANDLE,
				a_id,
				a_label,
				a_description);
			if (result.result != DMUI_RESULT_OK || !result.visible)
				return;
			a_drawValue();
			bool resetPressed{};
			(void)SettingsTable::EndRow(
				DMUI_INVALID_CLIENT_HANDLE,
				{ false, false },
				resetPressed);
		}

		[[nodiscard]] const char* ClientStatusLabel(
			const RegisteredClient& a_client,
			const ClientStatus* a_status) noexcept
		{
			if (a_client.callbackFailed)
				return "Unavailable";
			if (!a_status)
				return "Ready";
			switch (a_status->severity)
			{
			case DMUI_STATUS_SEVERITY_INFO:
				return "Info";
			case DMUI_STATUS_SEVERITY_SUCCESS:
				return "Success";
			case DMUI_STATUS_SEVERITY_WARNING:
				return "Warning";
			case DMUI_STATUS_SEVERITY_ERROR:
				return "Error";
			default:
				return "Unknown";
			}
		}

		[[nodiscard]] DMUI_StatusSeverity ClientStatusSeverity(
			const RegisteredClient& a_client,
			const ClientStatus* a_status) noexcept
		{
			if (a_client.callbackFailed)
				return DMUI_STATUS_SEVERITY_ERROR;
			return a_status ?
				a_status->severity :
				DMUI_STATUS_SEVERITY_SUCCESS;
		}

		[[nodiscard]] const ImVec4& StatusTextColor(
			DMUI_StatusSeverity a_severity) noexcept;

		void DrawHostHome() noexcept
		{
			(void)DrawTitleRow({
				.title = kHostHomePage.displayName.data(),
				.titleScale = Theme::kFeatureTitleScale,
				.summary = kHostHomePage.summary.data()
			});

			const auto& clients = RegisteredClients();
			const auto& pages = OrderedPages();
			const auto& actions = OrderedActions();
			const auto statusSnapshot = CurrentClientStatuses();
			const auto statuses = RollupClientStatuses(statusSnapshot);

			DrawSectionHeader("Host overview", PhosphorGlyph::kAppWindow);
			if (const auto table = SettingsTable::Begin(
					DMUI_INVALID_CLIENT_HANDLE,
					"##DearModdingUI.HostHomeOverview");
				table.result == DMUI_RESULT_OK && table.visible)
			{
				DrawHostHomeRow(
					"Host",
					"Host",
					"Shared menu owner and plugin version.",
					[]() noexcept {
						ImGui::Text(
							"%.*s %.*s",
							static_cast<int>(kHostDisplayName.size()),
							kHostDisplayName.data(),
							static_cast<int>(kHostVersion.size()),
							kHostVersion.data());
					});
				char registrySummary[128]{};
				std::snprintf(
					registrySummary,
					sizeof(registrySummary),
					"%zu mods | %zu pages | %zu actions",
					clients.size(),
					pages.size(),
					actions.size());
				DrawHostHomeRow(
					"Registry",
					"Client registry",
					"Live registrations available during this game session.",
					[&]() noexcept {
						ImGui::TextUnformatted(registrySummary);
					});
				(void)SettingsTable::End(DMUI_INVALID_CLIENT_HANDLE);
			}

			ImGui::Spacing();
			DrawSectionHeader("Registered mods", PhosphorGlyph::kPuzzlePiece);
			if (clients.empty())
			{
				DrawBulletText("No client mods registered this session.");
				return;
			}

			std::vector<const RegisteredClient*> sortedClients;
			sortedClients.reserve(clients.size());
			for (const auto& client : clients)
				sortedClients.push_back(&client);
			std::ranges::sort(
				sortedClients,
				[](const auto* a_left, const auto* a_right) {
					if (a_left->displayName != a_right->displayName)
						return a_left->displayName < a_right->displayName;
					return a_left->id < a_right->id;
				});

			const auto table = SettingsTable::Begin(
				DMUI_INVALID_CLIENT_HANDLE,
				"##DearModdingUI.HostHomeClients");
			if (table.result != DMUI_RESULT_OK || !table.visible)
				return;
			for (const auto* client : sortedClients)
			{
				const auto pageCount = std::ranges::count(
					pages,
					client->handle,
					&RegisteredPage::client);
				const auto actionCount = std::ranges::count(
					actions,
					client->handle,
					&RegisteredAction::client);
				char description[256]{};
				std::snprintf(
					description,
					sizeof(description),
					"%s | %td pages | %td actions",
					client->id.c_str(),
					pageCount,
					actionCount);
				char version[64]{};
				std::snprintf(
					version,
					sizeof(version),
					"Version %u.%u",
					client->version >> 16,
					client->version & 0xFFFFu);
				const auto* status =
					FindClientStatus(statuses, client->handle);
				const auto severity = ClientStatusSeverity(*client, status);
				std::string rowId{ "Client/" };
				rowId.append(client->id);
				DrawHostHomeRow(
					rowId.c_str(),
					client->displayName.c_str(),
					description,
					[&]() noexcept {
						ImGui::TextUnformatted(version);
						ImGui::SameLine();
						ImGui::TextColored(
							StatusTextColor(severity),
							"Status: %s",
							ClientStatusLabel(*client, status));
					});
			}
			(void)SettingsTable::End(DMUI_INVALID_CLIENT_HANDLE);
		}

		void DrawContent(
			const NavigationModel& a_model,
			ShellState& a_state) noexcept
		{
			ImGui::TableNextColumn();
			if (!ImGui::BeginChild(
					"##DearModdingPageFrame",
					{},
					ImGuiChildFlags_Borders))
			{
				ImGui::EndChild();
				return;
			}

			if (HostSettings::IsPanelOpen())
			{
				constexpr auto extentPolicy =
					TitleRowButtonExtentPolicy::kTitleBar;
				const auto buttonExtent = ResolveTitleRowButtonExtent(
					extentPolicy,
					ImGui::GetFontSize(),
					TitleBarButtonPadding());
				const auto hasCloseGlyph = HasIconGlyph(PhosphorGlyph::kX);
				constexpr const char* fallbackLabel{ "Back" };
				const auto closeButtonWidth = ActionButtonWidth(
					hasCloseGlyph,
					ImGui::CalcTextSize(fallbackLabel).x,
					buttonExtent,
					ImGui::GetStyle().FramePadding.x);
				const std::array actions{
					HostSettingsTitleButton{
						SettingsAction::kReset,
						"##DearModdingUI.HostSettingsResetButton",
						"Reset",
						"Reset saves the default sidebar layout immediately and "
						"loads other shipped interface defaults into the draft. "
						"Use Apply to save those.",
						SettingsActionButtonWidth(
							SettingsAction::kReset,
							"Reset",
							buttonExtent) },
					HostSettingsTitleButton{
						SettingsAction::kRevert,
						"##DearModdingUI.HostSettingsRevertButton",
						"Revert",
						"Revert discards pending interface edits and restores "
						"saved settings. Sidebar layout changes are already saved.",
						SettingsActionButtonWidth(
							SettingsAction::kRevert,
							"Revert",
							buttonExtent) },
					HostSettingsTitleButton{
						SettingsAction::kApply,
						"##DearModdingUI.HostSettingsApplyButton",
						"Apply",
						"Apply saves host settings to DearModdingUI.toml. "
						"Sidebar layout changes save immediately; appearance "
						"previews update live, and typography rebuilds once if needed.",
						SettingsActionButtonWidth(
							SettingsAction::kApply,
							"Apply",
							buttonExtent) }
				};
				const auto dirty =
					HostSettingsTitleActionEnabled(SettingsAction::kApply);
				std::array<TitleRowButton, actions.size() + 1> titleButtons{};
				for (size_t index = 0; index < actions.size(); ++index)
				{
					const auto& action = actions[index];
					const auto presentation =
						ResolveSettingsActionButtonPresentation(
							action.action,
							HasIconGlyph(SettingsActionGlyph(action.action)));
					const auto enabled =
						SettingsActionEnabled(action.action, dirty);
					titleButtons[index] = {
						action.id,
						action.width,
						presentation.glyph,
						action.label,
						action.tooltip,
						enabled
					};
				}
				titleButtons.back() = {
					"##DearModdingUI.HostSettingsBackButton",
					closeButtonWidth,
					hasCloseGlyph ? PhosphorGlyph::kX : char32_t{},
					fallbackLabel,
					"Back to the current mod page"
				};
				const auto pressed = DrawTitleRow({
					.title = "Interface Settings",
					.titleScale = Theme::kFeatureTitleScale,
					.buttons = titleButtons,
					.buttonExtentPolicy = extentPolicy
				});
				auto dismiss = false;
				if (pressed)
				{
					if (*pressed < actions.size())
					{
						InvokeHostSettingsTitleAction(
							actions[*pressed].action);
					}
					else
					{
						dismiss = true;
					}
				}
				DrawHostSettingsControls();
				if (dismiss)
					HostSettings::DismissPanel();
				ImGui::EndChild();
				return;
			}

			if (a_state.activeHostPage == HostPageKind::kHome)
			{
				DrawHostHome();
				ImGui::EndChild();
				return;
			}

			const auto* page = a_model.FindPage(a_state.activePage);
			if (!page)
			{
				ImGui::TextDisabled("Please select a page from the left.");
				ImGui::EndChild();
				return;
			}

			DrawPageHeader(*page);
			const auto presentation =
				DecidePagePresentation(page, PageFailed(page->handle));
			if (presentation == PagePresentation::kFailure)
			{
				DrawFailure(*page);
			}
			else if (presentation == PagePresentation::kContent)
			{
				ImGui::PushID(static_cast<int>(page->handle));
				if (!DrawPage(page->handle))
				{
					ImGui::Spacing();
					ImGui::SeparatorEx(
						ImGuiSeparatorFlags_Horizontal,
						SeparatorThickness());
					ImGui::Spacing();
					DrawFailure(*page);
				}
				ImGui::PopID();
			}
			ImGui::EndChild();
		}

		struct BulletTextOptions
		{
			std::optional<ImVec4> color;
			std::optional<float> ellipsisMaxX;
			bool overflowTooltip{ false };
		};

		void DrawBulletTextEntry(
			const char* a_text,
			const BulletTextOptions& a_options = {}) noexcept
		{
			a_text = a_text ? a_text : "";
			if (a_options.color)
				ImGui::PushStyleColor(ImGuiCol_Text, *a_options.color);
			ImGui::Bullet();
			if (a_options.ellipsisMaxX)
			{
				try
				{
					const auto availableWidth = (std::max)(
						*a_options.ellipsisMaxX -
							ImGui::GetCursorScreenPos().x,
						0.0f);
					const auto presentation = FitStatusText(
						a_text,
						availableWidth,
						[](std::string_view a_value) {
							return ImGui::CalcTextSize(
								a_value.data(),
								a_value.data() + a_value.size()).x;
						});
					ImGui::TextUnformatted(
						presentation.visible.data(),
						presentation.visible.data() +
							presentation.visible.size());
					if (a_options.overflowTooltip &&
						presentation.truncated &&
						ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
					{
						ImGui::SetTooltip(
							"%s",
							presentation.full.c_str());
					}
				}
				catch (...)
				{
					ImGui::TextUnformatted("");
				}
			}
			else
			{
				// Bullet positions the wrapped text with scaled frame padding.
				ImGui::TextWrapped("%s", a_text);
			}
			if (a_options.color)
				ImGui::PopStyleColor();
		}

		[[nodiscard]] const ImVec4& StatusTextColor(
			DMUI_StatusSeverity a_severity) noexcept
		{
			switch (a_severity)
			{
			case DMUI_STATUS_SEVERITY_SUCCESS:
				return Theme::kStatusPaletteDefaults.success;
			case DMUI_STATUS_SEVERITY_WARNING:
				return Theme::kStatusPaletteDefaults.warning;
			case DMUI_STATUS_SEVERITY_ERROR:
				return Theme::kStatusPaletteDefaults.error;
			default:
				return Theme::kStatusPaletteDefaults.info;
			}
		}

		void DrawFooterStatus(
			const StatusMessage& a_status,
			float a_runMaxX) noexcept
		{
			DrawBulletTextEntry(
				a_status.attributedText.c_str(),
				{
					.color = StatusTextColor(a_status.severity),
					.ellipsisMaxX = a_runMaxX,
					.overflowTooltip = true
				});
		}

		void DrawFooter(
			const NavigationModel& a_model,
			const ShellState& a_state) noexcept
		{
			const auto status = CurrentStatus();
			const auto start = ImGui::GetCursorScreenPos();
			const auto contentMaxX =
				start.x + ImGui::GetContentRegionAvail().x;
			const auto iconSize = HostChromeIconSize(ImGui::GetFontSize());
			const auto settingsButtonExtent = HostChromeButtonExtent(
				ImGui::GetFontSize(), TitleBarButtonPadding());
			const auto dismissButtonExtent = TitleBarButtonExtent(
				ImGui::GetFontSize(), TitleBarButtonPadding());
			const auto rowHeight = (std::max)(
				ImGui::GetFrameHeight(),
				settingsButtonExtent);
			const auto hasGearGlyph = HasIconGlyph(PhosphorGlyph::kGear);
			constexpr const char* fallbackLabel{ "Settings" };
			const auto buttonWidth = ActionButtonWidth(
				hasGearGlyph,
				ImGui::CalcTextSize(fallbackLabel).x,
				settingsButtonExtent,
				ImGui::GetStyle().FramePadding.x);
			const auto persistent = status && status->persistent;
			const auto hasDismissGlyph = HasIconGlyph(PhosphorGlyph::kX);
			constexpr const char* dismissLabel{ "Dismiss" };
			const auto dismissWidth = ActionButtonWidth(
				hasDismissGlyph,
				ImGui::CalcTextSize(dismissLabel).x,
				dismissButtonExtent,
				ImGui::GetStyle().FramePadding.x);
			const auto controls = ResolveFooterControlsLayout(
				start.x,
				contentMaxX,
				buttonWidth,
				persistent ? dismissWidth : 0.0f,
				ImGui::GetStyle().ItemSpacing.x);
			ImGui::PushClipRect(
				start,
				{ controls.runMaxX, start.y + rowHeight },
				true);
			ImGui::SetCursorScreenPos({
				start.x,
				start.y + RowContentOffsetY(
					rowHeight,
					CurrentFontRowContentMetrics(ImGui::GetFontSize()),
					RowContentMetric::kOptical)
			});
			DrawBulletText("Host: Evil Modding");
			if (const auto* client =
					a_model.FindClient(a_state.activeClient))
			{
				char modText[320]{};
				std::snprintf(
					modText,
					sizeof(modText),
					"Mod: %s",
					client->displayName.c_str());
				ImGui::SameLine();
				DrawBulletText(modText);
				char versionText[64]{};
				std::snprintf(
					versionText,
					sizeof(versionText),
					"Version: %u.%u",
					client->version >> 16,
					client->version & 0xFFFFu);
				ImGui::SameLine();
				DrawBulletText(versionText);
			}
			if (status)
			{
				ImGui::SameLine();
				DrawFooterStatus(*status, controls.runMaxX);
			}
			ImGui::PopClipRect();

			const auto actualDismissWidth =
				controls.dismissMaxX - controls.dismissMinX;
			if (persistent && actualDismissWidth > 0.0f &&
				DrawCompactChromeButton(
						"##DearModdingUI.StatusDismissButton",
						{
							controls.dismissMinX,
							start.y + RowContentOffsetY(
								rowHeight,
								{ dismissButtonExtent },
								RowContentMetric::kBox)
						},
						{ actualDismissWidth, dismissButtonExtent },
						hasDismissGlyph ? PhosphorGlyph::kX : char32_t{},
						hasDismissGlyph ? nullptr : dismissLabel,
						"Dismiss status",
						hasDismissGlyph ?
							IconColor(ImGui::GetColorU32(ImGuiCol_Text)) :
							ImGui::GetColorU32(ImGuiCol_Text)))
			{
				(void)DismissStatus(status->generation);
			}
			if (DrawCompactChromeButton(
					"##DearModdingUI.HostSettingsButton",
					{
						controls.settingsMinX,
						start.y + RowContentOffsetY(
						rowHeight,
							{ settingsButtonExtent },
							RowContentMetric::kBox)
					},
					{ buttonWidth, settingsButtonExtent },
					hasGearGlyph ? PhosphorGlyph::kGear : char32_t{},
					hasGearGlyph ? nullptr : fallbackLabel,
					"Interface settings",
					hasGearGlyph ?
						IconColor(ImGui::GetColorU32(ImGuiCol_Text)) :
						ImGui::GetColorU32(ImGuiCol_Text),
					HostSettings::IsPanelOpen(),
					iconSize))
			{
				HostSettings::TogglePanel(true);
			}
			ImGui::SetCursorScreenPos(start);
			ImGui::Dummy({ contentMaxX - start.x, rowHeight });
		}

		struct RuledHeadingOptions
		{
			const char* key;
			const char* text;
			char32_t glyph;
			std::optional<size_t> count;
			bool* expanded;
		};

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

			auto* drawList = ImGui::GetWindowDrawList();
			const auto position = ImGui::GetCursorScreenPos();
			const auto availableWidth = ImGui::GetContentRegionAvail().x;
			const auto textSize = ImGui::CalcTextSize(text);
			const auto layout = DecideInlineIconLayout(
				HasIconGlyph(a_options.glyph),
				textSize.x,
				textSize.y,
				ImGui::GetFontSize(),
				ImGui::GetStyle().ItemSpacing.x);
			const auto contentGap = ImGui::GetStyle().ItemSpacing.x;
			const auto lineLength = (std::max)(
				(availableWidth - layout.contentWidth - contentGap * 2.0f) *
					0.5f,
				0.0f);

			auto clicked = false;
			auto hovered = false;
			if (a_options.expanded)
			{
				ImGui::PushID(a_options.key);
				ImGui::SetCursorScreenPos(position);
				clicked = ImGui::InvisibleButton(
					"##DearModdingUI.RuledHeading",
					{ availableWidth, layout.contentHeight });
				hovered = ImGui::IsItemHovered();
			}
			else
			{
				ImGui::SetCursorScreenPos(position);
				ImGui::Dummy({ availableWidth, layout.contentHeight });
			}

			auto color = hovered ?
				Theme::kFeatureHeadingDefaults.colorHovered :
				Theme::kFeatureHeadingDefaults.colorDefault;
			if (a_options.expanded && !*a_options.expanded)
				color.w *= Theme::kFeatureHeadingDefaults.minimizedFactor;
			const auto packed = ImGui::GetColorU32(color);
			const auto contentMinX =
				position.x + lineLength + contentGap;
			const auto lineY = DrawIconText(
				{ contentMinX, position.y },
				layout.contentHeight,
				a_options.glyph,
				text,
				packed);
			if (lineLength > 0.0f)
			{
				drawList->AddLine(
					{ position.x, lineY },
					{ position.x + lineLength, lineY },
					packed);
			}
			const auto rightLineStart =
				contentMinX + layout.contentWidth + contentGap;
			if (rightLineStart < position.x + availableWidth)
			{
				drawList->AddLine(
					{ rightLineStart, lineY },
					{ position.x + availableWidth, lineY },
					packed);
			}
			if (a_options.expanded)
			{
				if (clicked)
					*a_options.expanded = !*a_options.expanded;
				ImGui::PopID();
			}
		}

		void SaveLayout() noexcept
		{
			const auto& io = ImGui::GetIO();
			if (io.IniFilename)
				ImGui::SaveIniSettingsToDisk(io.IniFilename);
		}
	}

	void ConfigurePreviewSidebarComparison(
		std::optional<SidebarLayoutKind> a_layoutOverride,
		bool a_overrideExpandedClients,
		std::span<const std::string> a_expandedClients)
	{
		auto& state = State();
		state.previewSidebarLayoutOverride = a_layoutOverride;
		state.drillDown = {};
		state.drillDownInitialized = false;
		if (a_overrideExpandedClients)
		{
			state.previewExpandedClients.emplace(
				a_expandedClients.begin(),
				a_expandedClients.end());
		}
		else
			state.previewExpandedClients.reset();
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
		const auto availableWidth = ImGui::GetContentRegionAvail().x;
		const auto frameHeight = ImGui::GetFrameHeight();

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4());
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4());
		ImGui::PushStyleColor(
			ImGuiCol_FrameBgActive,
			ImVec4(0.3f, 0.3f, 0.3f, 0.9f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4());
		ImGui::PushStyleColor(
			ImGuiCol_Text,
			Theme::kFullPalette[ImGuiCol_Text]);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		ImGui::PushStyleVar(
			ImGuiStyleVar_FramePadding,
			ImVec2(
				iconSpace,
				Theme::kSearchInputFramePaddingY * scale));
		ImGui::SetNextItemWidth(availableWidth);

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
			cursor.y + RowContentOffsetY(
				frameHeight,
				{ iconSize },
				RowContentMetric::kBox)
		};
		DrawCenteredIcon(
			ImGui::GetWindowDrawList(),
			PhosphorGlyph::kMagnifyingGlass,
			{
				iconPosition,
				{
					iconPosition.x + iconSize,
					iconPosition.y + iconSize
				}
			},
			iconSize,
			IconColor(
				ImGui::GetColorU32(ImGuiCol_Text),
				Theme::kSearchIconAlpha),
			nullptr);
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
		const auto hasGlyph = !presentation.useTextFallback;
		const auto labelWidth =
			a_fallbackLabel ? ImGui::CalcTextSize(a_fallbackLabel).x : 0.0f;
		const auto framePaddingX = ImGui::GetStyle().FramePadding.x;
		return a_action == SettingsAction::kReset ?
			SettingsTable::ResolveResetColumnWidth(
				hasGlyph,
				labelWidth,
				a_buttonExtent,
				framePaddingX) :
			ActionButtonWidth(
				hasGlyph,
				labelWidth,
				a_buttonExtent,
				framePaddingX);
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
		return DrawEnabledControl(
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
		DrawBulletTextEntry(a_text);
	}

	void DrawSectionHeader(const char* a_text, char32_t a_glyph) noexcept
	{
		DrawRuledHeading({
			.text = a_text,
			.glyph = a_glyph
		});
	}

	void DrawShell() noexcept
	{
		auto& state = State();
		if (HostSettings::IsPanelOpen() &&
			!state.paletteVisible &&
			!state.paletteOpenRequested &&
			ImGui::IsKeyPressed(ImGuiKey_Escape, false))
			HostSettings::DismissPanel();
		Theme::ApplyStyle();
		const auto sidebarLayout = ResolveSidebarLayout(
			HostSettings::EffectivePreview().sidebarLayout,
			state.previewSidebarLayoutOverride);
		if (state.sidebarLayout != sidebarLayout)
		{
			state.sidebarLayout = sidebarLayout;
			state.drillDown = {};
			state.drillDownInitialized = false;
		}
		const auto& model = Navigation();
		PruneRecentPages(model, state);
		const auto requested = SelectedPage();
		const auto previousPage = state.activePage;
		state.activePage =
			ResolvePageSelection(
				model,
				requested,
				state.activePage,
				state.activeHostPage.has_value());
		if (requested != DMUI_INVALID_PAGE_HANDLE &&
			state.activePage == requested)
			state.activeHostPage.reset();
		if (state.activePage != previousPage)
		{
			RecordRecentPage(model, state.activePage, state);
			ExpandPageAncestors(model, state.activePage, state);
		}
		if (requested != DMUI_INVALID_PAGE_HANDLE)
			ClearPageSelection(requested);
		if (const auto* client =
				model.FindClientForPage(state.activePage))
			state.activeClient = client->handle;
		else if (state.activeHostPage)
			state.activeClient = DMUI_INVALID_CLIENT_HANDLE;
		if (state.sidebarLayout == SidebarLayoutKind::DrillDown)
		{
			if (!state.drillDownInitialized)
			{
				state.drillDown = TransitionDrillDown(
					state.drillDown,
					DrillDownEvent::Open,
					state.activeClient);
				state.drillDownInitialized = true;
			}
			else if (requested != DMUI_INVALID_PAGE_HANDLE)
			{
				state.drillDown = TransitionDrillDown(
					state.drillDown,
					DrillDownEvent::SelectClient,
					state.activeClient);
			}
		}
		ApplyPreviewExpandedClients(model, state);

		const auto* viewport = ImGui::GetMainViewport();
		ImGui::DockSpaceOverViewport(
			0,
			viewport,
			ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::SetNextWindowPos(
			{ viewport->Size.x * 0.5f, viewport->Size.y * 0.5f },
			ImGuiCond_FirstUseEver,
			{ 0.5f, 0.5f });
		ImGui::SetNextWindowSize(
			{ viewport->Size.x * 0.90f, viewport->Size.y * 0.90f },
			ImGuiCond_FirstUseEver);

		ImGuiWindowClass windowClass{};
		windowClass.ClassId = ImHashStr("DearModdingUI.Host");
		windowClass.DockingAllowUnclassed = true;
		ImGui::SetNextWindowClass(&windowClass);

		auto open = true;
		auto windowFlags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoScrollbar;
		static bool wasDocked = false;
		if (!wasDocked)
			windowFlags |= ImGuiWindowFlags_NoTitleBar;

		const auto visible = BeginWithRoundedTitleBarButtons(
			"Evil Modding###DearModdingUI.Host",
			&open,
			windowFlags);
		wasDocked = ImGui::IsWindowDocked();

		const auto position = ImGui::GetWindowPos();
		const auto size = ImGui::GetWindowSize();
		const auto framebufferScale =
			ImGui::GetIO().DisplayFramebufferScale;
		BackgroundBlur::SetHostWindow(
			(position.x - viewport->Pos.x) * framebufferScale.x,
			(position.y - viewport->Pos.y) * framebufferScale.y,
			(position.x + size.x - viewport->Pos.x) * framebufferScale.x,
			(position.y + size.y - viewport->Pos.y) * framebufferScale.y,
			ImGui::GetStyle().WindowRounding *
				(std::max)(framebufferScale.x, framebufferScale.y));

		if (visible)
		{
			const auto drawHeaderClose =
				ShouldDrawHeaderClose(
					ImGui::IsWindowDocked(),
					(ImGui::GetCurrentWindow()->Flags &
						ImGuiWindowFlags_NoTitleBar) != 0);
			if (DrawHeader(model, state, drawHeaderClose))
				open = false;
			const auto footerButtonExtent = HostChromeButtonExtent(
				ImGui::GetFontSize(),
				TitleBarButtonPadding());
			const auto footerHeight =
				ReservedFooterHeight(
					(std::max)(
						ImGui::GetFrameHeight(),
						footerButtonExtent),
					ImGui::GetStyle().ItemSpacing.y,
					ImGui::GetStyle().WindowPadding.y,
					SeparatorThickness());
			ImGui::BeginChild(
				"Dear Modding Menus Table",
				{ 0.0f, -footerHeight });
			if (ImGui::BeginTable(
					"Dear Modding Menus Table",
					2,
					ImGuiTableFlags_SizingStretchProp |
						ImGuiTableFlags_Resizable))
			{
				ImGui::TableSetupColumn(
					"##DearModdingList",
					ImGuiTableColumnFlags_None,
					3.5f);
				ImGui::TableSetupColumn(
					"##DearModdingPage",
					ImGuiTableColumnFlags_None,
					6.5f);
				DrawNavigation(model, state);
				DrawContent(model, state);
				ImGui::EndTable();
			}
			ImGui::EndChild();
			DrawCommandPalette(model, state);

			ImGui::Spacing();
			ImGui::SeparatorEx(
				ImGuiSeparatorFlags_Horizontal,
				SeparatorThickness());
			const auto footerPosition = ImGui::GetCursorScreenPos();
			ImGui::SetCursorScreenPos({
				footerPosition.x,
				footerPosition.y + FooterRowAdjustmentY(
					ImGui::GetStyle().ItemSpacing.y,
					ImGui::GetStyle().WindowPadding.y)
			});
			DrawFooter(model, state);
		}
		ImGui::End();

		if (!open)
		{
			(void)SetMenuVisible(false);
			SaveLayout();
		}
	}
}
