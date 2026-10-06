#pragma once

#include "ImGuiTestContext.h"

#include <DearModdingUI/host/UIAdapter.h>

#include <DearModdingUI/Client.h>

#include <imgui/imgui.h>

namespace dmui::detail
{
	struct ClientTestAccess
	{
		static void Bind(
			Client& a_client,
			const DMUI_HostAPI& a_api,
			DMUI_ClientHandle a_handle,
			uint32_t a_abiMinor = DMUI_ABI_MINOR)
		{
			a_client.api_ = &a_api;
			a_client.uiAPI_ = a_api.ui;
			a_client.clientHandle_ = a_handle;
			a_client.abiMinor_ = a_abiMinor;
		}

		static DMUI_Result Draw(Client& a_client, std::function<void()> a_callback)
		{
			Client::PageRegistration registration{ 1u, &a_client, std::move(a_callback) };
			return Client::InvokeDrawingCallback<Client::PageRegistration>(&registration);
		}
	};
}

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
