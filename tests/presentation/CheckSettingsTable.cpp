#include <DearModdingUI/host/ImGuiRecovery.h>
#include <DearModdingUI/controls/SettingsTable.h>
#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/presentation/Theme.h>
#include <DearModdingUI/host/UIAdapter.h>
#include "../Harness.h"
#include "../support/ImGuiTestContext.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <DearModdingUI/Presentation.h>
#include <DearModdingUI/Client.h>


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
	}

	void run_settings_table_checks(Runner& runner)
	{
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
			const std::array raw{
				dmui::ChoiceOption<int>{ .value = 1, .label = "100% ## duplicate", .key = "first" }
			};
			const auto unknown = dmui::DrawChoice("unknown", 99, raw, "Unknown selection");
			const auto empty = dmui::DrawChoice("empty", 99, {});
			require(!unknown.changed && !unknown.completed && !unknown.selected &&
					!empty.changed && !empty.completed && !empty.selected,
				"unknown or empty choice was clamped or produced a selection");
			require(writes == 1 && edits == 1 && frame.IsAtBaseline() && frame.Errors() == 0,
				"empty-list presentation wrote storage or leaked ImGui state");
		});

		runner.test("ImGui recovery restores leaked callback stacks", [] {
			ImGuiTestFrame frame;
			auto recovery = ImGuiRecoverySnapshot::Capture();
			require(recovery.has_value(), "recovery snapshot was not captured");
			ImGui::PushID("leaked-id");
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{});
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
			ImGui::PushFont(ImGui::GetFont());
			require(ImGui::BeginTable("leaked-table", 1),
				"recovery table did not begin");

			const auto repaired = recovery->RecoverAfterCallback();
			require(repaired.Repaired() && frame.IsAtBaseline(),
				"callback recovery did not restore leaked ImGui stacks");
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

		runner.test("settings bracket early returns remain recoverable", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			constexpr DMUI_ClientHandle other{ 8 };
			ImGuiTestFrame frame;
			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
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
				const auto row = SettingsTable::BeginRow(
					owner, "row", "Setting", nullptr);
				require(row.result == DMUI_RESULT_OK && row.visible,
					"settings row did not begin");
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
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"settings table did not end");

				const auto hidden = SettingsTable::Begin(owner, "hidden-row");
				require(hidden.result == DMUI_RESULT_OK && hidden.visible,
					"settings table did not begin");
				ImGui::GetCurrentTable()->TempData->ReconcileColumnsRequests[1].Flags |=
					ImGuiTableColumnFlags_Disabled;
				const auto invisibleRow = SettingsTable::BeginRow(
					owner, "row", "Setting", nullptr);
				require(invisibleRow.result == DMUI_RESULT_OK && !invisibleRow.visible,
					"disabled value column did not produce an invisible row");
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"invisible row left the table bracket open");

				const auto abandoned = SettingsTable::Begin(owner, "abandoned");
				require(abandoned.result == DMUI_RESULT_OK && abandoned.visible &&
						SettingsTable::BeginRow(owner, "row", "Setting", nullptr).visible,
					"abandoned row fixture did not begin");
			}
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"early return or abandoned row changed the ImGui stack");
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
	}
}
