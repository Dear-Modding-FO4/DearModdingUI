#include "../Harness.h"
#include "../support/ImGuiTestContext.h"
#include "../support/PresentationTestSupport.h"

#include <DearModdingUI/controls/SettingsTable.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/HostAPIEntries.h>

#include <imgui/imgui.h>

#include <cstddef>
#include <cstring>
#include <Windows.h>

namespace
{
	class GuardedTable
	{
	public:
		GuardedTable(const void* a_table, size_t a_size)
		{
			SYSTEM_INFO info{};
			GetSystemInfo(&info);
			m_storage = VirtualAlloc(
				nullptr, info.dwPageSize * 2u, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			vmm_tests::require(m_storage != nullptr, "guarded table allocation failed");
			DWORD previous{};
			if (!VirtualProtect(
					static_cast<std::byte*>(m_storage) + info.dwPageSize,
					info.dwPageSize, PAGE_NOACCESS, &previous))
			{
				VirtualFree(m_storage, 0u, MEM_RELEASE);
				m_storage = nullptr;
				vmm_tests::require(false, "table guard protection failed");
			}
			m_table = static_cast<std::byte*>(m_storage) + info.dwPageSize - a_size;
			std::memcpy(m_table, a_table, a_size);
		}

		~GuardedTable()
		{
			VirtualFree(m_storage, 0u, MEM_RELEASE);
		}

		GuardedTable(const GuardedTable&) = delete;
		GuardedTable& operator=(const GuardedTable&) = delete;

		template <class Table>
		const Table* Get() const
		{
			return reinterpret_cast<const Table*>(m_table);
		}

	private:
		void* m_storage{};
		std::byte* m_table{};
	};
}

namespace DearModdingUI::HostInternal
{
	DMUI_Result ValidateDrawingClient(DMUI_ClientHandle a_client) noexcept
	{
		return a_client == DMUI_INVALID_CLIENT_HANDLE ?
			DMUI_RESULT_INVALID_ARGUMENT :
			DMUI_RESULT_OK;
	}
}

// Released ABI 2.0 clients request the bare major.
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
		runner.test("new headers negotiate older tables without reading appended slots", [] {
			const auto& currentUI = DearModdingUI::UI::API();
			GuardedTable uiPrefix{ &currentUI, offsetof(DMUI_UIAPI, inputTextEditor) };
			DMUI_HostAPI currentHost{};
			currentHost.abiMajor = DMUI_ABI_MAJOR;
			currentHost.ui = uiPrefix.Get<DMUI_UIAPI>();
			GuardedTable hostPrefix{
				&currentHost, offsetof(DMUI_HostAPI, requestOverlayFocus)
			};
			uint32_t minor{};
			uint32_t requests{};
			const auto* api = dmui::detail::NegotiateAPI(
				[&](uint32_t version) {
					require(
						version == DMUI_MAKE_ABI_VERSION(
							DMUI_ABI_MAJOR, DMUI_ABI_MINOR - requests),
						"minor requests were not descending");
					++requests;
					return version == DMUI_MAKE_ABI_VERSION(DMUI_ABI_MAJOR, 0u) ?
						hostPrefix.Get<DMUI_HostAPI>() : nullptr;
				},
				minor);
			require(api && minor == 0u && requests == DMUI_ABI_MINOR + 1u,
				"minor-zero host did not negotiate");
			support::presentation::ImGuiFrame frame;
			dmui::Client client{ "compatibility", "Compatibility", { 1, 0 } };
			dmui::detail::ClientTestAccess::Bind(client, *api, 1u, minor);
			require(client.AbiMinor() == 0u, "client lost the negotiated minor");
			const auto result = dmui::detail::ClientTestAccess::Draw(client, [&] {
				require(!client.RequestOverlayFocus(1u) &&
						client.LastResult() == DMUI_RESULT_UNSUPPORTED_ABI,
					"request read an absent host slot");
				require(!client.ReleaseOverlayFocus(1u) &&
						client.LastResult() == DMUI_RESULT_UNSUPPORTED_ABI,
					"release read an absent host slot");
				require(!client.QueryOverlayFocus(1u) &&
						client.LastResult() == DMUI_RESULT_UNSUPPORTED_ABI,
					"query read an absent host slot");
				DMUI_TextEditState state{};
				require(dmui::ui::checked::InputTextEditor(
						"editor", nullptr, nullptr, 0u, 0u, 0u, &state) ==
						DMUI_RESULT_UNSUPPORTED_ABI,
					"editor read an absent UI slot");
				std::string text{ "fallback" };
				require(!dmui::ui::InputTextEditor(
						"editor", nullptr, text, dmui::ui::TextEditFlags::kNone, 0u, state) &&
						dmui::ui::LastResult() == DMUI_RESULT_UNSUPPORTED_ABI &&
						text == "fallback",
					"text-buffer completion hid the unsupported editor result");
				require(!dmui::ui::BeginTooltipAt({ 0.0f, 0.0f }),
					"tooltip read an absent UI slot");
				require(dmui::ui::LastResult() == DMUI_RESULT_UNSUPPORTED_ABI,
					"unsupported UI result was not observable");
				require(dmui::ui::detail::currentContext->result == DMUI_RESULT_OK,
					"unsupported UI feature became sticky");
				dmui::ui::Spacing();
				require(dmui::ui::LastResult() == DMUI_RESULT_OK,
					"supported UI fallback failed");
			});
			require(result == DMUI_RESULT_OK, "feature probing disabled the drawing callback");
		});

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
