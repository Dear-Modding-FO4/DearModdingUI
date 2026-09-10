#pragma once

#include <DearModdingUI/API.h>

#include <type_traits>

namespace DearModdingUI::HostAPIInternal
{
	using std::remove_pointer_t;

	// Derive declarations from the public entries so adapter signatures cannot drift.
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::registerClient)> ApiRegisterClient;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::registerPage)> ApiRegisterPage;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::registerCategory)> ApiRegisterCategory;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::registerAction)> ApiRegisterAction;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::registerFrameObserver)> ApiRegisterFrameObserver;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::registerPageActivityObserver)> ApiRegisterPageActivityObserver;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::registerHotkeyAction)> ApiRegisterHotkeyAction;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::queryHotkeyBinding)> ApiQueryHotkeyBinding;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::unregisterHotkeyAction)> ApiUnregisterHotkeyAction;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::queryState)> ApiQueryState;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::requestFrame)> ApiRequestFrame;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::releaseFrame)> ApiReleaseFrame;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::isMenuVisible)> ApiIsMenuVisible;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::selectPage)> ApiSelectPage;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::attachSwapChain)> ApiAttachSwapChain;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::setStatus)> ApiSetStatus;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::reportDiagnostic)> ApiReportDiagnostic;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::getThemeColors)> ApiGetThemeColors;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::pushFont)> ApiPushFont;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::popFont)> ApiPopFont;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::drawSectionHeader)> ApiDrawSectionHeader;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::drawBulletText)> ApiDrawBulletText;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::drawSearchInput)> ApiDrawSearchInput;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::drawCollapsingSectionHeader)> ApiDrawCollapsingSectionHeader;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::drawLinkRow)> ApiDrawLinkRow;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::openExternal)> ApiOpenExternal;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::drawFaq)> ApiDrawFaq;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::drawSettingsActionButton)> ApiDrawSettingsActionButton;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::settingsActionButtonWidth)> ApiSettingsActionButtonWidth;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::settingsActionButtonExtent)> ApiSettingsActionButtonExtent;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::beginSettingsTable)> ApiBeginSettingsTable;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::beginSettingsRow)> ApiBeginSettingsRow;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::beginSettingsRowEx)> ApiBeginSettingsRowEx;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::endSettingsRow)> ApiEndSettingsRow;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::endSettingsTable)> ApiEndSettingsTable;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::queryVideoMemory)> ApiQueryVideoMemory;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::queryServices)> ApiQueryServices;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::queryUIAPI)> ApiQueryUIAPI;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::setHotkeyActionEnabled)> ApiSetHotkeyActionEnabled;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::importD3D11Image)> ApiImportD3D11Image;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::createImage)> ApiCreateImage;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::updateImage)> ApiUpdateImage;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::drawImage)> ApiDrawImage;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::releaseImage)> ApiReleaseImage;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::queryImage)> ApiQueryImage;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::configureOverlay)> ApiConfigureOverlay;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::queryOverlay)> ApiQueryOverlay;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::postNotification)> ApiPostNotification;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::drawAnnotatedPlot)> ApiDrawAnnotatedPlot;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::requestDialog)> ApiRequestDialog;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::pollDialogEvent)> ApiPollDialogEvent;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::resolveDialogSubmission)> ApiResolveDialogSubmission;
	[[nodiscard]] remove_pointer_t<decltype(DMUI_HostAPI::cancelDialog)> ApiCancelDialog;
}
