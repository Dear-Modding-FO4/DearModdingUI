#include <DearModdingUI/ImGuiRecovery.h>
#include <DearModdingUI/SettingsTable.h>
#include <DearModdingUI/Shell.h>
#include <DearModdingUI/Theme.h>
#include "Harness.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace DearModdingUI
{
	float SettingsActionButtonWidth(
		SettingsAction,
		const char*,
		float a_buttonExtent) noexcept
	{
		return a_buttonExtent;
	}

	bool DrawSettingsActionButton(
		const char*,
		const ImVec2&,
		const ImVec2&,
		SettingsAction,
		const char*,
		const char*,
		bool) noexcept
	{
		return false;
	}
}

namespace DearModdingUI::Theme
{
	FontGuard::FontGuard(FontRole, float) noexcept {}
	FontGuard::~FontGuard() noexcept = default;
}

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI;

		class ImGuiTestFrame
		{
		public:
			ImGuiTestFrame()
			{
				m_context = ImGui::CreateContext();
				auto& io = ImGui::GetIO();
				io.DisplaySize = { 1280.0f, 720.0f };
				io.DeltaTime = 1.0f / 60.0f;
				io.IniFilename = nullptr;
				io.ConfigErrorRecoveryEnableAssert = false;
				io.ConfigErrorRecoveryEnableDebugLog = false;
				io.ConfigErrorRecoveryEnableTooltip = false;
				(void)io.Fonts->Build();
				m_context->ErrorCallback = [](ImGuiContext*, void* a_userData, const char*) {
					++*static_cast<int*>(a_userData);
				};
				m_context->ErrorCallbackUserData = &m_errors;
				ImGui::NewFrame();
				ImGui::SetNextWindowSize({ 640.0f, 480.0f });
				(void)ImGui::Begin("##SettingsTableTest");
				ImGui::ErrorRecoveryStoreState(&m_recovery);
				m_idDepth = ImGui::GetCurrentWindow()->IDStack.Size;
			}

			~ImGuiTestFrame()
			{
				ImGui::ErrorRecoveryTryToRecoverState(&m_recovery);
				ImGui::End();
				ImGui::EndFrame();
				ImGui::DestroyContext(m_context);
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
					context->CurrentWindow->IDStack.Size == m_idDepth;
			}

		private:
			ImGuiContext* m_context{ nullptr };
			ImGuiErrorRecoveryState m_recovery;
			int m_idDepth{ 0 };
			int m_errors{ 0 };
		};
	}

	void run_settings_table_checks(Runner& runner)
	{
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

		runner.test("settings row closes its nested controls table", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			ImGuiTestFrame frame;
			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				const auto table = SettingsTable::Begin(owner, "settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"settings table did not begin");
				const auto row = SettingsTable::BeginRow(
					owner, "row", "Setting", "Description");
				require(row.result == DMUI_RESULT_OK && row.visible,
					"settings row did not begin");
				bool resetPressed{};
				require(SettingsTable::EndRow(
							owner, { true, false }, resetPressed) ==
						DMUI_RESULT_OK,
					"settings row rejected its nested table depth");
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"settings table did not end");
			}
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"balanced row changed the ImGui stack");
		});

		runner.test("settings row accepts a deliberately empty label", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			ImGuiTestFrame frame;
			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				const auto table = SettingsTable::Begin(owner, "settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"settings table did not begin");
				const auto row = SettingsTable::BeginRow(
					owner, "prose", "", "");
				require(row.result == DMUI_RESULT_OK && row.visible,
					"settings row rejected an empty label");
				ImGui::TextUnformatted("Prose");
				bool resetPressed{};
				require(SettingsTable::EndRow(
							owner, { false, false }, resetPressed) ==
						DMUI_RESULT_OK,
					"unlabeled settings row did not end");
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"settings table did not end");
			}
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"unlabeled row changed the ImGui stack");
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

		runner.test("first-frame host controls need no ImGui recovery", [] {
			ImGuiTestFrame frame;
			auto recovery = ImGuiRecoverySnapshot::Capture();
			require(recovery.has_value(), "recovery snapshot was not captured");
			const auto table = SettingsTable::Begin(
				DMUI_INVALID_CLIENT_HANDLE,
				"host-settings");
			require(table.result == DMUI_RESULT_OK && table.visible,
				"host settings table did not begin");

			const auto drawRow = [](const char* a_id, auto&& a_draw) {
				const auto row = SettingsTable::BeginRow(
					DMUI_INVALID_CLIENT_HANDLE,
					a_id,
					"Setting",
					"Description");
				require(row.result == DMUI_RESULT_OK && row.visible,
					"host settings row did not begin");
				a_draw();
				bool resetPressed{};
				require(SettingsTable::EndRow(
							DMUI_INVALID_CLIENT_HANDLE,
							{ true, false },
							resetPressed) == DMUI_RESULT_OK,
					"host settings row did not end");
			};

			float color[]{ 0.25f, 0.50f, 0.75f };
			drawRow("color", [&]() {
				(void)ImGui::ColorPicker3("##Value", color);
				for (int index = 0; index < 3; ++index)
				{
					ImGui::PushID(index);
					if (index > 0)
						ImGui::SameLine();
					(void)ImGui::ColorButton(
						"Preset",
						{ color[0], color[1], color[2], 1.0f });
					ImGui::PopID();
				}
			});
			bool checked{};
			drawRow("checkbox", [&]() {
				(void)ImGui::Checkbox("##Value", &checked);
			});
			float scalar{ 0.5f };
			drawRow("slider", [&]() {
				(void)ImGui::SliderFloat(
					"##Value",
					&scalar,
					0.0f,
					1.0f);
			});

			require(SettingsTable::End(DMUI_INVALID_CLIENT_HANDLE) ==
					DMUI_RESULT_OK,
				"host settings table did not end");
			const auto repaired = recovery->RecoverAfterCallback();
			require(!repaired.Repaired() && frame.Errors() == 0,
				"balanced first-frame controls required ImGui recovery");
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
	}
}
