#include "../Harness.h"
#include "../support/ImGuiTestContext.h"

#include <DearModdingUI/controls/SettingsTable.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/HostAPIEntries.h>

#include <imgui/imgui.h>

namespace DearModdingUI::HostInternal
{
	DMUI_Result ValidateDrawingClient(DMUI_ClientHandle a_client) noexcept
	{
		return a_client == DMUI_INVALID_CLIENT_HANDLE ?
			DMUI_RESULT_INVALID_ARGUMENT :
			DMUI_RESULT_OK;
	}
}

// A .0 client requests the bare major.
static_assert(DearModdingUI::ServesAbiVersion(DMUI_ABI_MAJOR));
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
		runner.test("field brackets use production adapters", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			DMUI_FieldBeginOptions beginOptions{ DMUI_FIELD_LAYOUT_LABEL_VALUE };
			uint32_t visible{ 1u };
			require(
				HostAPIInternal::ApiBeginField(
					owner, "null-options", "", nullptr, nullptr, &visible) ==
						DMUI_RESULT_INVALID_ARGUMENT &&
					visible == 0u,
				"field begin accepted null options");

			support::ImGuiTestContext imgui{
				{ .disableErrorRecovery = true }
			};
			imgui.BeginWindow(
				"##HostAPIFieldTest",
				{ 60.0f, 60.0f },
				{ 640.0f, 480.0f });
			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				const auto table = SettingsTable::Begin(owner, "settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"shared settings table did not begin");

				visible = 0u;
				require(
					HostAPIInternal::ApiBeginField(
						owner, "empty-label", "", nullptr, &beginOptions, &visible) ==
							DMUI_RESULT_OK &&
						visible != 0,
					"field rejected its published empty-label contract");
				ImGui::TextUnformatted("Value");
				DMUI_FieldEndOptions endOptions{ 0u, 0u };
				uint32_t resetPressed{ 1u };
				require(
					HostAPIInternal::ApiEndField(owner, nullptr, &resetPressed) ==
							DMUI_RESULT_INVALID_ARGUMENT &&
						resetPressed == 0u,
					"field end accepted null options");
				resetPressed = 1u;
				require(
					HostAPIInternal::ApiEndField(
						owner, &endOptions, &resetPressed) == DMUI_RESULT_OK &&
						resetPressed == 0u,
					"field did not end through the shared renderer");

				beginOptions.layout = DMUI_FIELD_LAYOUT_FULL_SPAN;
				visible = 0u;
				require(
					HostAPIInternal::ApiBeginField(
						owner, "full-span", "", "", &beginOptions, &visible) ==
							DMUI_RESULT_OK &&
						visible != 0,
					"full-span field did not begin");
				ImGui::TextUnformatted("Full-width value");
				endOptions.resetVisible = 1u;
				endOptions.resetEnabled = 1u;
				resetPressed = 1u;
				require(
					HostAPIInternal::ApiEndField(
						owner, &endOptions, &resetPressed) == DMUI_RESULT_OK &&
						resetPressed == 0u,
					"reset options did not use the shared field end");
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"shared settings table did not end");
			}
			imgui.EndWindow();
		});
	}
}
