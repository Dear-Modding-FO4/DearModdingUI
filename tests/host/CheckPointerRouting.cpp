#include "../Harness.h"
#include "../support/ImGuiTestContext.h"

#include <DearModdingUI/host/PointerRouting.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <DearModdingUI/host/UIAdapter.h>
#include <DearModdingUI/UI.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <bit>
#include <optional>

namespace vmm_tests
{
	using namespace DearModdingUI;

	namespace
	{
		constexpr DMUI_PageHandle kOwner{ 101 };
		constexpr ImVec2 kInside{ 150.0f, 150.0f };
		constexpr ImVec2 kOutside{ 600.0f, 500.0f };

		// Mirrors the platform: forwarded messages reach ImGui, diverted ones do not.
		class RoutedPointer
		{
		public:
			RoutedPointer()
			{
				PointerRouting::Reset();
				Frame();
				Frame();
			}

			~RoutedPointer()
			{
				PointerRouting::Reset();
			}

			void Frame(bool a_popup = false, DMUI_PageHandle a_owner = kOwner)
			{
				PointerRouting::BeginFrame(a_owner);
				imgui.BeginWindow("Console", { 100, 100 }, { 200, 200 },
					ImGuiWindowFlags_NoSavedSettings);
				if (a_popup)
				{
					ImGui::OpenPopup("Menu");
					if (ImGui::BeginPopup("Menu"))
						ImGui::EndPopup();
				}
				imgui.EndWindow(true);
			}

			void MoveTo(ImVec2 a_position)
			{
				ImGui::GetIO().MousePos = a_position;
				m_position = { a_position.x, a_position.y };
			}

			[[nodiscard]] DMUI_Vec2 Position() const noexcept
			{
				return m_position;
			}

			// The message position is what routing hit-tests, not io.MousePos.
			bool Press(uint32_t a_button, DMUI_PageHandle a_owner = kOwner, bool a_excluded = false,
				std::optional<DMUI_Vec2> a_at = std::nullopt)
			{
				const auto diverted =
					PointerRouting::RoutePress(a_owner, a_button, a_at.value_or(m_position), a_excluded) ==
					PointerRouting::Verdict::kDivert;
				if (!diverted)
					ImGui::GetIO().AddMouseButtonEvent(std::countr_zero(a_button), true);
				return diverted;
			}

			bool Release(uint32_t a_button, DMUI_PageHandle a_owner = kOwner)
			{
				const auto diverted =
					PointerRouting::RouteRelease(a_owner, a_button) == PointerRouting::Verdict::kDivert;
				if (!diverted)
					ImGui::GetIO().AddMouseButtonEvent(std::countr_zero(a_button), false);
				return diverted;
			}

			support::ImGuiTestContext imgui;

		private:
			DMUI_Vec2 m_position{};
		};
	}

	void run_pointer_routing_checks(Runner& runner)
	{
		runner.test("pointer routing diverts a press outside every window and keeps ImGui state", [] {
			RoutedPointer pointer;
			pointer.MoveTo(kInside);
			require(!pointer.Press(DMUI_POINTER_BUTTON_LEFT), "a press on a window was diverted");
			pointer.Frame();
			require(!pointer.Release(DMUI_POINTER_BUTTON_LEFT), "the release of an ImGui press was diverted");
			pointer.Frame();
			const auto* focused = ImGui::GetCurrentContext()->NavWindow;
			require(focused != nullptr, "clicking the window did not focus it");

			pointer.MoveTo(kOutside);
			require(pointer.Press(DMUI_POINTER_BUTTON_LEFT), "a world press reached ImGui");
			pointer.Frame();
			const auto held = PointerRouting::FrameFor(kOwner);
			require(held && held->pressed == DMUI_POINTER_BUTTON_LEFT &&
					held->down == DMUI_POINTER_BUTTON_LEFT && held->released == 0,
				"the diverted press was not reported as pressed and down");
			require(!ImGui::GetIO().MouseDown[0] && ImGui::GetCurrentContext()->NavWindow == focused,
				"a world press changed ImGui focus or button state");

			require(pointer.Release(DMUI_POINTER_BUTTON_LEFT), "the release of a diverted press reached ImGui");
			pointer.Frame();
			const auto released = PointerRouting::FrameFor(kOwner);
			require(released && released->down == 0 && released->pressed == 0 &&
					released->released == DMUI_POINTER_BUTTON_LEFT,
				"the diverted release was not reported for one frame");
		});

		runner.test("pointer routing reports a press and release within one frame", [] {
			RoutedPointer pointer;
			pointer.MoveTo(kOutside);
			require(pointer.Press(DMUI_POINTER_BUTTON_RIGHT) && pointer.Release(DMUI_POINTER_BUTTON_RIGHT),
				"a quick world click reached ImGui");
			pointer.Frame();
			const auto frame = PointerRouting::FrameFor(kOwner);
			require(frame && frame->down == 0 && frame->pressed == DMUI_POINTER_BUTTON_RIGHT &&
					frame->released == DMUI_POINTER_BUTTON_RIGHT,
				"a quick world click lost an edge");
			pointer.Frame();
			require(PointerRouting::FrameFor(kOwner)->pressed == 0, "an edge outlived its frame");
		});

		runner.test("pointer routing leaves ImGui presses, drags, and popups to ImGui", [] {
			RoutedPointer pointer;
			pointer.MoveTo(kInside);
			require(!pointer.Press(DMUI_POINTER_BUTTON_LEFT), "a press on a window was diverted");
			pointer.Frame();
			// A drag that leaves the window still belongs to ImGui, release included.
			pointer.MoveTo(kOutside);
			require(!pointer.Press(DMUI_POINTER_BUTTON_RIGHT), "a press during an ImGui drag was diverted");
			require(!pointer.Release(DMUI_POINTER_BUTTON_LEFT), "the release of an ImGui drag was diverted");
			require(!pointer.Release(DMUI_POINTER_BUTTON_RIGHT), "the release of an ImGui press was diverted");
			pointer.Frame();
			require(PointerRouting::FrameFor(kOwner)->pressed == 0, "an ImGui press was reported");

			pointer.Frame(true);
			pointer.Frame(true);
			require(!pointer.Press(DMUI_POINTER_BUTTON_LEFT), "a press with a popup open was diverted");
			require(!pointer.Release(DMUI_POINTER_BUTTON_LEFT), "the release of a popup press was diverted");
		});

		runner.test("pointer routing diverts wheel input only when no window takes it", [] {
			RoutedPointer pointer;
			pointer.MoveTo(kInside);
			require(PointerRouting::RouteWheel(kOwner, { 0.0f, 1.0f }, pointer.Position(), false) == PointerRouting::Verdict::kForward,
				"wheel over a window was diverted");
			pointer.MoveTo(kOutside);
			require(PointerRouting::RouteWheel(kOwner, { 0.0f, 1.0f }, pointer.Position(), false) == PointerRouting::Verdict::kDivert &&
					PointerRouting::RouteWheel(kOwner, { 0.0f, 2.0f }, pointer.Position(), false) == PointerRouting::Verdict::kDivert,
				"world wheel input reached ImGui");
			pointer.Frame();
			require(PointerRouting::FrameFor(kOwner)->wheel.y == 3.0f, "wheel input was not accumulated");
		});

		runner.test("pointer routing forwards everything without an owning page", [] {
			RoutedPointer pointer;
			pointer.MoveTo(kOutside);
			require(!pointer.Press(DMUI_POINTER_BUTTON_LEFT, DMUI_INVALID_PAGE_HANDLE) &&
					!pointer.Release(DMUI_POINTER_BUTTON_LEFT, DMUI_INVALID_PAGE_HANDLE) &&
					PointerRouting::RouteWheel(DMUI_INVALID_PAGE_HANDLE, { 0.0f, 1.0f }, pointer.Position(), false) ==
						PointerRouting::Verdict::kForward,
				"input was diverted without a pointer owner");

			require(pointer.Press(DMUI_POINTER_BUTTON_LEFT), "a world press reached ImGui");
			pointer.Frame(false, DMUI_INVALID_PAGE_HANDLE);
			require(!PointerRouting::FrameFor(kOwner), "an ended owner kept its frame");
			require(!pointer.Release(DMUI_POINTER_BUTTON_LEFT, DMUI_INVALID_PAGE_HANDLE),
				"a release after ownership ended was diverted");
		});

		runner.test("pointer routing hit-tests at the message position and leaves excluded input to ImGui", [] {
			RoutedPointer pointer;
			pointer.MoveTo(kOutside);
			require(!pointer.Press(DMUI_POINTER_BUTTON_LEFT, kOwner, false, DMUI_Vec2{ kInside.x, kInside.y }),
				"a press over a window was diverted by the stale cursor position");
			require(!pointer.Release(DMUI_POINTER_BUTTON_LEFT), "an ImGui release was diverted");
			pointer.Frame();
			pointer.MoveTo(kInside);
			require(pointer.Press(DMUI_POINTER_BUTTON_LEFT, kOwner, false, DMUI_Vec2{ kOutside.x, kOutside.y }),
				"a world press was forwarded because the cursor snapshot was over a window");
			require(pointer.Release(DMUI_POINTER_BUTTON_LEFT), "a diverted release reached ImGui");

			pointer.MoveTo(kOutside);
			require(!pointer.Press(DMUI_POINTER_BUTTON_RIGHT, kOwner, true) &&
					!pointer.Release(DMUI_POINTER_BUTTON_RIGHT) &&
					PointerRouting::RouteWheel(kOwner, { 0.0f, 1.0f }, pointer.Position(), true) ==
						PointerRouting::Verdict::kForward,
				"input a hotkey takes was diverted from the existing path");
			pointer.Frame();
			const auto frame = PointerRouting::FrameFor(kOwner);
			require(frame && frame->pressed == DMUI_POINTER_BUTTON_LEFT && frame->wheel.y == 0.0f,
				"excluded input was reported");
		});

		runner.test("pointer routing ends diverted buttons when mouse capture is lost", [] {
			RoutedPointer pointer;
			pointer.MoveTo(kOutside);
			require(pointer.Press(DMUI_POINTER_BUTTON_LEFT) && PointerRouting::HoldsButtons(),
				"a world press was not held");
			PointerRouting::LoseCapture();
			pointer.Frame();
			const auto frame = PointerRouting::FrameFor(kOwner);
			require(!PointerRouting::HoldsButtons() && frame && frame->down == 0 &&
					frame->released == DMUI_POINTER_BUTTON_LEFT,
				"a lost capture left the button down");
			require(pointer.Press(DMUI_POINTER_BUTTON_LEFT), "a new world press was not diverted after the loss");
		});

		runner.test("getPointerState reports diverted input to the owning page only", [] {
			RoutedPointer pointer;
			pointer.MoveTo(kOutside);
			require(pointer.Press(DMUI_POINTER_BUTTON_MIDDLE), "a world press reached ImGui");
			require(PointerRouting::RouteWheel(kOwner, { 0.0f, 1.0f }, pointer.Position(), false) == PointerRouting::Verdict::kDivert,
				"world wheel input reached ImGui");
			// A window under the cursor now captures the mouse, yet the routed wheel stays reported.
			pointer.MoveTo(kInside);
			PointerRouting::BeginFrame(kOwner);
			pointer.imgui.BeginWindow("Console", { 100, 100 }, { 200, 200 }, ImGuiWindowFlags_NoSavedSettings);
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
			pointer.imgui.EndWindow();
			require(owner.active == 1 && owner.captured == 1 && owner.pressed == DMUI_POINTER_BUTTON_MIDDLE &&
					owner.down == DMUI_POINTER_BUTTON_MIDDLE && owner.wheel.y == 1.0f &&
					owner.position.x == kInside.x && owner.viewport.x == 1280.0f,
				"the owning page did not receive the diverted input");
			require(other.active == 0 && other.pressed == 0 && other.down == 0 && other.wheel.y == 0.0f &&
					other.viewport.x == 0.0f,
				"another page received pointer input");
		});
	}
}
