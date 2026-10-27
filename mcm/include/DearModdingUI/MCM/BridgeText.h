#pragma once

#include <Support/Message.h>

#include <string>

// Bridge-authored text; keys resolve against the host owner DearModdingUI.
namespace DearModdingUI::MCM::BridgeText
{
	[[nodiscard]] inline std::string Render(
		const Support::Phrase& a_phrase,
		const Support::TextCatalog& a_catalog)
	{
		return Support::Message{ a_phrase }.Render(a_catalog);
	}

	inline constexpr Support::Phrase kGeneral{ "$DMUI_MCM_General", "General" };
	inline constexpr Support::Phrase kAction{ "$DMUI_MCM_Action", "Action" };
	inline constexpr Support::Phrase kButtonNoAction{
		"$DMUI_MCM_ButtonNoAction",
		"This button has no action."
	};
	inline constexpr Support::Phrase kSwfOnlyPage{
		"$DMUI_MCM_SwfOnlyPage",
		"This page contains only unrendered SWF components."
	};
	inline constexpr Support::Phrase kNone{ "$DMUI_MCM_None", "None" };
	inline constexpr Support::Phrase kPageName{ "$DMUI_MCM_PageName", "Page {}" };

	inline constexpr Support::Phrase kUnbound{ "$DMUI_MCM_Unbound", "Unbound" };
	inline constexpr Support::Phrase kCannotBind{
		"$DMUI_MCM_CannotBind",
		"Can't be bound"
	};
	inline constexpr Support::Phrase kBindingUnavailable{
		"$DMUI_MCM_BindingUnavailable",
		"Binding unavailable"
	};
	inline constexpr Support::Phrase kModifier{ "$DMUI_MCM_Modifier", "Modifier {}" };
	inline constexpr Support::Phrase kKeycode{ "$DMUI_MCM_Keycode", "Keycode {}" };

	inline constexpr Support::Phrase kReasonConditionFalse{
		"$DMUI_MCM_ReasonConditionFalse",
		"This row is hidden by its condition."
	};
	inline constexpr Support::Phrase kReasonConditionPending{
		"$DMUI_MCM_ReasonConditionPending",
		"Waiting to determine whether this row should be shown."
	};
	inline constexpr Support::Phrase kReasonUnsupported{
		"$DMUI_MCM_ReasonUnsupported",
		"This control is not supported."
	};
	inline constexpr Support::Phrase kReasonUndeclaredModSetting{
		"$DMUI_MCM_ReasonUndeclaredModSetting",
		"This setting is not declared in MCM settings.ini."
	};
	inline constexpr Support::Phrase kReasonKeybindUnbound{
		"$DMUI_MCM_ReasonKeybindUnbound",
		"This hotkey is not bound in MCM."
	};
	inline constexpr Support::Phrase kReasonKeybindDefinitionMissing{
		"$DMUI_MCM_ReasonKeybindDefinitionMissing",
		"This hotkey is not declared in the mod's MCM keybinds.json."
	};
	inline constexpr Support::Phrase kReasonKeybindDefinitionsMissing{
		"$DMUI_MCM_ReasonKeybindDefinitionsMissing",
		"This mod has no MCM keybinds.json, so its hotkeys cannot be bound."
	};
	inline constexpr Support::Phrase kReasonKeybindDefinitionsInvalid{
		"$DMUI_MCM_ReasonKeybindDefinitionsInvalid",
		"This mod's MCM keybinds.json could not be read, so its hotkeys cannot be bound."
	};
	inline constexpr Support::Phrase kReasonKeybindBindingsInvalid{
		"$DMUI_MCM_ReasonKeybindBindingsInvalid",
		"MCM's user key bindings could not be read."
	};
	inline constexpr Support::Phrase kReasonMcmNotInstalled{
		"$DMUI_MCM_ReasonMcmNotInstalled",
		"Mod Configuration Menu is not installed."
	};
	inline constexpr Support::Phrase kReasonMcmNotInstalledPage{
		"$DMUI_MCM_ReasonMcmNotInstalledPage",
		"Mod Configuration Menu is not installed, so mod settings cannot be changed."
	};
	inline constexpr Support::Phrase kReasonRuntimeNotReady{
		"$DMUI_MCM_ReasonRuntimeNotReady",
		"Load a save to change these settings."
	};
	inline constexpr Support::Phrase kReasonValuePending{
		"$DMUI_MCM_ReasonValuePending",
		"Waiting for this setting's value."
	};
	inline constexpr Support::Phrase kReasonValueMissing{
		"$DMUI_MCM_ReasonValueMissing",
		"This setting's value is unavailable."
	};
	inline constexpr Support::Phrase kReasonValueFailed{
		"$DMUI_MCM_ReasonValueFailed",
		"This setting's value could not be read."
	};
	inline constexpr Support::Phrase kReasonFileChoicesPending{
		"$DMUI_MCM_ReasonFileChoicesPending",
		"Waiting for this file list to be refreshed."
	};
	inline constexpr Support::Phrase kReasonFileChoicesFailed{
		"$DMUI_MCM_ReasonFileChoicesFailed",
		"This file list could not be read."
	};
	inline constexpr Support::Phrase kReasonUnsupportedAction{
		"$DMUI_MCM_ReasonUnsupportedAction",
		"This action is not supported."
	};
	inline constexpr Support::Phrase kReasonUnavailable{
		"$DMUI_MCM_ReasonUnavailable",
		"This setting is unavailable."
	};

	inline constexpr Support::Phrase kLoadingVisibility{
		"$DMUI_MCM_LoadingVisibility",
		"Loading setting visibility..."
	};
	// Counts follow a colon so no language has to inflect them.
	inline constexpr Support::Phrase kConditionsNotApplied{
		"$DMUI_MCM_ConditionsNotApplied",
		"Compatibility: conditions not applied: {}. A controller is inoperable or its "
		"value is missing or failed; dependent content is shown."
	};

	inline constexpr Support::Phrase kActionFailed{
		"$DMUI_MCM_ActionFailed",
		"Action '{}': {}"
	};
	inline constexpr Support::Phrase kExecutionFailed{
		"$DMUI_MCM_ExecutionFailed",
		"execution failed"
	};

	inline constexpr Support::Phrase kCompatibilitySummary{
		"$DMUI_MCM_CompatibilitySummary",
		"Compatibility: {}."
	};
	inline constexpr Support::Phrase kUnsupportedActions{
		"$DMUI_MCM_UnsupportedActions",
		"unsupported actions: {}"
	};
	inline constexpr Support::Phrase kUndeclaredSettings{
		"$DMUI_MCM_UndeclaredSettings",
		"undeclared persisted settings: {}"
	};

	inline constexpr Support::Phrase kFileListFailed{
		"$DMUI_MCM_FileListFailed",
		"Could not list files from path '{}' with mask '{}'."
	};
	inline constexpr Support::Phrase kFileListFailedDetail{
		"$DMUI_MCM_FileListFailedDetail",
		"Could not list files from path '{}' with mask '{}': {}."
	};
	inline constexpr Support::Phrase kFilePathMissing{
		"$DMUI_MCM_FilePathMissing",
		"the configured path is missing or empty"
	};
}
