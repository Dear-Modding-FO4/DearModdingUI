#pragma once

#include <DearModdingUI/API.h>

namespace DearModdingUI::RegistryCallbackDispatch
{
	[[nodiscard]] bool InvokeReady(
		DMUI_HostReadyCallback a_callback,
		const DMUI_HostReadyInfo* a_info,
		void* a_userData) noexcept;
	[[nodiscard]] bool InvokeUnavailable(
		DMUI_HostUnavailableCallback a_callback,
		DMUI_UnavailableReason a_reason,
		void* a_userData) noexcept;
	[[nodiscard]] DMUI_Result InvokePage(
		DMUI_PageDrawCallback a_callback,
		void* a_userData) noexcept;
	[[nodiscard]] bool InvokeAction(
		DMUI_ActionCallback a_callback,
		void* a_userData) noexcept;
	[[nodiscard]] bool InvokePageActivity(
		DMUI_PageActivityCallback a_callback,
		const DMUI_PageActivityInfo* a_info,
		void* a_userData) noexcept;
}
