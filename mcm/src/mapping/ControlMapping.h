#pragma once

#include "../support/Diagnostics.h"

#include <DearModdingUI/MCM/Compatibility.h>

namespace DearModdingUI::MCM::detail
{
	[[nodiscard]] bool UsesSignedNumbers(const Control& a_control);

	[[nodiscard]] bool HasValidIntegerSliderParameters(
		const Control& a_control) noexcept;

	void MapCheckboxDefault(
		const Control& a_control,
		dmui::SettingDescriptor& a_descriptor,
		Diagnostics& a_diagnostics);

	void MapDoubleControl(
		const Control& a_control,
		dmui::SettingDescriptor& a_descriptor,
		Diagnostics& a_diagnostics,
		MappedRow* a_sliderRow);

	void MapSignedControl(
		const Control& a_control,
		dmui::SettingDescriptor& a_descriptor,
		Diagnostics& a_diagnostics,
		MappedRow* a_sliderRow);
}
