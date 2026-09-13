#include "../Harness.h"
#include "../support/ImGuiTestContext.h"

#include <DearModdingUI/controls/SettingsTable.h>

#include <imgui/imgui.h>

#include <cstddef>

namespace DearModdingUI::HostInternal
{
	DMUI_Result ValidateDrawingClient(DMUI_ClientHandle a_client) noexcept
	{
		return a_client == DMUI_INVALID_CLIENT_HANDLE ?
			DMUI_RESULT_INVALID_ARGUMENT :
			DMUI_RESULT_OK;
	}
}

namespace vmm_tests
{
	using namespace DearModdingUI;

	void run_host_api_compatibility_checks(Runner& runner)
	{
		runner.test("published row layout uses production adapters", [] {
			const auto* api = DMUI_GetAPI(DMUI_HOST_ABI_1);
			require(api, "fixture host API was unavailable");
			require(
				offsetof(DMUI_HostAPI, beginSettingsRow) == 224 &&
					offsetof(DMUI_HostAPI, endSettingsRow) == 232 &&
					offsetof(DMUI_HostAPI, endSettingsTable) == 240 &&
					offsetof(DMUI_HostAPI, beginSettingsRowEx) == 248 &&
					offsetof(DMUI_HostAPI, registerPageActivityObserver) == 256 &&
					offsetof(DMUI_HostAPI, resolveIconGlyph) == 440 &&
					offsetof(DMUI_HostAPI, beginField) == 448 &&
					offsetof(DMUI_HostAPI, endField) == 464,
				"append-only host table layout changed");

			constexpr DMUI_ClientHandle owner{ 7 };
			DMUI_SettingsRowBeginOptions shortBeginOptions{
				sizeof(uint32_t),
				DMUI_SETTINGS_ROW_LAYOUT_LABEL_VALUE
			};
			uint32_t visible{ 1u };
			require(
				api->beginSettingsRowEx(
					owner,
					"null-options",
					"",
					nullptr,
					nullptr,
					&visible) == DMUI_RESULT_INVALID_ARGUMENT &&
					visible == 0u,
				"legacy begin accepted null options");
			visible = 1u;
			require(
				api->beginSettingsRowEx(
					owner,
					"short-options",
					"",
					nullptr,
					&shortBeginOptions,
					&visible) == DMUI_RESULT_STRUCT_TOO_SMALL &&
					visible == 0u,
				"legacy begin options did not enforce their published prefix");

			support::ImGuiTestContext imgui{
				{ .disableErrorRecovery = true }
			};
			imgui.BeginWindow(
				"##LegacyHostAPITest",
				{ 60.0f, 60.0f },
				{ 640.0f, 480.0f });
			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				const auto table = SettingsTable::Begin(owner, "legacy-settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"shared settings table did not begin");

				visible = 0u;
				require(
					api->beginSettingsRow(
						owner, "empty-label", "", nullptr, &visible) ==
							DMUI_RESULT_OK &&
						visible != 0,
					"legacy row rejected its published empty-label contract");
				ImGui::TextUnformatted("Legacy value");
				DMUI_SettingsRowOptions endOptions{
					sizeof(DMUI_SettingsRowOptions),
					0u,
					0u
				};
				uint32_t resetPressed{ 1u };
				require(
					api->endSettingsRow(owner, nullptr, &resetPressed) ==
							DMUI_RESULT_INVALID_ARGUMENT &&
						resetPressed == 0u,
					"legacy end accepted null options");
				resetPressed = 1u;
				const DMUI_SettingsRowOptions shortEndOptions{
					sizeof(uint32_t) * 2u,
					0u,
					0u
				};
				require(
					api->endSettingsRow(
						owner, &shortEndOptions, &resetPressed) ==
							DMUI_RESULT_STRUCT_TOO_SMALL &&
						resetPressed == 0u,
					"legacy end options did not enforce their published prefix");
				require(
					api->endSettingsRow(
						owner, &endOptions, &resetPressed) == DMUI_RESULT_OK &&
						resetPressed == 0u,
					"legacy row did not end through the shared renderer");

				DMUI_SettingsRowBeginOptions beginOptions{
					sizeof(DMUI_SettingsRowBeginOptions),
					DMUI_SETTINGS_ROW_LAYOUT_FULL_SPAN
				};
				visible = 0u;
				require(
					api->beginSettingsRowEx(
						owner,
						"full-span",
						"",
						"",
						&beginOptions,
						&visible) == DMUI_RESULT_OK &&
						visible != 0,
					"legacy full-span row did not begin");
				ImGui::TextUnformatted("Full-width legacy value");
				endOptions.resetVisible = 1u;
				endOptions.resetEnabled = 1u;
				resetPressed = 1u;
				require(
					api->endSettingsRow(
						owner, &endOptions, &resetPressed) == DMUI_RESULT_OK &&
						resetPressed == 0u,
					"legacy reset options did not use the shared row end");
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"shared settings table did not end");
			}
			imgui.EndWindow();
		});
	}
}
