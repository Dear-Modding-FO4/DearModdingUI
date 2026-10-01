#include <DearModdingUI/MCM/Keybinds.h>
#include <DearModdingUI/MCM/PageLookup.h>

#include <DearModdingUI/MCM/JsonNormalization.h>

#include "../support/TextFile.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace DearModdingUI::MCM
{
	namespace
	{
		using Json = nlohmann::json;

		template <class Result, class Parse>
		[[nodiscard]] Result LoadJson(
			const std::filesystem::path& a_path,
			Parse&& a_parse) noexcept
		{
			const auto file = detail::ReadTextFile(a_path);
			if (file.status == detail::TextFileStatus::kMissing)
				return {};
			if (file.status == detail::TextFileStatus::kFailed)
			{
				Result result;
				result.state = KeybindFileState::kMalformed;
				return result;
			}
			return a_parse(file.text);
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

		void SetKeybindState(
			MappedPage& a_page,
			MappedRow& a_row,
			std::string a_text,
			ResolvedInertState a_state)
		{
			a_row.keybindInertState = a_state;
			if (a_row.text)
				a_row.text->presentation.text = a_text;
			if (auto* descriptor =
					FindSettingDescriptor(a_page.settings, a_row.id))
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
		const auto label = KeyCatalog::Label(static_cast<uint32_t>(a_keycode));
		if (!label.empty())
			return std::string{ label };
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
