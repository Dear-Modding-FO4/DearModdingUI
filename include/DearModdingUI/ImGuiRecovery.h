#pragma once

#include <cstring>
#include <new>
#include <optional>

#include <imgui/imgui_internal.h>

namespace DearModdingUI
{
	struct ImGuiStackDepths
	{
		int windows{ 0 };
		int ids{ 0 };
		int tables{ 0 };
		int trees{ 0 };
		int colors{ 0 };
		int styleVariables{ 0 };
		int fonts{ 0 };
		int focusScopes{ 0 };
		int groups{ 0 };
		int itemFlags{ 0 };
		int popups{ 0 };
		int disabled{ 0 };

		[[nodiscard]] bool operator==(
			const ImGuiStackDepths&) const noexcept = default;
	};

	struct ImGuiRecoveryResult
	{
		ImGuiStackDepths before;
		ImGuiStackDepths after;

		[[nodiscard]] bool Repaired() const noexcept
		{
			return before != after;
		}
	};

	class ImGuiRecoverySnapshot
	{
	public:
		// Only valid inside an ImGui frame; CurrentWindow is null once Render() has run.
		[[nodiscard]] static std::optional<ImGuiRecoverySnapshot> Capture() noexcept
		{
			try
			{
				auto* context = ImGui::GetCurrentContext();
				if (!context || !context->CurrentWindow)
					return std::nullopt;
				return ImGuiRecoverySnapshot(*context);
			}
			catch (...)
			{
				return std::nullopt;
			}
		}

		ImGuiRecoverySnapshot(const ImGuiRecoverySnapshot&) = delete;
		ImGuiRecoverySnapshot& operator=(const ImGuiRecoverySnapshot&) = delete;

		ImGuiRecoverySnapshot(ImGuiRecoverySnapshot&& a_other) noexcept :
			m_context(a_other.m_context),
			m_stackState(a_other.m_stackState),
			m_nextWindowData(a_other.m_nextWindowData),
			m_nextItemData(a_other.m_nextItemData),
			m_openPopupData(a_other.m_openPopupData),
			m_openPopupSize(a_other.m_openPopupSize)
		{
			a_other.m_openPopupData = nullptr;
			a_other.m_openPopupSize = 0;
		}

		~ImGuiRecoverySnapshot()
		{
			if (m_openPopupData)
				ImGui::MemFree(m_openPopupData);
		}

		[[nodiscard]] ImGuiRecoveryResult RecoverAfterCallback() noexcept
		{
			ImGui::SetCurrentContext(m_context);
			const auto before = CaptureStackDepths(*m_context);
			auto& io = ImGui::GetIO();
			const auto assertEnabled = io.ConfigErrorRecoveryEnableAssert;
			io.ConfigErrorRecoveryEnableAssert = false;
			ImGui::ErrorRecoveryTryToRecoverState(&m_stackState);
			io.ConfigErrorRecoveryEnableAssert = assertEnabled;
			const auto after = CaptureStackDepths(*m_context);
			m_context->NextWindowData = m_nextWindowData;
			m_context->NextItemData = m_nextItemData;
			return { before, after };
		}

		[[nodiscard]] ImGuiRecoveryResult RecoverFailure() noexcept
		{
			const auto recovery = RecoverAfterCallback();
			if (m_context->OpenPopupStack.Data)
				ImGui::MemFree(m_context->OpenPopupStack.Data);
			m_context->OpenPopupStack.Data = m_openPopupData;
			m_context->OpenPopupStack.Size = m_openPopupSize;
			m_context->OpenPopupStack.Capacity = m_openPopupSize;
			m_openPopupData = nullptr;
			m_openPopupSize = 0;
			if (!m_context->OpenPopupStack.empty())
				ImGui::ClosePopupToLevel(0, true);
			return recovery;
		}

	private:
		[[nodiscard]] static ImGuiStackDepths CaptureStackDepths(
			const ImGuiContext& a_context) noexcept
		{
			const auto* window = a_context.CurrentWindow;
			return {
				a_context.CurrentWindowStack.Size,
				window ? window->IDStack.Size : 0,
				a_context.TablesTempDataStacked,
				window ? window->DC.TreeDepth : 0,
				a_context.ColorStack.Size,
				a_context.StyleVarStack.Size,
				a_context.FontStack.Size,
				a_context.FocusScopeStack.Size,
				a_context.GroupStack.Size,
				a_context.ItemFlagsStack.Size,
				a_context.BeginPopupStack.Size,
				a_context.DisabledStackSize
			};
		}

		explicit ImGuiRecoverySnapshot(ImGuiContext& a_context) :
			m_context(&a_context),
			m_nextWindowData(a_context.NextWindowData),
			m_nextItemData(a_context.NextItemData)
		{
			ImGui::ErrorRecoveryStoreState(&m_stackState);
			if (a_context.OpenPopupStack.empty())
				return;
			m_openPopupSize = a_context.OpenPopupStack.Size;
			m_openPopupData = static_cast<ImGuiPopupData*>(
				ImGui::MemAlloc(static_cast<size_t>(m_openPopupSize) * sizeof(ImGuiPopupData)));
			if (!m_openPopupData)
				throw std::bad_alloc();
			std::memcpy(
				m_openPopupData,
				a_context.OpenPopupStack.Data,
				static_cast<size_t>(m_openPopupSize) * sizeof(ImGuiPopupData));
		}

		ImGuiContext* m_context;
		ImGuiErrorRecoveryState m_stackState{};
		ImGuiNextWindowData m_nextWindowData;
		ImGuiNextItemData m_nextItemData;
		ImGuiPopupData* m_openPopupData{ nullptr };
		int m_openPopupSize{ 0 };
	};
}
