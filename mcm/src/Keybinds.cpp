#include <DearModdingUI/MCM/Keybinds.h>

#include <DearModdingUI/MCM/JsonNormalization.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <sstream>
#include <utility>

namespace DearModdingUI::MCM
{
	namespace
	{
		using Json = nlohmann::json;

		struct KeyNameEntry
		{
			int32_t code;
			std::string_view name;
		};

		constexpr std::array kKeyboardNames{
			KeyNameEntry{ 0x01, "Esc" },
			KeyNameEntry{ 0x02, "1" },
			KeyNameEntry{ 0x03, "2" },
			KeyNameEntry{ 0x04, "3" },
			KeyNameEntry{ 0x05, "4" },
			KeyNameEntry{ 0x06, "5" },
			KeyNameEntry{ 0x07, "6" },
			KeyNameEntry{ 0x08, "7" },
			KeyNameEntry{ 0x09, "8" },
			KeyNameEntry{ 0x0A, "9" },
			KeyNameEntry{ 0x0B, "0" },
			KeyNameEntry{ 0x0C, "-" },
			KeyNameEntry{ 0x0D, "=" },
			KeyNameEntry{ 0x0E, "Backspace" },
			KeyNameEntry{ 0x0F, "Tab" },
			KeyNameEntry{ 0x10, "Q" },
			KeyNameEntry{ 0x11, "W" },
			KeyNameEntry{ 0x12, "E" },
			KeyNameEntry{ 0x13, "R" },
			KeyNameEntry{ 0x14, "T" },
			KeyNameEntry{ 0x15, "Y" },
			KeyNameEntry{ 0x16, "U" },
			KeyNameEntry{ 0x17, "I" },
			KeyNameEntry{ 0x18, "O" },
			KeyNameEntry{ 0x19, "P" },
			KeyNameEntry{ 0x1A, "[" },
			KeyNameEntry{ 0x1B, "]" },
			KeyNameEntry{ 0x1C, "Enter" },
			KeyNameEntry{ 0x1D, "Left Ctrl" },
			KeyNameEntry{ 0x1E, "A" },
			KeyNameEntry{ 0x1F, "S" },
			KeyNameEntry{ 0x20, "D" },
			KeyNameEntry{ 0x21, "F" },
			KeyNameEntry{ 0x22, "G" },
			KeyNameEntry{ 0x23, "H" },
			KeyNameEntry{ 0x24, "J" },
			KeyNameEntry{ 0x25, "K" },
			KeyNameEntry{ 0x26, "L" },
			KeyNameEntry{ 0x27, ";" },
			KeyNameEntry{ 0x28, "'" },
			KeyNameEntry{ 0x29, "`" },
			KeyNameEntry{ 0x2A, "Left Shift" },
			KeyNameEntry{ 0x2B, "\\" },
			KeyNameEntry{ 0x2C, "Z" },
			KeyNameEntry{ 0x2D, "X" },
			KeyNameEntry{ 0x2E, "C" },
			KeyNameEntry{ 0x2F, "V" },
			KeyNameEntry{ 0x30, "B" },
			KeyNameEntry{ 0x31, "N" },
			KeyNameEntry{ 0x32, "M" },
			KeyNameEntry{ 0x33, "," },
			KeyNameEntry{ 0x34, "." },
			KeyNameEntry{ 0x35, "/" },
			KeyNameEntry{ 0x36, "Right Shift" },
			KeyNameEntry{ 0x37, "Numpad *" },
			KeyNameEntry{ 0x38, "Left Alt" },
			KeyNameEntry{ 0x39, "Space" },
			KeyNameEntry{ 0x3A, "Caps Lock" },
			KeyNameEntry{ 0x3B, "F1" },
			KeyNameEntry{ 0x3C, "F2" },
			KeyNameEntry{ 0x3D, "F3" },
			KeyNameEntry{ 0x3E, "F4" },
			KeyNameEntry{ 0x3F, "F5" },
			KeyNameEntry{ 0x40, "F6" },
			KeyNameEntry{ 0x41, "F7" },
			KeyNameEntry{ 0x42, "F8" },
			KeyNameEntry{ 0x43, "F9" },
			KeyNameEntry{ 0x44, "F10" },
			KeyNameEntry{ 0x45, "Num Lock" },
			KeyNameEntry{ 0x46, "Scroll Lock" },
			KeyNameEntry{ 0x47, "Numpad 7" },
			KeyNameEntry{ 0x48, "Numpad 8" },
			KeyNameEntry{ 0x49, "Numpad 9" },
			KeyNameEntry{ 0x4A, "Numpad -" },
			KeyNameEntry{ 0x4B, "Numpad 4" },
			KeyNameEntry{ 0x4C, "Numpad 5" },
			KeyNameEntry{ 0x4D, "Numpad 6" },
			KeyNameEntry{ 0x4E, "Numpad +" },
			KeyNameEntry{ 0x4F, "Numpad 1" },
			KeyNameEntry{ 0x50, "Numpad 2" },
			KeyNameEntry{ 0x51, "Numpad 3" },
			KeyNameEntry{ 0x52, "Numpad 0" },
			KeyNameEntry{ 0x53, "Numpad ." },
			KeyNameEntry{ 0x56, "OEM 102" },
			KeyNameEntry{ 0x57, "F11" },
			KeyNameEntry{ 0x58, "F12" },
			KeyNameEntry{ 0x64, "F13" },
			KeyNameEntry{ 0x65, "F14" },
			KeyNameEntry{ 0x66, "F15" },
			KeyNameEntry{ 0x70, "Kana" },
			KeyNameEntry{ 0x73, "ABNT C1" },
			KeyNameEntry{ 0x79, "Convert" },
			KeyNameEntry{ 0x7B, "No Convert" },
			KeyNameEntry{ 0x7D, "Yen" },
			KeyNameEntry{ 0x7E, "ABNT C2" },
			KeyNameEntry{ 0x8D, "Numpad =" },
			KeyNameEntry{ 0x90, "Previous Track" },
			KeyNameEntry{ 0x91, "At" },
			KeyNameEntry{ 0x92, "Colon" },
			KeyNameEntry{ 0x93, "Underline" },
			KeyNameEntry{ 0x94, "Kanji" },
			KeyNameEntry{ 0x95, "Stop" },
			KeyNameEntry{ 0x96, "AX" },
			KeyNameEntry{ 0x97, "Unlabeled" },
			KeyNameEntry{ 0x99, "Next Track" },
			KeyNameEntry{ 0x9C, "Numpad Enter" },
			KeyNameEntry{ 0x9D, "Right Ctrl" },
			KeyNameEntry{ 0xA0, "Mute" },
			KeyNameEntry{ 0xA1, "Calculator" },
			KeyNameEntry{ 0xA2, "Play/Pause" },
			KeyNameEntry{ 0xA4, "Media Stop" },
			KeyNameEntry{ 0xAE, "Volume Down" },
			KeyNameEntry{ 0xB0, "Volume Up" },
			KeyNameEntry{ 0xB2, "Web Home" },
			KeyNameEntry{ 0xB3, "Numpad ," },
			KeyNameEntry{ 0xB5, "Numpad /" },
			KeyNameEntry{ 0xB7, "SysRq" },
			KeyNameEntry{ 0xB8, "Right Alt" },
			KeyNameEntry{ 0xC5, "Pause" },
			KeyNameEntry{ 0xC7, "Home" },
			KeyNameEntry{ 0xC8, "Up" },
			KeyNameEntry{ 0xC9, "Page Up" },
			KeyNameEntry{ 0xCB, "Left" },
			KeyNameEntry{ 0xCD, "Right" },
			KeyNameEntry{ 0xCF, "End" },
			KeyNameEntry{ 0xD0, "Down" },
			KeyNameEntry{ 0xD1, "Page Down" },
			KeyNameEntry{ 0xD2, "Insert" },
			KeyNameEntry{ 0xD3, "Delete" },
			KeyNameEntry{ 0xDB, "Left Windows" },
			KeyNameEntry{ 0xDC, "Right Windows" },
			KeyNameEntry{ 0xDD, "Menu" },
			KeyNameEntry{ 0xDE, "Power" },
			KeyNameEntry{ 0xDF, "Sleep" },
			KeyNameEntry{ 0xE3, "Wake" },
			KeyNameEntry{ 0xE5, "Web Search" },
			KeyNameEntry{ 0xE6, "Web Favorites" },
			KeyNameEntry{ 0xE7, "Web Refresh" },
			KeyNameEntry{ 0xE8, "Web Stop" },
			KeyNameEntry{ 0xE9, "Web Forward" },
			KeyNameEntry{ 0xEA, "Web Back" },
			KeyNameEntry{ 0xEB, "My Computer" },
			KeyNameEntry{ 0xEC, "Mail" },
			KeyNameEntry{ 0xED, "Media Select" }
		};

		constexpr std::array<std::string_view, kMouseButtonCount> kMouseNames{
			"Mouse 1",
			"Mouse 2",
			"Mouse 3",
			"Mouse 4",
			"Mouse 5",
			"Mouse 6",
			"Mouse 7",
			"Mouse 8"
		};

		constexpr std::array<std::string_view, kMouseWheelDirectionCount>
			kMouseWheelNames{
				"Mouse Wheel Up",
				"Mouse Wheel Down"
			};

		constexpr std::array<std::string_view, kGamepadButtonCount>
			kGamepadNames{
				"D-Pad Up",
				"D-Pad Down",
				"D-Pad Left",
				"D-Pad Right",
				"Start",
				"Back",
				"Left Stick",
				"Right Stick",
				"Left Bumper",
				"Right Bumper",
				"A",
				"B",
				"X",
				"Y",
				"Left Trigger",
				"Right Trigger"
			};

		template <class Result, class Parse>
		[[nodiscard]] Result LoadJson(
			const std::filesystem::path& a_path,
			Parse&& a_parse) noexcept
		{
			try
			{
				std::ifstream stream{ a_path, std::ios::binary };
				if (!stream)
					return {};
				std::ostringstream buffer;
				buffer << stream.rdbuf();
				if (stream.bad())
				{
					Result result;
					result.state = KeybindFileState::kMalformed;
					return result;
				}
				return a_parse(buffer.str());
			}
			catch (...)
			{
				Result result;
				result.state = KeybindFileState::kMalformed;
				return result;
			}
		}

		[[nodiscard]] bool ReadString(
			const Json& a_object,
			std::string_view a_name,
			std::string& a_result)
		{
			const auto found = a_object.find(a_name);
			if (found == a_object.end() || !found->is_string())
				return false;
			a_result = found->get<std::string>();
			return !a_result.empty();
		}

		[[nodiscard]] dmui::SettingDescriptor* FindDescriptor(
			MappedPage& a_page,
			std::string_view a_id) noexcept
		{
			for (auto& group : a_page.settings.groups)
			{
				for (auto& setting : group.settings)
				if (setting.id == a_id)
					return &setting;
			}
			return nullptr;
		}

		void SetKeybindState(
			MappedPage& a_page,
			MappedRow& a_row,
			std::string a_text,
			ResolvedInertState a_state)
		{
			a_row.keybindInertState = a_state;
			if (a_row.text)
				a_row.text->presentation.text = a_text;
			if (auto* descriptor = FindDescriptor(a_page, a_row.id))
				descriptor->defaultValue = std::move(a_text);
		}
	}

	bool KeybindDefinitions::Contains(std::string_view a_id) const noexcept
	{
		return std::ranges::find(ids, a_id) != ids.end();
	}

	const UserKeybind* UserKeybinds::Find(
		std::string_view a_modName,
		std::string_view a_id) const noexcept
	{
		const auto found = std::ranges::find_if(
			bindings,
			[&](const UserKeybind& a_binding) {
				return a_binding.modName == a_modName && a_binding.id == a_id;
			});
		return found == bindings.end() ? nullptr : &*found;
	}

	KeybindDefinitions ParseKeybindDefinitions(std::string_view a_json) noexcept
	{
		KeybindDefinitions result;
		result.state = KeybindFileState::kMalformed;
		try
		{
			const auto root = Json::parse(
				NormalizeJson(a_json, { .invalidEscapePassThrough = true }),
				nullptr,
				false);
			if (!root.is_object() ||
				!ReadString(root, "modName", result.modName))
				return result;
			const auto keybinds = root.find("keybinds");
			if (keybinds == root.end() || !keybinds->is_array())
				return result;
			for (const auto& value : *keybinds)
			{
				std::string id;
				if (!value.is_object() || !ReadString(value, "id", id))
					return result;
				if (!std::ranges::contains(result.ids, id))
					result.ids.push_back(std::move(id));
			}
			result.state = KeybindFileState::kLoaded;
		}
		catch (...)
		{
			result.modName.clear();
			result.ids.clear();
		}
		return result;
	}

	KeybindDefinitions LoadKeybindDefinitions(
		const std::filesystem::path& a_path) noexcept
	{
		return LoadJson<KeybindDefinitions>(a_path, ParseKeybindDefinitions);
	}

	UserKeybinds ParseUserKeybinds(std::string_view a_json) noexcept
	{
		UserKeybinds result;
		result.state = KeybindFileState::kMalformed;
		try
		{
			const auto root = Json::parse(NormalizeJson(a_json), nullptr, false);
			if (!root.is_object())
				return result;
			const auto keybinds = root.find("keybinds");
			if (keybinds == root.end() || !keybinds->is_array())
				return result;
			for (const auto& value : *keybinds)
			{
				UserKeybind binding;
				if (!value.is_object() ||
					!ReadString(value, "modName", binding.modName) ||
					!ReadString(value, "id", binding.id))
					return result;
				const auto keycode = value.find("keycode");
				const auto modifiers = value.find("modifiers");
				if (keycode == value.end() ||
					!keycode->is_number_integer() ||
					modifiers == value.end() ||
					!modifiers->is_number_unsigned())
					return result;
				const auto rawKeycode = keycode->get<int64_t>();
				const auto rawModifiers = modifiers->get<uint64_t>();
				if (rawKeycode < (std::numeric_limits<int32_t>::min)() ||
					rawKeycode > (std::numeric_limits<int32_t>::max)() ||
					rawModifiers > (std::numeric_limits<uint32_t>::max)())
					return result;
				binding.keycode = static_cast<int32_t>(rawKeycode);
				binding.modifiers = static_cast<uint32_t>(rawModifiers);
				result.bindings.push_back(std::move(binding));
			}
			result.state = KeybindFileState::kLoaded;
		}
		catch (...)
		{
			result.bindings.clear();
		}
		return result;
	}

	UserKeybinds LoadUserKeybinds(const std::filesystem::path& a_path) noexcept
	{
		return LoadJson<UserKeybinds>(a_path, ParseUserKeybinds);
	}

	std::string KeyName(int32_t a_keycode)
	{
		if (a_keycode >= 0 && a_keycode < kKeyboardKeyCount)
		{
			const auto found = std::ranges::find(
				kKeyboardNames,
				a_keycode,
				&KeyNameEntry::code);
			if (found != kKeyboardNames.end())
				return std::string{ found->name };
		}
		else if (a_keycode >= kMouseButtonOffset &&
			a_keycode < kMouseButtonOffset + kMouseButtonCount)
		{
			return std::string{
				kMouseNames[static_cast<size_t>(a_keycode - kMouseButtonOffset)]
			};
		}
		else if (a_keycode >= kMouseWheelOffset &&
			a_keycode < kMouseWheelOffset + kMouseWheelDirectionCount)
		{
			return std::string{
				kMouseWheelNames[static_cast<size_t>(
					a_keycode - kMouseWheelOffset)]
			};
		}
		else if (a_keycode >= kGamepadButtonOffset &&
			a_keycode < kGamepadButtonOffset + kGamepadButtonCount)
		{
			return std::string{
				kGamepadNames[static_cast<size_t>(a_keycode - kGamepadButtonOffset)]
			};
		}
		return "Keycode " + std::to_string(a_keycode);
	}

	std::string FormatKeybind(int32_t a_keycode, uint32_t a_modifiers)
	{
		std::string result;
		const auto append = [&result](std::string_view a_value) {
			if (!result.empty())
				result.push_back('+');
			result.append(a_value);
		};
		if ((a_modifiers & 2u) != 0)
			append("Ctrl");
		if ((a_modifiers & 1u) != 0)
			append("Shift");
		if ((a_modifiers & 4u) != 0)
			append("Alt");
		if (const auto other = a_modifiers & ~7u)
			append("Modifier " + std::to_string(other));
		append(KeyName(a_keycode));
		return result;
	}

	void ApplyKeybinds(
		MappedPage& a_page,
		const KeybindDefinitions& a_definitions,
		const UserKeybinds& a_bindings,
		DiagnosticReporter& a_diagnostics) noexcept
	{
		try
		{
			for (auto& row : a_page.rows)
			{
				if (!row.keybindId)
					continue;
				if (a_definitions.state == KeybindFileState::kMissing)
				{
					SetKeybindState(
						a_page,
						row,
						"Can't be bound",
						{ InertReason::kKeybindDefinitionsMissing });
					continue;
				}
				if (a_definitions.state == KeybindFileState::kMalformed)
				{
					SetKeybindState(
						a_page,
						row,
						"Can't be bound",
						{ InertReason::kKeybindDefinitionsInvalid });
					continue;
				}
				if (!a_definitions.Contains(*row.keybindId))
				{
					SetKeybindState(
						a_page,
						row,
						"Can't be bound",
						{
							InertReason::kKeybindDefinitionMissing,
							InertReason::kKeybindDefinitionMissing
						});
					continue;
				}
				if (a_bindings.state == KeybindFileState::kMalformed)
				{
					SetKeybindState(
						a_page,
						row,
						"Binding unavailable",
						{ InertReason::kKeybindBindingsInvalid });
					continue;
				}
				const auto* binding = a_bindings.Find(
					a_definitions.modName,
					*row.keybindId);
				if (!binding)
				{
					SetKeybindState(
						a_page,
						row,
						"Unbound",
						{
							InertReason::kKeybindUnbound,
							InertReason::kKeybindUnbound
						});
					continue;
				}
				SetKeybindState(
					a_page,
					row,
					FormatKeybind(binding->keycode, binding->modifiers),
					{});
			}
		}
		catch (...)
		{
			a_diagnostics.Report({
				DiagnosticSeverity::kError,
				"keybind application",
				a_page.displayName,
				"keybind state could not be applied to the page"
			});
		}
	}
}
