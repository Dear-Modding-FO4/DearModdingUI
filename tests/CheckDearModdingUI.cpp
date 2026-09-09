#include <DearModdingUI/CarrierMenu.h>
#include <DearModdingUI/Diagnostics.h>
#include <DearModdingUI/Faq.h>
#include <DearModdingUI/FontCatalog.h>
#include <DearModdingUI/Health.h>
#include <DearModdingUI/Host.h>
#include <DearModdingUI/HostSettings.h>
#include <DearModdingUI/HostSettingsHealth.h>
#include <DearModdingUI/HostSettingsView.h>
#include <DearModdingUI/Home.h>
#include <DearModdingUI/LinkRow.h>
#include <DearModdingUI/MenuToggleKey.h>
#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/NavigationController.h>
#include <DearModdingUI/NavigationPresentation.h>
#include <DearModdingUI/Registry.h>
#include <DearModdingUI/Sidebar.h>
#include <DearModdingUI/SettingsTable.h>
#include <DearModdingUI/SettingsActions.h>
#include <DearModdingUI/Status.h>
#include <DearModdingUI/ShellGeometry.h>
#include <DearModdingUI/Theme.h>
#include <DearModdingUI/ThemeDefaults.h>
#include <DearModdingUI/TypographyHealth.h>
#include <DearModdingUI/UIAdapter.h>
#include <DearModdingUI/VisualDecisions.h>
#include <DearModdingUI/SidebarComparison.h>
#include "Harness.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <DearModdingUI/Client.h>

#include <Windows.h>
#include <bcrypt.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI;

		struct CallbackState
		{
			uint32_t ready{ 0 };
			uint32_t unavailable{ 0 };
			uint32_t draws{ 0 };
			DMUI_UnavailableReason reason{ DMUI_UNAVAILABLE_NONE };
		};

		struct PageActivityState
		{
			std::vector<DMUI_PageActivityInfo> events;
		};

		uint32_t s_mockRegistrations{};
		DMUI_HostServices s_mockServices{};
		DMUI_Result s_mockUIResult{ DMUI_RESULT_OK };
		uint32_t s_mockUIRevision{ DMUI_UI_REVISION_CURRENT };
		uint32_t s_mockUITableSize{ DMUI_UI_API_CURRENT_SIZE };
		bool s_mockMissingRequiredUIOperation{};
		bool s_mockMissingPlotLines{};
		uint32_t s_externalOpenCalls{};
		uint32_t s_externalNativeError{};
		DMUI_Result s_externalResult{ DMUI_RESULT_OK };
		ExternalOpenRequest s_externalRequest;
		uint32_t s_externalResolveCalls{};
		uint32_t s_externalResolveError{};
		DMUI_Result s_externalResolveResult{ DMUI_RESULT_OK };
		std::string s_externalVirtualFile;
		std::string s_externalPhysicalFile;

		struct SettingsMoveProbe
		{
			std::array<uint32_t, 2> flags{};
			size_t calls{ 0 };
			uint32_t firstError{ ERROR_ACCESS_DENIED };
			uint32_t secondError{ ERROR_ACCESS_DENIED };
			bool performSecondMove{ false };
		};

		bool ProbeSettingsMove(
			void* a_context,
			const std::filesystem::path& a_source,
			const std::filesystem::path& a_destination,
			uint32_t a_flags,
			uint32_t& a_nativeError) noexcept
		{
			auto& probe = *static_cast<SettingsMoveProbe*>(a_context);
			const auto index = probe.calls++;
			if (index < probe.flags.size())
				probe.flags[index] = a_flags;
			if (index == 0)
			{
				a_nativeError = probe.firstError;
				return false;
			}
			if (probe.performSecondMove)
			{
				if (MoveFileExW(
						a_source.c_str(),
						a_destination.c_str(),
						a_flags))
				{
					a_nativeError = ERROR_SUCCESS;
					return true;
				}
				a_nativeError = GetLastError();
				return false;
			}
			a_nativeError = probe.secondError;
			return false;
		}

		DMUI_Result FakeExternalFileResolver(
			std::string_view a_virtualFile,
			std::string& a_physicalFile,
			uint32_t* a_nativeError) noexcept
		{
			++s_externalResolveCalls;
			s_externalVirtualFile = a_virtualFile;
			if (a_nativeError)
				*a_nativeError = s_externalResolveError;
			if (s_externalResolveResult == DMUI_RESULT_OK)
				a_physicalFile = s_externalPhysicalFile;
			return s_externalResolveResult;
		}

		struct ExternalFileFixture
		{
			std::filesystem::path path;

			explicit ExternalFileFixture(std::string_view a_contents)
			{
				wchar_t directory[MAX_PATH]{};
				const auto length = GetTempPathW(MAX_PATH, directory);
				require(length != 0 && length < MAX_PATH, "temporary path was unavailable");
				wchar_t file[MAX_PATH]{};
				require(GetTempFileNameW(directory, L"dmu", 0, file) != 0,
					"temporary file creation failed");
				path = file;
				std::ofstream stream{ path, std::ios::binary | std::ios::trunc };
				stream.write(a_contents.data(), static_cast<std::streamsize>(a_contents.size()));
				require(stream.good(), "temporary file contents could not be written");
			}

			~ExternalFileFixture()
			{
				(void)SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
				(void)DeleteFileW(path.c_str());
			}

			[[nodiscard]] std::string Utf8() const
			{
				const auto text = path.u8string();
				return { text.begin(), text.end() };
			}
		};

		DMUI_Result FakeExternalOpen(
			const ExternalOpenRequest& a_request,
			uint32_t* a_nativeError) noexcept
		{
			++s_externalOpenCalls;
			s_externalRequest = a_request;
			if (a_nativeError)
				*a_nativeError = s_externalNativeError;
			return s_externalResult;
		}

		DMUI_Result DMUI_CALL MockRegisterClient(
			const DMUI_ClientDescriptor*,
			DMUI_ClientHandle*) noexcept
		{
			++s_mockRegistrations;
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL MockRegisterPage(
			DMUI_ClientHandle,
			const DMUI_PageDescriptor*,
			DMUI_PageHandle*) noexcept
		{
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL MockRegisterCategory(
			DMUI_ClientHandle,
			const DMUI_CategoryDescriptor*) noexcept
		{
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL MockQueryServices(
			DMUI_HostServicesInfo* a_services) noexcept
		{
			a_services->supportedServices = s_mockServices;
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL MockQueryUIAPI(
			uint32_t a_requestedUIAbi,
			uint32_t a_minimumRevision,
			uint32_t a_minimumTableSize,
			DMUI_UIAPIInfo* a_info) noexcept
		{
			static DMUI_UIAPI ui = DearModdingUI::UI::API();
			ui = DearModdingUI::UI::API();
			ui.structSize = s_mockUITableSize;
			ui.abiVersion = DMUI_UI_ABI_CURRENT;
			ui.revision = s_mockUIRevision;
			if (s_mockMissingRequiredUIOperation)
				ui.endCombo = nullptr;
			if (s_mockMissingPlotLines)
				ui.plotLines = nullptr;
			if (!a_info ||
				a_info->structSize < DMUI_UI_API_INFO_1_SIZE)
				return DMUI_RESULT_STRUCT_TOO_SMALL;
			a_info->abiVersion = ui.abiVersion;
			a_info->revision = ui.revision;
			a_info->tableSize = ui.structSize;
			a_info->api = nullptr;
			if (s_mockUIResult != DMUI_RESULT_OK)
				return s_mockUIResult;
			if (a_requestedUIAbi != ui.abiVersion ||
				a_minimumRevision > ui.revision ||
				a_minimumTableSize > ui.structSize)
				return DMUI_RESULT_UNSUPPORTED_ABI;
			a_info->api = &ui;
			return DMUI_RESULT_OK;
		}

		DMUI_HostAPI PreflightHostAPI() noexcept
		{
			DMUI_HostAPI api{};
			api.structSize = sizeof(api);
			api.hostAbiVersion = DMUI_HOST_ABI_CURRENT;
			api.apiVersion = DMUI_API_VERSION_CURRENT;
			api.registerClient = &MockRegisterClient;
			api.queryUIAPI = &MockQueryUIAPI;
			return api;
		}

		DMUI_Result DMUI_CALL MockOpenExternal(
			DMUI_ClientHandle,
			const DMUI_ExternalOpenDescriptor*,
			uint32_t*) noexcept
		{
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL MockCreateImage(
			DMUI_ClientHandle,
			const DMUI_ImageDescriptor*,
			DMUI_ImageHandle*) noexcept
		{
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL MockUpdateImage(
			DMUI_ClientHandle,
			DMUI_ImageHandle,
			const DMUI_ImageDescriptor*) noexcept
		{
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL MockDrawImage(
			DMUI_ClientHandle,
			DMUI_ImageHandle,
			const DMUI_ImageDrawOptions*) noexcept
		{
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL MockReleaseImage(
			DMUI_ClientHandle,
			DMUI_ImageHandle) noexcept
		{
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL MockQueryImage(
			DMUI_ClientHandle,
			DMUI_ImageHandle,
			DMUI_ImageInfo*) noexcept
		{
			return DMUI_RESULT_OK;
		}

		class SilentHealthReporter final : public HealthReporter
		{
		public:
			void Report(
				HealthEvent,
				const HealthSnapshot&) noexcept override
			{}
		};

		[[nodiscard]] bool SameColor(
			const ImVec4& a_left,
			const ImVec4& a_right) noexcept
		{
			return a_left.x == a_right.x &&
				a_left.y == a_right.y &&
				a_left.z == a_right.z &&
				a_left.w == a_right.w;
		}

		[[nodiscard]] std::string Sha256(const std::filesystem::path& a_path)
		{
			std::ifstream stream{ a_path, std::ios::binary };
			if (!stream)
				throw std::runtime_error("could not open file for SHA-256");
			const std::vector<unsigned char> bytes{
				std::istreambuf_iterator<char>{ stream },
				std::istreambuf_iterator<char>{}
			};

			BCRYPT_ALG_HANDLE algorithm{};
			BCRYPT_HASH_HANDLE hash{};
			DWORD objectSize{};
			DWORD hashSize{};
			DWORD resultSize{};
			if (BCryptOpenAlgorithmProvider(
					&algorithm,
					BCRYPT_SHA256_ALGORITHM,
					nullptr,
					0) < 0 ||
				BCryptGetProperty(
					algorithm,
					BCRYPT_OBJECT_LENGTH,
					reinterpret_cast<PUCHAR>(&objectSize),
					sizeof(objectSize),
					&resultSize,
					0) < 0 ||
				BCryptGetProperty(
					algorithm,
					BCRYPT_HASH_LENGTH,
					reinterpret_cast<PUCHAR>(&hashSize),
					sizeof(hashSize),
					&resultSize,
					0) < 0)
			{
				if (algorithm)
					BCryptCloseAlgorithmProvider(algorithm, 0);
				throw std::runtime_error("could not initialize SHA-256");
			}

			std::vector<unsigned char> object(objectSize);
			std::vector<unsigned char> digest(hashSize);
			const auto created = BCryptCreateHash(
				algorithm,
				&hash,
				object.data(),
				objectSize,
				nullptr,
				0,
				0);
			const auto hashed = created >= 0 ?
				BCryptHashData(
					hash,
					const_cast<PUCHAR>(bytes.data()),
					static_cast<ULONG>(bytes.size()),
					0) :
				created;
			const auto finished = hashed >= 0 ?
				BCryptFinishHash(hash, digest.data(), hashSize, 0) :
				hashed;
			if (hash)
				BCryptDestroyHash(hash);
			BCryptCloseAlgorithmProvider(algorithm, 0);
			if (finished < 0)
				throw std::runtime_error("could not calculate SHA-256");

			std::ostringstream result;
			result << std::hex << std::setfill('0');
			for (const auto byte : digest)
				result << std::setw(2) << static_cast<unsigned>(byte);
			return result.str();
		}

		void DMUI_CALL Ready(const DMUI_HostReadyInfo* a_info, void* a_userData) noexcept
		{
			auto& state = *static_cast<CallbackState*>(a_userData);
			++state.ready;
			(void)a_info;
		}

		void DMUI_CALL Unavailable(
			DMUI_UnavailableReason a_reason,
			void* a_userData) noexcept
		{
			auto& state = *static_cast<CallbackState*>(a_userData);
			++state.unavailable;
			state.reason = a_reason;
		}

		void DMUI_CALL Draw(void* a_userData) noexcept
		{
			++static_cast<CallbackState*>(a_userData)->draws;
		}

		DMUI_Result DMUI_CALL DrawPage(void* a_userData) noexcept
		{
			Draw(a_userData);
			return DMUI_RESULT_OK;
		}

		void DMUI_CALL ObservePageActivity(
			const DMUI_PageActivityInfo* a_info,
			void* a_userData)
		{
			static_cast<PageActivityState*>(a_userData)->events.push_back(*a_info);
		}

		void DMUI_CALL ThrowReady(const DMUI_HostReadyInfo*, void*)
		{
			throw std::runtime_error("ready");
		}

		void DMUI_CALL ThrowUnavailable(DMUI_UnavailableReason, void*)
		{
			throw std::runtime_error("unavailable");
		}

		void DMUI_CALL ThrowDraw(void*)
		{
			throw std::runtime_error("draw");
		}

		DMUI_Result DMUI_CALL ThrowDrawPage(void*)
		{
			throw std::runtime_error("draw");
		}

		DMUI_Result DMUI_CALL UnsupportedDrawPage(void*) noexcept
		{
			return DMUI_RESULT_UNSUPPORTED_ABI;
		}

		[[nodiscard]] DMUI_ClientDescriptor Client(
			const char* a_id,
			const char* a_name,
			CallbackState& a_state) noexcept
		{
			return {
				sizeof(DMUI_ClientDescriptor),
				DMUI_API_VERSION_CURRENT,
				a_id,
				a_name,
				DMUI_MAKE_VERSION(1, 0),
				&Ready,
				&Unavailable,
				&a_state,
				DMUI_CLIENT_CAPABILITY_NONE,
				nullptr
			};
		}

		[[nodiscard]] DMUI_PageDescriptor Page(
			const char* a_id,
			const char* a_name,
			const char* a_category,
			int32_t a_sort,
			DMUI_PageKind a_kind,
			CallbackState& a_state,
			const char* a_iconName = nullptr) noexcept
		{
			return {
				sizeof(DMUI_PageDescriptor),
				a_id,
				a_name,
				a_category,
				nullptr,
				a_sort,
				a_kind,
				&DrawPage,
				&a_state,
				a_iconName
			};
		}

		[[nodiscard]] DMUI_ActionDescriptor Action(
			const char* a_id,
			const char* a_label,
			const char* a_icon,
			int32_t a_sort,
			CallbackState& a_state) noexcept
		{
			return {
				sizeof(DMUI_ActionDescriptor),
				a_id,
				a_label,
				a_icon,
				nullptr,
				a_sort,
				&Draw,
				&a_state
			};
		}

		[[nodiscard]] DMUI_FrameObserverDescriptor FrameObserver(
			CallbackState& a_state) noexcept
		{
			return {
				sizeof(DMUI_FrameObserverDescriptor),
				&Draw,
				&a_state
			};
		}

		[[nodiscard]] DMUI_ClientHandle AddClient(
			Registry& a_registry,
			const char* a_id,
			const char* a_name,
			CallbackState& a_state,
			DMUI_ClientOrigin a_origin = DMUI_CLIENT_ORIGIN_NATIVE,
			const char* a_bridgeSourceLabel = nullptr)
		{
			auto descriptor = Client(a_id, a_name, a_state);
			descriptor.origin = a_origin;
			descriptor.bridgeSourceLabel = a_bridgeSourceLabel;
			DMUI_ClientHandle handle{};
			require(a_registry.RegisterClient(&descriptor, &handle) == DMUI_RESULT_OK,
				"client registration failed");
			return handle;
		}

		[[nodiscard]] DMUI_PageHandle AddPage(
			Registry& a_registry,
			DMUI_ClientHandle a_client,
			const char* a_id,
			const char* a_name,
			const char* a_category,
			int32_t a_sort,
			DMUI_PageKind a_kind,
			CallbackState& a_state,
			const char* a_iconName = nullptr)
		{
			auto descriptor = Page(
				a_id,
				a_name,
				a_category,
				a_sort,
				a_kind,
				a_state,
				a_iconName);
			DMUI_PageHandle handle{};
			require(a_registry.RegisterPage(a_client, &descriptor, &handle) == DMUI_RESULT_OK,
				"page registration failed");
			return handle;
		}

		void AddCategory(
			Registry& a_registry,
			DMUI_ClientHandle a_client,
			const char* a_id,
			const char* a_displayName,
			int32_t a_sortKey = 0,
			const char* a_iconName = nullptr)
		{
			const DMUI_CategoryDescriptor descriptor{
				sizeof(DMUI_CategoryDescriptor),
				a_id,
				a_displayName,
				a_sortKey,
				0,
				a_iconName
			};
			require(
				a_registry.RegisterCategory(a_client, &descriptor) ==
					DMUI_RESULT_OK,
				"category registration failed");
		}

		[[nodiscard]] DMUI_ActionHandle AddAction(
			Registry& a_registry,
			DMUI_ClientHandle a_client,
			const char* a_id,
			const char* a_label,
			const char* a_icon,
			int32_t a_sort,
			CallbackState& a_state)
		{
			auto descriptor = Action(a_id, a_label, a_icon, a_sort, a_state);
			DMUI_ActionHandle handle{};
			require(a_registry.RegisterAction(a_client, &descriptor, &handle) ==
					DMUI_RESULT_OK,
				"action registration failed");
			return handle;
		}
	}

	void run_dear_modding_ui_checks(Runner& runner)
	{
		runner.test("host API extensions preserve the published prefix", [] {
			require(
				offsetof(DMUI_HostAPI, beginSettingsRowEx) ==
						DMUI_HOST_API_END_SETTINGS_TABLE_SIZE &&
					DMUI_HOST_API_BEGIN_SETTINGS_ROW_EX_SIZE <
						DMUI_HOST_API_REGISTER_PAGE_ACTIVITY_OBSERVER_SIZE &&
					DMUI_HOST_API_REGISTER_PAGE_ACTIVITY_OBSERVER_SIZE <
						DMUI_HOST_API_DRAW_LINK_ROW_SIZE &&
					DMUI_HOST_API_DRAW_LINK_ROW_SIZE <
						DMUI_HOST_API_DRAW_FAQ_SIZE &&
					DMUI_HOST_API_DRAW_FAQ_SIZE <
						DMUI_HOST_API_REPORT_DIAGNOSTIC_SIZE &&
					DMUI_HOST_API_REPORT_DIAGNOSTIC_SIZE ==
						offsetof(DMUI_HostAPI, queryServices) &&
					DMUI_HOST_API_QUERY_SERVICES_SIZE <
						DMUI_HOST_API_SET_HOTKEY_ACTION_ENABLED_SIZE &&
					DMUI_HOST_API_SET_HOTKEY_ACTION_ENABLED_SIZE <
						DMUI_HOST_API_IMPORT_D3D11_IMAGE_SIZE &&
					DMUI_HOST_API_IMPORT_D3D11_IMAGE_SIZE <
						DMUI_HOST_API_DRAW_IMAGE_SIZE &&
					DMUI_HOST_API_DRAW_IMAGE_SIZE <
						DMUI_HOST_API_RELEASE_IMAGE_SIZE &&
					DMUI_HOST_API_RELEASE_IMAGE_SIZE <
						DMUI_HOST_API_QUERY_IMAGE_SIZE &&
					DMUI_HOST_API_QUERY_IMAGE_SIZE <
						DMUI_HOST_API_CONFIGURE_OVERLAY_SIZE &&
					DMUI_HOST_API_CONFIGURE_OVERLAY_SIZE <
						DMUI_HOST_API_QUERY_OVERLAY_SIZE &&
					DMUI_HOST_API_QUERY_OVERLAY_SIZE <
						DMUI_HOST_API_POST_NOTIFICATION_SIZE &&
					DMUI_HOST_API_POST_NOTIFICATION_SIZE <
						DMUI_HOST_API_DRAW_ANNOTATED_PLOT_SIZE &&
					DMUI_HOST_API_DRAW_ANNOTATED_PLOT_SIZE <
						DMUI_HOST_API_REQUEST_DIALOG_SIZE &&
					DMUI_HOST_API_REQUEST_DIALOG_SIZE <
						DMUI_HOST_API_POLL_DIALOG_EVENT_SIZE &&
					DMUI_HOST_API_POLL_DIALOG_EVENT_SIZE <
						DMUI_HOST_API_RESOLVE_DIALOG_SUBMISSION_SIZE &&
					DMUI_HOST_API_RESOLVE_DIALOG_SUBMISSION_SIZE <
						DMUI_HOST_API_CANCEL_DIALOG_SIZE &&
					DMUI_HOST_API_CANCEL_DIALOG_SIZE == 400 &&
					DMUI_HOST_API_CANCEL_DIALOG_SIZE <
						DMUI_HOST_API_CREATE_IMAGE_SIZE &&
					DMUI_HOST_API_CREATE_IMAGE_SIZE <
						DMUI_HOST_API_UPDATE_IMAGE_SIZE &&
					DMUI_HOST_API_UPDATE_IMAGE_SIZE <
						DMUI_HOST_API_REGISTER_CATEGORY_SIZE &&
					DMUI_HOST_API_REGISTER_CATEGORY_SIZE <
						DMUI_HOST_API_OPEN_EXTERNAL_SIZE &&
					DMUI_HOST_API_OPEN_EXTERNAL_SIZE <
						DMUI_HOST_API_QUERY_UI_API_SIZE &&
					sizeof(DMUI_HostAPI) ==
						DMUI_HOST_API_QUERY_UI_API_SIZE,
				"the versioned host API prefix moved");
		});

		runner.test("page activity reports client boundaries without false closes", [] {
			Registry registry;
			CallbackState firstState;
			CallbackState secondState;
			const auto firstClient = AddClient(
				registry,
				"first.mod",
				"First",
				firstState);
			const auto secondClient = AddClient(
				registry,
				"second.mod",
				"Second",
				secondState);
			AddCategory(registry, firstClient, "general", "General");
			AddCategory(registry, secondClient, "general", "General");
			const auto firstPage = AddPage(
				registry,
				firstClient,
				"first",
				"First",
				"general",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				firstState);
			const auto nextPage = AddPage(
				registry,
				firstClient,
				"next",
				"Next",
				"general",
				1,
				DMUI_PAGE_KIND_SETTINGS,
				firstState);
			const auto secondPage = AddPage(
				registry,
				secondClient,
				"second",
				"Second",
				"general",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				secondState);
			PageActivityState firstActivity;
			PageActivityState secondActivity;
			const DMUI_PageActivityObserverDescriptor firstObserver{
				sizeof(DMUI_PageActivityObserverDescriptor),
				&ObservePageActivity,
				&firstActivity
			};
			const DMUI_PageActivityObserverDescriptor secondObserver{
				sizeof(DMUI_PageActivityObserverDescriptor),
				&ObservePageActivity,
				&secondActivity
			};
			DMUI_PageActivityObserverHandle observer{};
			require(
				registry.RegisterPageActivityObserver(
					firstClient,
					&firstObserver,
					&observer) == DMUI_RESULT_OK &&
					registry.RegisterPageActivityObserver(
						secondClient,
						&secondObserver,
						&observer) == DMUI_RESULT_OK &&
					registry.Freeze(),
				"page activity observers did not register");

			registry.NotifyPageActivity(DMUI_INVALID_PAGE_HANDLE, firstPage);
			registry.NotifyPageActivity(firstPage, nextPage);
			registry.NotifyPageActivity(nextPage, secondPage);
			registry.NotifyPageActivity(secondPage, DMUI_INVALID_PAGE_HANDLE);

			require(
				firstActivity.events.size() == 3 &&
					firstActivity.events[0].kind ==
						DMUI_PAGE_ACTIVITY_ACTIVATED &&
					firstActivity.events[0].activePage == firstPage &&
					firstActivity.events[1].kind ==
						DMUI_PAGE_ACTIVITY_CHANGED &&
					firstActivity.events[1].previousPage == firstPage &&
					firstActivity.events[1].activePage == nextPage &&
					firstActivity.events[2].kind ==
						DMUI_PAGE_ACTIVITY_DEACTIVATED &&
					firstActivity.events[2].previousPage == nextPage &&
					secondActivity.events.size() == 2 &&
					secondActivity.events[0].kind ==
						DMUI_PAGE_ACTIVITY_ACTIVATED &&
					secondActivity.events[0].activePage == secondPage &&
					secondActivity.events[1].kind ==
						DMUI_PAGE_ACTIVITY_DEACTIVATED,
				"page activity invented a close/open pair within one client");
		});

		runner.test("settings brackets reject mismatched transitions", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			constexpr DMUI_ClientHandle other{ 8 };
			require(std::string_view{
						DMUI_ResultToString(DMUI_RESULT_UNBALANCED_BRACKET) } ==
					"UNBALANCED_BRACKET",
				"bracket error string was not published");
			SettingsTable::BracketState state;
			require(state.BeginRow(owner) == DMUI_RESULT_UNBALANCED_BRACKET,
				"row began without a table");
			require(state.EndTable(owner) == DMUI_RESULT_UNBALANCED_BRACKET,
				"idle table ended");
			require(state.BeginTable(owner) == DMUI_RESULT_OK,
				"table did not begin");
			require(state.BeginTable(owner) == DMUI_RESULT_UNBALANCED_BRACKET,
				"settings table nested");
			require(state.BeginRow(other) == DMUI_RESULT_UNBALANCED_BRACKET,
				"another owner began a row");
			require(state.BeginRow(owner) == DMUI_RESULT_OK,
				"row did not begin");
			require(state.EndTable(owner) == DMUI_RESULT_UNBALANCED_BRACKET,
				"table ended with an open row");
			require(state.EndRow(other) == DMUI_RESULT_UNBALANCED_BRACKET,
				"another owner ended a row");
			require(state.EndRow(owner) == DMUI_RESULT_OK,
				"row did not end");
			require(state.EndTable(owner) == DMUI_RESULT_OK,
				"table did not end");
			require(state.CurrentPhase() == SettingsTable::Phase::kIdle,
				"balanced table did not return to idle");
			require(state.BeginTable(owner) == DMUI_RESULT_OK &&
					state.BeginRow(owner) == DMUI_RESULT_OK,
				"state could not be reused");
			state.Reset();
			require(state.CurrentPhase() == SettingsTable::Phase::kIdle &&
					state.Owner() == DMUI_INVALID_CLIENT_HANDLE,
				"forced reset retained bracket state");
		});

		runner.test("settings row options enforce their versioned prefix", [] {
			require(SettingsTable::ValidateRowOptions(nullptr) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null row options were accepted");
			DMUI_SettingsRowOptions options{};
			options.structSize = DMUI_SETTINGS_ROW_OPTIONS_0_1_SIZE - 1;
			require(SettingsTable::ValidateRowOptions(&options) ==
					DMUI_RESULT_STRUCT_TOO_SMALL,
				"short row options were accepted");
			options.structSize = DMUI_SETTINGS_ROW_OPTIONS_0_1_SIZE;
			require(SettingsTable::ValidateRowOptions(&options) ==
					DMUI_RESULT_OK,
				"exact row options were rejected");
			options.structSize += sizeof(uint32_t);
			require(SettingsTable::ValidateRowOptions(&options) ==
					DMUI_RESULT_OK,
				"extended row options were rejected");
			DMUI_SettingsRowBeginOptions beginOptions{};
			beginOptions.structSize =
				DMUI_SETTINGS_ROW_BEGIN_OPTIONS_0_1_SIZE - 1;
			require(SettingsTable::ValidateRowBeginOptions(&beginOptions) ==
					DMUI_RESULT_STRUCT_TOO_SMALL,
				"short row begin options were accepted");
			beginOptions.structSize =
				DMUI_SETTINGS_ROW_BEGIN_OPTIONS_0_1_SIZE;
			beginOptions.layout = DMUI_SETTINGS_ROW_LAYOUT_FULL_SPAN;
			require(SettingsTable::ValidateRowBeginOptions(&beginOptions) ==
					DMUI_RESULT_OK,
				"full-span row begin options were rejected");
			beginOptions.layout = 2;
			require(SettingsTable::ValidateRowBeginOptions(&beginOptions) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"unknown row layout was accepted");
		});

		runner.test("declarative setting filters match metadata without reading values", [] {
			const dmui::SettingDescriptor setting{
				.id = "bHighResolution",
				.label = "High Resolution",
				.description = "Increases texture detail for distant objects.",
				.control = dmui::CheckboxSettingControl{},
				.defaultValue = false
			};
			require(
				dmui::MatchesSettingFilter(
					setting,
					"High Resolution",
					false,
					{ "HIGH RES", false }) &&
					dmui::MatchesSettingFilter(
						setting,
						"High Resolution",
						false,
						{ "bhigh", false }) &&
					dmui::MatchesSettingFilter(
						setting,
						"High Resolution",
						false,
						{ "DISTANT OBJECTS", false }),
				"case-insensitive metadata filtering lost a match");
			require(
				!dmui::MatchesSettingFilter(
					setting,
					"High Resolution",
					false,
					{ "shadows", false }) &&
					!dmui::MatchesSettingFilter(
						setting,
						"High Resolution",
						false,
						{ "", true }) &&
					dmui::MatchesSettingFilter(
						setting,
						"High Resolution",
						true,
						{ "", true }),
				"modified-only or negative filtering changed");

			const dmui::SettingGroup group{
				.id = "questions",
				.label = "Questions",
				.settings = {
					{ .id = "first", .label = "First" },
					{ .id = "second", .label = "Second" }
				},
				.rows = {
					dmui::SettingGroup::SettingIndex{ 0 },
					dmui::SettingGroup::DividerRow{},
					dmui::SettingGroup::SettingIndex{ 1 }
				}
			};
			const auto all = dmui::setting_detail::MatchingRows(group, {});
			const auto filtered =
				dmui::setting_detail::MatchingRows(group, { "second" });
			require(
				all.size() == 3 &&
					dmui::setting_detail::MatchingContentCount(all) == 2 &&
					filtered.size() == 1 &&
					dmui::setting_detail::MatchingContentCount(filtered) == 1,
				"divider rows changed heading counts or survived lone filtering");
		});

		runner.test("declarative pending count uses dirty state without value getters", [] {
			auto firstDirty = false;
			auto secondDirty = true;
			auto getterCalls = 0u;
			dmui::SettingsPage page{
				.groups = {
					{
						.id = "general",
						.label = "General",
						.settings = {
							{
								.id = "first",
								.label = "First",
								.control = dmui::CheckboxSettingControl{},
								.defaultValue = false,
								.binding = {
									.get = [&] {
										++getterCalls;
										return dmui::SettingValue{ false };
									}
								},
								.isDirty = [&] { return firstDirty; }
							},
							{
								.id = "second",
								.label = "Second",
								.control = dmui::CheckboxSettingControl{},
								.defaultValue = false,
								.binding = {
									.get = [&] {
										++getterCalls;
										return dmui::SettingValue{ false };
									}
								},
								.isDirty = [&] { return secondDirty; }
							}
						}
					}
				}
			};
			require(
				page.PendingCount() == 1 &&
					page.IsDirty() &&
					getterCalls == 0,
				"pending count read a bound value or lost dirty state");
			firstDirty = true;
			secondDirty = false;
			require(page.PendingCount() == 1 && getterCalls == 0,
				"pending count did not follow dynamic dirty state");
		});

		runner.test("declarative numeric controls select widgets and normalize values", [] {
			const dmui::DoubleSettingControl input;
			const dmui::DoubleSettingControl drag{
				.range = dmui::NumericSettingRange<double>{
					.minimum = 0.0
				}
			};
			const dmui::DoubleSettingControl slider{
				.range = dmui::NumericSettingRange<double>{
					.minimum = 0.0,
					.maximum = 1.0
				},
				.format = "%.2f"
			};
			require(
				dmui::ResolveNumericSettingWidget(input) ==
						dmui::NumericSettingWidget::kInput &&
					dmui::ResolveNumericSettingWidget(drag) ==
						dmui::NumericSettingWidget::kDrag &&
					dmui::ResolveNumericSettingWidget(slider) ==
						dmui::NumericSettingWidget::kSlider,
				"numeric range shape selected the wrong widget");
			require(
				dmui::ClampSettingNumber(
					std::numeric_limits<double>::quiet_NaN(),
					0.25,
					slider.range) == 0.25 &&
					dmui::ClampSettingNumber(2.0, 0.25, slider.range) == 1.0,
				"double recovery or clamping changed");
			require(
				dmui::ClampSettingNumber(
					int64_t{ -50 },
					int64_t{ 5 },
					std::optional{
						dmui::NumericSettingRange<int64_t>{
							.minimum = int64_t{ -10 },
							.maximum = int64_t{ 10 } } }) == -10 &&
					dmui::ClampSettingNumber(
						uint64_t{ 500 },
						uint64_t{ 5 },
						std::optional{
							dmui::NumericSettingRange<uint64_t>{
								.minimum = uint64_t{ 100 },
								.maximum = uint64_t{ 20 } } }) == 100,
				"signed, unsigned, or inverted bounds were not normalized");
			require(
				std::abs(
					dmui::QuantizeSettingNumber(
						0.61,
						std::optional{
							dmui::NumericQuantization<double>{ 0.2, 0.1 } }) -
					0.7) < 1.0e-12 &&
					dmui::QuantizeSettingNumber(
						int64_t{ -4 },
						std::optional{
							dmui::NumericQuantization<int64_t>{ 3, -10 } }) ==
						-4 &&
					dmui::QuantizeSettingNumber(
						uint64_t{ 18 },
						std::optional{
							dmui::NumericQuantization<uint64_t>{ 5, 3 } }) ==
						18,
				"numeric quantization stopped using its explicit origin");
		});

		runner.test("declarative defaults reset through accepted value bindings", [] {
			int64_t draft = 18;
			size_t setterCalls{};
			std::vector<dmui::SettingEditEvent> editEvents;
			auto setting = dmui::SettingDescriptor{
				.id = "threads",
				.label = "Worker threads",
				.control = dmui::SignedSettingControl{},
				.defaultValue = int64_t{ 8 },
				.binding = dmui::BindSetting(
					[&]() -> int64_t { return draft; },
					[&](int64_t a_value) -> int64_t {
						++setterCalls;
						draft = (std::min)(a_value, int64_t{ 16 });
						return draft;
					}),
				.onEdit = [&](const dmui::SettingEditEvent& a_event) {
					editEvents.push_back(a_event);
				}
			};
			static_assert(!std::is_nothrow_invocable_v<
				decltype(setting.binding.get)&>);
			static_assert(!std::is_nothrow_invocable_v<
				decltype(setting.binding.set)&,
				dmui::SettingValue>);
			require(
				!dmui::IsSettingDefault(
					setting,
					dmui::SettingValue{ draft }),
				"modified value was treated as its default");
			const auto reset = dmui::ResetSettingToDefault(setting);
			require(
				reset &&
					std::get<int64_t>(*reset) == 8 &&
					draft == 8 &&
					setterCalls == 1 &&
					editEvents.size() == 1 &&
					editEvents.front().changed &&
					editEvents.front().completed &&
					std::get<int64_t>(editEvents.front().value) == 8 &&
					dmui::IsSettingDefault(
						setting,
						dmui::SettingValue{ draft }),
				"reset did not emit one completed effective edit");
			require(
				dmui::ResetSettingToDefault(setting) &&
					setterCalls == 1 &&
					editEvents.size() == 1,
				"no-op reset wrote or emitted another completion");
			const auto accepted =
				setting.binding.set(dmui::SettingValue{ int64_t{ 99 } });
			require(
				std::get<int64_t>(accepted) == 16 &&
					draft == 16 &&
					setterCalls == 2 &&
					editEvents.size() == 1,
				"setter did not return the accepted clamped value");
			setting.isEnabled = [] { return false; };
			require(
				!dmui::ResetSettingToDefault(setting) &&
					draft == 16 &&
					setterCalls == 2 &&
					editEvents.size() == 1,
				"disabled reset wrote or emitted completion");
		});

		runner.test("unknown declarative controls resolve to a disabled fallback", [] {
			auto setterCalls = 0u;
			const dmui::SettingDescriptor setting{
				.id = "future",
				.label = "Future control",
				.control = dmui::UnsupportedSettingControl{ 0xFFFFu },
				.defaultValue = false,
				.binding = {
					.set = [&](dmui::SettingValue a_value) {
						++setterCalls;
						return a_value;
					}
				}
			};
			const auto presentation =
				dmui::ResolveSettingControlPresentation(setting.control);
			require(
				presentation.kind ==
						dmui::SettingControlKind::kUnsupported &&
					!presentation.supported &&
					!presentation.editable &&
					!presentation.resetVisible &&
					!dmui::SettingValueMatchesControl(
						setting.control,
						setting.defaultValue),
				"unknown kind did not select the noninteractive fallback");
			require(
				!dmui::ResetSettingToDefault(setting) &&
					setterCalls == 0,
				"unknown kind invoked an editable binding");
		});

		runner.test("status severity controls expiry and persistence", [] {
			const auto start = StatusClock::time_point{};
			StatusModel model;
			require(model.Set(
						StatusOwnerKind::kHost,
						"Evil Modding",
						DMUI_STATUS_SEVERITY_INFO,
						"Working",
						start) == DMUI_RESULT_OK,
				"info status was rejected");
			require(model.Snapshot(
						start +
						kTransientStatusLifetime -
						std::chrono::milliseconds{ 1 }).has_value(),
				"info status expired early");
			require(!model.Snapshot(start + kTransientStatusLifetime),
				"info status did not expire");

			require(model.Set(
						StatusOwnerKind::kHost,
						"Evil Modding",
						DMUI_STATUS_SEVERITY_SUCCESS,
						"Saved",
						start) == DMUI_RESULT_OK,
				"success status was rejected");
			require(!model.Snapshot(start + kTransientStatusLifetime),
				"success status did not expire");

			require(model.Set(
						StatusOwnerKind::kHost,
						"Evil Modding",
						DMUI_STATUS_SEVERITY_WARNING,
						"Warning",
						start) == DMUI_RESULT_OK,
				"warning status was rejected");
			require(model.Snapshot(start + std::chrono::hours{ 24 }).has_value(),
				"warning status expired");

			require(model.Set(
						StatusOwnerKind::kHost,
						"Evil Modding",
						DMUI_STATUS_SEVERITY_ERROR,
						"Error",
						start) == DMUI_RESULT_OK,
				"error status was rejected");
			require(model.Snapshot(start + std::chrono::hours{ 24 }).has_value(),
				"error status expired");
		});

		runner.test("persistent status can be dismissed without clearing a replacement", [] {
			const auto start = StatusClock::time_point{};
			StatusModel model;
			require(model.Set(
						StatusOwnerKind::kClient,
						"Client",
						DMUI_STATUS_SEVERITY_ERROR,
						"Persistent",
						start) == DMUI_RESULT_OK,
				"persistent status was rejected");
			const auto persistent = model.Snapshot(start);
			require(persistent && model.Dismiss(persistent->generation),
				"persistent status was not dismissed");
			require(!model.Snapshot(start), "dismissed status remained visible");

			require(model.Set(
						StatusOwnerKind::kClient,
						"Client",
						DMUI_STATUS_SEVERITY_WARNING,
						"Older",
						start) == DMUI_RESULT_OK,
				"older persistent status was rejected");
			const auto older = model.Snapshot(start);
			require(model.Set(
						StatusOwnerKind::kClient,
						"Replacement owner",
						DMUI_STATUS_SEVERITY_ERROR,
						"Replacement",
						start + std::chrono::milliseconds{ 1 }) == DMUI_RESULT_OK,
				"replacement status was rejected");
			const auto replacement =
				model.Snapshot(start + std::chrono::milliseconds{ 1 });
			require(older && !model.Dismiss(older->generation),
				"stale dismissal cleared a replacement");
			require(
					replacement &&
						replacement->generation > older->generation &&
						replacement->owner == "Replacement owner" &&
						replacement->message == "Replacement",
				"newest status did not supersede an older persistent status");
			require(
				model.Snapshot(start + std::chrono::milliseconds{ 1 })
						->message == "Replacement",
				"replacement was lost after stale dismissal");
		});

		runner.test("status truncation preserves text and UTF-8 boundaries", [] {
			const auto measure = [](std::string_view a_text) {
				return static_cast<float>(a_text.size());
			};
			const auto full = std::string{ "Community Shaders: A detailed failure message" };
			const auto truncated = FitStatusText(full, 24.0f, measure);
			require(
					truncated.truncated &&
						truncated.full == full &&
						truncated.visible != full &&
						truncated.visible.ends_with("\xE2\x80\xA6") &&
						measure(truncated.visible) <= 24.0f,
					"truncated status lost its full tooltip text");
			const auto fitting = FitStatusText(full, measure(full), measure);
			require(
					!fitting.truncated &&
						fitting.visible == full &&
						fitting.full == full,
					"fitting status was truncated");

			const std::string utf8{
				"Buffout \xF0\x9F\xA7\xAA status"
			};
			const auto utf8Truncated = FitStatusText(utf8, 11.0f, measure);
			require(
					utf8Truncated.truncated &&
						utf8Truncated.visible ==
							"Buffout \xE2\x80\xA6" &&
						utf8Truncated.full == utf8,
				"status truncation split a UTF-8 character");

			const auto clipped =
				FitStatusText("Buffout 4: Error", 0.0f, measure);
			require(
					clipped.truncated &&
						clipped.visible == "\xE2\x80\xA6" &&
						clipped.full == "Buffout 4: Error",
				"fully clipped status lost its overflow presentation");
		});

		runner.test("status validation rejects invalid clients and messages", [] {
			Registry registry;
			CallbackState state;
			const auto client = AddClient(
				registry, "status.mod", "Status Mod", state);
			std::string owner;
			require(
					ValidateStatusRequest(
						registry.CopyClientDisplayName(9999, owner),
						DMUI_STATUS_SEVERITY_INFO,
						"Message") == DMUI_RESULT_CLIENT_NOT_FOUND,
					"unaccepted status client was not rejected");
			const auto accepted = registry.CopyClientDisplayName(client, owner);
			require(
					accepted == DMUI_RESULT_OK && owner == "Status Mod",
					"accepted status owner was not copied");
			require(
					ValidateStatusRequest(
						accepted,
						DMUI_STATUS_SEVERITY_INFO,
						nullptr) == DMUI_RESULT_INVALID_ARGUMENT,
					"null status message was not rejected");
			require(
					ValidateStatusRequest(
						accepted,
						DMUI_STATUS_SEVERITY_INFO,
						"") == DMUI_RESULT_INVALID_ARGUMENT,
					"empty status message was not rejected");
			require(
					ValidateStatusRequest(
						accepted,
						99,
						"Message") == DMUI_RESULT_INVALID_ARGUMENT,
					"unknown status severity was not rejected");
		});

		runner.test("footer controls clamp, separate, and scale", [] {
			const auto narrow = ResolveFooterControlsLayout(
				20.0f,
				100.0f,
				60.0f,
				50.0f,
				8.0f);
			require(
					narrow.runMaxX == 20.0f &&
						narrow.dismissMinX >= 20.0f &&
						narrow.dismissMaxX + 8.0f <=
							narrow.settingsMinX &&
						narrow.settingsMaxX <= 100.0f,
					"narrow footer controls escaped their bounds");

			const auto base = ResolveFooterControlsLayout(
				0.0f,
				600.0f,
				40.0f,
				28.0f,
				8.0f);
			const auto scaled = ResolveFooterControlsLayout(
				0.0f,
				1200.0f,
				80.0f,
				56.0f,
				16.0f);
			require(
					scaled ==
						FooterControlsLayout{
							base.runMaxX * 2.0f,
							base.dismissMinX * 2.0f,
							base.dismissMaxX * 2.0f,
							base.settingsMinX * 2.0f,
							base.settingsMaxX * 2.0f
						},
					"footer controls did not follow the style scale");

			require(
				base.runMaxX <= base.dismissMinX &&
					base.dismissMaxX <= base.settingsMinX,
				"footer controls overlapped at baseline scale");
		});

		runner.test("two-pane sidebar preserves a measured page region", [] {
			require(
					ResolveSidebarPaneHeights(
						1000.0f,
						60.0f,
						2,
						240.0f) == SidebarPaneHeights{ 120.0f, 880.0f },
					"two mods consumed a proportional half of the sidebar");
			require(
					ResolveSidebarPaneHeights(
						1000.0f,
						60.0f,
						10,
						240.0f) == SidebarPaneHeights{ 600.0f, 400.0f },
					"ten mods did not retain their measured list height");
			require(
					ResolveSidebarPaneHeights(
						1000.0f,
						60.0f,
						20,
						240.0f) == SidebarPaneHeights{ 760.0f, 240.0f },
					"overflowing mods displaced the minimum page region");
			require(
					ResolveSidebarPaneHeights(
						-100.0f,
						60.0f,
						10,
						240.0f) == SidebarPaneHeights{},
					"negative space produced pane height");
			require(
					ResolveSidebarPaneHeights(
						1000.0f,
						60.0f,
						10,
						40.0f,
						3,
						240.0f) == SidebarPaneHeights{ 720.0f, 280.0f },
					"navigation headings were omitted from the mod pane budget");
			require(
					ResolveSidebarPaneHeights(
						1000.0f,
						60.0f,
						20,
						40.0f,
						3,
						240.0f) == SidebarPaneHeights{ 760.0f, 240.0f },
					"navigation headings displaced the minimum page region");
			require(
					ResolveSidebarPaneHeights(
						1000.0f,
						60.0f,
						8,
						40.0f,
						2,
						48.0f,
						240.0f) == SidebarPaneHeights{ 608.0f, 392.0f },
					"source controls were omitted from the mod pane budget");
		});

		runner.test("sidebar layout names parse and round trip", [] {
			for (const auto& layout : SIDEBAR_LAYOUTS)
			{
				require(
					ParseSidebarLayout(layout.id) == layout.kind &&
						SidebarLayoutKindName(layout.kind) == layout.id &&
						layout.preview,
					"sidebar layout name did not round trip");
			}
			require(
				!ParseSidebarLayout("columns") &&
					SidebarLayoutKindName(
						static_cast<SidebarLayoutKind>(99)) == "unknown" &&
					DEFAULT_SIDEBAR_LAYOUT == SidebarLayoutKind::Tree,
				"unknown or default sidebar layout handling changed");

			PersistedHostInterfaceSettings persisted;
			for (const auto value : { ""sv, "columns"sv, "iconrail"sv })
			{
				persisted.sidebarLayout = value;
				require(
					DecodeHostInterfaceSettings(persisted).sidebarLayout ==
						SidebarLayoutKind::Tree,
					"unavailable sidebar config did not fall back to tree");
			}
			require(
				!ParseUserSidebarLayout("iconrail"),
				"icon rail was accepted as a user setting");
		});

		runner.test("client descriptors reject null size and callback failures", [] {
			Registry registry;
			CallbackState state;
			DMUI_ClientHandle handle{};
			auto client = Client("sample.mod", "Sample", state);

			require(registry.RegisterClient(nullptr, &handle) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null descriptor was accepted");
			require(registry.RegisterClient(&client, nullptr) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null output was accepted");
			client.structSize = DMUI_CLIENT_DESCRIPTOR_0_1_SIZE - 1;
			require(registry.RegisterClient(&client, &handle) ==
					DMUI_RESULT_STRUCT_TOO_SMALL,
				"short client descriptor was accepted");
			client.structSize = sizeof(client);
			client.onHostReady = nullptr;
			require(registry.RegisterClient(&client, &handle) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"null ready callback was accepted");

			client.onHostReady = &Ready;
			require(registry.RegisterClient(&client, &handle) ==
					DMUI_RESULT_OK,
				"valid client was rejected");
			AddCategory(registry, handle, "general", "General");
			auto page = Page("settings", "Settings", "general", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			DMUI_PageHandle pageHandle{};
			require(registry.RegisterPage(handle, nullptr, &pageHandle) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null page descriptor was accepted");
			require(registry.RegisterPage(handle, &page, nullptr) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null page output was accepted");
			page.structSize = DMUI_PAGE_DESCRIPTOR_0_1_SIZE - 1;
			require(registry.RegisterPage(handle, &page, &pageHandle) ==
					DMUI_RESULT_STRUCT_TOO_SMALL,
				"short page descriptor was accepted");
			page.structSize = sizeof(page);
			page.kind = 3;
			require(registry.RegisterPage(handle, &page, &pageHandle) ==
					DMUI_RESULT_INVALID_PAGE_KIND,
				"removed page kind was accepted");
			page.kind = DMUI_PAGE_KIND_SETTINGS;
			page.draw = nullptr;
			require(registry.RegisterPage(handle, &page, &pageHandle) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"null page callback was accepted");
		});

		runner.test("client descriptors require and copy the complete 0.1 shape", [] {
			CallbackState state;

			Registry registry;
			auto shortDescriptor = Client("short.mod", "Short", state);
			shortDescriptor.structSize =
				static_cast<uint32_t>(
					offsetof(DMUI_ClientDescriptor, bridgeSourceLabel));
			DMUI_ClientHandle handle{};
			require(
				registry.RegisterClient(&shortDescriptor, &handle) ==
					DMUI_RESULT_STRUCT_TOO_SMALL,
				"a partial 0.1 client descriptor was accepted");

			char iconName[]{ "gauge" };
			auto client = Client("owned.mod", "Owned", state);
			client.iconName = iconName;
			require(registry.RegisterClient(&client, &handle) == DMUI_RESULT_OK,
				"client icon registration failed");
			iconName[0] = 'x';
			AddCategory(registry, handle, "general", "General");
			(void)AddPage(
				registry,
				handle,
				"settings",
				"Settings",
				"general",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state);
			require(registry.Freeze(), "registry with a client icon did not freeze");
			require(
				registry.Navigation().clients.size() == 1 &&
					registry.Navigation().clients.front().iconName == "gauge",
				"the client icon name was not deep-copied");
		});

		runner.test("navigation icon descriptor extensions preserve old prefixes", [] {
			Registry registry;
			CallbackState state;
			auto clientDescriptor =
				Client("icons.mod", "Icon Metadata", state);
			clientDescriptor.iconName = "cloud-sun";
			DMUI_ClientHandle client{};
			require(
				registry.RegisterClient(&clientDescriptor, &client) ==
					DMUI_RESULT_OK,
				"navigation icon client registration failed");

			DMUI_CategoryDescriptor oldCategory{
				DMUI_CATEGORY_DESCRIPTOR_0_1_SIZE,
				"old",
				"Lighting",
				0,
				0,
				"sun-horizon"
			};
			require(
				registry.RegisterCategory(client, &oldCategory) ==
					DMUI_RESULT_OK,
				"old category prefix was rejected");
			char categoryIcon[]{ "sun-horizon" };
			auto currentCategory = oldCategory;
			currentCategory.structSize = DMUI_CATEGORY_DESCRIPTOR_ICON_SIZE;
			currentCategory.id = "current";
			currentCategory.iconName = categoryIcon;
			require(
				registry.RegisterCategory(client, &currentCategory) ==
					DMUI_RESULT_OK,
				"current category icon was rejected");
			categoryIcon[0] = 'x';

			auto oldPage = Page(
				"old",
				"Lighting",
				"old",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state,
				"sun");
			oldPage.structSize = DMUI_PAGE_DESCRIPTOR_0_1_SIZE;
			DMUI_PageHandle oldPageHandle{};
			require(
				registry.RegisterPage(client, &oldPage, &oldPageHandle) ==
					DMUI_RESULT_OK,
				"old page prefix was rejected");
			char pageIcon[]{ "sliders-horizontal" };
			auto currentPage = Page(
				"current",
				"Current",
				"current",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state,
				pageIcon);
			DMUI_PageHandle currentPageHandle{};
			require(
				registry.RegisterPage(
					client,
					&currentPage,
					&currentPageHandle) == DMUI_RESULT_OK,
				"current page icon was rejected");
			pageIcon[0] = 'x';

			std::string oversized(129, 'x');
			auto invalidCategory = currentCategory;
			invalidCategory.id = "oversized";
			invalidCategory.iconName = oversized.c_str();
			require(
				registry.RegisterCategory(client, &invalidCategory) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"oversized category icon name was accepted");
			const char malformed[]{ '\x01', '\0' };
			auto invalidPage = Page(
				"malformed",
				"Malformed",
				"current",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state,
				malformed);
			DMUI_PageHandle invalidPageHandle{};
			require(
				registry.RegisterPage(
					client,
					&invalidPage,
					&invalidPageHandle) == DMUI_RESULT_INVALID_DESCRIPTOR,
				"malformed page icon name was accepted");
			require(registry.Freeze(), "navigation icon registry did not freeze");
			const auto& categories = registry.RegisteredCategories();
			require(
				categories.size() == 2 &&
					categories[0].iconName.empty() &&
					categories[1].iconName == "sun-horizon",
				"category icon prefix guards or copy lifetime changed");
			const auto& pages = registry.OrderedPages();
			const auto oldRegisteredPage = std::ranges::find(
				pages,
				"old",
				&RegisteredPage::id);
			const auto currentRegisteredPage = std::ranges::find(
				pages,
				"current",
				&RegisteredPage::id);
			require(
				pages.size() == 2 &&
					oldRegisteredPage != pages.end() &&
					oldRegisteredPage->iconName.empty() &&
					currentRegisteredPage != pages.end() &&
					currentRegisteredPage->iconName ==
						"sliders-horizontal",
				"page icon prefix guards or copy lifetime changed");
			const auto& navigation = registry.Navigation().clients.front();
			const auto navigationCategory = std::ranges::find(
				navigation.categories,
				"current",
				&NavigationCategory::id);
			require(
				navigation.iconName == "cloud-sun" &&
					navigationCategory != navigation.categories.end() &&
					navigationCategory->iconName == "sun-horizon" &&
					navigationCategory->pages.front().handle ==
						currentPageHandle &&
					navigationCategory->pages.front().iconName ==
						"sliders-horizontal",
				"navigation model dropped copied icon metadata");
		});

		runner.test("client service requirements fail before registration", [] {
			CallbackState state;
			Registry registry;
			auto descriptor =
				Client("required.mod", "Required", state);
			descriptor.requiredServices =
				DMUI_HOST_SERVICE_IMAGE_RESOURCES |
				DMUI_HOST_SERVICE_DIALOGS |
				DMUI_HOST_SERVICE_PIXEL_IMAGES;
			descriptor.apiVersion = DMUI_MAKE_VERSION(99u, 0u);
			DMUI_ClientHandle handle{};
			require(registry.RegisterClient(&descriptor, &handle) ==
						DMUI_RESULT_OK &&
					handle != DMUI_INVALID_CLIENT_HANDLE,
				"release metadata incorrectly gated client registration");

			Registry unavailable;
			auto unsupported =
				Client("unsupported.mod", "Unsupported", state);
			unsupported.requiredServices = UINT64_C(1) << 63u;
			handle = DMUI_INVALID_CLIENT_HANDLE;
			require(unavailable.RegisterClient(&unsupported, &handle) ==
						DMUI_RESULT_SERVICE_UNAVAILABLE &&
					handle == DMUI_INVALID_CLIENT_HANDLE &&
					unavailable.ClientCount() == 0,
				"unsupported service registered a partial client");

		});

		runner.test("official client preflight rejects incomplete host tables", [] {
			s_mockRegistrations = 0;
			s_mockServices = DMUI_HOST_SERVICE_IMAGE_RESOURCES;
			s_mockUIResult = DMUI_RESULT_OK;
			s_mockUIRevision = DMUI_UI_REVISION_CURRENT;
			s_mockUITableSize = DMUI_UI_API_CURRENT_SIZE;
			auto api = PreflightHostAPI();
			api.queryUIAPI = nullptr;
			const dmui::ClientOptions options{
				.requiredServices = DMUI_HOST_SERVICE_IMAGE_RESOURCES
			};

			api.hostAbiVersion = DMUI_HOST_ABI_CURRENT + 1;
			require(dmui::PreflightHostAPI(&api, options) ==
						DMUI_RESULT_UNSUPPORTED_ABI &&
					s_mockRegistrations == 0,
				"unknown host ABI generation reached client registration");
			api.hostAbiVersion = DMUI_HOST_ABI_CURRENT;
			api.apiVersion = DMUI_MAKE_VERSION(99u, 0u);
			require(dmui::PreflightHostAPI(&api, options) ==
						DMUI_RESULT_SERVICE_UNAVAILABLE &&
					s_mockRegistrations == 0,
				"release metadata incorrectly gated host ABI preflight");
			require(dmui::PreflightHostAPI(&api, options) ==
						DMUI_RESULT_SERVICE_UNAVAILABLE &&
					s_mockRegistrations == 0,
				"missing queryServices reached client registration");
			api.queryServices = &MockQueryServices;
			api.queryUIAPI = &MockQueryUIAPI;
			s_mockServices = DMUI_HOST_SERVICE_NONE;
			require(dmui::PreflightHostAPI(&api, options) ==
						DMUI_RESULT_SERVICE_UNAVAILABLE &&
					s_mockRegistrations == 0,
				"missing semantic service reached client registration");
			s_mockServices = DMUI_HOST_SERVICE_IMAGE_RESOURCES;
			require(dmui::PreflightHostAPI(&api, options) ==
						DMUI_RESULT_SERVICE_UNAVAILABLE &&
					s_mockRegistrations == 0,
				"advertised service with missing functions reached registration");
			api.importD3D11Image = [](DMUI_ClientHandle,
									 const DMUI_D3D11ImageDescriptor*,
									 DMUI_ImageHandle*) noexcept {
				return DMUI_RESULT_OK;
			};
			api.drawImage = &MockDrawImage;
			api.releaseImage = &MockReleaseImage;
			api.queryImage = &MockQueryImage;
			api.queryUIAPI = nullptr;
			require(dmui::PreflightHostAPI(&api, options) ==
						DMUI_RESULT_UNSUPPORTED_ABI &&
					s_mockRegistrations == 0,
				"missing stable UI query reached client registration");
			api.queryUIAPI = &MockQueryUIAPI;
			require(dmui::PreflightHostAPI(&api, options) == DMUI_RESULT_OK,
				"compatible host ABI failed with different release metadata");
			s_mockUIRevision = 0u;
			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_UNSUPPORTED_ABI,
				"old stable UI revision reached client registration");
			s_mockUIRevision = DMUI_UI_REVISION_CURRENT;
			s_mockUITableSize = DMUI_UI_API_REQUIRED_SIZE - 1u;
			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_UNSUPPORTED_ABI,
				"short required stable UI table reached client registration");
			s_mockUITableSize = DMUI_UI_API_REQUIRED_SIZE;
			require(dmui::PreflightHostAPI(&api, options) == DMUI_RESULT_OK,
				"missing optional UI tail incorrectly blocked connection");
			s_mockMissingRequiredUIOperation = true;
			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_UNSUPPORTED_ABI,
				"missing mandatory Begin/End pair operation reached registration");
			s_mockMissingRequiredUIOperation = false;
		});

		runner.test("official client preflight enforces requested UI tail operations", [] {
			s_mockUIResult = DMUI_RESULT_OK;
			s_mockUIRevision = DMUI_UI_REVISION_CURRENT;
			s_mockUITableSize = DMUI_UI_API_CURRENT_SIZE;
			s_mockMissingRequiredUIOperation = false;
			s_mockMissingPlotLines = true;
			auto api = PreflightHostAPI();

			const dmui::ClientOptions baseline{};
			require(
				dmui::PreflightHostAPI(&api, baseline) == DMUI_RESULT_OK,
				"missing optional UI tail blocked a baseline client");

			const dmui::ClientOptions plotLines{
				.minimumUIAPISize = DMUI_UI_API_PLOT_LINES_SIZE
			};
			require(
				dmui::PreflightHostAPI(&api, plotLines) ==
					DMUI_RESULT_UNSUPPORTED_ABI,
				"requested PlotLines tail accepted a null operation");

			s_mockMissingPlotLines = false;
			require(
				dmui::PreflightHostAPI(&api, plotLines) == DMUI_RESULT_OK,
				"available requested PlotLines tail failed preflight");
		});

		runner.test("navigation icon preflight requires page and category entries", [] {
			s_mockServices = DMUI_HOST_SERVICE_NAVIGATION_ICONS;
			auto api = PreflightHostAPI();
			api.queryServices = &MockQueryServices;
			const dmui::ClientOptions options{
				.requiredServices = DMUI_HOST_SERVICE_NAVIGATION_ICONS
			};
			require(
				dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_SERVICE_UNAVAILABLE,
				"navigation icon preflight accepted missing registration entries");
			api.registerPage = &MockRegisterPage;
			require(
				dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_SERVICE_UNAVAILABLE,
				"navigation icon preflight omitted category registration");
			api.registerCategory = &MockRegisterCategory;
			require(
				dmui::PreflightHostAPI(&api, options) == DMUI_RESULT_OK,
				"complete navigation icon surface failed preflight");
			api.structSize = DMUI_HOST_API_REGISTER_CATEGORY_SIZE - 1;
			require(
				dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_UNSUPPORTED_ABI,
				"truncated host table exposed the appended UI query");
		});

		runner.test("pixel-image preflight requires create update and shared entries", [] {
			s_mockServices = DMUI_HOST_SERVICE_PIXEL_IMAGES;
			auto api = PreflightHostAPI();
			api.queryServices = &MockQueryServices;
			const dmui::ClientOptions options{
				.requiredServices = DMUI_HOST_SERVICE_PIXEL_IMAGES
			};

			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_SERVICE_UNAVAILABLE,
				"advertised pixel images omitted all required entries");
			api.createImage = &MockCreateImage;
			api.updateImage = &MockUpdateImage;
			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_SERVICE_UNAVAILABLE,
				"pixel image preflight omitted shared handle entries");
			api.drawImage = &MockDrawImage;
			api.releaseImage = &MockReleaseImage;
			api.queryImage = &MockQueryImage;
			require(dmui::PreflightHostAPI(&api, options) == DMUI_RESULT_OK,
				"complete pixel image surface failed preflight");
			api.updateImage = nullptr;
			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_SERVICE_UNAVAILABLE,
				"pixel image preflight accepted a missing update entry");

			const dmui::ClientOptions unknown{
				.requiredServices = UINT64_C(1) << 63u
			};
			require(dmui::PreflightHostAPI(&api, unknown) ==
					DMUI_RESULT_SERVICE_UNAVAILABLE,
				"unknown service bit passed official preflight");
		});

		runner.test("bridged clients carry copied source labels", [] {
			Registry registry;
			CallbackState state;
			char sourceLabel[]{ "MCM" };
			auto client = Client("bridged.mod", "Bridged", state);
			client.origin = DMUI_CLIENT_ORIGIN_BRIDGED;
			client.bridgeSourceLabel = sourceLabel;
			DMUI_ClientHandle handle{};

			require(registry.RegisterClient(&client, &handle) == DMUI_RESULT_OK,
				"a bridged client was rejected");
			sourceLabel[0] = 'X';
			const auto& registered = registry.RegisteredClients().front();
			require(
				registered.origin == DMUI_CLIENT_ORIGIN_BRIDGED &&
					registered.bridgeSourceLabel == "MCM",
				"the bridge source label was not copied");

			auto contradictory =
				Client("native.source", "Native Source", state);
			contradictory.bridgeSourceLabel = "MCM";
			require(
				registry.RegisterClient(&contradictory, &handle) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"a native client carried a bridge source label");
		});

		runner.test("Health creates a section for each bridge source", [] {
			const std::vector<RegisteredClient> clients{
				{ .handle = 1, .id = "native-z", .displayName = "Zulu Native" },
				{ .handle = 6, .id = "native-a", .displayName = "Alpha Native" },
				{
					.handle = 2,
					.id = "zeta-two",
					.displayName = "Beta",
					.origin = DMUI_CLIENT_ORIGIN_BRIDGED,
					.bridgeSourceLabel = "Zeta"
				},
				{
					.handle = 3,
					.id = "alpha",
					.displayName = "Alpha",
					.origin = DMUI_CLIENT_ORIGIN_BRIDGED,
					.bridgeSourceLabel = "Alpha"
				},
				{
					.handle = 4,
					.id = "zeta-one",
					.displayName = "Able",
					.origin = DMUI_CLIENT_ORIGIN_BRIDGED,
					.bridgeSourceLabel = "Zeta"
				}
			};

			const auto sections = BuildHealthClientSections(clients);
			require(
				sections.size() == 3 &&
					sections[0].heading == "Registered mods" &&
					sections[0].clients.size() == 2 &&
					sections[0].clients[0]->id == "native-a" &&
					sections[0].clients[1]->id == "native-z" &&
					sections[1].heading == "Alpha mods" &&
					sections[1].clients.size() == 1 &&
					sections[1].clients[0]->id == "alpha" &&
					sections[2].heading == "Zeta mods" &&
					sections[2].clients.size() == 2 &&
					sections[2].clients[0]->id == "zeta-one" &&
					sections[2].clients[1]->id == "zeta-two",
				"bridge source sections or their member order were unstable");

			const std::vector<RegisteredClient> unnamedBridge{
				{
					.handle = 5,
					.id = "unnamed",
					.displayName = "Unnamed",
					.origin = DMUI_CLIENT_ORIGIN_BRIDGED
				}
			};
			const auto fallback = BuildHealthClientSections(unnamedBridge);
			require(
				fallback.size() == 1 &&
					fallback.front().heading == "Bridged mods",
				"an unnamed bridge did not receive the generic heading");
		});

		runner.test("overdue Health observations warn consistently without changing capability", [] {
			const auto start = HealthClock::time_point{} +
				std::chrono::seconds{ 10 };
			const auto deadline = start + std::chrono::seconds{ 10 };
			std::array snapshots{
				HealthSnapshot{
					"dmui.render.reconciliation",
					HealthState::kWaiting,
					start,
					deadline,
					"Waiting for the renderer." }
			};
			const auto before = deadline - std::chrono::seconds{ 1 };
			require(BuildHomeHealthSummary(snapshots, 0, before) ==
						"1 host subsystem starting" &&
					HomeHealthSeverity(snapshots, 0, before) ==
						HealthSeverity::kNeutral &&
					BuildHealthSubsystemRows(snapshots, before)[0].stateLabel ==
						"Waiting",
				"an expected wait was escalated before its deadline");

			const auto rows = BuildHealthSubsystemRows(snapshots, deadline);
			require(BuildHomeHealthSummary(snapshots, 0, deadline) ==
						"1 host subsystem needs attention" &&
					HomeHealthSeverity(snapshots, 0, deadline) ==
						HealthSeverity::kWarning &&
					rows[0].state == HealthState::kWaiting &&
					rows[0].stateLabel == "Waiting (deadline exceeded)" &&
					rows[0].reason == "Waiting for the renderer." &&
					HealthStatusSeverity(rows[0].severity) ==
						DMUI_STATUS_SEVERITY_WARNING,
				"an overdue wait disappeared from Home or Health");
			const auto report = BuildHealthDiagnosticsReport(
				"Host", "0.1.0", snapshots, {}, {}, {}, deadline);
			require(report.find("Waiting (deadline exceeded)") !=
						std::string::npos &&
					snapshots[0].state == HealthState::kWaiting,
				"the copied report omitted expiry or presentation changed capability");

			snapshots[0].state = HealthState::kProgressing;
			require(HomeHealthSeverity(snapshots, 0, deadline) ==
						HealthSeverity::kWarning &&
					BuildHealthSubsystemRows(snapshots, deadline)[0].stateLabel ==
						"Progressing (deadline exceeded)",
				"overdue progress was presented as normal startup");
			snapshots[0].state = HealthState::kFailed;
			require(HomeHealthSeverity(snapshots, 0, deadline) ==
						HealthSeverity::kError &&
					BuildHealthSubsystemRows(snapshots, deadline)[0].severity ==
						HealthSeverity::kError,
				"deadline warning downgraded a failed subsystem");
			snapshots[0].state = HealthState::kReady;
			require(BuildHomeHealthSummary(snapshots, 0, deadline) ==
						"All systems ready" &&
					HomeHealthSeverity(snapshots, 0, deadline) ==
						HealthSeverity::kSuccess &&
					BuildHealthSubsystemRows(snapshots, deadline)[0].stateLabel ==
						"Ready",
				"recovered readiness retained a stale deadline warning");
		});

		runner.test("client diagnostics aggregate by severity scope and summary", [] {
			DiagnosticStore store;
			DMUI_DiagnosticDescriptor diagnostic{
				DMUI_DIAGNOSTIC_DESCRIPTOR_0_1_SIZE,
				DMUI_STATUS_SEVERITY_WARNING,
				"General",
				"Expected a boolean value.",
				"First location"
			};
			require(
				store.Report(7, diagnostic) == DMUI_RESULT_OK &&
					store.Report(7, diagnostic) == DMUI_RESULT_OK,
				"matching diagnostics were rejected");
			diagnostic.detail = "Later location";
			diagnostic.scope = "Advanced";
			require(
				store.Report(7, diagnostic) == DMUI_RESULT_OK,
				"a distinct diagnostic scope was rejected");
			diagnostic.scope = "General";
			diagnostic.severity = DMUI_STATUS_SEVERITY_ERROR;
			require(
				store.Report(7, diagnostic) == DMUI_RESULT_OK,
				"a distinct diagnostic severity was rejected");

			const auto snapshot = store.Snapshot(7);
			require(
				snapshot &&
					snapshot->records.size() == 3 &&
					snapshot->records[0].occurrenceCount == 2 &&
					snapshot->records[0].detail == "First location",
				"diagnostic aggregation or first-detail retention changed");
		});

		runner.test("client diagnostic retention stays bounded", [] {
			DiagnosticStore store;
			std::vector<std::string> summaries;
			summaries.reserve(kDiagnosticRecordLimitPerClient + 2);
			for (size_t index = 0;
				index < kDiagnosticRecordLimitPerClient + 2;
				++index)
			{
				summaries.push_back(
					"Diagnostic " + std::to_string(index));
				const DMUI_DiagnosticDescriptor diagnostic{
					DMUI_DIAGNOSTIC_DESCRIPTOR_0_1_SIZE,
					DMUI_STATUS_SEVERITY_WARNING,
					"General",
					summaries.back().c_str(),
					nullptr
				};
				require(
					store.Report(9, diagnostic) == DMUI_RESULT_OK,
					"a bounded diagnostic report was rejected");
			}
			const DMUI_DiagnosticDescriptor repeated{
				DMUI_DIAGNOSTIC_DESCRIPTOR_0_1_SIZE,
				DMUI_STATUS_SEVERITY_WARNING,
				"General",
				summaries.front().c_str(),
				nullptr
			};
			require(
				store.Report(9, repeated) == DMUI_RESULT_OK,
				"an existing diagnostic stopped incrementing at the cap");
			const DMUI_DiagnosticDescriptor repeatedDropped{
				DMUI_DIAGNOSTIC_DESCRIPTOR_0_1_SIZE,
				DMUI_STATUS_SEVERITY_WARNING,
				"General",
				summaries[kDiagnosticRecordLimitPerClient].c_str(),
				nullptr
			};
			require(
				store.Report(9, repeatedDropped) == DMUI_RESULT_OK &&
					store.Report(9, repeatedDropped) == DMUI_RESULT_OK,
				"a dropped diagnostic report was rejected");

			const auto snapshot = store.Snapshot(9);
			require(
				snapshot &&
					snapshot->records.size() ==
						kDiagnosticRecordLimitPerClient &&
					snapshot->droppedReportCount == 4 &&
					snapshot->records.front().occurrenceCount == 2,
				"bounded diagnostic retention lost counts or admitted overflow");
		});

		runner.test("Health diagnostics report includes support context", [] {
			const std::vector<RegisteredClient> clients{
				{
					.handle = 4,
					.id = "example.mod",
					.displayName = "Example Mod",
					.version = DMUI_MAKE_VERSION(2, 5)
				}
			};
			const std::array statuses{
				ClientStatus{
					4,
					DMUI_STATUS_SEVERITY_WARNING
				}
			};
			const std::array subsystems{
				HealthSnapshot{
					"dmui.render.reconciliation",
					HealthState::kReady,
					{},
					{},
					"renderer attached"
				}
			};
			const std::array diagnostics{
				ClientDiagnosticSnapshot{
					4,
					{
						ClientDiagnosticRecord{
							4,
							DMUI_STATUS_SEVERITY_ERROR,
							"General",
							"Value could not be loaded.",
							"Missing setting id.",
							3
						}
					},
					2
				}
			};

			const auto report = BuildHealthDiagnosticsReport(
				"Evil Modding",
				"1.0.0",
				subsystems,
				clients,
				statuses,
				diagnostics);
			require(
				report.find("Evil Modding 1.0.0") != std::string::npos &&
					report.find("dmui.render.reconciliation: Ready") !=
						std::string::npos &&
					report.find("Example Mod 2.5 [Warning]") !=
						std::string::npos &&
					report.find("Value could not be loaded. (x3)") !=
						std::string::npos &&
					report.find(
						"2 further diagnostic reports were not retained.") !=
						std::string::npos,
				"the copied Health report omitted required support context");
		});

		runner.test("Home summarizes readiness and attention states", [] {
			require(
				BuildHomeHealthSummary({}, 0) ==
					"Host health not observed yet" &&
					HomeHealthSeverity({}, 0) == HealthSeverity::kNeutral,
				"an empty health registry claimed readiness");

			const std::array ready{
				HealthSnapshot{
					"dmui.render.reconciliation",
					HealthState::kReady,
					{},
					{},
					{} }
			};
			require(
				BuildHomeHealthSummary(ready, 0) == "All systems ready" &&
					HomeHealthSeverity(ready, 0) ==
						HealthSeverity::kSuccess,
				"ready health did not produce the success summary");

			const std::array waiting{
				HealthSnapshot{
					"dmui.render.reconciliation",
					HealthState::kWaiting,
					{},
					{},
					"renderer data is not initialized" }
			};
			require(
				BuildHomeHealthSummary(waiting, 2) ==
					"1 host subsystem starting; 2 mods need attention",
				"startup state did not remain distinct from client attention");

			const std::array degraded{
				HealthSnapshot{
					"dmui.configuration",
					HealthState::kDegraded,
					{},
					{},
					"Using defaults after a parse failure." }
			};
			require(
				BuildHomeHealthSummary(degraded, 0) ==
					"1 host subsystem needs attention" &&
					HomeHealthSeverity(degraded, 0) ==
						HealthSeverity::kWarning,
				"a degraded subsystem did not need attention");

			const std::array failed{
				HealthSnapshot{
					"dmui.input.game-interception",
					HealthState::kFailed,
					{},
					{},
					"PlayerCamera patch failed." }
			};
			require(
				HomeHealthSeverity(failed, 0) == HealthSeverity::kError,
				"a failed subsystem did not color the Home summary as an error");
		});

		runner.test("link-row API arguments reject malformed descriptors", [] {
			const DMUI_DrawLinkRowFn drawLinkRow =
				&ValidateLinkRowArguments;
			DMUI_ExternalOpenDescriptor external{
				DMUI_EXTERNAL_OPEN_DESCRIPTOR_0_1_SIZE,
				DMUI_EXTERNAL_TARGET_URI,
				"https://github.com/Dear-Modding-FO4/DearModdingUI"
			};
			DMUI_LinkDescriptor link{
				DMUI_LINK_DESCRIPTOR_0_1_SIZE,
				"GitHub",
				nullptr,
				0,
				1,
				DMUI_LINK_ACTION_COPY_TARGET,
				0,
				&external
			};
			require(
				drawLinkRow(
					DMUI_INVALID_CLIENT_HANDLE,
					nullptr,
					&link,
					1) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"a null link-row ID was accepted");
			require(
				drawLinkRow(
					DMUI_INVALID_CLIENT_HANDLE,
					"links",
					nullptr,
					1) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"a null non-empty link array was accepted");
			link.label = "";
			require(
				drawLinkRow(
					DMUI_INVALID_CLIENT_HANDLE,
					"links",
					&link,
					1) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"an empty link label was accepted");
			link.label = "GitHub";
			external.target = "";
			require(
				drawLinkRow(
					DMUI_INVALID_CLIENT_HANDLE,
					"links",
					&link,
					1) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"an enabled link without a target was accepted");
			external.target = "https://github.com/Dear-Modding-FO4/DearModdingUI";
			link.structSize = DMUI_LINK_DESCRIPTOR_0_1_SIZE - 1;
			require(
				drawLinkRow(
					DMUI_INVALID_CLIENT_HANDLE,
					"links",
					&link,
					1) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"a short link descriptor was accepted");
			link.structSize = DMUI_LINK_DESCRIPTOR_0_1_SIZE;
			require(
				drawLinkRow(
					DMUI_INVALID_CLIENT_HANDLE,
					"links",
					nullptr,
					0) ==
					DMUI_RESULT_OK,
				"an empty link row was rejected");
			link.enabled = 0;
			link.external = nullptr;
			require(
				drawLinkRow(
					DMUI_INVALID_CLIENT_HANDLE,
					"links",
					&link,
					1) == DMUI_RESULT_OK,
				"a disabled link required an executable action");
		});

		runner.test("external opening validates and dispatches typed targets", [] {
			s_externalOpenCalls = 0;
			s_externalResult = DMUI_RESULT_OK;
			s_externalNativeError = 0;
			const ExternalOpener opener{ &FakeExternalOpen };

			DMUI_ExternalOpenDescriptor descriptor{
				sizeof(DMUI_ExternalOpenDescriptor),
				DMUI_EXTERNAL_TARGET_URI,
				"https://example.invalid/docs"
			};
			require(opener.Open(&descriptor) == DMUI_RESULT_OK,
				"default URI dispatch failed");
			require(
				s_externalOpenCalls == 1 &&
					s_externalRequest.targetKind == DMUI_EXTERNAL_TARGET_URI &&
					s_externalRequest.target == "https://example.invalid/docs" &&
					s_externalRequest.application.empty(),
				"default URI did not use the associated-handler path");

			descriptor.targetKind = DMUI_EXTERNAL_TARGET_FILE;
			descriptor.target = "C:\\mods\\readme.txt";
			require(opener.Open(&descriptor) == DMUI_RESULT_OK,
				"absolute file dispatch failed");
			descriptor.targetKind = DMUI_EXTERNAL_TARGET_DIRECTORY;
			descriptor.target = "\\\\server\\mods";
			require(opener.Open(&descriptor) == DMUI_RESULT_OK,
				"absolute directory dispatch failed");

			const char* arguments[]{ "--line", "42", "" };
			descriptor.targetKind = DMUI_EXTERNAL_TARGET_FILE;
			descriptor.target = "C:\\mods\\settings.ini";
			descriptor.application = "C:\\Windows\\notepad.exe";
			descriptor.arguments = arguments;
			descriptor.argumentCount = static_cast<uint32_t>(std::size(arguments));
			descriptor.workingDirectory = "C:\\mods";
			require(opener.Open(&descriptor) == DMUI_RESULT_OK,
				"explicit application dispatch failed");
			require(
				s_externalRequest.application == "C:\\Windows\\notepad.exe" &&
					s_externalRequest.arguments.size() == 3 &&
					s_externalRequest.arguments.back().empty() &&
					s_externalRequest.target == "C:\\mods\\settings.ini" &&
					s_externalRequest.workingDirectory == "C:\\mods",
				"explicit application arguments were not preserved");

			descriptor.targetKind = DMUI_EXTERNAL_TARGET_NONE;
			descriptor.target = nullptr;
			descriptor.arguments = nullptr;
			descriptor.argumentCount = 0;
			descriptor.workingDirectory = nullptr;
			require(opener.Open(&descriptor) == DMUI_RESULT_OK,
				"application-only dispatch failed");

			s_externalResult = DMUI_RESULT_EXTERNAL_OPEN_FAILED;
			s_externalNativeError = 5;
			uint32_t nativeError{};
			require(
				opener.Open(&descriptor, &nativeError) ==
						DMUI_RESULT_EXTERNAL_OPEN_FAILED &&
					nativeError == 5 &&
					s_externalOpenCalls == 6,
				"platform dispatch failure was hidden or retried");
		});

		runner.test("external opening rejects ambiguous or unsafe descriptors", [] {
			ExternalOpenRequest request;
			DMUI_ExternalOpenDescriptor descriptor{
				sizeof(DMUI_ExternalOpenDescriptor),
				DMUI_EXTERNAL_TARGET_FILE,
				"relative.txt"
			};
			require(
				ValidateExternalOpenDescriptor(&descriptor, request) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"a relative file target was accepted");
			descriptor.targetKind = DMUI_EXTERNAL_TARGET_URI;
			descriptor.target = "not a URI";
			require(
				ValidateExternalOpenDescriptor(&descriptor, request) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"a URI without a scheme was accepted");
			descriptor.targetKind = DMUI_EXTERNAL_TARGET_NONE;
			descriptor.target = nullptr;
			require(
				ValidateExternalOpenDescriptor(&descriptor, request) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"an empty default-handler request was accepted");
			descriptor.application = "notepad.exe";
			require(
				ValidateExternalOpenDescriptor(&descriptor, request) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"a relative explicit application was accepted");
			descriptor.application = "C:\\Windows\\notepad.exe";
			descriptor.argumentCount = 129;
			require(
				ValidateExternalOpenDescriptor(&descriptor, request) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"an overflowing argument count was accepted");
			descriptor.argumentCount = 0;
			std::string oversized(32768, 'x');
			descriptor.targetKind = DMUI_EXTERNAL_TARGET_FILE;
			descriptor.target = oversized.c_str();
			require(
				ValidateExternalOpenDescriptor(&descriptor, request) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"an oversized target was accepted");
			const char invalidUtf8[]{ static_cast<char>(0xC3), '(', '\0' };
			descriptor.targetKind = DMUI_EXTERNAL_TARGET_URI;
			descriptor.target = invalidUtf8;
			require(
				ValidateExternalOpenDescriptor(&descriptor, request) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"malformed UTF-8 was accepted");
		});

		runner.test("virtual-file preflight requires advertised targets and opening entry", [] {
			auto api = PreflightHostAPI();
			api.queryServices = &MockQueryServices;
			api.openExternal = &MockOpenExternal;
			const dmui::ClientOptions options{
				.requiredServices = DMUI_HOST_SERVICE_VIRTUAL_FILE_TARGETS
			};
			s_mockServices = DMUI_HOST_SERVICE_EXTERNAL_OPEN;
			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_SERVICE_UNAVAILABLE,
				"ordinary external opening promised virtual-file resolution");
			s_mockServices |= DMUI_HOST_SERVICE_VIRTUAL_FILE_TARGETS;
			require(dmui::PreflightHostAPI(&api, options) == DMUI_RESULT_OK,
				"virtual-file service was not recognized");
			api.openExternal = nullptr;
			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_SERVICE_UNAVAILABLE,
				"virtual-file preflight omitted its dispatch entry");
			api.openExternal = &MockOpenExternal;
			api.structSize = DMUI_HOST_API_OPEN_EXTERNAL_SIZE - 1;
			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_UNSUPPORTED_ABI,
				"truncated host table exposed the appended UI query");
		});

		runner.test("virtual targets resolve the file before dispatching either action", [] {
			s_externalOpenCalls = 0;
			s_externalResolveCalls = 0;
			s_externalResult = DMUI_RESULT_OK;
			s_externalNativeError = 0;
			s_externalResolveResult = DMUI_RESULT_OK;
			s_externalResolveError = 0;
			s_externalPhysicalFile = "D:\\mod library\\winner\\settings.ini";
			const ExternalOpener opener{ &FakeExternalOpen, &FakeExternalFileResolver };
			const char* arguments[]{ "--reuse-window" };
			DMUI_ExternalOpenDescriptor descriptor{
				sizeof(DMUI_ExternalOpenDescriptor),
				DMUI_EXTERNAL_TARGET_VIRTUAL_FILE,
				"C:\\game\\Data\\settings.ini",
				"C:\\Tools\\viewer.exe",
				arguments,
				1,
				0,
				"C:\\Tools"
			};
			uint32_t nativeError{ 999 };
			require(opener.Open(&descriptor, &nativeError) == DMUI_RESULT_OK &&
					nativeError == 0 && s_externalResolveCalls == 1 &&
					s_externalVirtualFile == descriptor.target &&
					s_externalRequest.target == s_externalPhysicalFile &&
					s_externalRequest.targetKind == DMUI_EXTERNAL_TARGET_FILE,
				"virtual file was not resolved before opening");
			require(s_externalRequest.application == descriptor.application &&
					s_externalRequest.workingDirectory == descriptor.workingDirectory &&
					s_externalRequest.arguments == std::vector<std::string>{ "--reuse-window" },
				"resolution rewrote application selection or arguments");
			descriptor.targetKind = DMUI_EXTERNAL_TARGET_VIRTUAL_FILE_PARENT;
			descriptor.application = nullptr;
			descriptor.arguments = nullptr;
			descriptor.argumentCount = 0;
			descriptor.workingDirectory = nullptr;
			require(opener.Open(&descriptor) == DMUI_RESULT_OK &&
					s_externalResolveCalls == 2 &&
					s_externalVirtualFile == descriptor.target &&
					s_externalRequest.target == "D:\\mod library\\winner\\" &&
					s_externalRequest.targetKind == DMUI_EXTERNAL_TARGET_DIRECTORY &&
					s_externalRequest.application.empty(),
				"containing-folder action resolved the virtual directory or changed the handler");
			s_externalPhysicalFile = "D:\\settings.ini";
			require(opener.Open(&descriptor) == DMUI_RESULT_OK &&
					s_externalRequest.target == "D:\\",
				"root-level parent became drive-relative");
			s_externalPhysicalFile = "\\\\server\\share\\settings.ini";
			require(opener.Open(&descriptor) == DMUI_RESULT_OK &&
					s_externalRequest.target == "\\\\server\\share\\",
				"UNC root-level parent lost its share");
			for (const auto kind : { DMUI_EXTERNAL_TARGET_FILE, DMUI_EXTERNAL_TARGET_DIRECTORY })
			{
				descriptor.targetKind = kind;
				require(opener.Open(&descriptor) == DMUI_RESULT_OK &&
						s_externalRequest.target == descriptor.target &&
						s_externalResolveCalls == 4,
					"ordinary path was implicitly resolved");
			}
		});

		runner.test("virtual resolution failure never dispatches an unresolved fallback", [] {
			s_externalOpenCalls = 0;
			s_externalResolveCalls = 0;
			s_externalResolveError = ERROR_ACCESS_DENIED;
			const ExternalOpener opener{ &FakeExternalOpen, &FakeExternalFileResolver };
			DMUI_ExternalOpenDescriptor descriptor{
				sizeof(DMUI_ExternalOpenDescriptor),
				DMUI_EXTERNAL_TARGET_VIRTUAL_FILE,
				"C:\\game\\Data\\settings.ini"
			};
			for (const auto kind :
				{ DMUI_EXTERNAL_TARGET_VIRTUAL_FILE, DMUI_EXTERNAL_TARGET_VIRTUAL_FILE_PARENT })
			{
				descriptor.targetKind = kind;
				for (const auto failure : { DMUI_RESULT_EXTERNAL_RESOLUTION_FAILED,
						 DMUI_RESULT_EXTERNAL_RESOLUTION_UNSUPPORTED })
				{
					s_externalResolveResult = failure;
					uint32_t nativeError{};
					require(opener.Open(&descriptor, &nativeError) == failure &&
							nativeError == ERROR_ACCESS_DENIED && s_externalOpenCalls == 0,
						"resolution failure was hidden or fell back to a virtual path");
				}
				descriptor.target = "relative.ini";
				require(opener.Open(&descriptor) == DMUI_RESULT_INVALID_DESCRIPTOR,
					"relative virtual file reached the resolver");
				descriptor.target = "C:\\game\\Data\\settings.ini";
			}
			require(s_externalResolveCalls == 4,
				"invalid virtual descriptor reached the resolver");
			s_externalResolveResult = DMUI_RESULT_OK;
			s_externalResolveError = 0;
			s_externalPhysicalFile = "D:\\winner\\settings.ini";
			s_externalResult = DMUI_RESULT_EXTERNAL_OPEN_FAILED;
			s_externalNativeError = ERROR_FILE_NOT_FOUND;
			uint32_t nativeError{};
			require(opener.Open(&descriptor, &nativeError) == DMUI_RESULT_EXTERNAL_OPEN_FAILED &&
					nativeError == ERROR_FILE_NOT_FOUND && s_externalOpenCalls == 1,
				"launch failure was mislabeled as resolution failure or retried");
		});

		runner.test("backing-file resolution preserves readable file contents and releases handles", [] {
			const ExternalFileFixture fixture{ "resolution must not rewrite this file" };
			require(SetFileAttributesW(fixture.path.c_str(), FILE_ATTRIBUTE_READONLY) != 0,
				"read-only fixture attribute could not be set");
			std::string physical;
			uint32_t nativeError{ 999 };
			const auto requested = fixture.Utf8();
			require(ResolveExternalFile(requested, physical, &nativeError) == DMUI_RESULT_OK &&
					nativeError == 0,
				"mapped-file resolution failed for a readable read-only file");
			require(std::filesystem::equivalent(
						fixture.path, std::filesystem::path{ std::u8string(physical.begin(), physical.end()) }),
				"resolved physical file identifies a different file");
			DWORD before{};
			DWORD after{};
			require(GetProcessHandleCount(GetCurrentProcess(), &before) != 0,
				"process handle count was unavailable");
			for (int iteration = 0; iteration < 16; ++iteration)
				require(ResolveExternalFile(requested, physical, &nativeError) == DMUI_RESULT_OK,
					"repeated resolution failed");
			require(GetProcessHandleCount(GetCurrentProcess(), &after) != 0 && after == before,
				"resolution leaked file, mapping, or volume handles");
			const auto exclusive = CreateFileW(
				fixture.path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
			require(exclusive != INVALID_HANDLE_VALUE, "resolution retained a file lease");
			CloseHandle(exclusive);
			std::ifstream stream{ fixture.path, std::ios::binary };
			const std::string contents{ std::istreambuf_iterator<char>{ stream }, {} };
			require(contents == "resolution must not rewrite this file" &&
					(GetFileAttributesW(fixture.path.c_str()) & FILE_ATTRIBUTE_READONLY) != 0,
				"resolution modified file contents or attributes");

			ExternalFileFixture unicodeFixture{ "unicode" };
			auto renamed = unicodeFixture.path;
			renamed += L"-\u00e9-\u6d4b\u8bd5";
			require(MoveFileW(unicodeFixture.path.c_str(), renamed.c_str()) != 0,
				"Unicode fixture rename failed");
			unicodeFixture.path = renamed;
			const auto unicodeRequested = "\\\\?\\" + unicodeFixture.Utf8();
			require(ResolveExternalFile(unicodeRequested, physical, nullptr) == DMUI_RESULT_OK,
				"Unicode extended path resolution failed");
			require(std::filesystem::equivalent(unicodeFixture.path,
						std::filesystem::path{ std::u8string(physical.begin(), physical.end()) }),
				"Unicode backing filename was corrupted");
			s_externalResult = DMUI_RESULT_OK;
			s_externalNativeError = 0;
			const ExternalOpener opener{ &FakeExternalOpen };
			const DMUI_ExternalOpenDescriptor descriptor{
				sizeof(DMUI_ExternalOpenDescriptor),
				DMUI_EXTERNAL_TARGET_VIRTUAL_FILE_PARENT,
				unicodeRequested.c_str()
			};
			require(opener.Open(&descriptor) == DMUI_RESULT_OK &&
					s_externalRequest.targetKind == DMUI_EXTERNAL_TARGET_DIRECTORY &&
					std::filesystem::equivalent(unicodeFixture.path.parent_path(),
						std::filesystem::path{ std::u8string(
							s_externalRequest.target.begin(), s_externalRequest.target.end()) }),
				"real resolution did not reach fake dispatch with the physical parent");
		});

		runner.test("backing-file failures preserve output and never create or extend a file", [] {
			const ExternalFileFixture fixture{ "" };
			const auto requested = fixture.Utf8();
			std::string physical{ "unchanged" };
			uint32_t nativeError{};
			require(ResolveExternalFile(requested, physical, &nativeError) ==
						DMUI_RESULT_EXTERNAL_RESOLUTION_UNSUPPORTED &&
					nativeError == ERROR_FILE_INVALID && physical == "unchanged" &&
					std::filesystem::file_size(fixture.path) == 0,
				"empty file was extended or reported as successfully resolved");
			const auto missing = requested + ".missing";
			require(ResolveExternalFile(missing, physical, &nativeError) ==
						DMUI_RESULT_EXTERNAL_RESOLUTION_FAILED &&
					nativeError == ERROR_FILE_NOT_FOUND && physical == "unchanged" &&
					!std::filesystem::exists(fixture.path.wstring() + L".missing"),
				"missing file was created or resolution failure lost its native error");
			const auto parentText = fixture.path.parent_path().u8string();
			const std::string parent{ parentText.begin(), parentText.end() };
			require(ResolveExternalFile(parent, physical, &nativeError) ==
						DMUI_RESULT_EXTERNAL_RESOLUTION_UNSUPPORTED &&
					nativeError == ERROR_DIRECTORY && physical == "unchanged",
				"a directory was accepted as a backing file");
			for (const auto* device : { "\\\\.\\pipe\\dmui-never-open",
					 "\\\\?\\GLOBALROOT\\Device\\HarddiskVolume1\\anything",
					 "\\\\?\\pipe\\dmui-never-open" })
				require(ResolveExternalFile(device, physical, &nativeError) ==
						DMUI_RESULT_INVALID_DESCRIPTOR && nativeError == 0,
					"a device namespace reached file opening");

			const ExternalFileFixture unreadable{ "sharing violation" };
			const auto handle = CreateFileW(
				unreadable.path.c_str(),
				GENERIC_READ,
				0,
				nullptr,
				OPEN_EXISTING,
				0,
				nullptr);
			require(handle != INVALID_HANDLE_VALUE, "exclusive file fixture could not be opened");
			const std::unique_ptr<void, decltype(&CloseHandle)> exclusive{ handle, &CloseHandle };
			const auto unreadablePath = unreadable.Utf8();
			s_externalOpenCalls = 0;
			const ExternalOpener opener{ &FakeExternalOpen };
			DMUI_ExternalOpenDescriptor descriptor{
				sizeof(DMUI_ExternalOpenDescriptor),
				DMUI_EXTERNAL_TARGET_VIRTUAL_FILE,
				unreadablePath.c_str()
			};
			for (const auto kind :
				{ DMUI_EXTERNAL_TARGET_VIRTUAL_FILE, DMUI_EXTERNAL_TARGET_VIRTUAL_FILE_PARENT })
			{
				descriptor.targetKind = kind;
				uint32_t nativeError{};
				require(opener.Open(&descriptor, &nativeError) ==
							DMUI_RESULT_EXTERNAL_RESOLUTION_FAILED &&
						nativeError == ERROR_SHARING_VIOLATION && s_externalOpenCalls == 0,
					"unreadable backing file was dispatched or lost its Windows error");
			}
		});

		runner.test("Windows argv quoting preserves empty spaces quotes and slashes", [] {
			require(QuoteWindowsArgument(L"") == L"\"\"",
				"empty argument quoting changed");
			require(QuoteWindowsArgument(L"two words") == L"\"two words\"",
				"space-containing argument quoting changed");
			require(QuoteWindowsArgument(L"a\"b") == L"\"a\\\"b\"",
				"embedded quote escaping changed");
			require(
				QuoteWindowsArgument(L"C:\\path\\") == L"\"C:\\path\\\\\"",
				"trailing backslash escaping changed");
		});

		runner.test("FAQ API arguments reject malformed entries", [] {
			const DMUI_DrawFaqFn drawFaq = &ValidateFaqArguments;
			DMUI_FaqEntry entry{
				DMUI_FAQ_ENTRY_0_1_SIZE,
				"How do I open the menu?",
				"Press End."
			};
			require(
				drawFaq(
					DMUI_INVALID_CLIENT_HANDLE,
					nullptr,
					&entry,
					1) == DMUI_RESULT_INVALID_ARGUMENT,
				"a null FAQ ID was accepted");
			require(
				drawFaq(
					DMUI_INVALID_CLIENT_HANDLE,
					"faq",
					nullptr,
					1) == DMUI_RESULT_INVALID_ARGUMENT,
				"a null non-empty FAQ array was accepted");
			entry.question = "";
			require(
				drawFaq(
					DMUI_INVALID_CLIENT_HANDLE,
					"faq",
					&entry,
					1) == DMUI_RESULT_INVALID_ARGUMENT,
				"an empty FAQ question was accepted");
			entry.question = "How do I open the menu?";
			entry.answer = "";
			require(
				drawFaq(
					DMUI_INVALID_CLIENT_HANDLE,
					"faq",
					&entry,
					1) == DMUI_RESULT_INVALID_ARGUMENT,
				"an empty FAQ answer was accepted");
			entry.answer = "Press End.";
			entry.structSize = DMUI_FAQ_ENTRY_0_1_SIZE - 1;
			require(
				drawFaq(
					DMUI_INVALID_CLIENT_HANDLE,
					"faq",
					&entry,
					1) == DMUI_RESULT_INVALID_ARGUMENT,
				"a short FAQ entry was accepted");
			require(
				drawFaq(
					DMUI_INVALID_CLIENT_HANDLE,
					"faq",
					nullptr,
					0) == DMUI_RESULT_OK,
				"an empty FAQ was rejected");
		});

		runner.test("diagnostic API arguments reject malformed descriptors", [] {
			const DMUI_ReportDiagnosticFn reportDiagnostic =
				&ValidateDiagnosticArguments;
			DMUI_DiagnosticDescriptor diagnostic{
				DMUI_DIAGNOSTIC_DESCRIPTOR_0_1_SIZE,
				DMUI_STATUS_SEVERITY_WARNING,
				nullptr,
				"Expected a boolean value.",
				nullptr
			};
			require(
				reportDiagnostic(
					DMUI_INVALID_CLIENT_HANDLE,
					nullptr) == DMUI_RESULT_INVALID_ARGUMENT,
				"a null diagnostic descriptor was accepted");
			diagnostic.summary = "";
			require(
				reportDiagnostic(
					DMUI_INVALID_CLIENT_HANDLE,
					&diagnostic) == DMUI_RESULT_INVALID_ARGUMENT,
				"an empty diagnostic summary was accepted");
			diagnostic.summary = "Expected a boolean value.";
			diagnostic.severity = 99;
			require(
				reportDiagnostic(
					DMUI_INVALID_CLIENT_HANDLE,
					&diagnostic) == DMUI_RESULT_INVALID_ARGUMENT,
				"an unknown diagnostic severity was accepted");
			diagnostic.severity = DMUI_STATUS_SEVERITY_WARNING;
			diagnostic.structSize =
				DMUI_DIAGNOSTIC_DESCRIPTOR_0_1_SIZE - 1;
			require(
				reportDiagnostic(
					DMUI_INVALID_CLIENT_HANDLE,
					&diagnostic) == DMUI_RESULT_INVALID_ARGUMENT,
				"a short diagnostic descriptor was accepted");
		});

		runner.test("swapchain handoff requires a registered renderer replacement client", [] {
			Registry registry;
			CallbackState state;
			const auto regular = AddClient(registry, "regular.mod", "Regular", state);
			require(registry.ValidateSwapChainClient(regular) ==
					DMUI_RESULT_CLIENT_CAPABILITY_REQUIRED,
				"a regular client gained renderer replacement access");
			require(registry.ValidateSwapChainClient(9999) == DMUI_RESULT_CLIENT_NOT_FOUND,
				"an unknown client gained renderer replacement access");

			auto renderer = Client("renderer.mod", "Renderer", state);
			renderer.capabilities = DMUI_CLIENT_CAPABILITY_RENDERER_REPLACEMENT;
			DMUI_ClientHandle rendererHandle{};
			require(registry.RegisterClient(
						&renderer, &rendererHandle) == DMUI_RESULT_OK,
				"renderer replacement client was rejected");
			require(registry.ValidateSwapChainClient(rendererHandle) == DMUI_RESULT_OK,
				"renderer replacement capability was not retained");

			auto unknown = Client("unknown.mod", "Unknown", state);
			unknown.capabilities = 0x80000000u;
			DMUI_ClientHandle unknownHandle{};
			require(registry.RegisterClient(
						&unknown, &unknownHandle) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"unknown client capabilities were accepted");
		});

		runner.test("duplicate client and page IDs are rejected in their scopes", [] {
			Registry registry;
			CallbackState state;
			const auto first = AddClient(registry, "a.mod", "A", state);
			auto duplicate = Client("a.mod", "Other", state);
			DMUI_ClientHandle client{};
			require(registry.RegisterClient(&duplicate, &client) ==
					DMUI_RESULT_DUPLICATE_CLIENT_ID,
				"duplicate client ID was accepted");
			const auto second = AddClient(registry, "b.mod", "B", state);
			AddCategory(registry, first, "general", "General");
			AddCategory(registry, second, "general", "General");
			(void)AddPage(registry, first, "settings", "Settings", "general", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			auto page = Page("settings", "Duplicate", "general", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			DMUI_PageHandle pageHandle{};
			require(registry.RegisterPage(first, &page, &pageHandle) ==
					DMUI_RESULT_DUPLICATE_PAGE_ID,
				"duplicate page ID in one client was accepted");
			require(registry.RegisterPage(second, &page, &pageHandle) == DMUI_RESULT_OK,
				"same page ID in another client was rejected");
		});

		runner.test("action registration validates descriptors clients duplicates and freeze", [] {
			Registry registry;
			CallbackState state;
			const auto first = AddClient(
				registry, "actions.first", "First", state);
			const auto second = AddClient(
				registry, "actions.second", "Second", state);
			auto action = Action(
				"copy-diagnostics", "Copy diagnostics", "clipboard-text", 0, state);
			DMUI_ActionHandle handle{};

			require(registry.RegisterAction(first, nullptr, &handle) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null action descriptor was accepted");
			require(registry.RegisterAction(first, &action, nullptr) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null action output was accepted");
			require(registry.RegisterAction(
						DMUI_INVALID_CLIENT_HANDLE, &action, &handle) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"invalid action client handle was accepted");
			action.structSize = sizeof(action) - 1;
			require(registry.RegisterAction(first, &action, &handle) ==
					DMUI_RESULT_STRUCT_TOO_SMALL,
				"short action descriptor was accepted");
			action.structSize = sizeof(action);
			action.callback = nullptr;
			require(registry.RegisterAction(first, &action, &handle) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"null action callback was accepted");
			action.callback = &Draw;
			action.id = "invalid action";
			require(registry.RegisterAction(first, &action, &handle) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"invalid action ID was accepted");
			action.id = "copy-diagnostics";
			action.displayLabel = "";
			require(registry.RegisterAction(first, &action, &handle) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"empty action label was accepted");
			action.displayLabel = "Copy diagnostics";
			require(registry.RegisterAction(9999, &action, &handle) ==
					DMUI_RESULT_CLIENT_NOT_FOUND,
				"action for an unknown client was accepted");
			require(registry.RegisterAction(first, &action, &handle) ==
					DMUI_RESULT_OK,
				"valid action was rejected");
			require(registry.RegisterAction(first, &action, &handle) ==
					DMUI_RESULT_DUPLICATE_ACTION_ID,
				"duplicate action ID in one client was accepted");
			require(registry.RegisterAction(second, &action, &handle) ==
					DMUI_RESULT_OK,
				"same action ID in another client was rejected");
			require(registry.Freeze(), "action registry did not freeze");
			action.id = "late";
			require(registry.RegisterAction(first, &action, &handle) ==
					DMUI_RESULT_REGISTRATION_CLOSED,
				"action registration remained open after freeze");
		});

		runner.test("frame observer registration validates descriptors clients and freeze", [] {
			Registry registry;
			CallbackState state;
			const auto client = AddClient(
				registry, "observer.mod", "Observer", state);
			auto observer = FrameObserver(state);
			DMUI_FrameObserverHandle handle{ 99 };

			require(!registry.HasActiveFrameObservers(),
				"empty registry reported an active frame observer");
			require(registry.RegisterFrameObserver(client, nullptr, &handle) ==
					DMUI_RESULT_INVALID_ARGUMENT &&
					handle == 99,
				"null frame observer descriptor was accepted");
			require(registry.RegisterFrameObserver(client, &observer, nullptr) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null frame observer output was accepted");
			require(registry.RegisterFrameObserver(
						DMUI_INVALID_CLIENT_HANDLE, &observer, &handle) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"invalid frame observer client handle was accepted");
			observer.structSize = sizeof(observer) - 1;
			require(registry.RegisterFrameObserver(client, &observer, &handle) ==
					DMUI_RESULT_STRUCT_TOO_SMALL,
				"short frame observer descriptor was accepted");
			observer.structSize = sizeof(observer);
			observer.callback = nullptr;
			require(registry.RegisterFrameObserver(client, &observer, &handle) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"null frame observer callback was accepted");
			observer.callback = &Draw;
			require(registry.RegisterFrameObserver(9999, &observer, &handle) ==
					DMUI_RESULT_CLIENT_NOT_FOUND,
				"frame observer for an unknown client was accepted");
			require(registry.RegisterFrameObserver(client, &observer, &handle) ==
					DMUI_RESULT_OK &&
					handle != DMUI_INVALID_FRAME_OBSERVER_HANDLE &&
					registry.HasActiveFrameObservers(),
				"valid frame observer was rejected");
			require(registry.InvokeFrameObserver(handle) == DMUI_RESULT_OK &&
					state.draws == 1,
				"valid frame observer was not dispatched");
			require(registry.Freeze(), "frame observer registry did not freeze");
			require(registry.RegisterFrameObserver(client, &observer, &handle) ==
					DMUI_RESULT_REGISTRATION_CLOSED,
				"frame observer registration remained open after freeze");
		});

		runner.test("frame observer dispatch contains and disables callback failures", [] {
			Registry registry;
			CallbackState state;
			const auto client = AddClient(
				registry, "observer.mod", "Observer", state);
			auto observer = FrameObserver(state);
			observer.callback = &ThrowDraw;
			DMUI_FrameObserverHandle handle{};
			require(registry.RegisterFrameObserver(client, &observer, &handle) ==
					DMUI_RESULT_OK,
				"throwing frame observer registration failed");
			require(registry.InvokeFrameObserver(handle) == DMUI_RESULT_CALLBACK_FAILED,
				"throwing frame observer escaped its guard");
			require(!registry.HasActiveFrameObservers(),
				"failed frame observer remained active");
			require(registry.InvokeFrameObserver(handle) == DMUI_RESULT_CALLBACK_FAILED,
				"failed frame observer was invoked again");
		});

		runner.test("categories require unique client-scoped stable IDs", [] {
			Registry registry;
			CallbackState state;
			const auto first =
				AddClient(registry, "first.categories", "First", state);
			const auto second =
				AddClient(registry, "second.categories", "Second", state);
			AddCategory(registry, first, "general", "General");
			AddCategory(registry, second, "general", "Other General");
			AddCategory(registry, second, "second-only", "Second Only");

			const DMUI_CategoryDescriptor duplicate{
				sizeof(DMUI_CategoryDescriptor),
				"general",
				"Renamed",
				99,
				0
			};
			require(
				registry.RegisterCategory(first, &duplicate) ==
					DMUI_RESULT_DUPLICATE_CATEGORY_ID,
				"duplicate category ID was merged or replaced");
			auto invalid = duplicate;
			invalid.id = "bad id";
			require(
				registry.RegisterCategory(first, &invalid) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"category ID whitespace was accepted");
			invalid.id = "valid";
			invalid.displayName = " \t ";
			require(
				registry.RegisterCategory(first, &invalid) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"whitespace-only category label was accepted");

			auto unknown = Page(
				"unknown", "Unknown", "missing", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			DMUI_PageHandle page{};
			require(
				registry.RegisterPage(first, &unknown, &page) ==
					DMUI_RESULT_CATEGORY_NOT_FOUND,
				"unknown category reference was accepted");
			unknown.categoryId = "second-only";
			require(
				registry.RegisterPage(first, &unknown, &page) ==
					DMUI_RESULT_CATEGORY_NOT_FOUND,
				"another client's category reference was accepted");
			auto crossClient = Page(
				"cross", "Cross", "general", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			require(
				registry.RegisterPage(first, &crossClient, &page) ==
					DMUI_RESULT_OK,
				"owned category reference was rejected");
			require(registry.Freeze(), "category registry did not freeze");
			require(
				registry.RegisterCategory(first, &duplicate) ==
					DMUI_RESULT_REGISTRATION_CLOSED,
				"category registration remained open after freeze");
		});

		runner.test("category ordering uses sort key display name and stable ID", [] {
			Registry registry;
			CallbackState state;
			const auto client =
				AddClient(registry, "ordered.categories", "Ordered", state);
			AddCategory(registry, client, "General-10", "General", 10);
			AddCategory(registry, client, "Diagnostics11", "Diagnostics", 10);
			AddCategory(registry, client, "alpha", "Alpha");
			AddCategory(registry, client, "Diagnostics10", "Diagnostics", 10);
			(void)AddPage(
				registry, client, "uncategorized", "Uncategorized", nullptr, 100,
				DMUI_PAGE_KIND_SETTINGS, state);
			(void)AddPage(
				registry, client, "general", "General Page", "General-10", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			(void)AddPage(
				registry, client, "alpha", "Alpha Page", "alpha", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			const auto diagnosticsPage = AddPage(
				registry, client, "diagnostics-a", "Diagnostics A", "Diagnostics10", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			(void)AddPage(
				registry, client, "diagnostics-b", "Diagnostics B", "Diagnostics11", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			require(registry.Freeze(), "ordered category registry did not freeze");
			const auto& categories = registry.Navigation().clients.front().categories;
			require(
				categories.size() == 5 &&
					categories[0].id.empty() &&
					categories[1].id == "alpha" &&
					categories[2].id == "Diagnostics10" &&
					categories[3].id == "Diagnostics11" &&
					categories[4].id == "General-10",
				"category ordering or stable identity changed");
			require(
				categories[2].displayName == categories[3].displayName &&
					SidebarCategoryKey(
						registry.Navigation().clients.front(),
						categories[2].id) !=
						SidebarCategoryKey(
							registry.Navigation().clients.front(),
							categories[3].id),
				"equal category labels collapsed stable expansion identities");
			ClientSelectionState selection{
				client,
				diagnosticsPage
			};
			for (const auto layout : {
					 SidebarLayoutKind::Tree,
					 SidebarLayoutKind::TwoPane,
					 SidebarLayoutKind::DrillDown,
					 SidebarLayoutKind::IconRail })
			{
				SidebarBrowsingState browsing;
				RevealSidebarSelection(layout, registry.Navigation(), selection, browsing);
				require(
					browsing.categoryExpansion[
						"ordered.categories/Diagnostics10"],
					"a sidebar layout used the category label as expansion identity");
			}
		});

		runner.test("registration copies strings and grows beyond the old capacity", [] {
			Registry registry;
			CallbackState state;
			char clientId[] = "copy.mod";
			char clientName[] = "Copy";
			auto clientDescriptor = Client(clientId, clientName, state);
			DMUI_ClientHandle client{};
			require(registry.RegisterClient(
						&clientDescriptor, &client) == DMUI_RESULT_OK,
				"copy client failed");
			clientId[0] = 'x';
			clientName[0] = 'X';
			char actionId[] = "copy";
			char actionLabel[] = "Copy diagnostics";
			char actionIcon[] = "clipboard-text";
			char actionTooltip[] = "Copy a summary.";
			auto action = Action(actionId, actionLabel, actionIcon, 0, state);
			action.tooltip = actionTooltip;
			DMUI_ActionHandle actionHandle{};
			require(registry.RegisterAction(client, &action, &actionHandle) ==
					DMUI_RESULT_OK,
				"copy action failed");
			AddCategory(registry, client, "general", "General");
			actionId[0] = 'x';
			actionLabel[0] = 'X';
			actionIcon[0] = 'x';
			actionTooltip[0] = 'X';
			(void)AddAction(
				registry, client, "zulu", "Zulu", nullptr, 10, state);
			(void)AddAction(
				registry, client, "bravo", "Bravo", nullptr, -10, state);
			(void)AddAction(
				registry, client, "alpha", "Alpha", nullptr, 10, state);
			for (size_t index = 0; index < 32; ++index)
			{
				const auto id = "page-" + std::to_string(index);
				const auto name = "Page " + std::to_string(index);
				(void)AddPage(registry, client, id.c_str(), name.c_str(), "general",
					static_cast<int32_t>(index), DMUI_PAGE_KIND_SETTINGS, state);
			}
			require(registry.Freeze(), "registry did not freeze");
			require(registry.PageCount() == 32, "dynamic registry retained a fixed capacity");
			require(registry.OrderedPages().front().clientId == "copy.mod",
				"client ID was not copied");
			require(registry.OrderedPages().front().clientDisplayName == "Copy",
				"client name was not copied");
			const auto& actions = registry.OrderedActions();
			require(
					actions.size() == 4 &&
						actions[0].id == "bravo" &&
						actions[1].id == "copy" &&
						actions[2].id == "alpha" &&
						actions[3].id == "zulu",
				"actions did not order by sort key then stable ID");
			const auto copiedAction = std::ranges::find(
				actions,
				"copy",
				&RegisteredAction::id);
			require(
					copiedAction != actions.end() &&
						copiedAction->displayLabel ==
							"Copy diagnostics" &&
						copiedAction->iconName ==
							"clipboard-text" &&
						copiedAction->tooltip ==
							"Copy a summary.",
					"action descriptor strings were not copied");
		});

		runner.test("navigation groups clients categories and settings pages deterministically", [] {
			Registry registry;
			CallbackState state;
			const auto bravo = AddClient(registry, "bravo.mod", "Bravo", state);
			const auto alpha = AddClient(
				registry, "alpha.mod", "Alpha", state);
			AddCategory(registry, alpha, "general", "General");
			AddCategory(registry, alpha, "advanced", "Advanced", -10);
			AddCategory(registry, alpha, "hud", "HUD");
			AddCategory(registry, bravo, "general", "General");
			const auto ungrouped = AddPage(
				registry, alpha, "overview", "Overview", nullptr, 100,
				DMUI_PAGE_KIND_SETTINGS, state);
			const auto alphaLate = AddPage(registry, alpha, "late", "Late", "general", 20,
				DMUI_PAGE_KIND_SETTINGS, state);
			const auto alphaEarly = AddPage(registry, alpha, "early", "Early", "general", -10,
				DMUI_PAGE_KIND_SETTINGS, state);
			(void)AddPage(registry, alpha, "advanced", "Advanced", "advanced", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			const auto overlay = AddPage(registry, alpha, "overlay", "Overlay", "hud", 0,
				DMUI_PAGE_KIND_OVERLAY, state);
			(void)AddPage(registry, bravo, "settings", "Settings", "general", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			require(registry.Freeze(), "registry did not freeze");

			const auto& navigation = registry.Navigation();
			require(navigation.clients.size() == 2, "settings clients were not grouped");
			require(navigation.clients[0].id == "alpha.mod", "client order changed");
			require(navigation.clients[0].categories.size() == 3, "categories were not grouped");
			require(
				navigation.clients[0].categories[0].displayName.empty() &&
					navigation.clients[0].categories[0].pages[0].handle ==
						ungrouped,
				"uncategorized pages did not remain first");
			require(navigation.clients[0].categories[1].displayName == "Advanced",
				"category order changed");
			require(navigation.clients[0].categories[2].pages[0].handle == alphaEarly,
				"page sort key was ignored");
			require(navigation.clients[0].categories[2].pages[1].handle == alphaLate,
				"page sort order changed");
			require(navigation.FindPage(alphaEarly) != nullptr, "settings page was not indexed");
			require(navigation.FindPage(overlay) == nullptr,
				"overlay page entered settings navigation");
		});

		runner.test("navigation sections preserve declared origin and exact source identity", [] {
			Registry registry;
			CallbackState state;
			const auto nativeZulu = AddClient(
				registry,
				"native.zulu",
				"Zulu Native",
				state);
			const auto nativeAlpha = AddClient(
				registry,
				"native.alpha",
				"Alpha Native",
				state);
			const auto unnamed = AddClient(
				registry,
				"bridge.unnamed",
				"Unnamed Bridge",
				state,
				DMUI_CLIENT_ORIGIN_BRIDGED,
				"");
			const auto mcmZulu = AddClient(
				registry,
				"bridge.mcm-zulu",
				"Zulu MCM",
				state,
				DMUI_CLIENT_ORIGIN_BRIDGED,
				"MCM");
			const auto mcmAlpha = AddClient(
				registry,
				"bridge.mcm-alpha",
				"Alpha MCM",
				state,
				DMUI_CLIENT_ORIGIN_BRIDGED,
				"MCM");
			const auto colliding = AddClient(
				registry,
				"bridge.native-label",
				"Native Label Bridge",
				state,
				DMUI_CLIENT_ORIGIN_BRIDGED,
				"Native");
			const auto noSettings = AddClient(
				registry,
				"bridge.no-settings",
				"No Settings",
				state,
				DMUI_CLIENT_ORIGIN_BRIDGED,
				"Unused");
			(void)nativeZulu;
			AddCategory(registry, mcmZulu, "overview-category", "Overview");
			(void)AddPage(
				registry,
				nativeAlpha,
				"overview",
				"Overview",
				nullptr,
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state);
			(void)AddPage(
				registry,
				unnamed,
				"overview",
				"Overview",
				nullptr,
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state);
			(void)AddPage(
				registry,
				mcmZulu,
				"overview",
				"Overview",
				"overview-category",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state);
			(void)AddPage(
				registry,
				mcmAlpha,
				"overview",
				"Overview",
				nullptr,
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state);
			(void)AddPage(
				registry,
				colliding,
				"overview",
				"Overview",
				nullptr,
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state);
			(void)AddPage(
				registry,
				noSettings,
				"overlay",
				"Overlay",
				nullptr,
				0,
				DMUI_PAGE_KIND_OVERLAY,
				state);
			require(registry.Freeze(), "section registry did not freeze");

			const auto& navigation = registry.Navigation();
			const auto* nativeClient = navigation.FindClient(nativeAlpha);
			const auto* mcmClient = navigation.FindClient(mcmAlpha);
			const auto* equalCategoryClient = navigation.FindClient(mcmZulu);
			require(
				navigation.clients.size() == 5 &&
					nativeClient &&
					nativeClient->id == "native.alpha" &&
					nativeClient->origin ==
						DMUI_CLIENT_ORIGIN_NATIVE &&
					mcmClient &&
					mcmClient->id == "bridge.mcm-alpha" &&
					mcmClient->origin ==
						DMUI_CLIENT_ORIGIN_BRIDGED &&
					mcmClient->bridgeSourceLabel == "MCM",
				"navigation did not copy origin metadata or filter settings clients");

			const auto& sections = navigation.sections;
			require(
				sections.size() == 4 &&
					sections[0].origin == DMUI_CLIENT_ORIGIN_NATIVE &&
					sections[0].clientIndices.size() == 1 &&
					navigation.clients[sections[0].clientIndices[0]].id ==
						"native.alpha" &&
					sections[1].bridgeSourceLabel.empty() &&
					navigation.clients[sections[1].clientIndices[0]].id ==
						"bridge.unnamed" &&
					sections[2].bridgeSourceLabel == "MCM" &&
					sections[2].clientIndices.size() == 2 &&
					navigation.clients[sections[2].clientIndices[0]].id ==
						"bridge.mcm-alpha" &&
					navigation.clients[sections[2].clientIndices[1]].id ==
						"bridge.mcm-zulu" &&
					sections[3].bridgeSourceLabel == "Native" &&
					navigation.clients[sections[3].clientIndices[0]].id ==
						"bridge.native-label",
				"navigation sections lost deterministic source or client order");
			require(
				NavigationClientSectionLabel(
					DMUI_CLIENT_ORIGIN_NATIVE,
					{}) == "Native" &&
					NavigationClientSectionLabel(
						DMUI_CLIENT_ORIGIN_BRIDGED,
						{}) == "Bridged" &&
					NavigationClientSectionLabel(
						DMUI_CLIENT_ORIGIN_BRIDGED,
						"Native") == "Native",
				"navigation section labels changed factual source display");

			const auto bridged = DestinationsNavigationPresentation::Build(
				navigation,
				{ DMUI_CLIENT_ORIGIN_BRIDGED });
			require(
				bridged.sections.size() == 3 &&
					std::ranges::all_of(
						bridged.sections,
						[](const auto& a_section) {
							return a_section.showHeading;
						}),
				"origin filtering retained the other destination");
			require(
				mcmClient->categories[0].displayName.empty() &&
					equalCategoryClient &&
					equalCategoryClient->categories[0].displayName ==
						"Overview",
				"source grouping changed empty or display-name-equal categories");

			auto copied = navigation;
			auto moved = std::move(copied);
			require(
				moved.sections.size() == navigation.sections.size() &&
					moved.FindSectionForClient(mcmAlpha) &&
					moved.FindSectionForClient(mcmAlpha)
							->bridgeSourceLabel == "MCM" &&
					moved.clients[
						moved.FindSectionForClient(mcmAlpha)
							->clientIndices.front()]
							.handle == mcmAlpha,
				"navigation section membership did not survive copy and move");
		});

		runner.test("controlled source chrome follows external reveals across frames", [] {
			NavigationModel model;
			model.clients = {
				{
					1,
					"native",
					"Native",
					1,
					{ { "General", {
						{ 10, 1, "page", "Page", "General", {}, 0 }
					} } },
					{},
					DMUI_CLIENT_ORIGIN_NATIVE,
					{}
				},
				{
					2,
					"bridge",
					"Bridge",
					1,
					{ { "General", {
						{ 20, 2, "page", "Page", "General", {}, 0 }
					} } },
					{},
					DMUI_CLIENT_ORIGIN_BRIDGED,
					"MCM"
				}
			};
			model.sections = {
				{ DMUI_CLIENT_ORIGIN_NATIVE, {}, { 0 } },
				{ DMUI_CLIENT_ORIGIN_BRIDGED, "MCM", { 1 } }
			};
			ClientSelectionState selection;
			require(
				ApplyNavigationRequest(
					model,
					NavigationRequest::Page(10),
					selection).accepted,
				"initial native page selection failed");
			NavigationPresentationState presentationState;

			auto presentation = BuildNavigationPresentation(
				NavigationPresentationKind::Destinations,
				model,
				presentationState);
			const auto native = std::ranges::find(
				presentation.sourceControls,
				std::string_view{ "Native" },
				&NavigationSourceControl::label);
			const auto bridged = std::ranges::find(
				presentation.sourceControls,
				std::string_view{ "Bridged" },
				&NavigationSourceControl::label);
			require(
				native != presentation.sourceControls.end() &&
					bridged != presentation.sourceControls.end() &&
					native->selected &&
					ResolveControlledNavigationControlDecision(
						native->selected,
						false) ==
						ControlledNavigationControlDecision::None,
				"controlled source chrome treated active visibility as a click");

			require(
				ResolveControlledNavigationControlDecision(
					bridged->selected,
					true) ==
					ControlledNavigationControlDecision::Activate,
				"explicit source click did not request activation");
			const auto switchToBridged =
				ActivateNavigationSourceControl(
					NavigationPresentationKind::Destinations,
					model,
					bridged->id,
					selection,
					presentationState);
			require(
				switchToBridged.accepted &&
					switchToBridged.presentationChanged &&
					switchToBridged.navigation &&
					switchToBridged.navigation->kind ==
						NavigationRequestKind::Client &&
					switchToBridged.navigation->client == 2,
				"presentation did not own source fallback selection");
			require(
				ApplyNavigationRequest(
					model,
					*switchToBridged.navigation,
					selection).accepted,
				"source fallback request was not controller-compatible");

			const auto external = ApplyNavigationRequest(
				model,
				NavigationRequest::Page(10),
				selection);
			RevealNavigationClient(
				NavigationPresentationKind::Destinations,
				model,
				external.client,
				presentationState);
			presentation = BuildNavigationPresentation(
				NavigationPresentationKind::Destinations,
				model,
				presentationState);
			const auto staleBridged = std::ranges::find(
				presentation.sourceControls,
				std::string_view{ "Bridged" },
				&NavigationSourceControl::label);
			require(
				external.accepted &&
					external.revealSelection &&
					selection.activePage == 10 &&
					staleBridged != presentation.sourceControls.end() &&
					!staleBridged->selected &&
					ResolveControlledNavigationControlDecision(
						staleBridged->selected,
						false) ==
						ControlledNavigationControlDecision::None,
				"external reveal was interpreted as stale-tab user activation");

			SidebarBrowsingState browsing;
			RevealSidebarSelection(
				SidebarLayoutKind::DrillDown,
				model,
				selection,
				browsing);
			browsing.drillDown = TransitionDrillDown(
				browsing.drillDown,
				DrillDownEvent::Back);
			const auto samePage = ApplyNavigationRequest(
				model,
				NavigationRequest::Page(10),
				selection);
			RevealNavigationClient(
				NavigationPresentationKind::Destinations,
				model,
				samePage.client,
				presentationState);
			RevealSidebarSelection(
				SidebarLayoutKind::DrillDown,
				model,
				selection,
				browsing);
			require(
				samePage.accepted &&
					!samePage.selectionChanged &&
					samePage.revealSelection &&
					browsing.drillDown ==
						DrillDownState{ DrillDownLevel::Pages, 1 },
				"same-page external reveal did not restore layout browsing");
		});

		runner.test("navigation controller applies every selection through one path", [] {
			NavigationModel model;
			model.clients = {
				{
					1,
					"native",
					"Native",
					1,
					{ { {}, {
						{ 10, 1, "first", "First", {}, {}, 0 },
						{ 11, 1, "second", "Second", {}, {}, 10 }
					} } }
				},
				{
					2,
					"bridge",
					"Bridge",
					1,
					{ { {}, {
						{ 20, 2, "page", "Page", {}, {}, 0 }
					} } },
					{},
					DMUI_CLIENT_ORIGIN_BRIDGED,
					"MCM"
				}
			};
			model.sections = {
				{ DMUI_CLIENT_ORIGIN_NATIVE, {}, { 0 } },
				{ DMUI_CLIENT_ORIGIN_BRIDGED, "MCM", { 1 } }
			};
			ClientSelectionState selection;

			const auto client = ApplyNavigationRequest(
				model,
				NavigationRequest::Client(1),
				selection);
			require(
				client.accepted &&
					client.selectionChanged &&
					client.revealSelection &&
					selection.activeClient == 1 &&
					selection.activePage == 10 &&
					!selection.activeHostPage,
				"client request did not select its landing page");

			const auto page = ApplyNavigationRequest(
				model,
				NavigationRequest::Page(11),
				selection);
			const auto samePage = ApplyNavigationRequest(
				model,
				NavigationRequest::Page(11),
				selection);
			require(
				page.accepted &&
					page.selectionChanged &&
					samePage.accepted &&
					!samePage.selectionChanged &&
					samePage.revealSelection &&
					selection.recentPages.front() == 11,
				"same-page request did not retain explicit reveal semantics");

			const auto invalid = ApplyNavigationRequest(
				model,
				NavigationRequest::Page(999),
				selection);
			auto invalidKind = NavigationRequest::Page(11);
			invalidKind.kind =
				static_cast<NavigationRequestKind>(0xFFFFFFFFu);
			const auto unknown = ApplyNavigationRequest(
				model,
				invalidKind,
				selection);
			const auto invalidHost = ApplyNavigationRequest(
				model,
				NavigationRequest::Host(
					static_cast<HostPageKind>(0xFFFFFFFFu)),
				selection);
			const auto invalidClient = ApplyNavigationRequest(
				model,
				NavigationRequest::Client(999),
				selection);
			require(
				!invalid.accepted &&
					!unknown.accepted &&
					!invalidHost.accepted &&
					!invalidClient.accepted &&
					!selection.activeHostPage &&
					selection.activeClient == 1 &&
					selection.activePage == 11,
				"invalid request tag, host, or target changed selection");

			const auto host = ApplyNavigationRequest(
				model,
				NavigationRequest::Host(HostPageKind::kSettings),
				selection);
			require(
				host.accepted &&
					selection.activeHostPage ==
						HostPageKind::kSettings &&
					selection.activeClient == DMUI_INVALID_CLIENT_HANDLE &&
					selection.activePage == DMUI_INVALID_PAGE_HANDLE,
				"host request did not clear client selection");
		});

		runner.test("navigation search exposes pages and actions with row metadata", [] {
			NavigationModel model;
			model.clients.push_back({
				1,
				"dear-modding.addictol",
				"Addictol",
				DMUI_MAKE_VERSION(1, 0),
				{ { "Telemetry", {
					{ 10, 1, "frame-records", "Frame Records", "Telemetry",
						"Inspect captured frame events.", 10, "telemetry-internal" },
					{ 11, 1, "timings", "Timings", "Telemetry",
						"Inspect timing data.", 20, "telemetry-internal" }
				}, "telemetry-internal" } }
			});
			std::vector<RegisteredAction> actions{
				{
					20,
					1,
					"dear-modding.addictol",
					"Addictol",
					"copy-records",
					"Copy Records",
					"clipboard-text",
					"Copy captured frame records.",
					20,
					nullptr,
					nullptr,
					false
				},
				{
					21,
					2,
					"toolbox.mod",
					"Toolbox",
					"open-toolbox",
					"Open Toolbox",
					"toolbox",
					"Open the toolbox.",
					0,
					nullptr,
					nullptr,
					false
				}
			};

			const auto index = BuildNavigationSearchIndex(model, actions);
			require(index.size() == 5,
				"search index did not include clients, pages, and actions");
			const auto hits = SearchNavigation(model, actions, "records");
			require(hits.size() == 2,
				"page and action did not both match the query");
			require(
					hits[0].entry.kind == NavigationItemKind::kPage &&
						hits[0].entry.page == 10 &&
						hits[0].entry.category == "Telemetry" &&
						hits[1].entry.kind == NavigationItemKind::kAction &&
						hits[1].entry.action == 20 &&
						hits[1].entry.iconName == "clipboard-text" &&
						hits[1].entry.category.empty(),
					"search hits did not retain actionable row metadata");
			require(
				SearchNavigation(model, actions, "telemetry-internal").empty(),
				"search exposed an opaque category ID");
			const auto actionOnly = SearchNavigation(model, actions, "toolbox");
			require(
					actionOnly.size() == 1 &&
						actionOnly[0].entry.action == 21 &&
						actionOnly[0].entry.clientDisplayName == "Toolbox",
					"action-only client was omitted from global search");
			const auto owned = SearchNavigation(model, actions, "ADDICTOL");
			require(
				std::ranges::count_if(
					owned,
					[](const auto& a_hit) {
						return a_hit.entry.kind == NavigationItemKind::kPage;
					}) == 2,
				"mod-name search did not retain every owned page");
		});

		runner.test("navigation search ranks named matches above summaries case insensitively", [] {
			NavigationModel model;
			model.clients.push_back({
				1,
				"ranking.mod",
				"Ranking",
				DMUI_MAKE_VERSION(1, 0),
				{ { "General", {
					{ 10, 1, "named", "Frame Records", "General", {}, 20 },
					{ 11, 1, "summary", "Diagnostics", "General",
						"Includes frame records and timings.", 0 },
					{ 12, 1, "zulu", "Zulu", "General", "shared token", 10 },
					{ 13, 1, "bravo", "Bravo", "General", "shared token", -10 },
					{ 14, 1, "alpha", "Alpha", "General", "shared token", 10 }
				} } }
			});

			const auto hits = SearchNavigation(model, {}, "fRaMe ReCoRdS");
			require(hits.size() == 2,
				"case-insensitive search lost a matching page");
			require(
					hits[0].entry.page == 10 &&
						hits[0].match ==
							NavigationMatchQuality::kDisplayNameExact &&
						hits[1].entry.page == 11 &&
						hits[1].match == NavigationMatchQuality::kSummary,
					"title match did not outrank a summary match");
			const auto ties = SearchNavigation(model, {}, "token");
			require(ties.size() == 3,
				"equal-quality search did not return every hit");
			require(
					ties[0].entry.id == "bravo" &&
						ties[1].entry.id == "alpha" &&
						ties[2].entry.id == "zulu",
					"equal-quality hits ignored sort key or stable ID");
		});

		runner.test("recent pages stay bounded unique and prune stale handles", [] {
			NavigationModel model;
			model.clients.push_back({
				1,
				"recent.mod",
				"Recent",
				DMUI_MAKE_VERSION(1, 0),
				{ { "General", {
					{ 10, 1, "one", "One", "General", {}, 0 },
					{ 11, 1, "two", "Two", "General", {}, 10 },
					{ 12, 1, "three", "Three", "General", {}, 20 }
				} } }
			});
			ClientSelectionState state;
			RecordRecentPage(model, 10, state, 2);
			RecordRecentPage(model, 11, state, 2);
			RecordRecentPage(model, 10, state, 2);
			require(state.recentPages == std::vector<DMUI_PageHandle>{ 10, 11 },
				"recent pages did not move duplicates to the front");
			RecordRecentPage(model, 12, state, 2);
			require(state.recentPages == std::vector<DMUI_PageHandle>{ 12, 10 },
				"recent pages did not evict the oldest handle");
			RecordRecentPage(model, 9999, state, 2);
			require(state.recentPages == std::vector<DMUI_PageHandle>{ 12, 10 },
				"unknown page entered the recent list");

			NavigationModel rebuilt;
			rebuilt.clients.push_back({
				1,
				"recent.mod",
				"Recent",
				DMUI_MAKE_VERSION(1, 0),
				{ { "General", {
					{ 10, 1, "one", "One", "General", {}, 0 }
				} } }
			});
			PruneRecentPages(rebuilt, state);
			require(state.recentPages == std::vector<DMUI_PageHandle>{ 10 },
				"stale recent-page handle survived a model rebuild");
		});

		runner.test("page row labels namespace duplicate page IDs by mod", [] {
			const NavigationPage firstPage{
				10, 1, "settings", "Settings", "General", {}, 0
			};
			const NavigationPage secondPage{
				20, 2, "settings", "Settings", "General", {}, 0
			};
			const NavigationClient firstClient{
				1, "first.mod", "First", DMUI_MAKE_VERSION(1, 0), {}
			};
			const NavigationClient secondClient{
				2, "second.mod", "Second", DMUI_MAKE_VERSION(1, 0), {}
			};

			const auto firstLabel = PageRowLabel(firstClient, firstPage);
			const auto secondLabel = PageRowLabel(secondClient, secondPage);
			require(firstLabel == "###DearModdingPage/first.mod/settings",
				"page row label retained visible padding or changed ID format");
			require(secondLabel == "###DearModdingPage/second.mod/settings",
				"page row label omitted the owning mod ID");
			require(firstLabel != secondLabel,
				"duplicate page IDs in different mods produced colliding row labels");
		});

		runner.test("client status snapshots remain independent and expire", [] {
			const auto start = StatusClock::time_point{};
			StatusModel model;
			require(model.SetClient(
						2,
						"Second",
						DMUI_STATUS_SEVERITY_WARNING,
						"Warning",
						start) == DMUI_RESULT_OK,
				"client warning status was rejected");
			require(model.SetClient(
						1,
						"First",
						DMUI_STATUS_SEVERITY_INFO,
						"Working",
						start) == DMUI_RESULT_OK,
				"client info status was rejected");
			auto statuses = model.SnapshotClientStatuses(start);
			require(
					statuses.size() == 2 &&
						statuses[0].client == 1 &&
						statuses[0].severity == DMUI_STATUS_SEVERITY_INFO &&
						statuses[1].client == 2 &&
						statuses[1].severity == DMUI_STATUS_SEVERITY_WARNING,
					"client statuses superseded another mod");
			const std::array rollupInput{
				ClientStatus{ 2, DMUI_STATUS_SEVERITY_WARNING },
				ClientStatus{ 1, DMUI_STATUS_SEVERITY_SUCCESS },
				ClientStatus{ 2, DMUI_STATUS_SEVERITY_INFO },
				ClientStatus{ 1, DMUI_STATUS_SEVERITY_ERROR },
				ClientStatus{ DMUI_INVALID_CLIENT_HANDLE,
					DMUI_STATUS_SEVERITY_ERROR }
			};
			const auto rollups = RollupClientStatuses(rollupInput);
			require(
					rollups.size() == 2 &&
						rollups[0].client == 1 &&
						rollups[0].severity == DMUI_STATUS_SEVERITY_ERROR &&
						rollups[1].client == 2 &&
						rollups[1].severity == DMUI_STATUS_SEVERITY_WARNING,
				"status rollup retained an invalid client or weaker severity");

			require(model.SetClient(
						2,
						"Second",
						DMUI_STATUS_SEVERITY_SUCCESS,
						"Recovered",
						start) == DMUI_RESULT_OK,
				"client recovery status was rejected");
			statuses = model.SnapshotClientStatuses(
				start + kTransientStatusLifetime);
			require(statuses.empty(),
				"transient client statuses did not expire independently");
			require(model.SetClient(
						DMUI_INVALID_CLIENT_HANDLE,
						"Invalid",
						DMUI_STATUS_SEVERITY_ERROR,
						"Error",
						start) == DMUI_RESULT_INVALID_ARGUMENT,
				"invalid client status handle was accepted");
		});

		runner.test("Phosphor manifest matches shipped font", [] {
			require(
				!kPhosphorIconGlyphs.empty() &&
					FindPhosphorIconGlyphOrZero("gear") ==
						PhosphorGlyph::kGear,
				"generated Phosphor resolver lost a representative icon");
			const auto font = std::filesystem::current_path() /
				"data/F4SE/Plugins/DearModdingUI/Fonts/Phosphor/Phosphor-Fill.ttf";
			require(
				Sha256(font) ==
					"a53f5d2630cab5e3b7536ecb9d69d71519a2190298c22b1f8d770dd37bc2940a",
				"Phosphor Fill font no longer matches @phosphor-icons/web@2.1.2");
		});

		runner.test("icon resolution follows semantic fallback chain", [] {
			const auto cloudSun =
				FindPhosphorIconGlyphOrZero("cloud-sun");
			const auto sunHorizon =
				FindPhosphorIconGlyphOrZero("sun-horizon");
			const auto lightbulb =
				FindPhosphorIconGlyphOrZero("lightbulb");
			require(
				ResolveIconGlyph(IconKind::kClient, "acorn") ==
						FindPhosphorIconGlyphOrZero("acorn") &&
					ResolveIconGlyph(IconKind::kClient, "acorn") != char32_t{},
				"full generated icon catalog was not consulted");
			require(ResolveIconGlyph(IconKind::kCategory, "gear") ==
					PhosphorGlyph::kGear,
				"category did not prefer an explicit icon name");
			require(ResolveClientIconGlyph(
						"puzzle-piece",
						"Performance",
						"Weather Overhaul") ==
					PhosphorGlyph::kPuzzlePiece,
				"explicit client icon did not win");
			const auto performance =
				FindPhosphorIconGlyphOrZero("speedometer");
			const auto weather = FindPhosphorIconGlyphOrZero("cloud-sun");
			require(ResolveClientIconGlyph({}, "Performance", "Unknown") ==
					performance,
				"client category concept was not inferred");
			require(ResolveClientIconGlyph({}, {}, "Weather Overhaul") == weather,
				"whole-word display-name concept was not inferred");
			require(ResolveClientIconGlyph({}, {}, "Weathering Steel") ==
					PhosphorGlyph::kQuestion,
				"display-name inference matched a concept substring");
			require(ResolveClientIconGlyph(
						{},
						{},
						"Audio Performance Toolkit") == performance,
				"longest deterministic concept did not win");
			require(ResolveClientIconGlyph(
						{},
						{},
						"Lighting Graphics Toolkit") ==
					FindPhosphorIconGlyphOrZero("image"),
				"equal-length concepts did not use the stable lexical tie-break");
			require(ResolveActionIconGlyph("clipboard-text") ==
						PhosphorGlyph::kClipboardText &&
					ResolveActionIconGlyph("trash") ==
						PhosphorGlyph::kTrash &&
					ResolveActionIconGlyph("arrow-counter-clockwise") ==
						PhosphorGlyph::kArrowCounterClockwise,
				"canonical action icon names did not resolve");
			require(ResolveActionIconGlyph("unknown") == char32_t{} &&
					ResolveActionIconGlyph("clear-cache") == char32_t{} &&
					ResolveActionIconGlyph("restore-settings") == char32_t{} &&
					ResolveClientIconGlyph({}, {}, "Unknown") ==
						PhosphorGlyph::kQuestion,
				"action and client misses lost distinct fallbacks");
			require(
				ResolveCategoryIconGlyph(
					"Lighting",
					"Community Shaders",
					"dear-modding.community-shaders",
					"cloud-sun",
					"Sun Horizon") == sunHorizon &&
					ResolveCategoryIconGlyph(
						"Lighting",
						"Community Shaders",
						"dear-modding.community-shaders",
						"cloud-sun",
						"unknown") == lightbulb &&
					ResolveClientIconGlyph(
						"cloud-sun",
						"Lighting",
						"Community Shaders") == cloudSun,
				"category override and client icon selection became coupled");
			require(
				ResolveIconGlyph(
					IconKind::kCategory,
					"unknown",
					"Lighting") == lightbulb,
				"category semantic fallback ignored its metadata");
			NavigationClient navigationClient;
			navigationClient.id = "dear-modding.community-shaders";
			navigationClient.displayName = "Community Shaders";
			navigationClient.iconName = "cloud-sun";
			NavigationCategory navigationCategory;
			navigationCategory.id = "lighting";
			navigationCategory.displayName = "Lighting";
			navigationCategory.iconName = "sun-horizon";
			navigationClient.categories.push_back(navigationCategory);
			require(
				ResolveNavigationClientIconGlyph(navigationClient) == cloudSun &&
					ResolveNavigationCategoryIconGlyph(
						navigationClient,
						navigationClient.categories.front()) == sunHorizon,
				"navigation client and category resolvers lost independent overrides");
			NavigationSearchEntry page;
			page.kind = NavigationItemKind::kPage;
			page.displayName = "Unknown";
			page.iconName = "sun-horizon";
			page.category = "Lighting";
			require(
				ResolveNavigationSearchEntryGlyph(page) == sunHorizon,
				"explicit page palette icon did not win");
			page.iconName = "unknown";
			page.displayName = "Lighting Feature";
			require(
				ResolveNavigationSearchEntryGlyph(page) == lightbulb,
				"page-name palette inference did not follow explicit fallback");
			page.displayName = "Unknown";
			page.category = "Weather";
			require(
				ResolveNavigationSearchEntryGlyph(page) == weather,
				"page category metadata was not inferred");
			page.category = "Unknown";
			require(
				ResolveNavigationSearchEntryGlyph(page) ==
					PhosphorGlyph::kFiles,
				"page palette miss lost the Files fallback");

			NavigationSearchEntry action;
			action.kind = NavigationItemKind::kAction;
			action.displayName = "Copy Records";
			action.iconName = "trash";
			require(
				ResolveNavigationSearchEntryGlyph(action) ==
					PhosphorGlyph::kTrash,
				"explicit action palette icon did not win");
			action.iconName = "unknown";
			action.displayName = "Audio Feature";
			require(
				ResolveNavigationSearchEntryGlyph(action) ==
					PhosphorGlyph::kSpeakerHigh,
				"action-label palette inference did not run");
			action.displayName = "Run";
			require(
				ResolveNavigationSearchEntryGlyph(action) ==
					PhosphorGlyph::kTerminalWindow,
				"action palette miss lost the terminal fallback");
			require(
				ResolveActionIconGlyph("unknown") == char32_t{},
				"toolbar actions stopped preserving their text-only contract");
			for (const auto settingsAction : kSettingsActionOrder)
			{
				const auto icon =
					ResolveSettingsActionButtonPresentation(
						settingsAction,
						true);
				const auto fallback =
					ResolveSettingsActionButtonPresentation(
						settingsAction,
						false);
				require(
					icon.glyph == SettingsActionGlyph(settingsAction) &&
						!icon.useTextFallback &&
						fallback.glyph == char32_t{} &&
						fallback.useTextFallback,
					"missing settings glyph did not select text fallback");
			}
		});

		runner.test("raw icon glyph validation rejects truncating code points", [] {
			require(
				IsRepresentableIconGlyph<ImWchar>(PhosphorGlyph::kSun) &&
					!IsRepresentableIconGlyph<ImWchar>(char32_t{}) &&
					!IsRepresentableIconGlyph<ImWchar>(
						char32_t{ 0x1E472 }) &&
					!IsValidUnicodeScalar(char32_t{ 0xD800 }),
				"raw glyph validation allowed zero, invalid, or truncating values");
		});

		runner.test("host colors preserve bytes and select icon tint", [] {
			constexpr std::array colors{
				HostAccentColor{ 0x00, 0x00, 0x00 },
				HostAccentColor{ 0x42, 0xFA, 0x60 },
				HostAccentColor{ 0x56, 0xB4, 0xE9 },
				HostAccentColor{ 0xFF, 0xFF, 0xFF }
			};
			for (const auto color : colors)
			{
				require(
					HostAccentFromImVec4(HostAccentToImVec4(color)) == color,
					"color editor conversion changed a stored component");
			}
			const auto accent = HostAccentToImVec4(colors[1]);
			const ImVec4 text{ 1.0f, 1.0f, 1.0f, 1.0f };
			require(
				SameColor(
					Theme::ResolveIconTint(
						Theme::IconColorMode::kColored,
						accent,
						text),
					accent) &&
					SameColor(
						Theme::ResolveIconTint(
							Theme::IconColorMode::kMonochrome,
							accent,
							text),
						text),
				"icon color mode did not select accent or text tint");
		});

		runner.test("host close and footer gear stay clear of adjacent content", [] {
			require(ShouldDrawHeaderClose(false, true),
				"undocked titleless host lost its close button");
			require(
					!ShouldDrawHeaderClose(true, true) &&
						!ShouldDrawHeaderClose(true, false) &&
						!ShouldDrawHeaderClose(false, false),
					"host close duplicated a native or docked close affordance");
			struct Case
			{
				float fontSize;
				float uiScale;
			};
			constexpr std::array cases{
				Case{ 16.0f, 1.0f },
				Case{ 18.0f, 1.25f },
				Case{ 21.0f, 1.5f },
				Case{ 28.0f, 2.0f }
			};
			for (const auto& test : cases)
			{
				const auto fontSize = test.fontSize * test.uiScale;
				const auto padding = 2.0f * test.uiScale;
				const auto spacing = 8.0f * test.uiScale;
				const auto iconSize = HostChromeIconSize(fontSize);
				const auto extent = HostChromeButtonExtent(fontSize, padding);
				const auto header = ResolveTrailingControlLayout(
					24.0f, 1896.0f, extent, spacing);
				const auto footer = ResolveTrailingControlLayout(
					36.0f, 1264.0f, extent, spacing);
				require(
						iconSize == fontSize * 1.5f &&
							extent > TitleBarButtonExtent(fontSize, padding),
						"host chrome did not use its larger icon scale");
				require(
						header.controlMaxX == 1896.0f &&
							header.controlMinX == 1896.0f - extent &&
							header.adjacentMaxX ==
								header.controlMinX - spacing,
						"close button geometry changed");
				require(
						footer.controlMaxX == 1264.0f &&
							footer.controlMinX == 1264.0f - extent &&
							footer.adjacentMaxX ==
								footer.controlMinX - spacing,
						"footer gear geometry changed");
				require(
						header.adjacentMaxX <= header.controlMinX &&
							footer.adjacentMaxX <= footer.controlMinX,
						"host chrome overlapped adjacent content");
			}
		});

		runner.test("settings action rows keep fixed non-overlapping geometry", [] {
			struct Case
			{
				float fontSize;
				float uiScale;
			};
			constexpr std::array cases{
				Case{ 16.0f, 1.0f },
				Case{ 18.0f, 1.25f },
				Case{ 21.0f, 1.5f },
				Case{ 28.0f, 2.0f }
			};
			for (const auto& test : cases)
			{
				const auto fontSize = test.fontSize * test.uiScale;
				const auto buttonPadding = 2.0f * test.uiScale;
				const auto framePadding = 8.0f * test.uiScale;
				const auto spacing = 4.0f * test.uiScale;
				const auto buttonExtent = TitleBarButtonExtent(
					fontSize, buttonPadding);
				const std::array widths{
					ActionButtonWidth(true, 0.0f, buttonExtent, framePadding),
					ActionButtonWidth(true, 0.0f, buttonExtent, framePadding),
					ActionButtonWidth(true, 0.0f, buttonExtent, framePadding)
				};
				const auto cleanWidthSum =
					ResolveSettingsActionButtonWidthSum(widths, false, 0);
				const auto dirtyWidthSum =
					ResolveSettingsActionButtonWidthSum(widths, true, 7);
				require(cleanWidthSum == dirtyWidthSum,
					"draft state or pending count changed action-row extent");
				const auto clean = ResolveHostSettingsTitleRowLayout(
					100.0f,
					1900.0f,
					cleanWidthSum,
					widths.size(),
					buttonExtent,
					spacing);
				const auto dirty = ResolveHostSettingsTitleRowLayout(
					100.0f,
					1900.0f,
					dirtyWidthSum,
					widths.size(),
					buttonExtent,
					spacing);
				const auto cleanPage = ResolvePageActionRowLayout(
					100.0f,
					1900.0f,
					cleanWidthSum,
					widths.size(),
					spacing);
				const auto dirtyPage = ResolvePageActionRowLayout(
					100.0f,
					1900.0f,
					dirtyWidthSum,
					widths.size(),
					spacing);
				const auto priorClose = ResolveTrailingControlLayout(
					100.0f, 1900.0f, buttonExtent, spacing);
				require(
					clean.titleMaxX <= clean.actionsMinX &&
						clean.actionsMaxX + spacing == clean.closeMinX &&
						clean.closeMinX == priorClose.controlMinX &&
						clean.closeMaxX == priorClose.controlMaxX,
					"settings title controls overlapped");

				auto position = clean.actionsMinX;
				for (const auto width : widths)
				{
					require(position + width <= clean.actionsMaxX,
						"settings action exceeded its reserved strip");
					position += width + spacing;
				}
				require(position - spacing == clean.actionsMaxX,
					"settings action spacing changed");
				require(
					clean.reservedWidth == dirty.reservedWidth &&
						clean.actionsMinX == dirty.actionsMinX &&
						clean.closeMinX == dirty.closeMinX,
					"dirty state changed settings title geometry");
				require(
					cleanPage.reservedWidth == dirtyPage.reservedWidth &&
						cleanPage.actionsMinX == dirtyPage.actionsMinX,
					"pending count changed Addictol settings action geometry");
			}
		});


		runner.test("sidebar layout commits outside the discardable settings preview", [] {
			auto state = BeginHostSettingsDraft(
				DefaultHostInterfaceSettings());
			state.draft.accentColor = { 0x00, 0x72, 0xB2 };
			CommitHostSettingsSidebarLayout(
				state,
				SidebarLayoutKind::DrillDown);
			require(
				state.committed.sidebarLayout == SidebarLayoutKind::DrillDown &&
					state.draft.sidebarLayout == SidebarLayoutKind::DrillDown,
				"immediate layout commit did not update both settings baselines");
			LeaveHostSettingsDraft(state);
			require(
				!state.active &&
					state.draft.sidebarLayout == SidebarLayoutKind::DrillDown &&
					state.draft.accentColor ==
						DefaultHostInterfaceSettings().accentColor &&
					!HostSettingsDraftDiffers(state),
				"discarding cosmetic previews also discarded the saved layout");

			state = BeginHostSettingsDraft(state.committed);
			ResetHostSettingsDraft(state);
			CommitHostSettingsSidebarLayout(
				state,
				state.draft.sidebarLayout);
			RevertHostSettingsDraft(state);
			require(
				state.committed.sidebarLayout == DEFAULT_SIDEBAR_LAYOUT &&
					state.draft == state.committed,
				"reset and revert disagreed with the immediately saved layout");
		});

		runner.test("host settings draft applies once and discards safely", [] {
			auto state = BeginHostSettingsDraft(
				DefaultHostInterfaceSettings());
			const auto unchanged = ApplyHostSettingsDraft(state);
			require(!unchanged.settings,
				"unchanged settings draft produced a commit");

			const HostInterfaceSettings changed{
				Theme::IconColorMode::kMonochrome,
				SidebarLayoutKind::DrillDown,
				{ 0xD5, 0x5E, 0x00 },
				0.80f,
				{ 0x12, 0x12, 0x12 },
				0.70f,
				false,
				0.75f,
				1.75f,
				"Atkinson Hyperlegible",
				"Home"
			};
			state.draft = changed;
			const auto applied = ApplyHostSettingsDraft(state);
			require(applied.settings && *applied.settings == changed,
				"settings draft did not commit every changed field");
			require(state.committed == changed &&
					!HostSettingsDraftDiffers(state),
				"applied settings draft remained dirty");

			const auto repeated = ApplyHostSettingsDraft(state);
			require(!repeated.settings,
				"applied settings draft committed more than once");

			state = BeginHostSettingsDraft(changed);
			state.draft.accentColor = { 0xE6, 0x9F, 0x00 };
			RevertHostSettingsDraft(state);
			require(state.draft == changed &&
					!HostSettingsDraftDiffers(state),
				"revert did not restore committed preview fields");

			state.draft.accentColor = { 0xE6, 0x9F, 0x00 };
			LeaveHostSettingsDraft(state);
			require(!state.active && state.draft == changed,
				"leaving settings did not discard the draft");

			state = BeginHostSettingsDraft(changed);
			ResetHostSettingsDraft(state);
			require(state.draft == DefaultHostInterfaceSettings(),
				"reset did not populate shipped defaults");
			require(state.committed == changed &&
					HostSettingsDraftDiffers(state),
				"reset committed instead of updating the draft");
		});

		runner.test("host settings preview isolates typography rebuilds", [] {
			const auto committed = DefaultHostInterfaceSettings();
			auto draft = committed;
			require(!HostSettingsDraftRequiresAtlasRebuild(committed, draft),
				"unchanged settings requested an atlas rebuild");
			for (const bool changeFont : { false, true })
			{
				draft = committed;
				if (changeFont)
					draft.bodyFontFamily = "Atkinson Hyperlegible";
				else
					draft.uiScale = 1.25f;
				require(HostSettingsDraftRequiresAtlasRebuild(committed, draft),
					"an independent typography change did not request an atlas rebuild");
				require(PreviewHostInterfaceSettings(draft) ==
						PreviewHostInterfaceSettings(committed),
					"typography settings leaked into the live preview");
			}

			const auto checkAppearance = [&committed](const auto& appearance) {
				require(PreviewHostInterfaceSettings(appearance) !=
							PreviewHostInterfaceSettings(committed) &&
						!HostSettingsDraftRequiresAtlasRebuild(committed, appearance),
					"appearance was omitted from preview or requested an atlas rebuild");
			};
			draft = committed;
			draft.accentColor = { 0x00, 0x72, 0xB2 };
			checkAppearance(draft);
			draft = committed;
			draft.paletteBackgroundColor = { 0x12, 0x12, 0x12 };
			draft.paletteBackgroundOpacity = 0.70f;
			checkAppearance(draft);
			draft = committed;
			draft.sidebarLayout = SidebarLayoutKind::TwoPane;
			checkAppearance(draft);
		});

		runner.test("host settings persistence round trips every stored value", [] {
			std::array settings{
				DefaultHostInterfaceSettings(),
				HostInterfaceSettings{
					Theme::IconColorMode::kMonochrome,
					SidebarLayoutKind::TwoPane,
					{ 0x00, 0x72, 0xB2 },
					0.80f,
					{ 0x12, 0x12, 0x12 },
					0.70f,
					false,
					0.75f,
					1.75f,
					"Atkinson Hyperlegible"
				},
				HostInterfaceSettings{
					Theme::IconColorMode::kColored,
					SidebarLayoutKind::DrillDown,
					{ 0xD5, 0x5E, 0x00 },
					kMinWindowBackgroundOpacity,
					{ 0x02, 0x02, 0x02 },
					kMinPaletteBackgroundOpacity,
					true,
					kMinBackgroundBlurStrength,
					Theme::kMinUserScale,
					"Jost",
					"Delete"
				}
			};
			for (const auto& runtime : settings)
			{
				const auto persisted = EncodeHostInterfaceSettings(runtime);
				require(
					DecodeHostInterfaceSettings(persisted) == runtime,
					"stored host settings did not round trip");
				require(
					EncodeHostInterfaceSettings(
						DecodeHostInterfaceSettings(persisted)) == persisted,
					"encoded host settings did not round trip");
			}
		});

		runner.test("host settings clamp malformed persisted values and reset", [] {
			PersistedHostInterfaceSettings persisted;
			persisted.accentColor = "#GG00ZZ";
			persisted.windowBackgroundOpacity = -5.0f;
			persisted.paletteBackgroundColor = "#palette";
			persisted.paletteBackgroundOpacity =
				std::numeric_limits<float>::infinity();
			persisted.backgroundBlurStrength =
				std::numeric_limits<float>::infinity();
			persisted.uiScale = 99.0f;
			persisted.bodyFontFamily = "..\\escaped";
			persisted.menuToggleKey = "PageUp";
			const auto decoded = DecodeHostInterfaceSettings(persisted);
			require(
				decoded.accentColor == kDefaultHostAccentColor,
				"malformed accent did not fall back");
			require(
				decoded.windowBackgroundOpacity ==
					kMinWindowBackgroundOpacity,
				"window opacity did not clamp");
			require(
				decoded.paletteBackgroundColor ==
					kDefaultPaletteBackgroundColor,
				"malformed palette background did not fall back");
			require(
				decoded.paletteBackgroundOpacity ==
					kDefaultPaletteBackgroundOpacity,
				"non-finite palette opacity did not fall back");
			require(
				decoded.backgroundBlurStrength ==
					kDefaultBackgroundBlurStrength,
				"non-finite blur strength did not fall back");
			require(
				decoded.uiScale == Theme::kMaxUserScale,
				"UI scale did not clamp");
			require(
				decoded.bodyFontFamily == kDefaultBodyFontFamily,
				"malformed font family did not fall back");
			require(
				decoded.menuToggleKey ==
					MenuToggleKeyName(kMenuDefaultToggleKey),
				"malformed toggle key did not fall back");
			require(
				DefaultHostInterfaceSettings() == HostInterfaceSettings{},
				"reset did not restore shipped defaults");
		});

		runner.test("host settings health distinguishes absent valid and malformed files", [] {
			const auto root =
				std::filesystem::current_path() /
				".Build" /
				"Tests" /
				"health-config-fixture";
			std::error_code error;
			std::filesystem::remove_all(root, error);
			std::filesystem::create_directories(root, error);
			const auto path = root / "DearModdingUI.toml";

			auto loaded = LoadHostInterfaceSettings(path);
			require(
				loaded.disposition == HostSettingsLoadDisposition::kMissing &&
					loaded.detail.find("Using defaults") != std::string::npos,
				"an absent optional host config was not healthy");

			std::ofstream(path)
				<< "[Additional]\n"
				<< "sMenuToggleKey = \"End\"\n"
				<< "sMenuSidebarLayout = \"tree\"\n";
			loaded = LoadHostInterfaceSettings(path);
			require(
				loaded.disposition == HostSettingsLoadDisposition::kLoaded &&
					loaded.settings.menuToggleKey == "End",
				"a valid host config did not report loaded");

			std::ofstream(path, std::ios::trunc)
				<< "[Additional\n";
			loaded = LoadHostInterfaceSettings(path);
			require(
				loaded.disposition == HostSettingsLoadDisposition::kFailed &&
					loaded.detail.find("correct or remove") !=
						std::string::npos,
				"a malformed host config did not retain actionable failure");

			loaded = LoadHostInterfaceSettings(root);
			require(
				loaded.disposition == HostSettingsLoadDisposition::kFailed,
				"an unreadable host config path was treated as loaded");
			std::filesystem::remove_all(root, error);
		});

		runner.test("host settings health reports actual accepted fallbacks", [] {
			const auto root =
				std::filesystem::current_path() /
				".Build" /
				"Tests" /
				"health-config-corrections";
			std::error_code error;
			std::filesystem::remove_all(root, error);
			std::filesystem::create_directories(root, error);
			const auto path = root / "DearModdingUI.toml";
			std::ofstream(path)
				<< "[Additional]\n"
				<< "sMenuToggleKey = \"PageUp\"\n"
				<< "sMenuSidebarLayout = \"columns\"\n"
				<< "fMenuUiScale = 99.0\n"
				<< "sMenuAccentColor = \"bad\"\n"
				<< "sMenuBodyFontFamily = \"..\\\\escaped\"\n";

			const auto loaded = LoadHostInterfaceSettings(path);
			HostSettingsHealthState state;
			state.RecordLoad(loaded);
			auto observation = state.Observation();
			require(
				loaded.disposition == HostSettingsLoadDisposition::kCorrected &&
					loaded.settings.menuToggleKey == "End" &&
					observation.state == HealthState::kDegraded &&
					observation.reason.find(
						"sMenuToggleKey \"PageUp\" used \"End\"") !=
						std::string::npos,
				"configuration health reported a stale toggle fallback");

			state.RecordSaveFailure("access denied");
			observation = state.Observation();
			require(
				observation.state == HealthState::kDegraded &&
					observation.reason.find("access denied") !=
						std::string::npos &&
					observation.reason.find("sMenuToggleKey") !=
						std::string::npos,
				"a save failure erased the active load correction");

			state.RecordSaveSuccess(path.string());
			observation = state.Observation();
			require(
				observation.state == HealthState::kReady &&
					observation.reason.find("Saved accepted settings") !=
						std::string::npos,
				"a successful write did not resolve persisted configuration health");
			std::filesystem::remove_all(root, error);
		});

		runner.test("settings saves round trip through normal replacement and the error 17 fallback", [] {
			const auto root =
				std::filesystem::current_path() /
				".Build" /
				"Tests" /
				"settings-persistence";
			std::error_code error;
			std::filesystem::remove_all(root, error);
			std::filesystem::create_directories(root, error);
			const auto path = root / "DearModdingUI.toml";
			std::ofstream(path) << "old settings";

			PersistedHostInterfaceSettings settings;
			settings.menuToggleKey = "Home";
			settings.sidebarLayout = "twopane";
			settings.hotkeys.emplace("example.action", "Ctrl+H");
			const auto saved = PersistHostInterfaceSettings(path, settings);
			require(saved.saved && !saved.usedCrossVolumeFallback,
				"a same-volume settings replacement did not succeed normally");
			require(!std::filesystem::exists(path.string() + ".tmp"),
				"a successful settings replacement retained its temporary file");

			const auto loaded = LoadHostInterfaceSettings(path);
			require(
				loaded.disposition == HostSettingsLoadDisposition::kLoaded &&
					loaded.settings == settings,
				"persisted host settings were not loadable through production parsing");

			settings.menuToggleKey = "Insert";
			SettingsMoveProbe probe;
			probe.firstError = ERROR_NOT_SAME_DEVICE;
			probe.performSecondMove = true;
			const auto retried = PersistHostInterfaceSettings(
				path,
				settings,
				{ &probe, &ProbeSettingsMove });
			require(
				retried.saved &&
					retried.usedCrossVolumeFallback &&
					probe.calls == 2 &&
					(probe.flags[0] & MOVEFILE_COPY_ALLOWED) == 0 &&
					(probe.flags[1] & MOVEFILE_COPY_ALLOWED) != 0,
				"ERROR_NOT_SAME_DEVICE did not trigger the scoped copy fallback");
			require(
				LoadHostInterfaceSettings(path).settings == settings,
				"the cross-volume fallback did not install the serialized settings");
			require(!std::filesystem::exists(path.string() + ".tmp"),
				"the copy fallback retained its temporary file");
			std::filesystem::remove_all(root, error);
		});

		runner.test("failed settings saves preserve the configuration and explain the actual error", [] {
			const auto root =
				std::filesystem::current_path() /
				".Build" /
				"Tests" /
				"settings-save-failure";
			std::error_code error;
			std::filesystem::remove_all(root, error);
			std::filesystem::create_directories(root, error);
			const auto path = root / "DearModdingUI.toml";
			std::ofstream(path)
				<< "[Additional]\n"
				<< "sMenuToggleKey = \"Home\"\n";
			auto temporary = path;
			temporary += L".tmp";

			PersistedHostInterfaceSettings settings;
			settings.menuToggleKey = "Insert";
			for (const auto firstError : { ERROR_ACCESS_DENIED, ERROR_NOT_SAME_DEVICE })
			{
				SettingsMoveProbe probe;
				probe.firstError = firstError;
				const auto saved = PersistHostInterfaceSettings(
					path, settings, { &probe, &ProbeSettingsMove });
				const auto explanation = DescribeWindowsError(ERROR_ACCESS_DENIED);
				require(!saved.saved && saved.nativeError == ERROR_ACCESS_DENIED &&
							probe.calls == (firstError == ERROR_NOT_SAME_DEVICE ? 2 : 1) &&
							(probe.flags[0] & MOVEFILE_COPY_ALLOWED) == 0 &&
							!std::filesystem::exists(temporary),
					"failed replacement used an unrelated fallback or left temporary data");
				require(explanation != "No system explanation is available" &&
							saved.detail.find(explanation) != std::string::npos &&
							saved.detail.find("Windows error 5") != std::string::npos,
					"replacement failure omitted its readable system explanation");
				if (firstError == ERROR_NOT_SAME_DEVICE)
				{
					require((probe.flags[1] & MOVEFILE_COPY_ALLOWED) != 0 &&
								saved.detail.find(DescribeWindowsError(ERROR_NOT_SAME_DEVICE)) != std::string::npos,
						"failed cross-volume retry lost the original error 17 explanation");
				}
				const auto loaded = LoadHostInterfaceSettings(path);
				require(loaded.disposition == HostSettingsLoadDisposition::kLoaded &&
							loaded.settings.menuToggleKey == "Home",
					"failed replacement changed the stored toggle key");
			}

			std::filesystem::create_directory(temporary);
			const auto saved = PersistHostInterfaceSettings(path, settings);
			const auto explanation = DescribeWindowsError(saved.nativeError);
			const auto loaded = LoadHostInterfaceSettings(path);
			require(
				!saved.saved &&
					saved.nativeError != ERROR_SUCCESS &&
					saved.detail.find("Opening temporary settings file") !=
						std::string::npos &&
					explanation != "No system explanation is available" &&
					saved.detail.find(explanation) != std::string::npos &&
					loaded.disposition == HostSettingsLoadDisposition::kLoaded &&
					loaded.settings.menuToggleKey == "Home",
				"a real temporary open failure changed the current TOML or omitted its system explanation");
			std::filesystem::remove_all(root, error);
		});

		runner.test("typography health distinguishes requested fallback and failure", [] {
			Theme::TypographyLoadOutcome ready;
			ready.requestedFamily = "Jost";
			ready.effectiveFamily = "Jost";
			ready.requestedFamilyFound = true;
			ready.requestedBodyLoaded = true;
			ready.rolesLoaded.fill(true);
			ready.iconsLoaded = true;
			ready.usableAtlas = true;
			auto observation = Theme::ClassifyTypographyHealth(ready);
			require(
				observation.state == HealthState::kReady &&
					observation.reason.find("Phosphor") != std::string::npos,
				"complete typography did not report ready");

			auto fallback = ready;
			fallback.requestedFamily = "Missing Family";
			fallback.requestedFamilyFound = false;
			fallback.rolesLoaded[
				static_cast<size_t>(Theme::FontRole::kHeading)] = false;
			fallback.iconsLoaded = false;
			observation = Theme::ClassifyTypographyHealth(fallback);
			require(
				observation.state == HealthState::kDegraded &&
					observation.reason.find("Missing Family") !=
						std::string::npos &&
					observation.reason.find("heading") !=
						std::string::npos &&
					observation.reason.find("text-only") !=
						std::string::npos,
				"typography fallback health lost its concrete causes");

			fallback.emergencyFontUsed = true;
			fallback.effectiveFamily = "Built-in fallback";
			observation = Theme::ClassifyTypographyHealth(fallback);
			require(
				observation.state == HealthState::kDegraded &&
					observation.reason.find("emergency") !=
						std::string::npos,
				"usable emergency typography was reported as failed");

			fallback.usableAtlas = false;
			fallback.emergencyFontUsed = false;
			observation = Theme::ClassifyTypographyHealth(fallback);
			require(
				observation.state == HealthState::kFailed &&
					observation.reason.find("font atlas") !=
						std::string::npos,
				"missing typography capability was not failed");
		});

		runner.test("font families enumerate regular faces and fall back", [] {
			struct RemoveTree
			{
				std::filesystem::path path;
				~RemoveTree()
				{
					std::error_code error;
					std::filesystem::remove_all(path, error);
				}
			};

			const auto root =
				std::filesystem::temp_directory_path() /
				"AddictolDearModdingUIFontCatalog";
			std::error_code error;
			std::filesystem::remove_all(root, error);
			const RemoveTree cleanup{ root };
			std::filesystem::create_directories(root / "Jost", error);
			std::filesystem::create_directories(
				root / "Atkinson Hyperlegible", error);
			std::filesystem::create_directories(root / "Phosphor", error);
			std::ofstream(root / "Jost" / "Jost-Regular.ttf").put('\0');
			std::ofstream(
				root /
				"Atkinson Hyperlegible" /
				"AtkinsonHyperlegible-Bold.ttf").put('\0');
			std::ofstream(
				root /
				"Atkinson Hyperlegible" /
				"AtkinsonHyperlegible-Regular.ttf").put('\0');
			std::ofstream(
				root /
				"Phosphor" /
				"Phosphor-Fill.ttf").put('\0');

			const auto families = FontCatalog::Enumerate(root);
			require(families.size() == 2,
				"font family folders were not enumerated safely");
			const auto* atkinson = FontCatalog::Resolve(
				"atkinson hyperlegible", families, "Jost");
			require(
				atkinson &&
					atkinson->name == "Atkinson Hyperlegible" &&
					atkinson->regularFile.ends_with(
						"AtkinsonHyperlegible-Regular.ttf"),
				"font family did not choose its regular face");
			const auto* fallback = FontCatalog::Resolve(
				"Missing Family", families, "Jost");
			require(fallback && fallback->name == "Jost",
				"missing font family did not fall back to Jost");
		});

		runner.test("one-page navigation and failed-page presentation remain stable", [] {
			Registry registry;
			CallbackState state;
			const auto client = AddClient(registry, "single.mod", "Single", state);
			AddCategory(registry, client, "general", "General");
			const auto page = AddPage(registry, client, "only", "Only", "general", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			require(registry.Freeze(), "registry did not freeze");
			const auto& navigation = registry.Navigation();
			require(navigation.clients.size() == 1, "single client was omitted");
			require(navigation.clients[0].categories.size() == 1, "single category was omitted");
			require(navigation.FirstPage() == page, "single page was not the fallback");
			require(DecidePagePresentation(navigation.FindPage(page), false) ==
					PagePresentation::kContent,
				"healthy page did not present content");
			require(DecidePagePresentation(navigation.FindPage(page), true) ==
					PagePresentation::kFailure,
				"failed page did not present a stable error");
			registry.MarkPageFailed(page);
			require(registry.PageFailed(page), "failed page state was not retained");
			require(registry.HasSettingsPages(), "failed page removed the host's settings shell");
			require(DecidePagePresentation(nullptr, false) == PagePresentation::kEmpty,
				"missing page did not present an empty state");
		});

		runner.test("theme scaling clamps and composes user scale", [] {
			const auto minimum = Theme::ResolveFontSize(1);
			const auto baseline = Theme::ResolveFontSize(1080);
			const auto highResolution = Theme::ResolveFontSize(2160);
			const auto maximum = Theme::ResolveFontSize(8640);
			require(
				minimum == Theme::kMinFontSize &&
					baseline > minimum &&
					highResolution > baseline &&
					maximum == Theme::kMaxFontSize,
				"font scaling lost its resolution boundaries");
			require(
				ResolveUiScale(1.0f, 1080) == 1.0f &&
					ResolveUiScale(2.0f, 1080) == 1.0f &&
					ResolveUiScale(1.0f, 2160) == 2.0f &&
					ResolveUiScale(
						1.0f,
						1080,
						Theme::kMaxUserScale) == 2.0f,
				"resolution and accessibility scaling no longer compose");
		});

		runner.test("absent icons reserve no navigation layout space", [] {
			const auto absent = DecideInlineIconLayout(false, 80.0f, 20.0f, 20.0f, 4.0f);
			require(!absent.drawIcon &&
					absent.iconSize == 0.0f &&
					absent.textOffset == 0.0f &&
					absent.contentWidth == 80.0f &&
					absent.contentHeight == 20.0f,
				"absent icon left a blank layout box");

			const auto present = DecideInlineIconLayout(true, 80.0f, 18.0f, 20.0f, 4.0f);
			require(present.drawIcon &&
					present.iconSize == 20.0f &&
					present.textOffset == 24.0f &&
					present.contentWidth == 104.0f &&
					present.contentHeight == 20.0f,
				"present icon layout did not align to the font");
		});

		runner.test("cursor ownership follows modal visibility", [] {
			const auto overlay = DecideCursorPresentation(false);
			require(!overlay.captureInput &&
					!overlay.hideOperatingSystemCursor &&
					!overlay.drawSoftwareCursor &&
					!overlay.drawCustomCursor,
				"overlay-only drawing acquired a cursor");

			const auto modal = DecideCursorPresentation(true);
			require(modal.captureInput &&
					modal.hideOperatingSystemCursor &&
					modal.drawSoftwareCursor &&
					!modal.drawCustomCursor,
				"modal drawing did not own exactly one software cursor");
			require(
				static_cast<uint32_t>(modal.drawSoftwareCursor) +
						static_cast<uint32_t>(modal.drawCustomCursor) ==
					1,
				"the modal host did not present exactly one cursor");

			require(DecideCursorTransition(false, true) ==
					CursorOwnershipTransition::kAcquire,
				"menu open did not acquire cursor ownership");
			require(DecideCursorTransition(true, false) ==
					CursorOwnershipTransition::kRelease,
				"menu close did not release cursor ownership");
			require(DecideCursorTransition(true, true) ==
					CursorOwnershipTransition::kNone,
				"steady modal state retriggered ownership");
			require(DecideCursorTransition(false, false) ==
					CursorOwnershipTransition::kNone,
				"steady overlay state changed ownership");
		});

		runner.test("carrier menu open and close messages remain balanced", [] {
			CarrierMenu::State state{};
			require(
				CarrierMenu::Transition(state, CarrierMenu::Event::kOpen) ==
					CarrierMenu::Action::kShow,
				"the first modal open did not show the carrier");
			require(
				CarrierMenu::Transition(state, CarrierMenu::Event::kOpen) ==
					CarrierMenu::Action::kNone,
				"a repeated modal frame showed the carrier twice");
			require(
				CarrierMenu::Transition(state, CarrierMenu::Event::kClose) ==
					CarrierMenu::Action::kHide,
				"the modal close did not hide the carrier");
			require(
				CarrierMenu::Transition(state, CarrierMenu::Event::kClose) ==
					CarrierMenu::Action::kNone,
				"a repeated close hid the carrier twice");
			require(!state.open, "the balanced sequence retained cursor ownership");
		});

		runner.test("carrier menu cleanup paths hide exactly one open entry", [] {
			constexpr std::array cleanupEvents{
				CarrierMenu::Event::kShutdown,
				CarrierMenu::Event::kBackendFailure,
				CarrierMenu::Event::kRetarget,
				CarrierMenu::Event::kGameTransition,
				CarrierMenu::Event::kOverlayOnly
			};
			for (const auto event : cleanupEvents)
			{
				CarrierMenu::State state{};
				require(
					CarrierMenu::Transition(state, CarrierMenu::Event::kOpen) ==
						CarrierMenu::Action::kShow,
					"a cleanup scenario did not establish an open carrier");
				require(
					CarrierMenu::Transition(state, event) ==
						CarrierMenu::Action::kHide,
					"a cleanup scenario did not balance its show");
				require(
					CarrierMenu::Transition(state, event) ==
						CarrierMenu::Action::kNone,
					"a cleanup scenario queued a second hide");
				require(!state.open, "a cleanup scenario retained cursor ownership");
			}

			CarrierMenu::State overlay{};
			require(
				CarrierMenu::Transition(
					overlay,
					CarrierMenu::Event::kOverlayOnly) ==
					CarrierMenu::Action::kNone,
				"an overlay-only frame opened or hid a carrier");
		});

		runner.test("registry freeze rejects late clients and pages", [] {
			Registry registry;
			CallbackState state;
			const auto client = AddClient(registry, "freeze.mod", "Freeze", state);
			require(registry.Freeze(), "registry did not freeze");
			auto lateClient = Client("late.mod", "Late", state);
			DMUI_ClientHandle clientHandle{};
			require(registry.RegisterClient(
						&lateClient, &clientHandle) ==
					DMUI_RESULT_REGISTRATION_CLOSED,
				"late client was accepted");
			auto latePage = Page("late", "Late", "General", 0, DMUI_PAGE_KIND_SETTINGS, state);
			DMUI_PageHandle pageHandle{};
			require(registry.RegisterPage(client, &latePage, &pageHandle) ==
					DMUI_RESULT_REGISTRATION_CLOSED,
				"late page was accepted");
		});

		runner.test("ready and unavailable notifications happen exactly once", [] {
			CallbackState readyState;
			Registry readyRegistry;
			(void)AddClient(readyRegistry, "ready.mod", "Ready", readyState);
			require(readyRegistry.Freeze(), "ready registry did not freeze");
			const DMUI_HostReadyInfo info{
				sizeof(DMUI_HostReadyInfo),
				DMUI_API_VERSION_CURRENT
			};
			readyRegistry.NotifyReady(info);
			readyRegistry.NotifyReady(info);
			readyRegistry.NotifyUnavailable(DMUI_UNAVAILABLE_BACKEND_FAILED);
			require(readyState.ready == 1 && readyState.unavailable == 0,
				"ready client received duplicate or mixed notifications");

			CallbackState unavailableState;
			Registry unavailableRegistry;
			(void)AddClient(
				unavailableRegistry, "fallback.mod", "Fallback", unavailableState);
			require(unavailableRegistry.Freeze(), "unavailable registry did not freeze");
			unavailableRegistry.NotifyUnavailable(DMUI_UNAVAILABLE_BACKEND_FAILED);
			unavailableRegistry.NotifyUnavailable(DMUI_UNAVAILABLE_HOST_DISABLED);
			unavailableRegistry.NotifyReady(info);
			require(unavailableState.ready == 0 && unavailableState.unavailable == 1,
				"unavailable client received duplicate or mixed notifications");
			require(unavailableState.reason == DMUI_UNAVAILABLE_BACKEND_FAILED,
				"unavailable reason changed");
		});

		runner.test("throwing client callbacks are isolated by host guards", [] {
			const DMUI_HostReadyInfo info{
				sizeof(DMUI_HostReadyInfo),
				DMUI_API_VERSION_CURRENT
			};

			CallbackState readyState;
			Registry readyRegistry;
			auto readyClient = Client("throw-ready.mod", "Throw Ready", readyState);
			readyClient.onHostReady = &ThrowReady;
			DMUI_ClientHandle readyHandle{};
			require(readyRegistry.RegisterClient(
						&readyClient, &readyHandle) == DMUI_RESULT_OK,
				"throwing ready client was not registered");
			AddCategory(readyRegistry, readyHandle, "general", "General");
			const auto readyPage = AddPage(
				readyRegistry,
				readyHandle,
				"settings",
				"Settings",
				"general",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				readyState);
			require(readyRegistry.Freeze(), "throwing ready registry did not freeze");
			readyRegistry.NotifyReady(info);
			require(readyRegistry.PageFailed(readyPage),
				"a client with a throwing ready callback remained drawable");

			CallbackState drawState;
			Registry drawRegistry;
			const auto drawClient = AddClient(
				drawRegistry, "throw-draw.mod", "Throw Draw", drawState);
			AddCategory(drawRegistry, drawClient, "general", "General");
			auto drawPageDescriptor = Page(
				"settings", "Settings", "general", 0, DMUI_PAGE_KIND_SETTINGS, drawState);
			drawPageDescriptor.draw = &ThrowDrawPage;
			DMUI_PageHandle drawPage{};
			require(drawRegistry.RegisterPage(
						drawClient, &drawPageDescriptor, &drawPage) == DMUI_RESULT_OK,
				"throwing draw page was not registered");
			require(drawRegistry.InvokePage(drawPage) == DMUI_RESULT_CALLBACK_FAILED,
				"a throwing page escaped its host guard");
			require(drawRegistry.InvokePage(drawPage) == DMUI_RESULT_CALLBACK_FAILED,
				"a faulted page was invoked again");

			auto drawActionDescriptor = Action(
				"throw", "Throw", nullptr, 0, drawState);
			drawActionDescriptor.callback = &ThrowDraw;
			DMUI_ActionHandle drawAction{};
			require(drawRegistry.RegisterAction(
						drawClient, &drawActionDescriptor, &drawAction) ==
					DMUI_RESULT_OK,
				"throwing action was not registered");
			require(drawRegistry.InvokeAction(drawAction) ==
					DMUI_RESULT_CALLBACK_FAILED,
				"a throwing action escaped its host guard");
			require(drawRegistry.ActionFailed(drawAction),
				"faulted action was not permanently disabled");
			require(drawRegistry.InvokeAction(drawAction) ==
					DMUI_RESULT_CALLBACK_FAILED,
				"a faulted action was invoked again");

			CallbackState unavailableState;
			CallbackState healthyState;
			Registry unavailableRegistry;
			auto unavailableClient = Client(
				"throw-unavailable.mod", "Throw Unavailable", unavailableState);
			unavailableClient.onHostUnavailable = &ThrowUnavailable;
			DMUI_ClientHandle unavailableHandle{};
			require(unavailableRegistry.RegisterClient(
						&unavailableClient,
						&unavailableHandle) == DMUI_RESULT_OK,
				"throwing unavailable client was not registered");
			(void)AddClient(
				unavailableRegistry, "healthy.mod", "Healthy", healthyState);
			require(unavailableRegistry.Freeze(), "unavailable registry did not freeze");
			unavailableRegistry.NotifyUnavailable(DMUI_UNAVAILABLE_BACKEND_FAILED);
			require(healthyState.unavailable == 1,
				"a throwing unavailable callback blocked the next client");
		});

		runner.test("overlay frame demand is reference counted and never makes settings demand frames", [] {
			Registry registry;
			CallbackState state;
			const auto client = AddClient(registry, "frames.mod", "Frames", state);
			AddCategory(registry, client, "general", "General");
			AddCategory(registry, client, "hud", "HUD");
			const auto settings = AddPage(registry, client, "settings", "Settings", "general", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			const auto overlay = AddPage(registry, client, "overlay", "Overlay", "hud", 0,
				DMUI_PAGE_KIND_OVERLAY, state);
			require(registry.RequestFrame(client, settings) == DMUI_RESULT_INVALID_PAGE_KIND,
				"settings page requested overlay frames");
			require(registry.RequestFrame(client, overlay) == DMUI_RESULT_OK,
				"first overlay request failed");
			require(registry.RequestFrame(client, overlay) == DMUI_RESULT_OK,
				"second overlay request failed");
			require(registry.DemandedOverlayCount() == 1, "one overlay counted twice");
			require(registry.ReleaseFrame(client, overlay) == DMUI_RESULT_OK,
				"first overlay release failed");
			require(registry.IsFrameDemanded(overlay), "one release cleared two requests");
			require(registry.ReleaseFrame(client, overlay) == DMUI_RESULT_OK,
				"second overlay release failed");
			require(!registry.IsFrameDemanded(overlay), "balanced releases left demand");
			require(registry.ReleaseFrame(client, overlay) == DMUI_RESULT_NO_FRAME_DEMAND,
				"unbalanced release was accepted");
			require(registry.HasSettingsPages(), "overlay behavior hid modal settings");
		});

		runner.test("page callbacks receive userdata and failed lookups stay isolated", [] {
			Registry registry;
			CallbackState state;
			const auto client = AddClient(registry, "draw.mod", "Draw", state);
			AddCategory(registry, client, "general", "General");
			const auto page = AddPage(registry, client, "draw", "Draw", "general", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			require(registry.InvokePage(page) == DMUI_RESULT_OK, "draw callback failed");
			require(state.draws == 1, "draw callback did not receive userdata");
			require(registry.InvokePage(page + 1) == DMUI_RESULT_PAGE_NOT_FOUND,
				"unknown page was invoked");
			const auto action = AddAction(
				registry, client, "draw", "Draw", nullptr, 0, state);
			require(registry.InvokeAction(action) == DMUI_RESULT_OK,
				"action callback failed");
			require(state.draws == 2, "action callback did not receive userdata");
			require(registry.InvokeAction(action + 1) == DMUI_RESULT_ACTION_NOT_FOUND,
				"unknown action was invoked");
		});

		runner.test("page UI errors cross the callback boundary without becoming false draws", [] {
			Registry registry;
			CallbackState state;
			const auto client =
				AddClient(registry, "ui-error.mod", "UI error", state);
			AddCategory(registry, client, "general", "General");
			auto descriptor =
				Page("ui-error", "UI error", "general", 0,
					DMUI_PAGE_KIND_SETTINGS, state);
			descriptor.draw = &UnsupportedDrawPage;
			DMUI_PageHandle page{};
			require(
				registry.RegisterPage(client, &descriptor, &page) ==
					DMUI_RESULT_OK,
				"UI-error page registration failed");
			require(
				registry.InvokePage(page) == DMUI_RESULT_UNSUPPORTED_ABI &&
					registry.PageFailed(page) &&
					registry.InvokePage(page) == DMUI_RESULT_CALLBACK_FAILED,
				"UI error was not surfaced and isolated at the callback boundary");
		});
	}
}
