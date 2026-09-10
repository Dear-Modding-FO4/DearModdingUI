#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/controls/ChromeGeometry.h>
#include <DearModdingUI/controls/Controls.h>
#include "../support/ImGuiTestContext.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;
	using namespace support::host;

	namespace
	{
		inline constexpr ImVec4 kRowHoverColor{
			0.13f,
			0.47f,
			0.91f,
			1.0f
		};

		struct RowFrame
		{
			RowResult row;
			bool hovered{};
			bool active{};
			bool highlightRendered{};
			ImRect highlightBounds;
			ImGuiID id{};
		};

		class InteractiveRow
		{
		public:
			InteractiveRow()
			{
				auto& io = ImGui::GetIO();
				io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
				ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered] =
					kRowHoverColor;
			}

			RowFrame Frame(
				const RowOptions& a_options,
				ImVec2 a_mouse,
				bool a_mouseDown,
				bool a_focusRow = false,
				bool a_disabled = false)
			{
				auto& io = ImGui::GetIO();
				io.AddMousePosEvent(a_mouse.x, a_mouse.y);
				io.AddMouseButtonEvent(ImGuiMouseButton_Left, a_mouseDown);
				m_imgui.BeginWindow(
					"##SelectableRowTest",
					{ 0.0f, 0.0f },
					{ 420.0f, 180.0f },
					ImGuiWindowFlags_NoDecoration |
						ImGuiWindowFlags_NoSavedSettings,
					ImGuiCond_Always);
				ImGui::SetCursorScreenPos({ 20.0f, 40.0f });
				if (a_disabled)
					ImGui::BeginDisabled();
				const auto row = DrawSelectableRow(a_options);
				RowFrame result{
					.row = row,
					.hovered = ImGui::IsItemHovered(),
					.active = ImGui::IsItemActive(),
					.id = ImGui::GetItemID()
				};
				const auto color = ImGui::GetColorU32(kRowHoverColor);
				for (const auto& vertex :
					ImGui::GetWindowDrawList()->VtxBuffer)
				{
					if (vertex.col != color)
						continue;
					if (!result.highlightRendered)
					{
						result.highlightBounds = { vertex.pos, vertex.pos };
						result.highlightRendered = true;
					}
					else
					{
						result.highlightBounds.Min.x = (std::min)(
							result.highlightBounds.Min.x,
							vertex.pos.x);
						result.highlightBounds.Min.y = (std::min)(
							result.highlightBounds.Min.y,
							vertex.pos.y);
						result.highlightBounds.Max.x = (std::max)(
							result.highlightBounds.Max.x,
							vertex.pos.x);
						result.highlightBounds.Max.y = (std::max)(
							result.highlightBounds.Max.y,
							vertex.pos.y);
					}
				}
				if (a_focusRow)
				{
					ImGui::SetNavWindow(ImGui::GetCurrentWindow());
					ImGui::SetNavID(
						result.id,
						ImGuiNavLayer_Main,
						ImGui::GetCurrentFocusScope(),
						ImGui::WindowRectAbsToRel(
							ImGui::GetCurrentWindow(),
							row.rect));
					ImGui::GetCurrentContext()->NavCursorVisible = true;
				}
				if (a_disabled)
					ImGui::EndDisabled();
				m_imgui.EndWindow(true);
				return result;
			}

			void Key(ImGuiKey a_key, bool a_down)
			{
				ImGui::GetIO().AddKeyEvent(a_key, a_down);
			}

		private:
			support::ImGuiTestContext m_imgui{
				{
					.displaySize = { 640.0f, 360.0f },
					.disableInputTrickle = true,
					.disableErrorRecovery = true
				}
			};
		};

		[[nodiscard]] ImVec2 ArrowPoint(const ImRect& a_rect)
		{
			return {
				a_rect.Min.x + ImGui::GetFontSize() * 0.5f,
				a_rect.GetCenter().y
			};
		}

		[[nodiscard]] ImVec2 LabelPoint(const ImRect& a_rect)
		{
			return {
				a_rect.Min.x + ImGui::GetFontSize() * 2.5f,
				a_rect.GetCenter().y
			};
		}

		void RequireFullHighlight(const RowFrame& a_frame)
		{
			require(a_frame.hovered, "row hover API lost the hovered row");
			require(
				a_frame.highlightRendered,
				"row hover highlight was not rendered");
			constexpr auto epsilon = 0.01f;
			require(
				a_frame.highlightBounds.Min.x <=
						a_frame.row.rect.Min.x + epsilon &&
					a_frame.highlightBounds.Min.y <=
						a_frame.row.rect.Min.y + epsilon &&
					a_frame.highlightBounds.Max.x >=
						a_frame.row.rect.Max.x - epsilon &&
					a_frame.highlightBounds.Max.y >=
						a_frame.row.rect.Max.y - epsilon,
				"row hover highlight did not cover the full row");
		}
	}

	void run_host_control_checks(Runner& runner)
	{
		runner.test("host close and footer gear stay clear of adjacent content", [] {
			require(ShouldDrawHeaderClose(false, true),
				"undocked titleless host lost its close button");
			require(
					!ShouldDrawHeaderClose(true, true) &&
						!ShouldDrawHeaderClose(true, false) &&
						!ShouldDrawHeaderClose(false, false),
					"host close duplicated a native or docked close affordance");
			struct Case
			{
				float fontSize;
				float uiScale;
			};
			constexpr std::array cases{
				Case{ 16.0f, 1.0f },
				Case{ 18.0f, 1.25f },
				Case{ 21.0f, 1.5f },
				Case{ 28.0f, 2.0f }
			};
			for (const auto& test : cases)
			{
				const auto fontSize = test.fontSize * test.uiScale;
				const auto padding = 2.0f * test.uiScale;
				const auto spacing = 8.0f * test.uiScale;
				const auto iconSize = HostChromeIconSize(fontSize);
				const auto extent = HostChromeButtonExtent(fontSize, padding);
				const auto header = ResolveTrailingControlLayout(
					24.0f, 1896.0f, extent, spacing);
				const auto footer = ResolveTrailingControlLayout(
					36.0f, 1264.0f, extent, spacing);
				require(
						iconSize == fontSize * 1.5f &&
							extent > TitleBarButtonExtent(fontSize, padding),
						"host chrome did not use its larger icon scale");
				require(
						header.controlMaxX == 1896.0f &&
							header.controlMinX == 1896.0f - extent &&
							header.adjacentMaxX ==
								header.controlMinX - spacing,
						"close button geometry changed");
				require(
						footer.controlMaxX == 1264.0f &&
							footer.controlMinX == 1264.0f - extent &&
							footer.adjacentMaxX ==
								footer.controlMinX - spacing,
						"footer gear geometry changed");
				require(
						header.adjacentMaxX <= header.controlMinX &&
							footer.adjacentMaxX <= footer.controlMinX,
						"host chrome overlapped adjacent content");
			}
		});

		runner.test(
			"collapsible row hover remains owned by the full row",
			[] {
				struct Case
				{
					RowHighlightStyle highlight;
					RowClickBehavior click;
				};
				constexpr std::array cases{
					Case{
						RowHighlightStyle::kSelectable,
						RowClickBehavior::kSelect
					},
					Case{
						RowHighlightStyle::kRoundedFill,
						RowClickBehavior::kToggle
					}
				};
				for (const auto& test : cases)
				{
					InteractiveRow ui;
					bool expanded{};
					const RowOptions options{
						.id = "CollapsibleRow",
						.label = "Collapsible row",
						.leadingAffordance =
							RowLeadingAffordance::kArrow,
						.expanded = &expanded,
						.textColor =
							ImGui::GetColorU32(ImGuiCol_Text),
						.hoveredTextColor =
							ImGui::GetColorU32(ImGuiCol_Text),
						.highlightStyle = test.highlight,
						.clickBehavior = test.click
					};
					const auto initial = ui.Frame(
						options,
						{ -100.0f, -100.0f },
						false);
					const auto arrow = ArrowPoint(initial.row.rect);
					(void)ui.Frame(options, arrow, false);
					RequireFullHighlight(ui.Frame(options, arrow, false));

					const auto label = LabelPoint(initial.row.rect);
					RequireFullHighlight(ui.Frame(options, label, false));
					RequireFullHighlight(ui.Frame(options, arrow, false));
					RequireFullHighlight(ui.Frame(options, arrow, false));
				}
			});

		runner.test(
			"selectable collapsible row routes press origin without overlap",
			[] {
				InteractiveRow ui;
				bool expanded{};
				const RowOptions options{
					.id = "SelectableCollapsibleRow",
					.label = "Selectable collapsible row",
					.leadingAffordance = RowLeadingAffordance::kArrow,
					.expanded = &expanded,
					.textColor = ImGui::GetColorU32(ImGuiCol_Text),
					.hoveredTextColor =
						ImGui::GetColorU32(ImGuiCol_Text)
				};
				const auto initial = ui.Frame(
					options,
					{ -100.0f, -100.0f },
					false);
				const auto arrow = ArrowPoint(initial.row.rect);
				const auto label = LabelPoint(initial.row.rect);

				(void)ui.Frame(options, arrow, false);
				const auto arrowDown = ui.Frame(options, arrow, true);
				require(
					arrowDown.active,
					"arrow press did not activate the full row");
				const auto arrowRelease = ui.Frame(options, arrow, false);
				require(
					expanded && !arrowRelease.row.pressed,
					"arrow click did not toggle without selecting");

				(void)ui.Frame(options, label, true);
				const auto labelRelease = ui.Frame(options, label, false);
				require(
					labelRelease.row.pressed && expanded,
					"label click did not select without toggling");

				(void)ui.Frame(options, arrow, true);
				const auto crossedFromArrow =
					ui.Frame(options, label, false);
				require(
					!crossedFromArrow.row.pressed && expanded,
					"arrow-to-label release was not canceled");

				(void)ui.Frame(options, label, true);
				const auto crossedFromLabel =
					ui.Frame(options, arrow, false);
				require(
					!crossedFromLabel.row.pressed && expanded,
					"label-to-arrow release was not canceled");

				(void)ui.Frame(options, arrow, false, true);
				ui.Key(ImGuiKey_Enter, true);
				const auto keyboard = ui.Frame(options, arrow, false);
				ui.Key(ImGuiKey_Enter, false);
				(void)ui.Frame(options, arrow, false);
				require(
					keyboard.row.pressed && expanded,
					"keyboard activation followed parked mouse position");

				(void)ui.Frame(options, arrow, true, false, true);
				const auto disabledRelease =
					ui.Frame(options, arrow, false, false, true);
				require(
					!disabledRelease.row.pressed && expanded,
					"disabled collapsible row accepted input");
			});
	}
}
