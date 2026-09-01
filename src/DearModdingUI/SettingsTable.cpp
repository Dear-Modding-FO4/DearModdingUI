#include <DearModdingUI/SettingsTable.h>

#include <DearModdingUI/SettingsActions.h>
#include <DearModdingUI/Shell.h>
#include <DearModdingUI/Theme.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cfloat>
#include <new>
#include <string>

namespace DearModdingUI::SettingsTable
{
	namespace
	{
		struct RenderState
		{
			BracketState bracket;
			ImGuiTable* table{ nullptr };
			ImGuiTable* controls{ nullptr };
			ImGuiWindow* window{ nullptr };
			int tableIdDepth{ 0 };
			int rowIdDepth{ 0 };
			float labelHeight{ 0.0f };
			float buttonExtent{ 0.0f };
			float resetWidth{ 0.0f };
			std::string label;
			std::string description;
		};

		thread_local RenderState s_state;
		thread_local DMUI_ClientHandle s_callbackClient{
			DMUI_INVALID_CLIENT_HANDLE
		};
		thread_local uint32_t s_callbackDepth{ 0 };

		[[nodiscard]] bool HasDrawingContext() noexcept
		{
			const auto* context = ImGui::GetCurrentContext();
			return context && context->CurrentWindow;
		}

		[[nodiscard]] bool HasExpectedTable(
			const ImGuiTable* a_table,
			int a_idDepth) noexcept
		{
			const auto* context = ImGui::GetCurrentContext();
			return context &&
				context->CurrentTable == a_table &&
				context->CurrentWindow == s_state.window &&
				context->CurrentWindow->IDStack.Size == a_idDepth;
		}

		[[nodiscard]] float LabelBlockHeight(
			const char* a_label,
			const char* a_description,
			float a_wrapWidth) noexcept
		{
			float labelHeight{};
			{
				const Theme::FontGuard font{ Theme::FontRole::kSubheading };
				labelHeight = ImGui::CalcTextSize(
					a_label,
					nullptr,
					false,
					a_wrapWidth).y;
			}
			if (!a_description || a_description[0] == '\0')
				return labelHeight;

			float descriptionHeight{};
			{
				const Theme::FontGuard font{ Theme::FontRole::kSubtext };
				descriptionHeight = ImGui::CalcTextSize(
					a_description,
					nullptr,
					false,
					a_wrapWidth).y;
			}
			return labelHeight +
				ImGui::GetStyle().ItemSpacing.y +
				descriptionHeight;
		}

		void DrawLabel(
			const ImVec2& a_origin,
			float a_width,
			float a_rowContentHeight) noexcept
		{
			ImGui::SetCursorScreenPos({
				a_origin.x,
				a_origin.y + CenterOffsetY(
					a_rowContentHeight,
					s_state.labelHeight)
			});
			ImGui::PushTextWrapPos(a_origin.x + a_width);
			{
				const Theme::FontGuard font{ Theme::FontRole::kSubheading };
				ImGui::TextUnformatted(s_state.label.c_str());
			}
			if (!s_state.description.empty())
			{
				const Theme::FontGuard font{ Theme::FontRole::kSubtext };
				ImGui::TextDisabled("%s", s_state.description.c_str());
			}
			ImGui::PopTextWrapPos();
		}

		[[nodiscard]] float ResetColumnWidth(float a_buttonExtent) noexcept
		{
			return SettingsActionButtonWidth(
				SettingsAction::kReset,
				"Reset",
				a_buttonExtent);
		}

		void ClearRowState() noexcept
		{
			s_state.controls = nullptr;
			s_state.rowIdDepth = 0;
			s_state.labelHeight = 0.0f;
			s_state.buttonExtent = 0.0f;
			s_state.resetWidth = 0.0f;
			s_state.label.clear();
			s_state.description.clear();
		}
	}

	ClientCallbackGuard::ClientCallbackGuard(
		DMUI_ClientHandle a_client) noexcept
	{
		if (s_callbackDepth == 0)
		{
			ResetBracketState();
			s_callbackClient = a_client;
		}
		++s_callbackDepth;
	}

	ClientCallbackGuard::~ClientCallbackGuard() noexcept
	{
		if (s_callbackDepth == 0)
			return;
		--s_callbackDepth;
		if (s_callbackDepth == 0)
		{
			ResetBracketState();
			s_callbackClient = DMUI_INVALID_CLIENT_HANDLE;
		}
	}

	bool AcceptsClient(DMUI_ClientHandle a_client) noexcept
	{
		return s_callbackDepth == 1 &&
			a_client != DMUI_INVALID_CLIENT_HANDLE &&
			a_client == s_callbackClient;
	}

	BeginResult Begin(
		DMUI_ClientHandle a_owner,
		const char* a_id) noexcept
	{
		if (!a_id || a_id[0] == '\0')
			return { DMUI_RESULT_INVALID_ARGUMENT, false };
		if (!HasDrawingContext())
			return { DMUI_RESULT_HOST_NOT_READY, false };

		const auto transition = s_state.bracket.BeginTable(a_owner);
		if (transition != DMUI_RESULT_OK)
			return { transition, false };

		const auto visible = ImGui::BeginTable(
			a_id,
			2,
			ImGuiTableFlags_SizingStretchProp |
				ImGuiTableFlags_RowBg |
				ImGuiTableFlags_BordersInnerH |
				ImGuiTableFlags_PadOuterX);
		if (!visible)
		{
			(void)s_state.bracket.EndTable(a_owner);
			return { DMUI_RESULT_OK, false };
		}

		ImGui::TableSetupColumn(
			"Setting",
			ImGuiTableColumnFlags_WidthStretch,
			3.0f);
		ImGui::TableSetupColumn(
			"Value",
			ImGuiTableColumnFlags_WidthStretch,
			2.0f);
		s_state.table = ImGui::GetCurrentTable();
		s_state.window = ImGui::GetCurrentWindow();
		s_state.tableIdDepth = s_state.window->IDStack.Size;
		return { DMUI_RESULT_OK, true };
	}

	BeginResult BeginRow(
		DMUI_ClientHandle a_owner,
		const char* a_id,
		const char* a_label,
		const char* a_description) noexcept
	{
		if (!a_id || a_id[0] == '\0' || !a_label || a_label[0] == '\0')
			return { DMUI_RESULT_INVALID_ARGUMENT, false };
		if (!HasExpectedTable(s_state.table, s_state.tableIdDepth))
			return { DMUI_RESULT_UNBALANCED_BRACKET, false };

		try
		{
			s_state.label = a_label;
			s_state.description = a_description ? a_description : "";
		}
		catch (const std::bad_alloc&)
		{
			ClearRowState();
			return { DMUI_RESULT_RESOURCE_EXHAUSTED, false };
		}
		catch (...)
		{
			ClearRowState();
			return { DMUI_RESULT_CALLBACK_FAILED, false };
		}

		const auto transition = s_state.bracket.BeginRow(a_owner);
		if (transition != DMUI_RESULT_OK)
		{
			ClearRowState();
			return { transition, false };
		}

		ImGui::PushID(a_id);
		s_state.rowIdDepth = s_state.window->IDStack.Size;
		ImGui::TableNextRow();
		(void)ImGui::TableSetColumnIndex(0);
		const auto wrapWidth = (std::max)(
			ImGui::GetContentRegionAvail().x,
			ImGui::GetFontSize());
		s_state.labelHeight = LabelBlockHeight(
			s_state.label.c_str(),
			s_state.description.c_str(),
			wrapWidth);
		ImGui::Dummy({ 0.0f, s_state.labelHeight });
		(void)ImGui::TableSetColumnIndex(1);

		s_state.buttonExtent = ImGui::GetFrameHeight();
		s_state.resetWidth = ResetColumnWidth(s_state.buttonExtent);
		const auto visible = ImGui::BeginTable(
			"##DearModdingUI.SettingsRowControls",
			2,
			ImGuiTableFlags_SizingStretchProp);
		if (!visible)
		{
			ImGui::PopID();
			(void)s_state.bracket.EndRow(a_owner);
			ClearRowState();
			return { DMUI_RESULT_OK, false };
		}

		ImGui::TableSetupColumn(
			"##Value",
			ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn(
			"##Reset",
			ImGuiTableColumnFlags_WidthFixed,
			s_state.resetWidth);
		ImGui::TableNextRow(
			ImGuiTableRowFlags_None,
			s_state.buttonExtent + ImGui::GetStyle().CellPadding.y * 2.0f);
		(void)ImGui::TableSetColumnIndex(0);
		ImGui::SetNextItemWidth(-FLT_MIN);
		s_state.controls = ImGui::GetCurrentTable();
		return { DMUI_RESULT_OK, true };
	}

	DMUI_Result EndRow(
		DMUI_ClientHandle a_owner,
		const RowOptions& a_options,
		bool& a_resetPressed) noexcept
	{
		a_resetPressed = false;
		if (s_state.bracket.CurrentPhase() != Phase::kRow ||
			s_state.bracket.Owner() != a_owner ||
			!HasExpectedTable(s_state.controls, s_state.rowIdDepth))
			return DMUI_RESULT_UNBALANCED_BRACKET;
		(void)ImGui::TableSetColumnIndex(1);
		const auto resetCellOrigin = ImGui::GetCursorScreenPos();
		const auto resetRow = ImGui::TableGetCellBgRect(
			s_state.controls,
			1);
		const auto resetContentHeight = (std::max)(
			resetRow.GetHeight() -
				s_state.controls->RowCellPaddingY * 2.0f,
			0.0f);
		if (a_options.resetVisible)
		{
			a_resetPressed = DrawSettingsActionButton(
				"##DearModdingUI.SettingsRowReset",
				{
					resetCellOrigin.x,
					resetCellOrigin.y + CenterOffsetY(
						resetContentHeight,
						s_state.buttonExtent)
				},
				{ s_state.resetWidth, s_state.buttonExtent },
				SettingsAction::kReset,
				"Reset",
				a_options.resetEnabled ?
					"Reset this setting to its default." :
					"This setting already uses its default.",
				a_options.resetEnabled);
		}
		ImGui::EndTable();

		if (!HasExpectedTable(s_state.table, s_state.rowIdDepth))
			return DMUI_RESULT_UNBALANCED_BRACKET;
		(void)ImGui::TableSetColumnIndex(0);
		const auto labelOrigin = ImGui::GetCursorScreenPos();
		const auto labelWidth = (std::max)(
			ImGui::GetContentRegionAvail().x,
			ImGui::GetFontSize());
		// Table row geometry follows ItemSize, unlike Selectable's inflated item rectangle.
		const auto labelRow = ImGui::TableGetCellBgRect(s_state.table, 0);
		const auto labelContentHeight = (std::max)(
			labelRow.GetHeight() -
				s_state.table->RowCellPaddingY * 2.0f,
			0.0f);
		DrawLabel(labelOrigin, labelWidth, labelContentHeight);
		ImGui::PopID();
		ClearRowState();
		return s_state.bracket.EndRow(a_owner);
	}

	DMUI_Result End(DMUI_ClientHandle a_owner) noexcept
	{
		if (s_state.bracket.CurrentPhase() != Phase::kTable ||
			s_state.bracket.Owner() != a_owner ||
			!HasExpectedTable(s_state.table, s_state.tableIdDepth))
			return DMUI_RESULT_UNBALANCED_BRACKET;

		ImGui::EndTable();
		s_state.table = nullptr;
		s_state.window = nullptr;
		s_state.tableIdDepth = 0;
		return s_state.bracket.EndTable(a_owner);
	}

	void ResetBracketState() noexcept
	{
		s_state.bracket.Reset();
		s_state.table = nullptr;
		s_state.controls = nullptr;
		s_state.window = nullptr;
		s_state.tableIdDepth = 0;
		ClearRowState();
	}
}
