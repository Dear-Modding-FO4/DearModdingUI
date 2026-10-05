#include "../Harness.h"
#include "../support/PresentationTestSupport.h"
#include <DearModdingUI/host/ImGuiRecovery.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <imgui/imgui_internal.h>
#include <limits>

namespace vmm_tests
{
	using namespace DearModdingUI;
	using support::presentation::ImGuiFrame;

	void run_draw_list_checks(Runner& runner)
	{
		runner.test("draw clips balance per target and interleave with failed callback recovery", [] {
			ImGuiFrame frame;
			auto recovery = ImGuiRecoverySnapshot::Capture();
			auto* window = ImGui::GetWindowDrawList();
			auto* foreground = ImGui::GetForegroundDrawList();
			auto* background = ImGui::GetBackgroundDrawList();
			const auto windowDepth = window->_ClipRectStack.Size;
			const auto foregroundDepth = foreground->_ClipRectStack.Size;
			const auto backgroundDepth = background->_ClipRectStack.Size;
			{
				UI::DrawListClipScope clips;
				require(dmui::ui::WindowDrawList().PushClipRect({ 30, 30 }, { 90, 90 }) &&
					dmui::ui::ForegroundDrawList().PushClipRect({ 10, 10 }, { 100, 100 }),
					"clip pushes failed");
				require(dmui::ui::checked::DrawListPopClipRect(DMUI_DRAW_TARGET_BACKGROUND) ==
					DMUI_RESULT_INVALID_ARGUMENT, "pop consumed another target's clip");
				dmui::ui::WindowDrawList().PopClipRect();
				require(window->_ClipRectStack.Size == windowDepth && !clips.Balanced(),
					"independent target pop failed or hid foreground imbalance");
				{
					UI::DrawListClipScope nested;
					require(dmui::ui::checked::DrawListPopClipRect(DMUI_DRAW_TARGET_FOREGROUND) ==
						DMUI_RESULT_INVALID_ARGUMENT, "nested callback popped its caller's clip");
				}
				require(dmui::ui::BackgroundDrawList().PushClipRect({ 10, 10 }, { 100, 100 }),
					"background push failed");
				require(dmui::ui::WindowDrawList().PushClipRect({ 30, 30 }, { 90, 90 }),
					"parent clip failed");
				ImGui::BeginChild("Unfinished child", { 100, 100 });
				require(dmui::ui::checked::DrawListPopClipRect(DMUI_DRAW_TARGET_WINDOW) ==
					DMUI_RESULT_INVALID_ARGUMENT, "another window consumed its parent's clip");
				require(dmui::ui::checked::DrawListPopClipRect(DMUI_DRAW_TARGET_FOREGROUND) ==
					DMUI_RESULT_OK, "foreground pop incorrectly depended on current window");
				require(dmui::ui::WindowDrawList().PushClipRect({ 40, 40 }, { 70, 70 }),
					"child clip failed");
				if (dmui::ui::BeginTooltip())
				{
					require(dmui::ui::WindowDrawList().PushClipRect({ 40, 40 }, { 70, 70 }),
						"tooltip clip failed");
					require(dmui::ui::checked::EndTooltip() == DMUI_RESULT_INVALID_ARGUMENT,
						"native window end consumed an outstanding client clip");
				}
			}
			(void)recovery->RecoverFailure();
			require(window->_ClipRectStack.Size == windowDepth &&
				foreground->_ClipRectStack.Size == foregroundDepth &&
				background->_ClipRectStack.Size == backgroundDepth,
				"failed callback leaked a draw-list clip");
			UI::DrawListClipScope clean;
			{
				dmui::ui::ClipRectScope clip{ dmui::ui::WindowDrawList(), { 30, 30 }, { 90, 90 } };
			}
			require(clean.Balanced(), "RAII clip failed to balance");
		});

		runner.test("EndTable reports an inner client clip and recovery preserves native clips", [] {
			ImGuiFrame frame;
			auto* list = ImGui::GetWindowDrawList();
			const auto depth = list->_ClipRectStack.Size;
			for (const bool beforeLayout : { false, true })
			{
				auto recovery = ImGuiRecoverySnapshot::Capture();
				dmui::ui::detail::ClearError();
				{
					UI::DrawListClipScope clips;
					require(dmui::ui::BeginTable("Inner clip", 1), "table did not open");
					const auto push = [&] {
						require(dmui::ui::WindowDrawList().PushClipRect({ 30, 30 }, { 300, 300 }),
							"inner clip failed");
					};
					if (beforeLayout)
						push();
					dmui::ui::TableNextRow();
					(void)dmui::ui::TableNextColumn();
					if (!beforeLayout)
						push();
					dmui::ui::EndTable();
					require(dmui::ui::detail::LastResult() == DMUI_RESULT_INVALID_ARGUMENT &&
						GImGui->CurrentTable != nullptr && list->_ClipRectStack.Size == depth + 2,
						"EndTable consumed an inner client clip instead of reporting it");
				}
				(void)recovery->RecoverFailure();
				require(list->_ClipRectStack.Size == depth && GImGui->CurrentTable == nullptr,
					"inner clip recovery corrupted the native clip stack");
			}

			const auto windows = GImGui->CurrentWindowStack.Size;
			const auto tables = GImGui->TablesTempDataStacked;
			for (const bool fail : { false, true })
			{
				auto recovery = ImGuiRecoverySnapshot::Capture();
				dmui::ui::detail::ClearError();
				{
					UI::DrawListClipScope clips;
					{
						dmui::ui::ClipRectScope clip{ dmui::ui::WindowDrawList(), { 30, 30 }, { 300, 300 } };
						require(dmui::ui::BeginTable("Enclosed table", 1), "table did not open");
						dmui::ui::TableNextRow();
						(void)dmui::ui::TableNextColumn();
						if (!fail)
						{
							require(dmui::ui::checked::EndTable() == DMUI_RESULT_OK &&
								list->_ClipRectStack.Size == depth + 1,
								"EndTable rejected or consumed its enclosing client clip");
						}
					}
					if (!fail)
						require(clips.Balanced() && list->_ClipRectStack.Size == depth,
							"enclosing clip did not balance");
					else
						require(!clips.Balanced() && list->_ClipRectStack.Size == depth + 2,
							"failed clip destructor consumed ImGui's table clip");
				}
				(void)recovery->RecoverFailure();
				require(list->_ClipRectStack.Size == depth &&
					GImGui->CurrentWindowStack.Size == windows &&
					GImGui->TablesTempDataStacked == tables,
					"table recovery over-popped or leaked a clip/scope");
			}
		});

		runner.test("draw geometry rejects malformed inputs without submitting vertices", [] {
			ImGuiFrame frame;
			const auto before = ImGui::GetWindowDrawList()->VtxBuffer.Size;
			const DMUI_Vec2 invalid{ std::numeric_limits<float>::quiet_NaN(), 1 };
			const DMUI_Vec2 valid{ 50, 50 };
			const auto& api = UI::API();
			require(api.drawListAddLine(1, DMUI_DRAW_TARGET_WINDOW, valid, invalid, ~0u, 1) == DMUI_RESULT_INVALID_ARGUMENT &&
				api.drawListAddRect(1, DMUI_DRAW_TARGET_WINDOW, valid, valid, ~0u, 0, -1) == DMUI_RESULT_INVALID_ARGUMENT &&
				api.drawListAddPolyline(1, DMUI_DRAW_TARGET_WINDOW, nullptr, 3, ~0u, 0, 1) == DMUI_RESULT_INVALID_ARGUMENT &&
				api.drawListAddPolygonFilled(1, DMUI_DRAW_TARGET_WINDOW, &valid, 65537, ~0u) == DMUI_RESULT_INVALID_ARGUMENT &&
				api.drawListAddPolyline(1, DMUI_DRAW_TARGET_WINDOW, &invalid, 1, ~0u, 0, 1) == DMUI_RESULT_INVALID_ARGUMENT &&
				api.drawListAddText(1, DMUI_DRAW_TARGET_WINDOW, valid, ~0u, "\xC0\xAF", 2, 0) == DMUI_RESULT_INVALID_ARGUMENT &&
				api.drawListAddLine(1, 99, valid, valid, ~0u, 1) == DMUI_RESULT_INVALID_ARGUMENT,
				"invalid geometry reached ImGui");
			require(ImGui::GetWindowDrawList()->VtxBuffer.Size == before,
				"rejected input submitted partial geometry");
		});
	}
}
