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
				return RenderFrame(
					[&] { return DrawSelectableRow(a_options); },
					a_mouse, a_mouseDown, a_focusRow, a_disabled);
			}

			RowFrame HeadingFrame(
				const RuledHeadingOptions& a_options,
				ImVec2 a_mouse,
				bool a_mouseDown,
				bool a_focusRow = false)
			{
				return RenderFrame(
					[&] {
						DrawRuledHeading(a_options);
						return RowResult{
							.rect = { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() }
						};
					},
					a_mouse, a_mouseDown, a_focusRow, false);
			}

			void Key(ImGuiKey a_key, bool a_down)
			{
				ImGui::GetIO().AddKeyEvent(a_key, a_down);
			}

		private:
			template <class Draw>
			RowFrame RenderFrame(
				Draw a_draw,
				ImVec2 a_mouse,
				bool a_mouseDown,
				bool a_focusRow,
				bool a_disabled)
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
				const auto row = a_draw();
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
				constexpr std::array cases{
					RowHighlightStyle::kSelectable,
					RowHighlightStyle::kRoundedFill
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
						.highlightStyle = test
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
			"expandable rows share whole-row mouse keyboard and disabled behavior",
			[] {
				for (const auto highlight :
					{ RowHighlightStyle::kSelectable, RowHighlightStyle::kRoundedFill })
				{
					InteractiveRow ui;
					bool expanded{};
					const RowOptions options{
						.id = "ExpandableRow",
						.label = "Expandable row",
						.selected = true,
						.leadingAffordance = RowLeadingAffordance::kArrow,
						.expanded = &expanded,
						.textColor = ImGui::GetColorU32(ImGuiCol_Text),
						.hoveredTextColor = ImGui::GetColorU32(ImGuiCol_Text),
						.highlightStyle = highlight
					};
					const auto initial = ui.Frame(
						options, { -100.0f, -100.0f }, false);
					const auto arrow = ArrowPoint(initial.row.rect);
					const auto label = LabelPoint(initial.row.rect);
					(void)ui.Frame(options, arrow, false);
					const auto arrowDown = ui.Frame(options, arrow, true);
					require(arrowDown.active && !expanded,
						"row toggled before release or did not own the press");
					const auto arrowRelease = ui.Frame(options, arrow, false);
					require(expanded && arrowRelease.row.pressed,
						"arrow click did not activate and expand the row");

					for (const auto expected : { false, true })
					{
						(void)ui.Frame(options, label, false);
						(void)ui.Frame(options, label, true);
						const auto released = ui.Frame(options, label, false);
						require(released.row.pressed && expanded == expected,
							"label clicks did not collapse and reopen the row");
					}

					(void)ui.Frame(options, arrow, true);
					require(ui.Frame(options, label, false).row.pressed && !expanded,
						"arrow-to-label release did not stay within the same row");
					(void)ui.Frame(options, label, true);
					require(ui.Frame(options, arrow, false).row.pressed && expanded,
						"label-to-arrow release did not stay within the same row");

					(void)ui.Frame(options, arrow, true);
					require(
						!ui.Frame(options, { 600.0f, 300.0f }, false).row.pressed &&
							expanded,
						"release outside the row did not cancel the toggle");

					for (const auto key : { ImGuiKey_Enter, ImGuiKey_Space })
					{
						const auto before = expanded;
						(void)ui.Frame(options, arrow, false, true);
						ui.Key(key, true);
						const auto keyboard = ui.Frame(options, arrow, false);
						ui.Key(key, false);
						(void)ui.Frame(options, arrow, false);
						require(keyboard.row.pressed && expanded != before,
							"keyboard activation did not toggle the whole row");
					}

					const auto before = expanded;
					(void)ui.Frame(options, arrow, true, false, true);
					const auto disabledRelease =
						ui.Frame(options, arrow, false, false, true);
					require(!disabledRelease.row.pressed && expanded == before,
						"disabled row accepted a toggle");
					(void)ui.Frame(options, arrow, false, true, true);
					ui.Key(ImGuiKey_Enter, true);
					const auto disabledKeyboard = ui.Frame(options, arrow, false, false, true);
					ui.Key(ImGuiKey_Enter, false);
					(void)ui.Frame(options, arrow, false, false, true);
					require(!disabledKeyboard.row.pressed && expanded == before,
						"disabled row accepted keyboard activation");
				}
			});

		runner.test("leading and centered headings share expansion interaction", [] {
			for (const auto layout :
				{ RuledHeadingLayout::kLeadingRow, RuledHeadingLayout::kCentered })
			{
				InteractiveRow ui;
				bool expanded{};
				const RuledHeadingOptions options{
					.key = "SharedHeading",
					.text = "Shared heading",
					.expanded = &expanded,
					.layout = layout
				};
				(void)ui.HeadingFrame(options, { -100.0f, -100.0f }, false);
				const auto initial = ui.HeadingFrame(
					options, { -100.0f, -100.0f }, false);
				const auto point = initial.row.rect.GetCenter();
				for (const auto expected : { true, false, true })
				{
					(void)ui.HeadingFrame(options, point, false);
					(void)ui.HeadingFrame(options, point, true);
					(void)ui.HeadingFrame(options, point, false);
					require(expanded == expected,
						"heading layout changed whole-row toggle behavior");
				}
				(void)ui.HeadingFrame(options, point, false, true);
				ui.Key(ImGuiKey_Enter, true);
				(void)ui.HeadingFrame(options, point, false);
				ui.Key(ImGuiKey_Enter, false);
				(void)ui.HeadingFrame(options, point, false);
				require(!expanded, "heading ignored keyboard expansion");
			}
		});
	}
}
