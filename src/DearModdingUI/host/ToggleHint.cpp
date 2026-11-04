#include <DearModdingUI/host/ToggleHint.h>

#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/localization/Localization.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/settings/HostSettings.h>
#include <Support/Message.h>

#include <chrono>

namespace DearModdingUI::ToggleHint
{
	namespace
	{
		using namespace std::chrono_literals;

		constexpr Support::Phrase kTitle{
			"$DMUI_ToggleHint_Title",
			"Press {} to open the menu"
		};
		constexpr Support::Phrase kTitleWithGamepad{
			"$DMUI_ToggleHint_TitleGamepad",
			"Press {} or {} to open the menu"
		};
		constexpr Support::Phrase kSettingsRow{
			"$DMUI_ToggleHint_SettingsRow",
			"Or select {} in the game's Settings menu."
		};
		constexpr Support::Phrase kChangeKey{
			"$DMUI_ToggleHint_ChangeKey",
			"Rebind it in its settings."
		};
		constexpr auto kDuration = 15s;
	}

	void ShowOnce(bool a_gamepadConnected) noexcept
	{
		if (!HostSettings::ClaimToggleHint())
			return;
		try
		{
			const auto& settings = HostSettings::Current();
			const auto key = FormatHotkeyChord(settings.menuToggleKey);
			const auto title = a_gamepadConnected ?
				Localization::Render(Support::Message{
					kTitleWithGamepad, key, FormatHotkeyChord(settings.menuToggleGamepad) }) :
				Localization::Render(Support::Message{ kTitle, key });
			const auto body = Localization::Render(Support::Message::Join(
				{ settings.settingsMenuEntry ?
						Support::Message{ kSettingsRow, Localization::Text(kMenuEntryLabel) } :
						Support::Message{},
					Support::Message{ kChangeKey } },
				" "));
			PresentationServices::PostHostNotification(
				DMUI_STATUS_SEVERITY_INFO, title, body, kDuration);
		}
		catch (...)
		{}
	}
}
