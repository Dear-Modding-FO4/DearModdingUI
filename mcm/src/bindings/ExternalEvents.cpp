#include <DearModdingUI/MCM/ExternalEvents.h>

#include <iterator>

namespace DearModdingUI::MCM
{
	namespace
	{
		void Append(
			std::vector<McmExternalEvent>& a_target,
			std::vector<McmExternalEvent> a_events)
		{
			a_target.insert(
				a_target.end(),
				std::make_move_iterator(a_events.begin()),
				std::make_move_iterator(a_events.end()));
		}
	}

	std::vector<McmExternalEvent> OverlayOpenedExternalEvents()
	{
		return { { "OnMCMOpen", {} } };
	}

	std::vector<McmExternalEvent> OverlayClosedExternalEvents()
	{
		return { { "OnMCMClose", {} } };
	}

	std::vector<McmExternalEvent> MenuOpenedExternalEvents(
		std::string_view a_modName)
	{
		std::vector<McmExternalEvent> result{
			{ "OnMCMMenuOpen", {} }
		};
		if (!a_modName.empty())
			result.push_back(
				{ "OnMCMMenuOpen|" + std::string{ a_modName }, {} });
		return result;
	}

	std::vector<McmExternalEvent> MenuClosedExternalEvents(
		std::string_view a_modName)
	{
		if (a_modName.empty())
			return {};
		// The shipped SWF sends the filtered event without MCM.psc's documented modName argument.
		return { { "OnMCMMenuClose|" + std::string{ a_modName }, {} } };
	}

	std::vector<McmExternalEvent> McmEventLifecycle::PageActivated(
		std::string_view a_modName)
	{
		std::vector<McmExternalEvent> result;
		if (!sessionOpen_)
		{
			sessionOpen_ = true;
			Append(result, OverlayOpenedExternalEvents());
		}
		activeMod_ = a_modName;
		Append(result, MenuOpenedExternalEvents(a_modName));
		return result;
	}

	std::vector<McmExternalEvent> McmEventLifecycle::PageDeactivated(
		std::string_view a_modName,
		bool a_overlayVisible)
	{
		if (!activeMod_ || *activeMod_ != a_modName)
			return {};
		auto result = MenuClosedExternalEvents(a_modName);
		activeMod_.reset();
		// MCM's focus-based OnMCMClose is unreliable, so only genuine overlay closure ends the session.
		if (!a_overlayVisible)
			Append(result, CloseOverlay());
		return result;
	}

	std::vector<McmExternalEvent> McmEventLifecycle::OverlayVisibilityChanged(
		bool a_visible)
	{
		if (a_visible || !sessionOpen_)
			return {};
		auto result = activeMod_ ?
			MenuClosedExternalEvents(*activeMod_) :
			std::vector<McmExternalEvent>{};
		activeMod_.reset();
		Append(result, CloseOverlay());
		return result;
	}

	std::vector<McmExternalEvent> McmEventLifecycle::CloseOverlay()
	{
		if (!sessionOpen_)
			return {};
		sessionOpen_ = false;
		return OverlayClosedExternalEvents();
	}
}
