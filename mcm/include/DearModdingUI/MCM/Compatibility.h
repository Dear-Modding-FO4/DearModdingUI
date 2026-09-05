#pragma once

#include <DearModdingUI/MCM/TextMarkup.h>

#include <DearModdingUI/Client.h>

#include <cstddef>
#include <concepts>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace DearModdingUI::MCM
{
	using ValueWriteResult = std::expected<dmui::SettingValue, std::string>;
	using ValueWriteCompletion = std::function<void(ValueWriteResult)>;

	enum class ControlType : uint8_t
	{
		kSwitch,
		kSlider,
		kStepper,
		kMenu,
		kInput,
		kText,
		kGroup,
		kSpacing,
		kHidden,
		kButton,
		kKeymap,
		kColor,
		kImage,
		kUnknown
	};

	using Scalar = std::variant<bool, int64_t, uint64_t, double, std::string>;

	enum class SourceFamily : uint8_t
	{
		kGlobal,
		kProperty,
		kModSetting,
		kUnknown
	};

	enum class SourceValueKind : uint8_t
	{
		kBool,
		kInt,
		kFloat,
		kString,
		kNone
	};

	struct SourceType
	{
		SourceFamily family{ SourceFamily::kUnknown };
		SourceValueKind value{ SourceValueKind::kNone };
		std::string raw;
	};

	enum class ConditionType : uint8_t
	{
		kControl,
		kAll,
		kAny,
		kUnknown
	};

	struct GroupCondition
	{
		ConditionType type{ ConditionType::kUnknown };
		int64_t control{};
		std::string rawOperator;
		std::vector<GroupCondition> operands;
	};

	enum class DeclarationState : uint8_t
	{
		kUnknown,
		kDeclared,
		kUndeclared
	};

	struct GlobalBinding
	{
		std::string form;
	};

	struct PropertyBinding
	{
		std::string form;
		std::optional<std::string> scriptName;
		std::string propertyName;
	};

	struct ModSettingBinding
	{
		std::string section;
		std::string key;
		DeclarationState declaration{ DeclarationState::kUnknown };
	};

	struct ValueArgument
	{
		SourceValueKind type{ SourceValueKind::kNone };
	};

	struct ValueTemplateArgument
	{
		std::string value;
		SourceValueKind type{ SourceValueKind::kNone };
	};

	using ActionArgument = std::variant<
		bool,
		int64_t,
		uint64_t,
		double,
		std::string,
		ValueArgument,
		ValueTemplateArgument>;

	struct CallFunctionAction
	{
		std::string form;
		std::optional<std::string> scriptName;
		std::string function;
		std::vector<ActionArgument> arguments;
	};

	struct CallGlobalFunctionAction
	{
		std::string script;
		std::string function;
		std::vector<ActionArgument> arguments;
	};

	struct CallExternalFunctionAction
	{
		std::string plugin;
		std::string function;
		std::vector<ActionArgument> arguments;
	};

	struct RunConsoleCommandAction
	{
		std::string command;
	};

	struct SendEventAction
	{
		std::string event;
		std::vector<ActionArgument> arguments;
	};

	using Action = std::variant<
		CallFunctionAction,
		CallGlobalFunctionAction,
		CallExternalFunctionAction,
		RunConsoleCommandAction,
		SendEventAction>;

	struct Image
	{
		std::string library;
		std::string symbol;
	};

	struct ValueOptions
	{
		std::optional<SourceType> sourceType;
		std::optional<std::string> sourceForm;
		std::optional<std::string> scriptName;
		std::optional<std::string> propertyName;
		std::optional<std::string> modSettingId;
		std::optional<Scalar> defaultValue;
		std::optional<double> minimum;
		std::optional<double> maximum;
		std::optional<double> step;
		std::optional<std::string> format;
		std::vector<Scalar> options;
	};

	struct Control
	{
		std::string id;
		std::string text;
		std::string help;
		std::string rawType;
		std::string location;
		ControlType type{ ControlType::kUnknown };
		std::optional<ValueOptions> valueOptions;
		std::optional<GroupCondition> groupCondition;
		std::optional<int64_t> groupControl;
		std::optional<bool> html;
		std::optional<std::string> alignment;
		std::optional<Action> action;
		std::optional<Image> image;
		size_t sourceIndex{};
	};

	struct Page
	{
		std::string id;
		std::string displayName;
		std::string location;
		std::vector<Control> controls;
		bool root{};
	};

	struct Configuration
	{
		std::optional<int64_t> minimumMcmVersion;
		std::string modName;
		std::string displayName;
		std::vector<std::string> pluginRequirements;
		std::vector<Page> pages;
	};

	struct MappedBinding
	{
		std::string descriptorId;
		dmui::SettingValue target;
		SourceValueKind valueKind{ SourceValueKind::kNone };
		std::string rawSourceType;
		std::variant<GlobalBinding, PropertyBinding, ModSettingBinding> source;
		std::string cacheKey;

		[[nodiscard]] SourceFamily Family() const noexcept
		{
			return std::visit(
				[](const auto& a_source) {
					using T = std::remove_cvref_t<decltype(a_source)>;
					if constexpr (std::same_as<T, GlobalBinding>)
						return SourceFamily::kGlobal;
					else if constexpr (std::same_as<T, PropertyBinding>)
						return SourceFamily::kProperty;
					else
						return SourceFamily::kModSetting;
				},
				source);
		}
	};

	struct MappedText
	{
		std::string descriptorId;
		TextPresentation presentation;
	};

	enum class ValueRoute : uint8_t
	{
		kSource,
		kLocalUiState
	};

	enum class InertReason : uint8_t
	{
		kNone,
		kConditionFalse,
		kConditionPending,
		kUnsupported,
		kUndeclaredModSetting,
		kKeybindUnbound,
		kKeybindDefinitionMissing,
		kKeybindDefinitionsMissing,
		kKeybindDefinitionsInvalid,
		kKeybindBindingsInvalid,
		kMcmNotInstalled,
		kRuntimeNotReady,
		kValuePending,
		kValueMissing,
		kValueFailed,
		kUnsupportedAction
	};

	enum class InertReasonScope : uint8_t
	{
		kEnvironment,
		kRow
	};

	struct InertReasonMetadata
	{
		InertReasonScope scope{ InertReasonScope::kRow };
		std::string_view text;
		std::string_view pageText;
	};

	[[nodiscard]] constexpr InertReasonMetadata Describe(
		InertReason a_reason) noexcept
	{
		switch (a_reason)
		{
		case InertReason::kNone:
			return { InertReasonScope::kRow, {}, {} };
		case InertReason::kConditionFalse:
			return {
				InertReasonScope::kRow,
				"This row is hidden by its condition.",
				{}
			};
		case InertReason::kConditionPending:
			return {
				InertReasonScope::kRow,
				"Waiting to determine whether this row should be shown.",
				{}
			};
		case InertReason::kUnsupported:
			return {
				InertReasonScope::kRow,
				"This control is not supported.",
				{}
			};
		case InertReason::kUndeclaredModSetting:
			return {
				InertReasonScope::kRow,
				"This setting is not declared in MCM settings.ini.",
				{}
			};
		case InertReason::kKeybindUnbound:
			return {
				InertReasonScope::kRow,
				"This hotkey is not bound in MCM.",
				{}
			};
		case InertReason::kKeybindDefinitionMissing:
			return {
				InertReasonScope::kRow,
				"This hotkey is not declared in the mod's MCM keybinds.json.",
				{}
			};
		case InertReason::kKeybindDefinitionsMissing:
			return {
				InertReasonScope::kEnvironment,
				{},
				"This mod has no MCM keybinds.json, so its hotkeys cannot be bound."
			};
		case InertReason::kKeybindDefinitionsInvalid:
			return {
				InertReasonScope::kEnvironment,
				{},
				"This mod's MCM keybinds.json could not be read, so its hotkeys cannot be bound."
			};
		case InertReason::kKeybindBindingsInvalid:
			return {
				InertReasonScope::kEnvironment,
				{},
				"MCM's user key bindings could not be read."
			};
		case InertReason::kMcmNotInstalled:
			return {
				InertReasonScope::kEnvironment,
				"Mod Configuration Menu is not installed.",
				"Mod Configuration Menu is not installed, so mod settings cannot be changed."
			};
		case InertReason::kRuntimeNotReady:
			return {
				InertReasonScope::kEnvironment,
				"Load a save to change these settings.",
				"Load a save to change these settings."
			};
		case InertReason::kValuePending:
			return {
				InertReasonScope::kRow,
				"Waiting for this setting's value.",
				{}
			};
		case InertReason::kValueMissing:
			return {
				InertReasonScope::kRow,
				"This setting's value is unavailable.",
				{}
			};
		case InertReason::kValueFailed:
			return {
				InertReasonScope::kRow,
				"This setting's value could not be read.",
				{}
			};
		case InertReason::kUnsupportedAction:
			return {
				InertReasonScope::kRow,
				"This action is not supported.",
				{}
			};
		}
		return {
			InertReasonScope::kRow,
			"This setting is unavailable.",
			{}
		};
	}

	struct ResolvedInertState
	{
		InertReason governingReason{ InertReason::kNone };
		InertReason rowReason{ InertReason::kNone };
	};

	struct MappedRow
	{
		std::string id;
		bool emitted{};
		bool unsupported{};
		std::optional<int64_t> groupControl;
		std::optional<GroupCondition> groupCondition;
		std::optional<MappedBinding> binding;
		std::optional<SourceType> unmappedSource;
		std::optional<MappedText> text;
		std::optional<Action> action;
		std::optional<Image> image;
		ValueRoute valueRoute{ ValueRoute::kSource };
		std::optional<std::string> keybindId;
		std::optional<ResolvedInertState> keybindInertState;
		std::optional<InertReason> actionInertReason;
		std::function<ResolvedInertState()> resolveInertState;
		std::function<dmui::SettingValue(
			dmui::SettingValue,
			ValueWriteCompletion)> writeValue;
	};

	struct MappedPage
	{
		std::string id;
		std::string displayName;
		dmui::SettingsPage settings;
		std::vector<MappedRow> rows;
		size_t localUiStateRows{};
	};

	struct PageCompatibilitySummary
	{
		size_t rows{};
		size_t unsupported{};
		size_t bindings{};
		size_t localUiStateRows{};
		size_t unknownBindings{};
		size_t undeclaredModSettings{};
		size_t resolvedKeybinds{};
		size_t actions{};
		size_t images{};
		size_t pendingConditions{};
		size_t unevaluableConditions{};
		size_t visibleRows{};
	};

	enum class DiagnosticSeverity : uint8_t
	{
		kWarning,
		kError
	};

	struct Diagnostic
	{
		DiagnosticSeverity severity{ DiagnosticSeverity::kError };
		std::string source;
		std::string location;
		std::string message;
	};

	struct LoadResult
	{
		std::optional<Configuration> configuration;
		std::vector<MappedPage> pages;
		std::vector<Diagnostic> diagnostics;
	};

	[[nodiscard]] LoadResult ParseConfig(
		std::string_view a_json,
		std::string_view a_source = "<memory>") noexcept;

	[[nodiscard]] LoadResult LoadConfig(
		const std::filesystem::path& a_path) noexcept;

	[[nodiscard]] PageCompatibilitySummary SummarizeCompatibility(
		const MappedPage& a_page) noexcept;
}
