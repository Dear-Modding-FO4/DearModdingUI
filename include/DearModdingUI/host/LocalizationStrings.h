#pragma once

#include <Support/Localization.h>

namespace DearModdingUI
{
	inline static LocalizeString lsHost("$Host", "Host");
	inline static LocalizeString lsMod("$Mod", "Mod");
	inline static LocalizeString lsVersion("$Version", "Version");
	inline static LocalizeString lsStatusDetailsPopup("$StatusDetailsPopup", "Status details");
	inline static LocalizeString lsNavRequestRejected("$NavRequestRejected", "Navigation request was rejected");
	inline static LocalizeString lsClose("$Close", "Close");
	inline static LocalizeString lsCloseMenu("$CloseMenu", "Close menu");
	inline static LocalizeString lsErrorFrom("$ErrorFrom", "Error from");
	inline static LocalizeString lsCopyDetails("$CopyDetails", "Copy details");
	inline static LocalizeString lsMessagePleaseSelectPage("$MessagePleaseSelectPage", "Please select a page from the left.");
	inline static LocalizeString lsMessageActionDisabledAfterCallbacks("$MessageActionDisabledAfterCallbacks", "Action disabled after its callback failed.");
	inline static LocalizeString lsMessageFailedCallbackPage("$MessageFailedCallbackPages", "The mod's page callback failed and has been disabled for this session. Other pages remain available.");
	inline static LocalizeString lsCouldNotBeDisplayed("$CouldNotBeDisplayed", "could not be displayed");
	inline static LocalizeString lsSettings("$Settings", "Settings");
	inline static LocalizeString lsDismiss("$Dismiss", "Dismiss");
	inline static LocalizeString lsDismissStatus("$DismissStatus", "Dismiss status");
	inline static LocalizeString lsInterfaceSettings("$InterfaceSettings", "Interface settings");
	inline static LocalizeString lsPages("$Pages", "Pages");
	inline static LocalizeString lsModsHi("$ModsHi", "Mods");
	inline static LocalizeString lsMessageSelectModToBrowse("$MessageSelectModToBrowse", "Select a mod to browse its pages.");
	inline static LocalizeString lsAllMods("$AllMods", "All Mods");
	inline static LocalizeString lsDeadlineExceeded("$DeadlineExceeded", "(deadline exceeded)");
	inline static LocalizeString lsError("$Error", "error");
	inline static LocalizeString lsErrors("$Errors", "errors");
	inline static LocalizeString lsWarning("$Warning", "warning");
	inline static LocalizeString lsWarnings("$Warnings", "warnings");
	inline static LocalizeString lsSuccess("$Success", "success");
	inline static LocalizeString lsSuccesses("$Successes", "successes");
	inline static LocalizeString lsInfo("$Info", "info");
	inline static LocalizeString lsFurtherDiagnosticReport("$FurtherDiagnosticReport", "further diagnostic report was not retained");
	inline static LocalizeString lsFurtherDiagnosticReports("$FurtherDiagnosticReports", "further diagnostic reports were not retained");
	inline static LocalizeString lsScope("$Scope", "Scope");
	inline static LocalizeString lsSecondShort("$SecondShort", "s");
	inline static LocalizeString lsMinuteShort("$MinuteShort", "m");
	inline static LocalizeString lsHourShort("$HourShort", "h");
	inline static LocalizeString lsRegisteredMods("$RegisteredMods", "Registered mods");
	inline static LocalizeString lsBridgedMods("$BridgedMods", "Bridged mods");
	inline static LocalizeString lsMods("$Mods", "mods");
	inline static LocalizeString lsDiagnosticsReport("$DiagnosticsReport", "diagnostics report");
	inline static LocalizeString lsHostSubsystems("$HostSubsystems", "Host subsystems");
	inline static LocalizeString lsNoObservations("$NoObservations", "No observations");
	inline static LocalizeString lsReportedDiagnostics("$ReportedDiagnostics", "Reported diagnostics");
	inline static LocalizeString lsNone("$None", "None");
	inline static LocalizeString lsMessageNoHostSubsystem("$MessageNoHostSubsystem", "No host subsystem observations are available.");
	inline static LocalizeString lsAbout("$About", "About");
	inline static LocalizeString lsOverview("$Overview", "Overview");
	inline static LocalizeString lsPagesSm("$PagesSm", "pages");
	inline static LocalizeString lsActionsSm("$ActionsSm", "actions");
	inline static LocalizeString lsQuickLinks("$QuickLinks", "Quick Links");
	inline static LocalizeString lsFaq("$Faq", "FAQ");
	inline static LocalizeString lsCopyReport("$CopyReport", "Copy report");
	inline static LocalizeString lsCopyReportMessage("$CopyReportMessage", "Copy a diagnostics report to the clipboard.");
	inline static LocalizeString lsNoClientModsMessage("$NoClientModsMessage", "No client mods registered this session.");
	inline static LocalizeString lsStatus("$Status", "Status");
	inline static LocalizeString lsInState("$InState", "in state");
	inline static LocalizeString lsNoClientDiagnosticsMessage("$NoClientDiagnosticsMessage", "No client diagnostics have been reported.");
	inline static LocalizeString lsReportedProblems("$ReportedProblems", "Reported problems");
	inline static LocalizeString lsCommandPalettePopup("$CommandPalettePopup", "Search mods, pages, and actions");
	inline static LocalizeString lsActions("$Actions", "Actions");
	inline static LocalizeString lsSearchInputUnavailable("$SearchInputUnavailable", "Search input unavailable.");
	inline static LocalizeString lsRecentPages("$RecentPages", "Recent pages");
	inline static LocalizeString lsResults("$Results", "Results");
	inline static LocalizeString lsNoRecentPagesYet("$NoRecentPagesYet", "No recent pages yet.");
	inline static LocalizeString lsNoMatchingMods("$NoMatchingMods", "No matching mods, pages, or actions.");
	inline static LocalizeString lsHomeAboutText("$HomeAboutText",
		"DearModdingUI is a shared settings menu for Fallout 4. "
		"Mods register their own pages in one overlay instead of each "
		"shipping a separate menu, and mods that were never built for it "
		"can appear here too.");
	inline static LocalizeString lsPress("$Press", "Press");

	inline static LocalizeString lsFaq1("$Faq1", "How do I open the menu?");
	inline static LocalizeString lsFaqA1("$FaqA1",
		"to open or close DearModdingUI. You can change this key on "
		"the Settings page.");
	inline static LocalizeString lsFaq2("$Faq2", "Where are settings stored?");
	inline static LocalizeString lsFaqA2("$FaqA2", "Host settings are stored in Data/F4SE/Plugins/DearModdingUI.toml.");
	inline static LocalizeString lsFaq3("$Faq3", "Why is a mod page missing or grayed out?");
	inline static LocalizeString lsFaqA3("$FaqA3", "Open the Health page to see whether the host or that mod reported a problem.");
	inline static LocalizeString lsFaq4("$Faq4", "Does this replace a mod's own menu?");
	inline static LocalizeString lsFaqA4("$FaqA4", "No. Mods register pages and DearModdingUI draws them in the shared overlay.");
	inline static LocalizeString lsAllSystemsReady("$AllSystemsReady", "All systems ready");
	inline static LocalizeString lsSystemsPartMsgFailed1_1("$SystemsPartMsgFailed1_1", "host subsystem and");
	inline static LocalizeString lsSystemsPartMsgFailed2_1("$SystemsPartMsgFailed2_1", "host subsystems and");
	inline static LocalizeString lsSystemsPartMsgFailed1_2("$SystemsPartMsgFailed1_2", "mod needs attention");
	inline static LocalizeString lsSystemsPartMsgFailed2_2("$SystemsPartMsgFailed2_2", "mods need attention");
	inline static LocalizeString lsSystemsPartMsgFailed3_1("$SystemsPartMsgFailed3_1", "host subsystem starting");
	inline static LocalizeString lsSystemsPartMsgFailed4_1("$SystemsPartMsgFailed4_1", "host subsystems starting");
	inline static LocalizeString lsSystemsPartMsgFailed5_1("$SystemsPartMsgFailed5_1", "host subsystem needs attention");
	inline static LocalizeString lsSystemsPartMsgFailed6_1("$SystemsPartMsgFailed6_1", "host subsystems need attention");
	inline static LocalizeString lsHostHealthNotObservedYet("$HostHealthNotObservedYet", "Host health not observed yet");
}