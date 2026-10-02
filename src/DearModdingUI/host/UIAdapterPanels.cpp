#include "UIAdapterInternal.h"
#include <DearModdingUI/UIBindings.generated.h>
#include <DearModdingUI/host/UILayoutState.h>
#include <DearModdingUI/presentation/ThemeLayout.h>
#include <DearModdingUI/presentation/Theme.h>

#include <imgui/imgui_internal.h>

#include <cmath>
#include <utility>
#include <vector>

namespace DearModdingUI::UI
{
	namespace
	{
		using namespace AdapterInternal;

		struct StyleEntry
		{
			DMUI_ClientHandle client;
			DMUI_UIStyleVar kind;
			ImVec2 previous;
			int nativeDepth;
		};
		struct PanelEntry
		{
			DMUI_ClientHandle client;
			ImGuiWindow* window;
		};
		struct LastPanel
		{
			ImGuiWindow* parent{};
			ImGuiID id{};
			ImVec2 cursor{};
			int frame{ -1 };
		};

		thread_local std::vector<StyleEntry> s_styles;
		thread_local std::vector<PanelEntry> s_panels;
		thread_local LayoutDepths s_floor;
		thread_local LastPanel s_lastPanel;

		bool FollowsPanel() noexcept
		{
			return s_lastPanel.frame == ImGui::GetFrameCount() &&
				s_lastPanel.parent == ImGui::GetCurrentWindow() &&
				s_lastPanel.id == GImGui->LastItemData.ID;
		}

		void EndPanelWindow()
		{
			ImGui::EndChild();
			s_lastPanel = { ImGui::GetCurrentWindow(), GImGui->LastItemData.ID,
				ImGui::GetCursorScreenPos(), ImGui::GetFrameCount() };
		}

		void RestoreStyle(const StyleEntry& a_entry) noexcept
		{
			if (a_entry.kind == DMUI_UI_STYLE_VAR_SECTION_GAP)
				Theme::Layout().sectionGap = a_entry.previous.x;
			else if (a_entry.kind == DMUI_UI_STYLE_VAR_PANEL_PADDING)
				Theme::Layout().panelPadding = a_entry.previous;
		}

		DMUI_Result PushStyle(DMUI_ClientHandle a_client, DMUI_UIStyleVar a_kind,
			ImVec2 a_value, uint32_t a_components) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			const auto host = a_kind == DMUI_UI_STYLE_VAR_SECTION_GAP ||
				a_kind == DMUI_UI_STYLE_VAR_PANEL_PADDING;
			ImGuiStyleVar native{};
			ImVec2 previous{};
			if (host)
			{
				const auto scalar = a_kind == DMUI_UI_STYLE_VAR_SECTION_GAP;
				if (a_components != (scalar ? 1u : 2u) ||
					!std::isfinite(a_value.x) || !std::isfinite(a_value.y) ||
					a_value.x < 0.0f || a_value.y < 0.0f)
					return DMUI_RESULT_INVALID_ARGUMENT;
				previous = scalar ? ImVec2{ Theme::Layout().sectionGap, 0.0f } :
					Theme::Layout().panelPadding;
			}
			else
			{
				const auto translated = Bindings::TranslateStyleVar(a_kind, native);
				if (translated != DMUI_RESULT_OK)
					return translated;
				const auto* info = ImGui::GetStyleVarInfo(native);
				if (info->DataType != ImGuiDataType_Float || info->Count != a_components)
					return DMUI_RESULT_INVALID_ARGUMENT;
			}
			try
			{
				s_styles.push_back({ a_client, a_kind, previous, host ? -1 : GImGui->StyleVarStack.Size });
			}
			catch (...)
			{
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			}
			if (a_kind == DMUI_UI_STYLE_VAR_SECTION_GAP)
				Theme::Layout().sectionGap = a_value.x;
			else if (a_kind == DMUI_UI_STYLE_VAR_PANEL_PADDING)
				Theme::Layout().panelPadding = a_value;
			else if (a_components == 1)
				ImGui::PushStyleVar(native, a_value.x);
			else
				ImGui::PushStyleVar(native, a_value);
			return DMUI_RESULT_OK;
		}
	}

	LayoutDepths GetLayoutDepths() noexcept { return { s_styles.size(), s_panels.size() }; }

	LayoutRecovery::LayoutRecovery() noexcept :
		m_depth(GetLayoutDepths()), m_previousFloor(s_floor)
	{
		s_floor = m_depth;
		s_lastPanel = {};
	}

	LayoutRecovery::LayoutRecovery(LayoutRecovery&& a_other) noexcept :
		m_depth(a_other.m_depth), m_previousFloor(a_other.m_previousFloor),
		m_active(std::exchange(a_other.m_active, false))
	{}

	LayoutRecovery::~LayoutRecovery() noexcept { Recover(); }

	void LayoutRecovery::Recover() noexcept
	{
		if (!std::exchange(m_active, false))
			return;
		// Native recovery owns ImGui entries; only restore host-owned values here.
		while (s_styles.size() > m_depth.styles)
		{
			RestoreStyle(s_styles.back());
			s_styles.pop_back();
		}
		s_panels.resize(m_depth.panels);
		s_floor = m_previousFloor;
		s_lastPanel = {};
	}

	namespace Bindings
	{
		DMUI_Result DMUI_CALL PushStyleVarFloat(
			DMUI_ClientHandle a_client, DMUI_UIStyleVar a_styleVar, float a_value) noexcept
		{
			return PushStyle(a_client, a_styleVar, { a_value, 0.0f }, 1u);
		}

		DMUI_Result DMUI_CALL PushStyleVarVec2(
			DMUI_ClientHandle a_client, DMUI_UIStyleVar a_styleVar, DMUI_Vec2 a_value) noexcept
		{
			return PushStyle(a_client, a_styleVar, Native(a_value), 2u);
		}

		DMUI_Result DMUI_CALL PopStyleVar(DMUI_ClientHandle a_client, int32_t a_count) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			if (a_count < 0 || static_cast<size_t>(a_count) > s_styles.size() - s_floor.styles)
				return DMUI_RESULT_INVALID_ARGUMENT;
			auto nativeDepth = GImGui->StyleVarStack.Size;
			for (size_t i = 0; i < static_cast<size_t>(a_count); ++i)
			{
				const auto& entry = s_styles[s_styles.size() - i - 1];
				if (entry.client != a_client ||
					(entry.nativeDepth >= 0 && entry.nativeDepth != --nativeDepth))
					return DMUI_RESULT_INVALID_ARGUMENT;
			}
			for (int32_t i = 0; i < a_count; ++i)
			{
				const auto& entry = s_styles.back();
				if (entry.nativeDepth >= 0)
					ImGui::PopStyleVar();
				else
					RestoreStyle(entry);
				s_styles.pop_back();
			}
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL SameLine(
			DMUI_ClientHandle a_client, float a_offsetFromStartX, float a_spacing) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			if (a_offsetFromStartX == 0.0f && a_spacing < 0.0f && FollowsPanel())
				a_spacing = Theme::Layout().sectionGap;
			ImGui::SameLine(a_offsetFromStartX, a_spacing);
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL BeginPanel(DMUI_ClientHandle a_client, const char* a_id,
			DMUI_Vec2 a_size, DMUI_UIPanelFlags a_flags, uint32_t* a_visible) noexcept
		{
			if (!a_visible)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_visible = 0;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			constexpr auto known = DMUI_UI_PANEL_FLAGS_NO_BACKGROUND | DMUI_UI_PANEL_FLAGS_SCROLLABLE;
			if (!a_id || !*a_id || !std::isfinite(a_size.x) || !std::isfinite(a_size.y) ||
				(a_flags & ~known) != 0)
				return DMUI_RESULT_INVALID_ARGUMENT;
			try
			{
				s_panels.reserve(s_panels.size() + 1);
			}
			catch (...)
			{
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			}
			auto* parent = ImGui::GetCurrentWindow();
			const auto cursor = ImGui::GetCursorScreenPos();
			if (FollowsPanel() && !parent->DC.IsSameLine && !parent->DC.IsSetPos &&
				cursor.x == s_lastPanel.cursor.x && cursor.y == s_lastPanel.cursor.y)
				Theme::SectionSpacing();
			const auto background = (a_flags & DMUI_UI_PANEL_FLAGS_NO_BACKGROUND) == 0;
			ImGuiChildFlags childFlags = background ?
				ImGuiChildFlags_FrameStyle : ImGuiChildFlags_AlwaysUseWindowPadding;
			if (a_size.y == 0.0f)
				childFlags |= ImGuiChildFlags_AutoResizeY;
			ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None;
			if ((a_flags & DMUI_UI_PANEL_FLAGS_SCROLLABLE) == 0)
				windowFlags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
			if (!background)
				windowFlags |= ImGuiWindowFlags_NoBackground;
			ImGui::PushStyleVar(background ? ImGuiStyleVar_FramePadding : ImGuiStyleVar_WindowPadding,
				Theme::Layout().panelPadding);
			if (background)
				ImGui::PushStyleColor(ImGuiCol_FrameBg, Native(Theme::ColorSnapshot().panel));
			const auto visible = ImGui::BeginChild(a_id, Native(a_size), childFlags, windowFlags);
			if (background)
				ImGui::PopStyleColor();
			ImGui::PopStyleVar();
			if (!visible)
			{
				EndPanelWindow();
				return DMUI_RESULT_OK;
			}
			s_panels.push_back({ a_client, ImGui::GetCurrentWindow() });
			*a_visible = 1;
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL EndPanel(DMUI_ClientHandle a_client) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			if (s_panels.size() <= s_floor.panels || s_panels.back().client != a_client ||
				s_panels.back().window != ImGui::GetCurrentWindow() ||
				(GImGui->CurrentTable && GImGui->CurrentTable->InnerWindow == ImGui::GetCurrentWindow()))
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto result = EndDrawWindow(a_client, &EndPanelWindow);
			if (result == DMUI_RESULT_OK)
				s_panels.pop_back();
			return result;
		}
	}
}
