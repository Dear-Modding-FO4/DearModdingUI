#pragma once

#include <DearModdingUI/API.h>
#include <imgui/imgui.h>

namespace DearModdingUI::ModalCoordinator
{
	struct Owner
	{
		DMUI_ClientHandle client{};
		DMUI_PageHandle page{};
		bool operator==(const Owner&) const noexcept = default;
	};

	// Render-thread state; notification and modal queries also support input threads.
	[[nodiscard]] bool ReserveDialog(DMUI_DialogHandle a_dialog) noexcept;
	void ReleaseDialog(DMUI_DialogHandle a_dialog) noexcept;
	[[nodiscard]] ImGuiID DialogPopupId() noexcept;
	[[nodiscard]] bool BeginDialog(const char* a_title, bool* a_open);
	void Open(Owner a_owner, const char* a_id);
	[[nodiscard]] bool Begin(
		Owner a_owner, const char* a_id, bool a_modal, bool* a_open,
		ImGuiWindowFlags a_flags, bool a_hasCloseButton = true);
	[[nodiscard]] bool IsOpen(Owner a_owner, const char* a_id) noexcept;
	[[nodiscard]] bool HasBracket(Owner a_owner) noexcept;
	[[nodiscard]] bool End(Owner a_owner) noexcept;
	[[nodiscard]] bool CloseCurrent(Owner a_owner) noexcept;
	void ClosePage(DMUI_PageHandle a_page) noexcept;
	void ClosePages() noexcept;
	void NotifyMenuClosed() noexcept;
	void BeginFrame() noexcept;
	void TouchPage(DMUI_PageHandle a_page) noexcept;
	void FinishFrame() noexcept;
	void Dismiss(ImGuiID a_id, size_t a_depth) noexcept;
	[[nodiscard]] bool HasModal() noexcept;
}
