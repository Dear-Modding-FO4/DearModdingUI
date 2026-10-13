#include "../Harness.h"
#include "../support/ImGuiTestContext.h"

#include <DearModdingUI/host/PointerRouting.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <DearModdingUI/host/UIAdapter.h>
#include <DearModdingUI/UI.h>

#include <imgui/imgui.h>

namespace vmm_tests
{
	using namespace DearModdingUI;

	void run_pointer_routing_checks(Runner& runner)
	{
		runner.test("world clicks reach only the owning page", [] {
			constexpr DMUI_PageHandle kOwner{ 101 };
			support::ImGuiTestContext imgui;
			PointerRouting::Reset();
			for (int frame = 0; frame < 2; ++frame)
			{
				PointerRouting::BeginFrame(kOwner);
				imgui.BeginWindow("Console", { 100, 100 }, { 200, 200 }, ImGuiWindowFlags_NoSavedSettings);
				imgui.EndWindow(true);
			}
			require(PointerRouting::RoutePress(kOwner, DMUI_POINTER_BUTTON_LEFT, { 600, 500 }, false) ==
					PointerRouting::Verdict::kDivert,
				"a world press was not diverted");

			PointerRouting::BeginFrame(kOwner);
			imgui.BeginWindow("Console", { 100, 100 }, { 200, 200 }, ImGuiWindowFlags_NoSavedSettings);
			const RenderExecution::Guard execution{ RenderExecution::Phase::kFrameDraw };
			const dmui::ui::detail::ScopedContext ui{ &UI::API(), 77 };
			DMUI_PointerState owner{};
			DMUI_PointerState other{ 1 };
			{
				const RenderExecution::ClientGuard page{ 77, true, kOwner };
				owner = dmui::ui::GetPointerState();
			}
			{
				const RenderExecution::ClientGuard page{ 77, true, kOwner + 1 };
				other = dmui::ui::GetPointerState();
			}
			imgui.EndWindow();
			PointerRouting::Reset();
			require(owner.active == 1 && owner.pressed == DMUI_POINTER_BUTTON_LEFT &&
					owner.down == DMUI_POINTER_BUTTON_LEFT,
				"the owning page did not receive the world press");
			require(other.active == 0 && other.pressed == 0 && other.down == 0 && other.viewport.x == 0.0f,
				"another page received pointer input");
		});
	}
}
