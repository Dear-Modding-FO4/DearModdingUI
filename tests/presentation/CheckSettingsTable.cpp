#include <DearModdingUI/host/ImGuiRecovery.h>
#include <DearModdingUI/controls/SettingsTable.h>
#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/presentation/Theme.h>
#include <DearModdingUI/host/UIAdapter.h>
#include <DearModdingUI/settings/HostSettingsColorControls.h>
#include "../Harness.h"
#include "../support/ImGuiTestContext.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <DearModdingUI/Presentation.h>
#include <DearModdingUI/Client.h>

#include <cmath>
#include <limits>

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI;

		[[nodiscard]] DMUI_Result AcceptUIClient(
			DMUI_ClientHandle) noexcept
		{
			return DMUI_RESULT_OK;
		}

		class ImGuiTestFrame
		{
		public:
			explicit ImGuiTestFrame(ImVec2 a_position = { 60.0f, 60.0f })
			{
				m_imgui.Get()->ErrorCallback = [](ImGuiContext*, void* a_userData, const char*) {
					++*static_cast<int*>(a_userData);
				};
				m_imgui.Get()->ErrorCallbackUserData = &m_errors;
				m_imgui.BeginWindow(
					"##SettingsTableTest",
					a_position,
					{ 640.0f, 480.0f });
				ImGui::ErrorRecoveryStoreState(&m_recovery);
				m_idDepth = ImGui::GetCurrentWindow()->IDStack.Size;
				m_colorDepth = m_imgui.Get()->ColorStack.Size;
				m_disabledDepth = m_imgui.Get()->DisabledStackSize;
				m_wrapDepth =
					ImGui::GetCurrentWindow()->DC.TextWrapPosStack.Size;
				m_beginPopupDepth = m_imgui.Get()->BeginPopupStack.Size;
			}

			~ImGuiTestFrame()
			{
				ImGui::ErrorRecoveryTryToRecoverState(&m_recovery);
				m_imgui.EndWindow();
			}

			[[nodiscard]] int Errors() const noexcept
			{
				return m_errors;
			}

			[[nodiscard]] bool IsAtBaseline() const noexcept
			{
				const auto* context = ImGui::GetCurrentContext();
				return context &&
					context->CurrentTable == nullptr &&
					context->CurrentWindow->IDStack.Size == m_idDepth &&
					context->ColorStack.Size == m_colorDepth &&
					context->DisabledStackSize == m_disabledDepth &&
					context->CurrentWindow->DC.TextWrapPosStack.Size ==
						m_wrapDepth &&
					context->BeginPopupStack.Size == m_beginPopupDepth;
			}

		private:
			UI::Testing::ValidationOverride m_uiValidation{ &AcceptUIClient };
			dmui::ui::detail::ScopedContext m_uiContext{
				&UI::API(),
				1u
			};
			support::ImGuiTestContext m_imgui{
				{ .disableErrorRecovery = true }
			};
			ImGuiErrorRecoveryState m_recovery;
			int m_idDepth{ 0 };
			int m_colorDepth{ 0 };
			int m_disabledDepth{ 0 };
			int m_wrapDepth{ 0 };
			int m_beginPopupDepth{ 0 };
			int m_errors{ 0 };
		};

		class FeedbackAppearanceGuard
		{
		public:
			FeedbackAppearanceGuard() noexcept :
				m_original(FieldFeedback::CurrentAppearance())
			{}

			~FeedbackAppearanceGuard() noexcept
			{
				FieldFeedback::SetAppearance(m_original);
			}

		private:
			FieldFeedback::Appearance m_original;
		};

		void BeginFeedbackField(DMUI_ClientHandle a_owner, const char* a_id)
		{
			const auto field = SettingsTable::BeginField(
				a_owner, a_id, a_id, nullptr,
				SettingsTable::RowLayout::kLabelValue);
			require(field.result == DMUI_RESULT_OK && field.visible,
				"feedback field did not begin");
		}

		void EndFeedbackField(DMUI_ClientHandle a_owner, bool a_reset = false)
		{
			bool resetPressed{};
			require(SettingsTable::EndField(
						a_owner, { a_reset, false }, resetPressed) == DMUI_RESULT_OK,
				"feedback field did not end");
		}

		[[nodiscard]] DMUI_ThemeColors TestTheme() noexcept
		{
			return {
				sizeof(DMUI_ThemeColors),
				{ 1.0f, 0.0f, 0.0f, 1.0f },
				{ 2.0f, 0.0f, 0.0f, 1.0f },
				{ 3.0f, 0.0f, 0.0f, 1.0f },
				{ 4.0f, 0.0f, 0.0f, 1.0f },
				{ 5.0f, 0.0f, 0.0f, 1.0f },
				{ 6.0f, 0.0f, 0.0f, 1.0f },
				{ 7.0f, 0.0f, 0.0f, 1.0f },
				{ 8.0f, 0.0f, 0.0f, 1.0f },
				{ 9.0f, 0.0f, 0.0f, 1.0f },
				{ 10.0f, 0.0f, 0.0f, 1.0f },
				{ 11.0f, 0.0f, 0.0f, 1.0f },
				{ 12.0f, 0.0f, 0.0f, 1.0f },
				{ 13.0f, 0.0f, 0.0f, 1.0f },
				{ 14.0f, 0.0f, 0.0f, 1.0f }
			};
		}
	}

	void run_settings_table_checks(Runner& runner)
	{
		runner.test("presentation tones resolve every theme role", [] {
			const auto theme = TestTheme();
			const dmui::TextTone tones[]{
				dmui::TextTone::kSuccess,
				dmui::TextTone::kWarning,
				dmui::TextTone::kError,
				dmui::TextTone::kInfo,
				dmui::TextTone::kMuted,
				dmui::TextTone::kAccent,
				dmui::TextTone::kAccentMuted,
				dmui::TextTone::kStatusDisable,
				dmui::TextTone::kStatusError,
				dmui::TextTone::kStatusWarning,
				dmui::TextTone::kStatusRestartNeeded,
				dmui::TextTone::kStatusCurrentHotkey,
				dmui::TextTone::kStatusSuccess,
				dmui::TextTone::kStatusInfo
			};
			for (size_t index = 0; index < std::size(tones); ++index)
			{
				const auto resolved =
					dmui::ResolveTextColor(theme, tones[index]);
				require(
					resolved.result == DMUI_RESULT_OK &&
						resolved.color &&
						resolved.color->x == static_cast<float>(index + 1),
					"theme tone selected the wrong field");
			}
			const auto inherited = dmui::ResolveTextColor(
				DMUI_ThemeColors{},
				dmui::TextTone::kInherit);
			require(inherited && !inherited.color,
				"inherit tone required a theme color");
			auto truncated = theme;
			truncated.structSize = DMUI_THEME_COLORS_0_1_SIZE - 1;
			require(
				dmui::ResolveTextColor(
					truncated,
					dmui::TextTone::kAccent).result ==
					DMUI_RESULT_STRUCT_TOO_SMALL,
				"truncated theme snapshot was accepted");
			require(
				dmui::ResolveTextColor(
					theme,
					static_cast<dmui::TextTone>(255)).result ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"unknown text tone was accepted");
		});

		runner.test("styled text balances color and wrapping stacks", [] {
			ImGuiTestFrame frame;
			auto text = std::string(4096, 'x');
			text += " 100% ##literal ";
			text += "\xE2\x98\x83";
			require(
				dmui::DrawStyledText(
					text,
					TestTheme(),
					{
						.tone = dmui::TextTone::kStatusRestartNeeded,
						.wrapped = true
					}) == DMUI_RESULT_OK,
				"styled text draw failed");
			require(
				dmui::DrawStyledText(
					{},
					DMUI_ThemeColors{},
					{}) == DMUI_RESULT_OK,
				"empty inherited text required a theme");
			require(
				dmui::DrawStyledText(
					"text",
					TestTheme(),
					{ .fontRole = DMUI_FONT_ROLE_BODY }) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"snapshot-only styled text accepted a client font role");
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"styled text changed the ImGui stack");
		});

		runner.test("client text helpers fail before drawing when disconnected", [] {
			ImGuiTestFrame frame;
			dmui::Client client{
				"tests.presentation",
				"Presentation tests",
				{ 1, 0 }
			};
			const auto start = ImGui::GetCursorScreenPos();
			require(
				!dmui::DrawStyledText(
					client,
					"not drawn",
					{ .tone = dmui::TextTone::kAccent }) &&
					client.LastResult() == DMUI_RESULT_CLIENT_NOT_FOUND,
				"disconnected styled text did not report its failure");
			require(
				!dmui::DrawLabeledValue(
					client,
					"Label",
					"not drawn",
					{
						.valueStyle = {
							.fontRole = DMUI_FONT_ROLE_BODY,
							.tone = dmui::TextTone::kSuccess
						}
					}) &&
					client.LastResult() == DMUI_RESULT_CLIENT_NOT_FOUND,
				"disconnected labeled value did not preflight");
			const auto end = ImGui::GetCursorScreenPos();
			require(start.x == end.x && start.y == end.y,
				"failed text helper drew a partial value");
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"failed text helper changed the ImGui stack");
		});

		runner.test("disabled and tooltip scopes end idempotently", [] {
			ImGuiTestFrame frame;
			{
				const dmui::DisabledScope disabled{ false };
				ImGui::TextUnformatted("enabled");
			}
			const auto start = ImGui::GetCursorScreenPos();
			ImGui::GetIO().MousePos = { start.x + 1.0f, start.y + 1.0f };
			ImGui::Dummy({ 40.0f, 20.0f });
			{
				dmui::TooltipScope tooltip{
					dmui::ui::HoveredFlags::kAllowWhenDisabled
				};
				if (tooltip.Visible())
				ImGui::TextUnformatted("rich tooltip");
				const auto ended = tooltip.End();
				require(!tooltip.End(), "tooltip ended twice");
				require(!tooltip.Visible(), "ended tooltip remained visible");
				require(!tooltip.Hovered() || ended,
					"hovered tooltip did not open");
			}
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"presentation scope changed the ImGui stack");
		});

		runner.test("choice draw preserves unknown and empty values", [] {
			ImGuiTestFrame frame;
			const std::array options{
				dmui::ChoiceOption<int>{
					.value = 1,
					.label = "100% ## duplicate",
					.key = "first"
				},
				dmui::ChoiceOption<int>{
					.value = 2,
					.label = "100% ## duplicate",
					.key = "second",
					.enabled = false
				}
			};
			ImGui::PushID("known");
			ImGui::OpenPopupEx(
				ImHashStr("##ComboPopup", 0, ImGui::GetID("##Choice")),
				ImGuiPopupFlags_None);
			ImGui::PopID();
			ImGui::LogToBuffer();
			const auto known = dmui::DrawChoice(
				"known",
				1,
				options,
				"Unavailable",
				"Quality mode ## literal");
			const std::string logged{
				ImGui::GetCurrentContext()->LogBuffer.c_str()
			};
			ImGui::LogFinish();
			const auto& popups = ImGui::GetCurrentContext()->OpenPopupStack;
			require(popups.Size == 1 && popups[0].Window &&
					popups[0].Window->Active &&
					popups[0].Window->DC.CursorMaxPos.y >
						popups[0].Window->DC.CursorStartPos.y,
				"choice regression did not draw the actual combo popup");
			require(
				!known.changed && !known.completed && !known.selected,
				"drawing a choice changed its current value");
			require(
				logged.contains("Quality mode ## literal"),
				"choice display label was hidden or treated as identity");
			const auto unknown = dmui::DrawChoice(
				"unknown",
				99,
				options,
				"Unknown selection");
			require(
				!unknown.changed && !unknown.completed && !unknown.selected,
				"unknown choice was silently clamped");
			const auto empty = dmui::DrawChoice(
				"empty",
				99,
				{});
			require(
				!empty.changed && !empty.completed && !empty.selected,
				"empty choice produced a selection");
			const auto unchanged =
				dmui::presentation_detail::ResolveChoiceActivation(
					1,
					options[0],
					true);
			const auto disabled =
				dmui::presentation_detail::ResolveChoiceActivation(
					1,
					options[1],
					true);
			auto enabled = options[1];
			enabled.enabled = true;
			const auto changed =
				dmui::presentation_detail::ResolveChoiceActivation(
					1,
					enabled,
					true);
			require(
				!unchanged.changed && !unchanged.completed &&
					!unchanged.selected &&
					!disabled.changed && !disabled.completed &&
					!disabled.selected &&
					changed.changed && changed.completed &&
					changed.selected == 2,
				"choice activation return semantics changed");
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"choice draw changed the ImGui stack");
		});

		runner.test("declarative choices separate unmatched labels from stored values", [] {
			ImGuiTestFrame frame;
			std::string stored{ "missing.xml" };
			size_t writes{};
			size_t edits{};
			dmui::SettingDescriptor setting;
			setting.control = dmui::ChoiceSettingControl{
				.options = { { "", "None" }, { "known.xml", "Known preset" } },
				.unmatchedLabel = "None"
			};
			setting.defaultValue = std::string{};
			setting.binding = dmui::BindSetting(
				[&] { return stored; },
				[&](std::string value) {
					++writes;
					stored = std::move(value);
					return stored;
				});
			setting.onEdit = [&](const dmui::SettingEditEvent& event) {
				++edits;
				require(event.changed && event.completed &&
						std::get<std::string>(event.value).empty(),
					"choice reset did not emit one completed empty-string change");
			};

			const auto drawAndCheck = [&](const char* id, std::string_view expected) {
				ImGui::PushID(id);
				ImGui::LogToBuffer();
				const auto drawn = dmui::setting_detail::DrawBoundSetting(
					setting, setting.binding.get());
				const std::string logged{ ImGui::GetCurrentContext()->LogBuffer.c_str() };
				ImGui::LogFinish();
				ImGui::PopID();
				require(!drawn.changed && !drawn.completed &&
						std::get<std::string>(drawn.value) == stored,
					"choice presentation changed the bound value");
				require(logged.contains(expected),
					"choice did not draw the requested preview label");
			};
			drawAndCheck("unmatched", "None");
			require(stored == "missing.xml" && writes == 0 && edits == 0 &&
					!dmui::IsSettingDefault(setting, setting.binding.get()),
				"unmatched presentation masked the stored non-default value");

			const auto cleared = dmui::ResetSettingToDefault(setting);
			require(cleared && std::get<std::string>(*cleared).empty() &&
					stored.empty() && writes == 1 && edits == 1,
				"reset did not clear the real unknown value");
			(void)dmui::ResetSettingToDefault(setting);
			require(writes == 1 && edits == 1,
				"already-default reset wrote or emitted a change");
			stored = "known.xml";
			drawAndCheck("matched", "Known preset");

			auto& control = std::get<dmui::ChoiceSettingControl>(setting.control);
			control = {};
			stored = "missing.xml";
			drawAndCheck("empty-list", "Unavailable");
			require(writes == 1 && edits == 1 && frame.IsAtBaseline() && frame.Errors() == 0,
				"empty-list presentation wrote storage or leaked ImGui state");
		});

		runner.test("ImGui recovery reports repaired stack depths", [] {
			ImGuiTestFrame frame;
			auto recovery = ImGuiRecoverySnapshot::Capture();
			require(recovery.has_value(), "recovery snapshot was not captured");
			ImGui::PushID("leaked-id");
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{});
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
			ImGui::PushFont(ImGui::GetFont());
			require(ImGui::BeginTable("leaked-table", 1),
				"diagnostic table did not begin");

			const auto repaired = recovery->RecoverAfterCallback();
			require(repaired.Repaired(), "stack repair was not reported");
			require(
				repaired.before.tables == repaired.after.tables + 1 &&
					repaired.before.ids == repaired.after.ids + 2 &&
					repaired.before.colors == repaired.after.colors + 1 &&
					repaired.before.styleVariables ==
						repaired.after.styleVariables + 1 &&
					repaired.before.fonts == repaired.after.fonts + 1,
				"reported stack depths did not describe the repair");
			require(frame.IsAtBaseline(),
				"reported recovery did not restore the ImGui baseline");
		});

		runner.test("settings descriptions wrap inside translated window columns", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			constexpr auto description =
				"This description must remain entirely inside the label column, "
				"wrapping onto additional lines rather than disappearing beneath "
				"the value control. Moving the window must not change its wrapping.";
			for (const auto position : { ImVec2{ 60.0f, 60.0f }, ImVec2{ 420.0f, 100.0f } })
			{
				ImGuiTestFrame frame{ position };
				{
					const SettingsTable::ClientCallbackGuard guard{ owner };
					const auto table = SettingsTable::Begin(owner, "wrapped-settings");
					require(table.result == DMUI_RESULT_OK && table.visible,
						"wrapped settings table did not begin");
					const auto tableId = ImGui::GetCurrentTable()->ID;
					const auto row = SettingsTable::BeginRow(
						owner, "wrapped", "Setting", description);
					require(row.result == DMUI_RESULT_OK && row.visible,
						"wrapped settings row did not begin");
					const auto* outer = ImGui::GetCurrentContext()->Tables.GetByKey(tableId);
					const auto& column = outer->Columns[0];
					const auto expected = ImGui::CalcTextSize(
						description, nullptr, false, column.WorkMaxX - column.WorkMinX);
					const auto labelMaxX = column.WorkMaxX;
					ImGui::Button("Value");
					bool resetPressed{};
					require(SettingsTable::EndRow(
								owner, { false, false }, resetPressed) == DMUI_RESULT_OK,
						"wrapped settings row did not end");
					const auto drawn = ImGui::GetCurrentContext()->LastItemData.Rect;
					require(drawn.Max.x <= labelMaxX + 0.5f &&
							drawn.GetHeight() >= expected.y - 0.5f &&
							expected.y > ImGui::GetFontSize(),
						"description wrapping escaped its column or lost text lines");
					require(SettingsTable::End(owner) == DMUI_RESULT_OK,
						"wrapped settings table did not end");
				}
				require(frame.IsAtBaseline() && frame.Errors() == 0,
					"description wrapping changed the ImGui stack");
			}
		});

		runner.test("full-span settings row spans the label and value cells", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			ImGuiTestFrame frame;
			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				const auto table = SettingsTable::Begin(owner, "settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"settings table did not begin");
				const auto row = SettingsTable::BeginRow(
					owner,
					"prose",
					"",
					"",
					SettingsTable::RowLayout::kFullSpan);
				require(row.result == DMUI_RESULT_OK && row.visible,
					"full-span settings row did not begin");
				const auto controlsRect = ImGui::GetCurrentTable()->OuterRect;
				ImGui::TextWrapped("Full-width prose");
				bool resetPressed{};
				require(SettingsTable::EndRow(
							owner, { false, false }, resetPressed) ==
						DMUI_RESULT_OK,
					"full-span settings row did not end");
				const auto* outerTable = ImGui::GetCurrentTable();
				const auto labelCell = ImGui::TableGetCellBgRect(outerTable, 0);
				const auto expectedMaxX =
					ImGui::TableGetCellBgRect(outerTable, 1).Max.x -
					outerTable->CellPaddingX;
				require(controlsRect.Min.x >= labelCell.Min.x &&
						controlsRect.Min.x < labelCell.Max.x &&
						controlsRect.Max.x == expectedMaxX,
					"full-span controls did not span both settings cells");
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"settings table did not end");
			}
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"full-span row changed the ImGui stack");
		});

		runner.test("settings row survives ImGui table pool growth", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			ImGuiTestFrame frame;
			auto& tables = ImGui::GetCurrentContext()->Tables;
			auto seed = 0;
			const auto seedTable = [&]() {
				ImGui::PushID(seed++);
				require(ImGui::BeginTable("seed", 1),
					"table pool seed did not begin");
				ImGui::EndTable();
				ImGui::PopID();
			};
			seedTable();
			while (tables.GetBufSize() + 1 < tables.Buf.Capacity)
				seedTable();
			const auto capacity = tables.Buf.Capacity;

			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				auto recovery = ImGuiRecoverySnapshot::Capture();
				require(recovery.has_value(), "recovery snapshot was not captured");
				const auto table = SettingsTable::Begin(owner, "settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"settings table did not begin");
				require(tables.Buf.Capacity == capacity,
					"outer settings table unexpectedly grew the table pool");
				const auto row = SettingsTable::BeginRow(
					owner, "row", "Setting", "Description");
				require(row.result == DMUI_RESULT_OK && row.visible,
					"settings row did not begin");
				require(tables.Buf.Capacity > capacity,
					"nested controls table did not grow the table pool");
				bool value{};
				(void)ImGui::Checkbox("##Value", &value);
				bool resetPressed{};
				require(SettingsTable::EndRow(
							owner, { true, false }, resetPressed) ==
						DMUI_RESULT_OK,
					"table pool growth invalidated the settings row");
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"table pool growth invalidated the settings table");
				const auto repaired = recovery->RecoverAfterCallback();
				require(!repaired.Repaired(),
					"table pool growth required ImGui recovery");
			}
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"table pool growth changed the ImGui stack");
		});

		runner.test("first-frame page callback preserves its host table", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			ImGuiTestFrame frame;
			require(ImGui::BeginTable(
						"host-shell",
						2,
						ImGuiTableFlags_SizingStretchProp |
							ImGuiTableFlags_Resizable),
				"host shell table did not begin");
			ImGui::TableSetupColumn("Navigation", ImGuiTableColumnFlags_None, 3.0f);
			ImGui::TableSetupColumn("Page", ImGuiTableColumnFlags_None, 7.0f);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Navigation");
			ImGui::TableNextColumn();
			require(ImGui::BeginChild("page-frame", {}, ImGuiChildFlags_Borders),
				"page frame did not begin");
			ImGui::PushID(11);

			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				auto recovery = ImGuiRecoverySnapshot::Capture();
				require(recovery.has_value(), "recovery snapshot was not captured");
				const auto table = SettingsTable::Begin(owner, "settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"client settings table did not begin");
				const auto row = SettingsTable::BeginRow(
					owner, "row", "Setting", "Description");
				require(row.result == DMUI_RESULT_OK && row.visible,
					"client settings row did not begin");
				bool value{};
				(void)ImGui::Checkbox("##Value", &value);
				bool resetPressed{};
				require(SettingsTable::EndRow(
							owner, { true, false }, resetPressed) ==
						DMUI_RESULT_OK,
					"client settings row did not end");
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"client settings table did not end");
				const auto repaired = recovery->RecoverAfterCallback();
				require(!repaired.Repaired(),
					"balanced page callback required ImGui recovery");
			}

			ImGui::PopID();
			ImGui::EndChild();
			ImGui::EndTable();
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"page callback changed the host table stack");
		});

		runner.test("invisible settings row opens no bracket", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			ImGuiTestFrame frame;
			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				const auto table = SettingsTable::Begin(owner, "settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"settings table did not begin");
				auto* currentTable = ImGui::GetCurrentTable();
				currentTable->TempData->ReconcileColumnsRequests[1].Flags |=
					ImGuiTableColumnFlags_Disabled;
				const auto row = SettingsTable::BeginRow(
					owner, "row", "Setting", nullptr);
				require(row.result == DMUI_RESULT_OK && !row.visible,
					"disabled value column did not produce an invisible row");
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"invisible row left the table bracket open");
			}
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"invisible row changed the ImGui stack");
		});

		runner.test("settings bracket early returns remain recoverable", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			constexpr DMUI_ClientHandle other{ 8 };
			ImGuiTestFrame frame;
			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				require(SettingsTable::Begin(owner, "").result ==
						DMUI_RESULT_INVALID_ARGUMENT,
					"invalid table id was accepted");
				auto* window = ImGui::GetCurrentWindow();
				window->SkipItems = true;
				const auto invisibleTable =
					SettingsTable::Begin(owner, "clipped");
				window->SkipItems = false;
				require(invisibleTable.result == DMUI_RESULT_OK &&
						!invisibleTable.visible,
					"invisible table opened a bracket");
				const auto table = SettingsTable::Begin(owner, "settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"settings table did not begin");
				require(SettingsTable::Begin(owner, "nested").result ==
						DMUI_RESULT_UNBALANCED_BRACKET,
					"nested settings table was accepted");
				require(SettingsTable::BeginRow(
							owner, "", "Setting", nullptr).result ==
						DMUI_RESULT_INVALID_ARGUMENT,
					"invalid row id was accepted");
				require(SettingsTable::BeginRow(
							other, "other", "Setting", nullptr).result ==
						DMUI_RESULT_UNBALANCED_BRACKET,
					"foreign owner began a row");
				const auto row = SettingsTable::BeginRow(
					owner, "row", "Setting", nullptr);
				require(row.result == DMUI_RESULT_OK && row.visible,
					"valid row did not begin after an early return");
				require(SettingsTable::BeginRow(
							owner, "nested", "Setting", nullptr).result ==
						DMUI_RESULT_UNBALANCED_BRACKET,
					"nested settings row was accepted");
				require(SettingsTable::End(owner) ==
						DMUI_RESULT_UNBALANCED_BRACKET,
					"settings table ended with an open row");
				bool resetPressed{};
				ImGui::PushID("client-damage");
				require(SettingsTable::EndRow(
							owner, {}, resetPressed) ==
						DMUI_RESULT_UNBALANCED_BRACKET,
					"row ended through client stack damage");
				ImGui::PopID();
				require(SettingsTable::EndRow(
							other, {}, resetPressed) ==
						DMUI_RESULT_UNBALANCED_BRACKET,
					"foreign owner ended the row");
				require(SettingsTable::EndRow(
							owner, {}, resetPressed) ==
						DMUI_RESULT_OK,
					"row could not recover from a rejected end");
				require(SettingsTable::End(other) ==
						DMUI_RESULT_UNBALANCED_BRACKET,
					"foreign owner ended the settings table");
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"settings table did not end");
			}
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"early return changed the ImGui stack");
		});

		runner.test("abandoned settings row is unwound at callback exit", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			ImGuiTestFrame frame;
			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				const auto table = SettingsTable::Begin(owner, "settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"settings table did not begin");
				const auto row = SettingsTable::BeginRow(
					owner, "row", "Setting", nullptr);
				require(row.result == DMUI_RESULT_OK && row.visible,
					"settings row did not begin");
			}
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"callback recovery reported or retained abandoned brackets");
		});

		runner.test("standalone feedback owns text and recovers its brackets", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			constexpr DMUI_ClientHandle other{ 8 };
			FeedbackAppearanceGuard appearance;
			FieldFeedback::SetAppearance({
				FieldFeedbackPlacement::kUnderLabel,
				FieldFeedback::kDefaultInfoColor,
				FieldFeedback::kDefaultWarningColor,
				FieldFeedback::kDefaultErrorColor
			});

			ImGuiTestFrame frame;
			{
				const SettingsTable::ClientCallbackGuard guard{ owner, false };
				require(!SettingsTable::AcceptsClient(owner) &&
						SettingsTable::AcceptsFieldClient(owner),
					"standalone field widened settings-table permission");
				auto message = std::string{
					"Original client-owned 100% ##literal caf\xC3\xA9\nSecond line."
				};
				ImGui::LogToBuffer();
				BeginFeedbackField(owner, "standalone");
				bool value{};
				(void)ImGui::Checkbox("##Value", &value);
				const auto controlId =
					ImGui::GetCurrentContext()->LastItemData.ID;
				require(SettingsTable::SetFieldFeedback(
							other,
							DMUI_FIELD_FEEDBACK_SEVERITY_WARNING,
							"foreign") ==
						DMUI_RESULT_UNBALANCED_BRACKET &&
						SettingsTable::SetFieldFeedback(
							owner,
							DMUI_FIELD_FEEDBACK_SEVERITY_WARNING,
							message) == DMUI_RESULT_OK &&
						ImGui::GetCurrentContext()->LastItemData.ID ==
							controlId,
					"feedback ownership or last-item semantics changed");
				message.assign(message.size(), 'x');
				EndFeedbackField(owner);
				const std::string logged{
					ImGui::GetCurrentContext()->LogBuffer.c_str()
				};
				ImGui::LogFinish();
				require(
					logged.contains(
						"Warning: Original client-owned 100% ##literal caf\xC3\xA9") &&
						logged.contains("Second line.") &&
						!logged.contains(std::string(16, 'x')),
					"feedback did not retain and draw the literal client text");

				BeginFeedbackField(owner, "abandoned");
				ImGui::Button("Value");
				require(SettingsTable::SetFieldFeedback(
							owner,
							DMUI_FIELD_FEEDBACK_SEVERITY_ERROR,
							"Abandoned") == DMUI_RESULT_OK,
					"abandoned field feedback was rejected");
			}
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"standalone field recovery changed the ImGui stack");
		});

		runner.test("under-control feedback follows the complete control group", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			FeedbackAppearanceGuard appearance;
			FieldFeedback::SetAppearance({
				FieldFeedbackPlacement::kUnderControl,
				FieldFeedback::PackColor(0x2A, 0xE3, 0x4D),
				FieldFeedback::kDefaultWarningColor,
				FieldFeedback::kDefaultErrorColor
			});

			ImGuiTestFrame frame;
			const SettingsTable::ClientCallbackGuard guard{ owner };
			const auto table = SettingsTable::Begin(owner, "reset-alignment");
			require(table.result == DMUI_RESULT_OK && table.visible,
				"reset alignment table did not begin");
			BeginFeedbackField(owner, "grouped-control");
			ImGui::BeginGroup();
			ImGui::Button("Primary control");
			ImGui::TextUnformatted("Additional grouped control content");
			ImGui::EndGroup();
			const auto controlRect =
				ImGui::GetCurrentContext()->LastItemData.Rect;
			const auto resetMinX =
				ImGui::GetCurrentTable()->Columns[1].WorkMinX;
			auto* draw = ImGui::GetWindowDrawList();
			const auto firstVertex = draw->VtxBuffer.Size;
			require(SettingsTable::SetFieldFeedback(
						owner,
						DMUI_FIELD_FEEDBACK_SEVERITY_INFO,
						"Informational text below the entire grouped control.") ==
					DMUI_RESULT_OK,
				"under-control feedback was rejected");
			EndFeedbackField(owner, true);

			const auto feedbackColor = ImGui::GetColorU32(
				FieldFeedback::SeverityColor(
					DMUI_FIELD_FEEDBACK_SEVERITY_INFO));
			auto feedbackMinY = (std::numeric_limits<float>::max)();
			auto feedbackMaxY = (std::numeric_limits<float>::lowest)();
			auto resetMinY = (std::numeric_limits<float>::max)();
			auto resetMaxY = (std::numeric_limits<float>::lowest)();
			for (int index = firstVertex; index < draw->VtxBuffer.Size; ++index)
			{
				const auto& vertex = draw->VtxBuffer[index];
				if (vertex.col == feedbackColor)
				{
					feedbackMinY = (std::min)(feedbackMinY, vertex.pos.y);
					feedbackMaxY = (std::max)(feedbackMaxY, vertex.pos.y);
				}
				if (vertex.pos.x >= resetMinX)
				{
					resetMinY = (std::min)(resetMinY, vertex.pos.y);
					resetMaxY = (std::max)(resetMaxY, vertex.pos.y);
				}
			}
			require(
				feedbackMaxY > feedbackMinY &&
					feedbackMinY >= controlRect.Max.y &&
					resetMaxY > resetMinY &&
					std::abs(
						(resetMinY + resetMaxY) * 0.5f -
						(controlRect.Min.y + controlRect.Max.y) * 0.5f) <
						ImGui::GetTextLineHeight(),
				"feedback overlapped the group or displaced reset");

			BeginFeedbackField(owner, "cleared");
			ImGui::Button("Cleared control");
			require(
				ImGui::GetCurrentContext()->LastItemData.Rect.Min.y >
					feedbackMaxY,
				"next row overlapped under-control feedback");
			require(SettingsTable::SetFieldFeedback(
						owner,
						DMUI_FIELD_FEEDBACK_SEVERITY_ERROR,
						"Transient") == DMUI_RESULT_OK &&
					SettingsTable::SetFieldFeedback(
						owner,
						DMUI_FIELD_FEEDBACK_SEVERITY_INFO,
						{}) == DMUI_RESULT_OK,
				"same-frame feedback clear failed");
			EndFeedbackField(owner);
			const auto* outer = ImGui::GetCurrentTable();
			const auto clearedHeight = outer->RowPosY2 - outer->RowPosY1;

			BeginFeedbackField(owner, "plain");
			ImGui::Button("Plain control");
			EndFeedbackField(owner);
			outer = ImGui::GetCurrentTable();
			const auto plainHeight = outer->RowPosY2 - outer->RowPosY1;
			require(std::abs(clearedHeight - plainHeight) < 0.5f,
				"cleared feedback retained row extent");
			require(SettingsTable::End(owner) == DMUI_RESULT_OK,
				"under-control table did not end");
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"under-control feedback changed the ImGui stack");
		});

		runner.test("feedback strip wraps inside padding before the next row", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			FeedbackAppearanceGuard appearance;
			FieldFeedback::SetAppearance({
				FieldFeedbackPlacement::kFullWidthStrip,
				FieldFeedback::kDefaultInfoColor,
				FieldFeedback::PackColor(0x2A, 0xE3, 0x4D),
				FieldFeedback::kDefaultErrorColor
			});

			ImGuiTestFrame frame;
			const SettingsTable::ClientCallbackGuard guard{ owner };
			const auto table = SettingsTable::Begin(owner, "strip-layout");
			require(table.result == DMUI_RESULT_OK && table.visible,
				"strip table did not begin");
			const auto outerId = ImGui::GetCurrentTable()->ID;
			BeginFeedbackField(owner, "strip");
			ImGui::Button("Value");
			const auto* outer =
				ImGui::GetCurrentContext()->Tables.GetByKey(outerId);
			const auto stripMinX = outer->Columns[0].WorkMinX;
			const auto stripMaxX = outer->Columns[1].WorkMaxX;
			auto* draw = ImGui::GetWindowDrawList();
			const auto firstVertex = draw->VtxBuffer.Size;
			ImGui::LogToBuffer();
			require(SettingsTable::SetFieldFeedback(
						owner,
						DMUI_FIELD_FEEDBACK_SEVERITY_WARNING,
						"100% ##literal caf\xC3\xA9\n"
						"A long second line wraps across the full-width strip "
						"without escaping its horizontal padding.") ==
					DMUI_RESULT_OK,
				"strip feedback was rejected");
			EndFeedbackField(owner);
			const std::string logged{
				ImGui::GetCurrentContext()->LogBuffer.c_str()
			};
			ImGui::LogFinish();

			const auto feedbackColor = ImGui::GetColorU32(
				FieldFeedback::SeverityColor(
					DMUI_FIELD_FEEDBACK_SEVERITY_WARNING));
			auto textMinX = (std::numeric_limits<float>::max)();
			auto textMaxX = (std::numeric_limits<float>::lowest)();
			auto textMinY = (std::numeric_limits<float>::max)();
			auto textMaxY = (std::numeric_limits<float>::lowest)();
			for (int index = firstVertex; index < draw->VtxBuffer.Size; ++index)
			{
				const auto& vertex = draw->VtxBuffer[index];
				if (vertex.col != feedbackColor ||
					vertex.pos.x <= stripMinX + 3.0f * Theme::Scale())
					continue;
				textMinX = (std::min)(textMinX, vertex.pos.x);
				textMaxX = (std::max)(textMaxX, vertex.pos.x);
				textMinY = (std::min)(textMinY, vertex.pos.y);
				textMaxY = (std::max)(textMaxY, vertex.pos.y);
			}
			const auto padding = 8.0f * Theme::Scale();
			require(
				textMaxY - textMinY > ImGui::GetTextLineHeight() &&
					textMinX >= stripMinX + padding - 0.5f &&
					textMaxX <= stripMaxX - padding + 0.5f &&
					logged.contains(
						"Warning: 100% ##literal caf\xC3\xA9") &&
					logged.contains("A long second line"),
				"strip text lost inline literal formatting or padding");

			BeginFeedbackField(owner, "following");
			ImGui::Button("Following control");
			require(
				ImGui::GetCurrentContext()->LastItemData.Rect.Min.y > textMaxY,
				"next row overlapped the feedback strip");
			EndFeedbackField(owner);
			require(SettingsTable::End(owner) == DMUI_RESULT_OK,
				"strip table did not end");
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"feedback strip changed the ImGui stack");
		});

		runner.test("color presets align with roomy and narrow pickers", [] {
			ImGuiTestFrame frame;
			const std::array presets{
				HostSettingsViewDetail::ColorPreset{
					"Blue", "Blue", { 0x00, 0x72, 0xB2 } },
				HostSettingsViewDetail::ColorPreset{
					"Orange", "Orange", { 0xE6, 0x9F, 0x00 } },
				HostSettingsViewDetail::ColorPreset{
					"Sky", "Sky", { 0x56, 0xB4, 0xE9 } },
				HostSettingsViewDetail::ColorPreset{
					"Purple", "Purple", { 0xCC, 0x79, 0xA7 } }
			};
			auto color = HostAccentColor{};
			const auto draw = [&](const char* id, float width) {
				ImGui::PushID(id);
				const auto result =
					HostSettingsViewDetail::DrawColorSettingControl(
						color,
						presets,
						width);
				ImGui::PopID();
				return result;
			};
			const auto roomy = draw("roomy", 420.0f);
			const auto narrow = draw("narrow", 145.0f);
			const auto aligned = [](const auto& result) {
				return std::abs(
						result.pickerMaxX - result.presetsMaxX) < 0.5f &&
					result.presetsMinX >= result.pickerMinX - 0.5f &&
					std::abs(
						result.pickerHeight - result.presetHeight) < 0.5f;
			};
			require(
				aligned(roomy) &&
					aligned(narrow) &&
					roomy.presetsInline &&
					!narrow.presetsInline,
				"color presets did not share the picker span");
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"color setting control changed the ImGui stack");
		});
	}
}
