#include "../Harness.h"
#include "../support/ImGuiTestContext.h"

#include <DearModdingUI/controls/SettingsTable.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/HostAPIEntries.h>

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

// Released ABI 2.0 clients request the bare major; newer minors need a newer host.
static_assert(DearModdingUI::ServesAbiVersion(2u));
static_assert(DearModdingUI::ServesAbiVersion(DMUI_ABI_VERSION));
static_assert(!DearModdingUI::ServesAbiVersion(
	DMUI_MAKE_ABI_VERSION(DMUI_ABI_MAJOR, DMUI_ABI_MINOR + 1u)));
static_assert(!DearModdingUI::ServesAbiVersion(
	DMUI_MAKE_ABI_VERSION(DMUI_ABI_MAJOR + 1u, 0u)));
static_assert(!DearModdingUI::ServesAbiVersion(
	DMUI_MAKE_ABI_VERSION(DMUI_ABI_MAJOR - 1u, DMUI_ABI_MINOR)));

namespace vmm_tests
{
	using namespace DearModdingUI;

	void run_host_api_compatibility_checks(Runner& runner)
	{
		runner.test("settings rows use production adapters", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			uint32_t visible{ 1u };
			require(
				HostAPIInternal::ApiBeginSettingsRowEx(
					owner,
					"null-options",
					"",
					nullptr,
					nullptr,
					&visible) == DMUI_RESULT_INVALID_ARGUMENT &&
					visible == 0u,
				"legacy begin accepted null options");
			visible = 1u;

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
					HostAPIInternal::ApiBeginSettingsRow(
						owner, "empty-label", "", nullptr, &visible) ==
							DMUI_RESULT_OK &&
						visible != 0,
					"legacy row rejected its published empty-label contract");
				ImGui::TextUnformatted("Legacy value");
				DMUI_SettingsRowOptions endOptions{
					0u,
					0u
				};
				uint32_t resetPressed{ 1u };
				require(
					HostAPIInternal::ApiEndSettingsRow(owner, nullptr, &resetPressed) ==
							DMUI_RESULT_INVALID_ARGUMENT &&
						resetPressed == 0u,
					"legacy end accepted null options");
				resetPressed = 1u;
				require(
					HostAPIInternal::ApiEndSettingsRow(
						owner, &endOptions, &resetPressed) == DMUI_RESULT_OK &&
						resetPressed == 0u,
					"legacy row did not end through the shared renderer");

				DMUI_SettingsRowBeginOptions beginOptions{
					DMUI_SETTINGS_ROW_LAYOUT_FULL_SPAN
				};
				visible = 0u;
				require(
					HostAPIInternal::ApiBeginSettingsRowEx(
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
					HostAPIInternal::ApiEndSettingsRow(
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
