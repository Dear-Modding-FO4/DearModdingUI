#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <vector>

namespace DearModdingUI::MCM
{
	struct McmExternalEvent
	{
		std::string name;
		std::vector<std::string> arguments;

		[[nodiscard]] bool operator==(const McmExternalEvent&) const = default;
	};

	[[nodiscard]] std::vector<McmExternalEvent> OverlayOpenedExternalEvents();
	[[nodiscard]] std::vector<McmExternalEvent> OverlayClosedExternalEvents();
	[[nodiscard]] std::vector<McmExternalEvent> MenuOpenedExternalEvents(
		std::string_view a_modName);
	[[nodiscard]] std::vector<McmExternalEvent> MenuClosedExternalEvents(
		std::string_view a_modName);

	class McmEventLifecycle
	{
	public:
		[[nodiscard]] std::vector<McmExternalEvent> PageActivated(
			std::string_view a_modName);
		[[nodiscard]] std::vector<McmExternalEvent> PageDeactivated(
			std::string_view a_modName,
			bool a_overlayVisible);
		[[nodiscard]] std::vector<McmExternalEvent> OverlayVisibilityChanged(
			bool a_visible);

	private:
		[[nodiscard]] std::vector<McmExternalEvent> CloseOverlay();

		bool sessionOpen_{};
		std::optional<std::string> activeMod_;
	};
}
