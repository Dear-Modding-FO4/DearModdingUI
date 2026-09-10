#include "PresentationTestSupport.h"

namespace vmm_tests::support::presentation
{
	DMUI_Result AcceptUIClient(DMUI_ClientHandle) noexcept
	{
		return DMUI_RESULT_OK;
	}

	ImGuiFrame::ImGuiFrame()
	{
		m_imgui.BeginWindow("##PresentationServicesTest");
	}

	ImGuiFrame::~ImGuiFrame()
	{
		m_imgui.EndWindow();
	}

	InteractiveImGui::InteractiveImGui() = default;
	InteractiveImGui::~InteractiveImGui() = default;

	void InteractiveImGui::Begin(
		ImVec2 a_mouse,
		bool a_mouseDown,
		const char* a_input)
	{
		auto& io = ImGui::GetIO();
		io.AddMousePosEvent(a_mouse.x, a_mouse.y);
		io.AddMouseButtonEvent(ImGuiMouseButton_Left, a_mouseDown);
		if (a_input)
			io.AddInputCharactersUTF8(a_input);
		m_imgui.BeginWindow(
			"##InteractivePresentationTest",
			{ 0.0f, 0.0f },
			{ 640.0f, 480.0f },
			ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_NoSavedSettings,
			ImGuiCond_Always);
		ImGui::SetCursorScreenPos({ 20.0f, 20.0f });
		ImGui::SetNextItemWidth(200.0f);
	}

	void InteractiveImGui::End()
	{
		m_imgui.EndWindow(true);
	}

	void InteractiveImGui::Key(ImGuiKey a_key, bool a_down)
	{
		ImGui::GetIO().AddKeyEvent(a_key, a_down);
	}
}
