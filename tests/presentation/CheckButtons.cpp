#include "../Harness.h"
#include "../support/ImGuiTestContext.h"

#include <DearModdingUI/controls/Button.h>
#include <DearModdingUI/controls/Tooltip.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI;

		constexpr ImVec2 kOffscreen{ -1000.0f, -1000.0f };

		struct ButtonFrame
		{
			bool pressed{};
			bool tooltipDue{};
			ImGuiID id{};
			ImRect rect;
		};

		// One frame with the mouse at a_mouse and the left button at a_down.
		[[nodiscard]] ButtonFrame DrawFrame(
			support::ImGuiTestContext& a_imgui,
			const ButtonSpec& a_spec,
			ImVec2 a_mouse,
			bool a_down = false)
		{
			auto& io = ImGui::GetIO();
			io.AddMousePosEvent(a_mouse.x, a_mouse.y);
			io.AddMouseButtonEvent(0, a_down);
			a_imgui.BeginWindow("##ButtonTest", { 20.0f, 20.0f }, { 320.0f, 200.0f });
			ButtonFrame frame;
			frame.pressed = DrawButton(a_spec);
			frame.tooltipDue = Tooltip::IsDueForLastItem();
			frame.id = ImGui::GetItemID();
			frame.rect = { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() };
			a_imgui.EndWindow();
			return frame;
		}

		[[nodiscard]] bool Clicks(
			support::ImGuiTestContext& a_imgui,
			const ButtonSpec& a_spec)
		{
			const auto center = DrawFrame(a_imgui, a_spec, kOffscreen).rect.GetCenter();
			(void)DrawFrame(a_imgui, a_spec, center);
			(void)DrawFrame(a_imgui, a_spec, center, true);
			(void)DrawFrame(a_imgui, a_spec, center, true);
			return DrawFrame(a_imgui, a_spec, center, false).pressed;
		}
	}

	void run_button_checks(Runner& runner)
	{
		runner.test("buttons report a click and ignore disabled ones", [] {
			support::ImGuiTestContext imgui{ { .disableInputTrickle = true } };
			ButtonSpec button{ .label = "Run" };
			require(Clicks(imgui, button), "an enabled button missed its click");
			button.enabled = false;
			require(!Clicks(imgui, button), "a disabled button reported a click");
		});

		runner.test("buttons fit their content and honor explicit sizes", [] {
			support::ImGuiTestContext imgui;
			imgui.BeginWindow("##ButtonSize");
			const auto fit = MeasureButton({ .label = "Run" });
			const auto wide = MeasureButton({ .label = "Run", .size = { 200.0f, 0.0f } });
			imgui.EndWindow();
			require(fit.x > 0.0f && fit.y > 0.0f, "a label button measured empty");
			require(wide.x == 200.0f && wide.y == fit.y,
				"an explicit axis changed the other axis or was ignored");
		});

		runner.test("button descriptors need an id or label and a known style", [] {
			DMUI_ButtonDescriptor button{};
			require(ValidateButton(&button) == DMUI_RESULT_INVALID_ARGUMENT,
				"a button without id or label was accepted");
			button.label = "Run";
			require(ValidateButton(&button) == DMUI_RESULT_OK,
				"a label-only button was rejected");
			button.style = DMUI_BUTTON_STYLE_SUBTLE + 1u;
			require(ValidateButton(&button) == DMUI_RESULT_INVALID_ARGUMENT,
				"an unknown style was accepted");
		});

		runner.test("item tooltips follow the theme hover delay while disabled", [] {
			support::ImGuiTestContext imgui{ { .disableInputTrickle = true } };
			ImGui::GetStyle().HoverDelayNormal = 0.5f;
			ImGui::GetIO().DeltaTime = 0.1f;
			const ButtonSpec button{ .label = "Run", .enabled = false };
			const auto center = DrawFrame(imgui, button, kOffscreen).rect.GetCenter();
			require(!DrawFrame(imgui, button, center).tooltipDue,
				"the tooltip appeared before the hover delay");
			auto due = false;
			for (auto frame = 0; frame < 10 && !due; ++frame)
				due = DrawFrame(imgui, button, center).tooltipDue;
			require(due, "the tooltip never appeared on a disabled item");
		});

		runner.test("item tooltips appear on navigation focus without the mouse", [] {
			support::ImGuiTestContext imgui{ { .disableInputTrickle = true } };
			ImGui::GetStyle().HoverDelayNormal = 0.3f;
			ImGui::GetIO().DeltaTime = 0.1f;
			ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			const ButtonSpec button{ .label = "Run" };
			const auto laidOut = DrawFrame(imgui, button, kOffscreen);
			require(!laidOut.tooltipDue, "an unfocused item showed a tooltip");

			auto& context = *imgui.Get();
			context.NavWindow = ImGui::FindWindowByName("##ButtonTest");
			context.NavId = laidOut.id;
			context.NavCursorVisible = true;
			context.NavHighlightItemUnderNav = true;
			auto due = false;
			for (auto frame = 0; frame < 10 && !due; ++frame)
				due = DrawFrame(imgui, button, kOffscreen).tooltipDue;
			require(due, "a navigation-focused item never showed its tooltip");
		});
	}
}
