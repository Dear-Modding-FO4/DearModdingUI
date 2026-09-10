#pragma once

#include "../Harness.h"

#include <Platform/files/ExternalOpen.h>
#include <DearModdingUI/pages/Health.h>
#include <DearModdingUI/host/Registry.h>
#include <DearModdingUI/host/UIAdapter.h>

#include <DearModdingUI/Client.h>

#include <imgui/imgui.h>

#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace vmm_tests::support::host
{
	using namespace DearModdingUI;

	struct CallbackState
	{
		uint32_t ready{};
		uint32_t unavailable{};
		uint32_t draws{};
		DMUI_UnavailableReason reason{ DMUI_UNAVAILABLE_NONE };
	};

	struct PageActivityState
	{
		std::vector<DMUI_PageActivityInfo> events;
	};

	extern uint32_t s_mockRegistrations;
	extern DMUI_HostServices s_mockServices;
	extern DMUI_Result s_mockUIResult;
	extern uint32_t s_mockUIRevision;
	extern uint32_t s_mockUITableSize;
	extern bool s_mockMissingRequiredUIOperation;
	extern bool s_mockMissingPlotLines;
	extern uint32_t s_externalOpenCalls;
	extern uint32_t s_externalNativeError;
	extern DMUI_Result s_externalResult;
	extern ExternalOpenRequest s_externalRequest;
	extern uint32_t s_externalResolveCalls;
	extern uint32_t s_externalResolveError;
	extern DMUI_Result s_externalResolveResult;
	extern std::string s_externalVirtualFile;
	extern std::string s_externalPhysicalFile;

	struct SettingsMoveProbe
	{
		std::array<uint32_t, 2> flags{};
		size_t calls{};
		uint32_t firstError{ ERROR_ACCESS_DENIED };
		uint32_t secondError{ ERROR_ACCESS_DENIED };
		bool performSecondMove{};
	};

	bool ProbeSettingsMove(
		void* a_context,
		const std::filesystem::path& a_source,
		const std::filesystem::path& a_destination,
		uint32_t a_flags,
		uint32_t& a_nativeError) noexcept;

	DMUI_Result FakeExternalFileResolver(
		std::string_view a_virtualFile,
		std::string& a_physicalFile,
		uint32_t* a_nativeError) noexcept;

	struct ExternalFileFixture
	{
		std::filesystem::path path;

		explicit ExternalFileFixture(std::string_view a_contents);
		~ExternalFileFixture();

		[[nodiscard]] std::string Utf8() const;
	};

	DMUI_Result FakeExternalOpen(
		const ExternalOpenRequest& a_request,
		uint32_t* a_nativeError) noexcept;

	DMUI_Result DMUI_CALL MockRegisterClient(
		const DMUI_ClientDescriptor* a_descriptor,
		DMUI_ClientHandle* a_handle) noexcept;
	DMUI_Result DMUI_CALL MockRegisterPage(
		DMUI_ClientHandle a_client,
		const DMUI_PageDescriptor* a_descriptor,
		DMUI_PageHandle* a_handle) noexcept;
	DMUI_Result DMUI_CALL MockRegisterCategory(
		DMUI_ClientHandle a_client,
		const DMUI_CategoryDescriptor* a_descriptor) noexcept;
	DMUI_Result DMUI_CALL MockQueryServices(
		DMUI_HostServicesInfo* a_services) noexcept;
	DMUI_Result DMUI_CALL MockQueryUIAPI(
		uint32_t a_requestedUIAbi,
		uint32_t a_minimumRevision,
		uint32_t a_minimumTableSize,
		DMUI_UIAPIInfo* a_info) noexcept;

	[[nodiscard]] DMUI_HostAPI PreflightHostAPI() noexcept;

	DMUI_Result DMUI_CALL MockOpenExternal(
		DMUI_ClientHandle a_client,
		const DMUI_ExternalOpenDescriptor* a_descriptor,
		uint32_t* a_nativeError) noexcept;
	DMUI_Result DMUI_CALL MockCreateImage(
		DMUI_ClientHandle a_client,
		const DMUI_ImageDescriptor* a_descriptor,
		DMUI_ImageHandle* a_handle) noexcept;
	DMUI_Result DMUI_CALL MockUpdateImage(
		DMUI_ClientHandle a_client,
		DMUI_ImageHandle a_image,
		const DMUI_ImageDescriptor* a_descriptor) noexcept;
	DMUI_Result DMUI_CALL MockDrawImage(
		DMUI_ClientHandle a_client,
		DMUI_ImageHandle a_image,
		const DMUI_ImageDrawOptions* a_options) noexcept;
	DMUI_Result DMUI_CALL MockReleaseImage(
		DMUI_ClientHandle a_client,
		DMUI_ImageHandle a_image) noexcept;
	DMUI_Result DMUI_CALL MockQueryImage(
		DMUI_ClientHandle a_client,
		DMUI_ImageHandle a_image,
		DMUI_ImageInfo* a_info) noexcept;

	class SilentHealthReporter final : public HealthReporter
	{
	public:
		void Report(
			HealthEvent a_event,
			const HealthSnapshot& a_snapshot) noexcept override;
	};

	[[nodiscard]] bool SameColor(
		const ImVec4& a_left,
		const ImVec4& a_right) noexcept;
	[[nodiscard]] std::string Sha256(
		const std::filesystem::path& a_path);

	void DMUI_CALL Ready(
		const DMUI_HostReadyInfo* a_info,
		void* a_userData) noexcept;
	void DMUI_CALL Unavailable(
		DMUI_UnavailableReason a_reason,
		void* a_userData) noexcept;
	void DMUI_CALL Draw(void* a_userData) noexcept;
	DMUI_Result DMUI_CALL DrawPage(void* a_userData) noexcept;
	void DMUI_CALL ObservePageActivity(
		const DMUI_PageActivityInfo* a_info,
		void* a_userData);
	void DMUI_CALL ThrowReady(
		const DMUI_HostReadyInfo* a_info,
		void* a_userData);
	void DMUI_CALL ThrowUnavailable(
		DMUI_UnavailableReason a_reason,
		void* a_userData);
	void DMUI_CALL ThrowDraw(void* a_userData);
	DMUI_Result DMUI_CALL ThrowDrawPage(void* a_userData);
	DMUI_Result DMUI_CALL UnsupportedDrawPage(void* a_userData) noexcept;

	[[nodiscard]] DMUI_ClientDescriptor Client(
		const char* a_id,
		const char* a_name,
		CallbackState& a_state) noexcept;
	[[nodiscard]] DMUI_PageDescriptor Page(
		const char* a_id,
		const char* a_name,
		const char* a_category,
		int32_t a_sort,
		DMUI_PageKind a_kind,
		CallbackState& a_state,
		const char* a_iconName = nullptr) noexcept;
	[[nodiscard]] DMUI_ActionDescriptor Action(
		const char* a_id,
		const char* a_label,
		const char* a_icon,
		int32_t a_sort,
		CallbackState& a_state) noexcept;
	[[nodiscard]] DMUI_FrameObserverDescriptor FrameObserver(
		CallbackState& a_state) noexcept;

	[[nodiscard]] DMUI_ClientHandle AddClient(
		Registry& a_registry,
		const char* a_id,
		const char* a_name,
		CallbackState& a_state,
		DMUI_ClientOrigin a_origin = DMUI_CLIENT_ORIGIN_NATIVE,
		const char* a_bridgeSourceLabel = nullptr);
	[[nodiscard]] DMUI_PageHandle AddPage(
		Registry& a_registry,
		DMUI_ClientHandle a_client,
		const char* a_id,
		const char* a_name,
		const char* a_category,
		int32_t a_sort,
		DMUI_PageKind a_kind,
		CallbackState& a_state,
		const char* a_iconName = nullptr);
	void AddCategory(
		Registry& a_registry,
		DMUI_ClientHandle a_client,
		const char* a_id,
		const char* a_displayName,
		int32_t a_sortKey = 0,
		const char* a_iconName = nullptr);
	[[nodiscard]] DMUI_ActionHandle AddAction(
		Registry& a_registry,
		DMUI_ClientHandle a_client,
		const char* a_id,
		const char* a_label,
		const char* a_icon,
		int32_t a_sort,
		CallbackState& a_state);
}
