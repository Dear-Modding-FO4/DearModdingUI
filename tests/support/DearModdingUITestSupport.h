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
