#include <DearModdingUI/presentation/Theme.h>

namespace DearModdingUI::Theme
{
	DMUI_ThemeTokens TokenSnapshot() noexcept
	{
		const auto& tokens = Current().tokens;
		const auto convert = [](const ImVec4& a_color) {
			return DMUI_Vec4{ a_color.x, a_color.y, a_color.z, a_color.w };
		};
		return {
			convert(tokens.text),
			convert(tokens.muted),
			convert(tokens.accent),
			convert(tokens.accentMuted),
			convert(tokens.info),
			convert(tokens.success),
			convert(tokens.warning),
			convert(tokens.error),
			convert(tokens.disabled),
			convert(tokens.restartNeeded),
			convert(tokens.currentHotkey),
			convert(tokens.rowAlt)
		};
	}

	ImVec4 TextColor(dmui::TextTone a_tone) noexcept
	{
		const auto& tokens = Current().tokens;
		switch (a_tone)
		{
		case dmui::TextTone::kMuted:
			return tokens.muted;
		case dmui::TextTone::kAccent:
			return tokens.accent;
		case dmui::TextTone::kAccentMuted:
			return tokens.accentMuted;
		case dmui::TextTone::kInfo:
			return tokens.info;
		case dmui::TextTone::kSuccess:
			return tokens.success;
		case dmui::TextTone::kWarning:
			return tokens.warning;
		case dmui::TextTone::kError:
			return tokens.error;
		case dmui::TextTone::kDisabled:
			return tokens.disabled;
		case dmui::TextTone::kRestartNeeded:
			return tokens.restartNeeded;
		case dmui::TextTone::kCurrentHotkey:
			return tokens.currentHotkey;
		case dmui::TextTone::kInherit:
		default:
			return tokens.text;
		}
	}

	dmui::TextTone StatusTone(DMUI_Severity a_severity) noexcept
	{
		switch (a_severity)
		{
		case DMUI_SEVERITY_SUCCESS:
			return dmui::TextTone::kSuccess;
		case DMUI_SEVERITY_WARNING:
			return dmui::TextTone::kWarning;
		case DMUI_SEVERITY_ERROR:
			return dmui::TextTone::kError;
		default:
			return dmui::TextTone::kInfo;
		}
	}

	ImVec4 StatusTextColor(DMUI_Severity a_severity) noexcept
	{
		return TextColor(StatusTone(a_severity));
	}
}
