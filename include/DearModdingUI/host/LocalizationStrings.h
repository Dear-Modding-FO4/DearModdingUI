#pragma once

#include <Support/Localization.h>

namespace DearModdingUI
{
	LOCALIZE_STRING(lsHost, "$Host", "Host");
	LOCALIZE_STRING(lsMod, "$Mod", "Mod");
	LOCALIZE_STRING(lsVersion, "$Version", "Version");
	LOCALIZE_STRING(lsStatusDetailsPopup, "$StatusDetailsPopup", "Status details");
	LOCALIZE_STRING(lsNavRequestRejected, "$NavRequestRejected", "Navigation request was rejected");
	LOCALIZE_STRING(lsClose, "$Close", "Close");
	LOCALIZE_STRING(lsCloseMenu, "$CloseMenu", "Close menu");
	LOCALIZE_STRING(lsErrorFrom, "$ErrorFrom", "Error from");
	LOCALIZE_STRING(lsCopyDetails, "$CopyDetails", "Copy details");
	LOCALIZE_STRING(lsMessagePleaseSelectPage, "$MessagePleaseSelectPage", "Please select a page from the left.");
	LOCALIZE_STRING(lsMessageActionDisabledAfterCallbacks, "$MessageActionDisabledAfterCallbacks", "Action disabled after its callback failed.");
	LOCALIZE_STRING(lsMessageFailedCallbackPage, "$MessageFailedCallbackPages", "The mod's page callback failed and has been disabled for this session. Other pages remain available.");
	LOCALIZE_STRING(lsCouldNotBeDisplayed, "$CouldNotBeDisplayed", "could not be displayed");
	LOCALIZE_STRING(lsSettings, "$Settings", "Settings");
	LOCALIZE_STRING(lsDismiss, "$Dismiss", "Dismiss");
	LOCALIZE_STRING(lsDismissStatus, "$DismissStatus", "Dismiss status");
	LOCALIZE_STRING(lsInterfaceSettings, "$InterfaceSettings", "Interface settings");
	LOCALIZE_STRING(lsPages, "$Pages", "Pages");
	LOCALIZE_STRING(lsModsHi, "$ModsHi", "Mods");
	LOCALIZE_STRING(lsMessageSelectModToBrowse, "$MessageSelectModToBrowse", "Select a mod to browse its pages.");
	LOCALIZE_STRING(lsAllMods, "$AllMods", "All Mods");
	LOCALIZE_STRING(lsDeadlineExceeded, "$DeadlineExceeded", "(deadline exceeded)");
	LOCALIZE_STRING(lsError, "$Error", "error");
	LOCALIZE_STRING(lsErrors, "$Errors", "errors");
	LOCALIZE_STRING(lsWarning, "$Warning", "warning");
	LOCALIZE_STRING(lsWarnings, "$Warnings", "warnings");
	LOCALIZE_STRING(lsSuccess, "$Success", "success");
	LOCALIZE_STRING(lsSuccesses, "$Successes", "successes");
	LOCALIZE_STRING(lsInfo, "$Info", "info");
	LOCALIZE_STRING(lsFurtherDiagnosticReport, "$FurtherDiagnosticReport", "further diagnostic report was not retained");
	LOCALIZE_STRING(lsFurtherDiagnosticReports, "$FurtherDiagnosticReports", "further diagnostic reports were not retained");
	LOCALIZE_STRING(lsScope, "$Scope", "Scope");
	LOCALIZE_STRING(lsSecondShort, "$SecondShort", "s");
	LOCALIZE_STRING(lsMinuteShort, "$MinuteShort", "m");
	LOCALIZE_STRING(lsHourShort, "$HourShort", "h");
	LOCALIZE_STRING(lsRegisteredMods, "$RegisteredMods", "Registered mods");
	LOCALIZE_STRING(lsBridgedMods, "$BridgedMods", "Bridged mods");
	LOCALIZE_STRING(lsMods, "$Mods", "mods");
	LOCALIZE_STRING(lsDiagnosticsReport, "$DiagnosticsReport", "diagnostics report");
	LOCALIZE_STRING(lsHostSubsystems, "$HostSubsystems", "Host subsystems");
	LOCALIZE_STRING(lsNoObservations, "$NoObservations", "No observations");
	LOCALIZE_STRING(lsReportedDiagnostics, "$ReportedDiagnostics", "Reported diagnostics");
	LOCALIZE_STRING(lsNone, "$None", "None");
	LOCALIZE_STRING(lsMessageNoHostSubsystem, "$MessageNoHostSubsystem", "No host subsystem observations are available.");
	LOCALIZE_STRING(lsAbout, "$About", "About");
	LOCALIZE_STRING(lsOverview, "$Overview", "Overview");
	LOCALIZE_STRING(lsPagesSm, "$PagesSm", "pages");
	LOCALIZE_STRING(lsActionsSm, "$ActionsSm", "actions");
	LOCALIZE_STRING(lsQuickLinks, "$QuickLinks", "Quick Links");
	LOCALIZE_STRING(lsFaq, "$Faq", "FAQ");
	LOCALIZE_STRING(lsCopyReport, "$CopyReport", "Copy report");
	LOCALIZE_STRING(lsCopyReportMessage, "$CopyReportMessage", "Copy a diagnostics report to the clipboard.");
	LOCALIZE_STRING(lsNoClientModsMessage, "$NoClientModsMessage", "No client mods registered this session.");
	LOCALIZE_STRING(lsStatus, "$Status", "Status");
	LOCALIZE_STRING(lsInState, "$InState", "in state");
	LOCALIZE_STRING(lsNoClientDiagnosticsMessage, "$NoClientDiagnosticsMessage", "No client diagnostics have been reported.");
	LOCALIZE_STRING(lsReportedProblems, "$ReportedProblems", "Reported problems");
	LOCALIZE_STRING(lsCommandPalettePopup, "$CommandPalettePopup", "Search mods, pages, and actions");
	LOCALIZE_STRING(lsActions, "$Actions", "Actions");
	LOCALIZE_STRING(lsSearchInputUnavailable, "$SearchInputUnavailable", "Search input unavailable.");
	LOCALIZE_STRING(lsRecentPages, "$RecentPages", "Recent pages");
	LOCALIZE_STRING(lsResults, "$Results", "Results");
	LOCALIZE_STRING(lsNoRecentPagesYet, "$NoRecentPagesYet", "No recent pages yet.");
	LOCALIZE_STRING(lsNoMatchingMods, "$NoMatchingMods", "No matching mods, pages, or actions.");
	LOCALIZE_STRING(lsHomeAboutText, "$HomeAboutText",
		"DearModdingUI is a shared settings menu for Fallout 4. "
		"Mods register their own pages in one overlay instead of each "
		"shipping a separate menu, and mods that were never built for it "
		"can appear here too.");
	LOCALIZE_STRING(lsPress, "$Press", "Press");
	LOCALIZE_STRING(lsFaq1, "$Faq1", "How do I open the menu?");
	LOCALIZE_STRING(lsFaqA1, "$FaqA1",
		"to open or close DearModdingUI. You can change this key on "
		"the Settings page.");
	LOCALIZE_STRING(lsFaq2, "$Faq2", "Where are settings stored?");
	LOCALIZE_STRING(lsFaqA2, "$FaqA2", "Host settings are stored in Data/F4SE/Plugins/DearModdingUI.toml.");
	LOCALIZE_STRING(lsFaq3, "$Faq3", "Why is a mod page missing or grayed out?");
	LOCALIZE_STRING(lsFaqA3, "$FaqA3", "Open the Health page to see whether the host or that mod reported a problem.");
	LOCALIZE_STRING(lsFaq4, "$Faq4", "Does this replace a mod's own menu?");
	LOCALIZE_STRING(lsFaqA4, "$FaqA4", "No. Mods register pages and DearModdingUI draws them in the shared overlay.");
	LOCALIZE_STRING(lsAllSystemsReady, "$AllSystemsReady", "All systems ready");
	LOCALIZE_STRING(lsSystemsPartMsgFailed1_1, "$SystemsPartMsgFailed1_1", "host subsystem and");
	LOCALIZE_STRING(lsSystemsPartMsgFailed2_1, "$SystemsPartMsgFailed2_1", "host subsystems and");
	LOCALIZE_STRING(lsSystemsPartMsgFailed1_2, "$SystemsPartMsgFailed1_2", "mod needs attention");
	LOCALIZE_STRING(lsSystemsPartMsgFailed2_2, "$SystemsPartMsgFailed2_2", "mods need attention");
	LOCALIZE_STRING(lsSystemsPartMsgFailed3_1, "$SystemsPartMsgFailed3_1", "host subsystem starting");
	LOCALIZE_STRING(lsSystemsPartMsgFailed4_1, "$SystemsPartMsgFailed4_1", "host subsystems starting");
	LOCALIZE_STRING(lsSystemsPartMsgFailed5_1, "$SystemsPartMsgFailed5_1", "host subsystem needs attention");
	LOCALIZE_STRING(lsSystemsPartMsgFailed6_1, "$SystemsPartMsgFailed6_1", "host subsystems need attention");
	LOCALIZE_STRING(lsHostHealthNotObservedYet, "$HostHealthNotObservedYet", "Host health not observed yet");
	LOCALIZE_STRING(lsCopyTarget, "$CopyTarget", "Copy target.");
	LOCALIZE_STRING(lsOpenPhysicalBackingFile, "$OpenPhysicalBackingFile", "Open physical backing file");
	LOCALIZE_STRING(lsOpenPhysicalContainingFolder, "$OpenPhysicalContainingFolder", "Open physical containing folder");
	LOCALIZE_STRING(lsOpenWithSysDef, "$OpenWithSysDef", "Open with system default");
	LOCALIZE_STRING(lsOpenWithSelApp, "$OpenWithSelApp", "Open with selected application");
}