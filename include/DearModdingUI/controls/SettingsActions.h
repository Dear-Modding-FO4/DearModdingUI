#pragma once

#include <DearModdingUI/IconGlyphs.h>

#include <cstdint>

namespace DearModdingUI
{
	enum class SettingsAction : uint32_t
	{
		kReset,
		kRevert,
		kApply
	};

	[[nodiscard]] constexpr char32_t SettingsActionGlyph(
		SettingsAction a_action) noexcept
	{
		switch (a_action)
		{
		case SettingsAction::kReset:
			return dmui::PhosphorGlyph::kArrowsClockwise;
		case SettingsAction::kRevert:
			return dmui::PhosphorGlyph::kArrowCounterClockwise;
		case SettingsAction::kApply:
			return dmui::PhosphorGlyph::kFloppyDisk;
		}
		return {};
	}

	[[nodiscard]] constexpr bool SettingsActionEnabled(
		SettingsAction a_action,
		bool a_dirty) noexcept
	{
		return a_action == SettingsAction::kReset || a_dirty;
	}
}
