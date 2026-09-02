#pragma once

#include <DearModdingUI/Client.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace DearModdingUI::MCM
{
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

	struct MappedPage
	{
		std::string id;
		std::string displayName;
		dmui::SettingsPage settings;
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
}
