#pragma once

#include "ImGuiTestContext.h"

#include <DearModdingUI/host/UIAdapter.h>

#include <DearModdingUI/Client.h>

#include <imgui/imgui.h>

namespace vmm_tests::support::presentation
{
	[[nodiscard]] DMUI_Result AcceptUIClient(
		DMUI_ClientHandle a_client) noexcept;

	class ImGuiFrame
	{
	public:
		ImGuiFrame();
		~ImGuiFrame();

		ImGuiFrame(const ImGuiFrame&) = delete;
		ImGuiFrame& operator=(const ImGuiFrame&) = delete;

	private:
		DearModdingUI::UI::Testing::ValidationOverride m_uiValidation{
			&AcceptUIClient
		};
		dmui::ui::detail::ScopedContext m_uiContext{
			&DearModdingUI::UI::API(),
			1u
		};
		ImGuiTestContext m_imgui{
			{ .disableErrorRecovery = true }
		};
	};

	class InteractiveImGui
	{
	public:
		InteractiveImGui();
		~InteractiveImGui();

		InteractiveImGui(const InteractiveImGui&) = delete;
		InteractiveImGui& operator=(const InteractiveImGui&) = delete;

		void Begin(
			ImVec2 a_mouse,
			bool a_mouseDown,
			const char* a_input = nullptr);
		void End();
		void Key(ImGuiKey a_key, bool a_down);

	private:
		DearModdingUI::UI::Testing::ValidationOverride m_uiValidation{
			&AcceptUIClient
		};
		dmui::ui::detail::ScopedContext m_uiContext{
			&DearModdingUI::UI::API(),
			1u
		};
		ImGuiTestContext m_imgui{
			{
				.displaySize = { 640.0f, 480.0f },
				.disableInputTrickle = true,
				.disableErrorRecovery = true
			}
		};
	};
}
