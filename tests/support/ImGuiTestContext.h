#pragma once

#include <imgui/imgui.h>

namespace vmm_tests::support
{
	struct ImGuiContextOptions
	{
		ImVec2 displaySize{ 1280.0f, 720.0f };
		float deltaTime{ 1.0f / 60.0f };
		bool disableInputTrickle{};
		bool disableErrorRecovery{};
	};

	class ImGuiTestContext
	{
	public:
		explicit ImGuiTestContext(
			const ImGuiContextOptions& a_options = {})
		{
			m_context = ImGui::CreateContext();
			auto& io = ImGui::GetIO();
			io.DisplaySize = a_options.displaySize;
			io.DeltaTime = a_options.deltaTime;
			io.IniFilename = nullptr;
			io.ConfigInputTrickleEventQueue =
				!a_options.disableInputTrickle;
			if (a_options.disableErrorRecovery)
			{
				io.ConfigErrorRecoveryEnableAssert = false;
				io.ConfigErrorRecoveryEnableDebugLog = false;
				io.ConfigErrorRecoveryEnableTooltip = false;
			}
			(void)io.Fonts->Build();
		}

		ImGuiTestContext(const ImGuiTestContext&) = delete;
		ImGuiTestContext& operator=(const ImGuiTestContext&) = delete;

		~ImGuiTestContext()
		{
			ImGui::DestroyContext(m_context);
		}

		void BeginWindow(
			const char* a_name,
			ImGuiWindowFlags a_flags = ImGuiWindowFlags_None)
		{
			ImGui::NewFrame();
			(void)ImGui::Begin(a_name, nullptr, a_flags);
			m_frameOpen = true;
		}

		void BeginWindow(
			const char* a_name,
			ImVec2 a_position,
			ImVec2 a_size,
			ImGuiWindowFlags a_flags = ImGuiWindowFlags_None,
			ImGuiCond a_condition = ImGuiCond_None)
		{
			ImGui::NewFrame();
			ImGui::SetNextWindowPos(a_position, a_condition);
			ImGui::SetNextWindowSize(a_size, a_condition);
			(void)ImGui::Begin(a_name, nullptr, a_flags);
			m_frameOpen = true;
		}

		void EndWindow(bool a_render = false)
		{
			if (!m_frameOpen)
				return;
			ImGui::End();
			if (a_render)
				ImGui::Render();
			else
				ImGui::EndFrame();
			m_frameOpen = false;
		}

		[[nodiscard]] ImGuiContext* Get() const noexcept
		{
			return m_context;
		}

	private:
		ImGuiContext* m_context{};
		bool m_frameOpen{};
	};
}
