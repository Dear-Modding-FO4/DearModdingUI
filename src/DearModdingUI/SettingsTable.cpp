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
			ImGuiID tableId{ 0 };
			ImGuiID controlsId{ 0 };
			ImGuiWindow* window{ nullptr };
			int tableIdDepth{ 0 };
			int rowIdDepth{ 0 };
			int controlsIdDepth{ 0 };
			float labelHeight{ 0.0f };
			float buttonExtent{ 0.0f };
			float resetWidth{ 0.0f };
			float fullSpanMaxX{ 0.0f };
			float savedWorkRectMaxX{ 0.0f };
			ImRect labelRect;
			std::string label;
			std::string description;
			RowLayout layout{ RowLayout::kLabelValue };
			bool fullSpanClipPushed{ false };
		};

		thread_local RenderState s_state;
		thread_local DMUI_ClientHandle s_callbackClient{
			DMUI_INVALID_CLIENT_HANDLE
		};
		thread_local uint32_t s_callbackDepth{ 0 };
		thread_local ImGuiErrorRecoveryState s_callbackRecovery;
		thread_local bool s_hasCallbackRecovery{ false };

		[[nodiscard]] bool HasDrawingContext() noexcept
		{
			const auto* context = ImGui::GetCurrentContext();
			return context && context->CurrentWindow;
		}

		[[nodiscard]] bool HasExpectedTable(
			ImGuiID a_tableId,
			int a_idDepth) noexcept
		{
			const auto* context = ImGui::GetCurrentContext();
			return context &&
				context->CurrentTable &&
				context->CurrentTable->ID == a_tableId &&
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

		[[nodiscard]] ImRect TableRowContentRect(
			const ImGuiTable* a_table,
			int a_column) noexcept
		{
			if (!a_table)
				return {};
			const auto origin = ImGui::GetCursorScreenPos();
			const auto width = (std::max)(
				ImGui::GetContentRegionAvail().x,
				0.0f);
			const auto cell = ImGui::TableGetCellBgRect(a_table, a_column);
			// Table rows follow ItemSize, so remove cell padding rather than Selectable spacing.
			const auto content = ResolveRowContentRect(
				RowContentRectKind::kTable,
				{
					origin.x,
					cell.Min.y,
					origin.x + width,
					cell.Max.y
				},
				a_table->RowCellPaddingY);
			return {
				{ content.minX, content.minY },
				{ content.maxX, content.maxY }
			};
		}

		void DrawLabel(const ImRect& a_contentRect) noexcept
		{
			ImGui::SetCursorScreenPos({
				a_contentRect.Min.x,
				a_contentRect.Min.y + RowContentOffsetY(
					a_contentRect.GetHeight(),
					{ s_state.labelHeight },
					RowContentMetric::kBox)
			});
			ImGui::PushTextWrapPos(
				a_contentRect.Min.x +
					(std::max)(
						a_contentRect.GetWidth(),
						ImGui::GetFontSize()));
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
			s_state.controlsId = 0;
			s_state.rowIdDepth = 0;
			s_state.controlsIdDepth = 0;
			s_state.labelHeight = 0.0f;
			s_state.buttonExtent = 0.0f;
			s_state.resetWidth = 0.0f;
			s_state.fullSpanMaxX = 0.0f;
			s_state.savedWorkRectMaxX = 0.0f;
			s_state.labelRect = {};
			s_state.label.clear();
			s_state.description.clear();
			s_state.layout = RowLayout::kLabelValue;
			s_state.fullSpanClipPushed = false;
		}

		void RestoreFullSpanContent() noexcept
		{
			if (!s_state.fullSpanClipPushed || !s_state.window)
				return;
			ImGui::PopClipRect();
			s_state.window->WorkRect.Max.x = s_state.savedWorkRectMaxX;
			s_state.fullSpanClipPushed = false;
		}

		void ClearTableState() noexcept
		{
			s_state.tableId = 0;
			s_state.window = nullptr;
			s_state.tableIdDepth = 0;
		}

		void RecoverOpenBrackets() noexcept
		{
			const auto owner = s_state.bracket.Owner();
			if (s_state.bracket.CurrentPhase() == Phase::kRow &&
				HasExpectedTable(
					s_state.controlsId,
					s_state.controlsIdDepth))
			{
				ImGui::EndTable();
				RestoreFullSpanContent();
				if (HasExpectedTable(s_state.tableId, s_state.rowIdDepth))
				{
					ImGui::PopID();
					ClearRowState();
					(void)s_state.bracket.EndRow(owner);
				}
			}

			if (s_state.bracket.CurrentPhase() == Phase::kTable &&
				HasExpectedTable(s_state.tableId, s_state.tableIdDepth))
			{
				ImGui::EndTable();
				ClearTableState();
				(void)s_state.bracket.EndTable(owner);
			}
		}
	}

	ClientCallbackGuard::ClientCallbackGuard(
		DMUI_ClientHandle a_client) noexcept
	{
		if (s_callbackDepth == 0)
		{
			ResetBracketState();
			s_callbackClient = a_client;
			s_hasCallbackRecovery = HasDrawingContext();
			if (s_hasCallbackRecovery)
				ImGui::ErrorRecoveryStoreState(&s_callbackRecovery);
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
			RecoverOpenBrackets();
			if (s_hasCallbackRecovery && HasDrawingContext())
				ImGui::ErrorRecoveryTryToRecoverState(&s_callbackRecovery);
			ResetBracketState();
			s_callbackClient = DMUI_INVALID_CLIENT_HANDLE;
			s_hasCallbackRecovery = false;
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
		s_state.tableId = ImGui::GetCurrentTable()->ID;
		s_state.window = ImGui::GetCurrentWindow();
		s_state.tableIdDepth = s_state.window->IDStack.Size;
		return { DMUI_RESULT_OK, true };
	}

	BeginResult BeginRow(
		DMUI_ClientHandle a_owner,
		const char* a_id,
		const char* a_label,
		const char* a_description,
		RowLayout a_layout) noexcept
	{
		if (!a_id || a_id[0] == '\0' || !a_label)
			return { DMUI_RESULT_INVALID_ARGUMENT, false };
		if (!HasExpectedTable(s_state.tableId, s_state.tableIdDepth))
			return { DMUI_RESULT_UNBALANCED_BRACKET, false };

		try
		{
			s_state.label = a_label;
			s_state.description = a_description ? a_description : "";
			s_state.layout = a_layout;
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
		const auto labelOrigin = ImGui::GetCursorScreenPos();
		if (a_layout == RowLayout::kFullSpan)
		{
			const auto* table = ImGui::GetCurrentTable();
			const auto finalCell = ImGui::TableGetCellBgRect(table, 1);
			s_state.fullSpanMaxX =
				finalCell.Max.x - table->CellPaddingX;
			s_state.savedWorkRectMaxX = s_state.window->WorkRect.Max.x;
			s_state.window->WorkRect.Max.x = s_state.fullSpanMaxX;
			const auto clip = s_state.window->ClipRect;
			ImGui::PushClipRect(
				clip.Min,
				{ s_state.fullSpanMaxX, clip.Max.y },
				false);
			s_state.fullSpanClipPushed = true;
		}
		const auto wrapWidth = (std::max)(
			ImGui::GetContentRegionAvail().x,
			ImGui::GetFontSize());
		s_state.labelHeight = LabelBlockHeight(
			s_state.label.c_str(),
			s_state.description.c_str(),
			wrapWidth);
		if (s_state.labelHeight > 0.0f)
		{
			s_state.labelRect = {
				labelOrigin,
				{
					labelOrigin.x + wrapWidth,
					labelOrigin.y + s_state.labelHeight
				}
			};
			ImGui::Dummy({ 0.0f, s_state.labelHeight });
		}
		if (a_layout == RowLayout::kLabelValue)
			(void)ImGui::TableSetColumnIndex(1);

		s_state.buttonExtent = ImGui::GetFrameHeight();
		s_state.resetWidth = ResetColumnWidth(s_state.buttonExtent);
		const auto visible = ImGui::BeginTable(
			"##DearModdingUI.SettingsRowControls",
			2,
			ImGuiTableFlags_SizingStretchProp,
			a_layout == RowLayout::kFullSpan ?
				ImVec2{
					(std::max)(
						s_state.fullSpanMaxX -
							ImGui::GetCursorScreenPos().x,
						0.0f),
					0.0f
				} :
				ImVec2{});
		if (!visible)
		{
			RestoreFullSpanContent();
			ImGui::PopID();
			(void)s_state.bracket.EndRow(a_owner);
			ClearRowState();
			return { DMUI_RESULT_OK, false };
		}
		s_state.controlsId = ImGui::GetCurrentTable()->ID;
		s_state.controlsIdDepth = s_state.window->IDStack.Size;

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
			!HasExpectedTable(
				s_state.controlsId,
				s_state.controlsIdDepth))
			return DMUI_RESULT_UNBALANCED_BRACKET;
		const auto* controls = ImGui::GetCurrentTable();
		(void)ImGui::TableSetColumnIndex(1);
		const auto resetContentRect = TableRowContentRect(controls, 1);
		if (a_options.resetVisible)
		{
			a_resetPressed = DrawSettingsActionButton(
				"##DearModdingUI.SettingsRowReset",
				{
					resetContentRect.Min.x,
					resetContentRect.Min.y + RowContentOffsetY(
						resetContentRect.GetHeight(),
						{ s_state.buttonExtent },
						RowContentMetric::kBox)
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
		RestoreFullSpanContent();

		if (!HasExpectedTable(s_state.tableId, s_state.rowIdDepth))
			return DMUI_RESULT_UNBALANCED_BRACKET;
		const auto* table = ImGui::GetCurrentTable();
		(void)ImGui::TableSetColumnIndex(0);
		if (s_state.layout == RowLayout::kFullSpan)
		{
			if (s_state.labelHeight > 0.0f)
			{
				const auto clip = s_state.window->ClipRect;
				ImGui::PushClipRect(
					clip.Min,
					{ s_state.fullSpanMaxX, clip.Max.y },
					false);
				DrawLabel(s_state.labelRect);
				ImGui::PopClipRect();
			}
		}
		else
		{
			DrawLabel(TableRowContentRect(table, 0));
		}
		ImGui::PopID();
		ClearRowState();
		return s_state.bracket.EndRow(a_owner);
	}

	DMUI_Result End(DMUI_ClientHandle a_owner) noexcept
	{
		if (s_state.bracket.CurrentPhase() != Phase::kTable ||
			s_state.bracket.Owner() != a_owner ||
			!HasExpectedTable(s_state.tableId, s_state.tableIdDepth))
			return DMUI_RESULT_UNBALANCED_BRACKET;

		ImGui::EndTable();
		ClearTableState();
		return s_state.bracket.EndTable(a_owner);
	}

	void ResetBracketState() noexcept
	{
		s_state.bracket.Reset();
		ClearRowState();
		ClearTableState();
	}
}
