#include "ControlDecoding.h"

#include "../support/Diagnostics.h"

#include <utility>

namespace DearModdingUI::MCM::detail
{
	ControlType DecodeControlType(std::string_view a_type)
	{
		const auto type = ToLowerAscii(a_type);
		if (type == "switch" || type == "switcher")
			return ControlType::kSwitch;
		if (type == "slider")
			return ControlType::kSlider;
		if (type == "stepper")
			return ControlType::kStepper;
		if (type == "menu" || type == "enum" || type == "dropdown")
			return ControlType::kMenu;
		if (type == "dropdownfiles")
			return ControlType::kFileMenu;
		if (type == "input" || type == "textinput")
			return ControlType::kInput;
		if (type == "text")
			return ControlType::kText;
		if (type == "header" || type == "section")
			return ControlType::kGroup;
		if (type == "empty" || type == "spacer")
			return ControlType::kSpacing;
		if (type == "hidden" || type == "hiddenswitcher")
			return ControlType::kHidden;
		if (type == "button")
			return ControlType::kButton;
		if (type == "keymap" || type == "hotkey")
			return ControlType::kKeymap;
		if (type == "color")
			return ControlType::kColor;
		if (type == "image")
			return ControlType::kImage;
		return ControlType::kUnknown;
	}

	SourceType DecodeSourceType(std::string a_raw)
	{
		SourceType result;
		const auto type = ToLowerAscii(a_raw);
		result.raw = std::move(a_raw);
		if (type.starts_with("globalvalue"))
			result.family = SourceFamily::kGlobal;
		else if (type.starts_with("propertyvalue"))
			result.family = SourceFamily::kProperty;
		else if (type.starts_with("modsetting"))
			result.family = SourceFamily::kModSetting;
		else
			return result;
		if (type.ends_with("bool"))
			result.value = SourceValueKind::kBool;
		else if (type.ends_with("int"))
			result.value = SourceValueKind::kInt;
		else if (type.ends_with("float"))
			result.value = SourceValueKind::kFloat;
		else if (type.ends_with("string"))
			result.value = SourceValueKind::kString;
		return result;
	}

	bool NeedsValueOptions(ControlType a_type) noexcept
	{
		switch (a_type)
		{
		case ControlType::kSwitch:
		case ControlType::kSlider:
		case ControlType::kStepper:
		case ControlType::kMenu:
		case ControlType::kFileMenu:
		case ControlType::kInput:
			return true;
		default:
			return false;
		}
	}
}
