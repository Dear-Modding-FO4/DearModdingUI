#pragma once

#include <DearModdingUI/MCM/Compatibility.h>
#include <DearModdingUI/MCM/DiagnosticReporter.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace DearModdingUI::MCM
{
	// Mirrored from CommonLibF4's F4SE/InputMap.h because dmui-mcm cannot depend on F4SE headers.
	inline constexpr int32_t kKeyboardKeyCount = 256;
	inline constexpr int32_t kMouseButtonOffset = 256;
	inline constexpr int32_t kMouseButtonCount = 8;
	inline constexpr int32_t kMouseWheelOffset = 264;
	inline constexpr int32_t kMouseWheelDirectionCount = 2;
	inline constexpr int32_t kGamepadButtonOffset = 266;
	inline constexpr int32_t kGamepadButtonCount = 16;
	inline constexpr int32_t kMaximumMacroCode = 282;

	enum class KeybindFileState : uint8_t
	{
		kMissing,
		kLoaded,
		kMalformed
	};

	struct KeybindDefinitions
	{
		KeybindFileState state{ KeybindFileState::kMissing };
		std::string modName;
		std::vector<std::string> ids;

		[[nodiscard]] bool Contains(std::string_view a_id) const noexcept;
	};

	struct UserKeybind
	{
		int32_t keycode{};
		uint32_t modifiers{};
		std::string modName;
		std::string id;
	};

	struct UserKeybinds
	{
		KeybindFileState state{ KeybindFileState::kMissing };
		std::vector<UserKeybind> bindings;

		[[nodiscard]] const UserKeybind* Find(
			std::string_view a_modName,
			std::string_view a_id) const noexcept;
	};

	[[nodiscard]] KeybindDefinitions ParseKeybindDefinitions(
		std::string_view a_json) noexcept;

	[[nodiscard]] KeybindDefinitions LoadKeybindDefinitions(
		const std::filesystem::path& a_path) noexcept;

	[[nodiscard]] UserKeybinds ParseUserKeybinds(
		std::string_view a_json) noexcept;

	[[nodiscard]] UserKeybinds LoadUserKeybinds(
		const std::filesystem::path& a_path) noexcept;

	[[nodiscard]] std::string KeyName(int32_t a_keycode);

	[[nodiscard]] std::string FormatKeybind(
		int32_t a_keycode,
		uint32_t a_modifiers);

	void ApplyKeybinds(
		MappedPage& a_page,
		const KeybindDefinitions& a_definitions,
		const UserKeybinds& a_bindings,
		DiagnosticReporter& a_diagnostics) noexcept;
}
