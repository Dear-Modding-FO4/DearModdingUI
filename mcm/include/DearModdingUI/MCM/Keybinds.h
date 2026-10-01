#pragma once

#include <DearModdingUI/MCM/Compatibility.h>
#include <DearModdingUI/MCM/DiagnosticReporter.h>
#include <Support/KeyCatalog.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace DearModdingUI::MCM
{
	using KeyCatalog::kKeyboardKeyCount;
	using KeyCatalog::kMouseButtonOffset;
	using KeyCatalog::kMouseButtonCount;
	using KeyCatalog::kMouseWheelOffset;
	using KeyCatalog::kMouseWheelDirectionCount;
	using KeyCatalog::kGamepadButtonOffset;
	using KeyCatalog::kGamepadButtonCount;
	using KeyCatalog::kMaximumMacroCode;

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
