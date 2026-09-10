#include <DearModdingUI/navigation/NavigationPresentation.h>

#if defined(DMUI_PREVIEW)

#include <algorithm>
#include <cassert>

namespace DearModdingUI
{
	namespace
	{
		inline constexpr NavigationSourceControlId kNativeControl{ 1 };
		inline constexpr NavigationSourceControlId kBridgedControl{ 2 };

		[[nodiscard]] bool HasOrigin(
			const NavigationModel& a_model,
			DMUI_ClientOrigin a_origin) noexcept
		{
			return std::ranges::any_of(
				a_model.sections,
				[&](const auto& a_section) {
					return a_section.origin == a_origin;
				});
		}

		[[nodiscard]] std::optional<DMUI_ClientOrigin> ControlOrigin(
			NavigationSourceControlId a_control) noexcept
		{
			if (a_control == kNativeControl)
				return DMUI_CLIENT_ORIGIN_NATIVE;
			if (a_control == kBridgedControl)
				return DMUI_CLIENT_ORIGIN_BRIDGED;
			return std::nullopt;
		}

		[[nodiscard]] DMUI_ClientOrigin EffectiveOrigin(
			const NavigationModel& a_model,
			DMUI_ClientOrigin a_preferred) noexcept
		{
			if (HasOrigin(a_model, a_preferred))
				return a_preferred;
			return HasOrigin(a_model, DMUI_CLIENT_ORIGIN_NATIVE) ?
				DMUI_CLIENT_ORIGIN_NATIVE :
				DMUI_CLIENT_ORIGIN_BRIDGED;
		}

		[[nodiscard]] std::optional<NavigationRequest> FirstClientRequest(
			const NavigationModel& a_model,
			DMUI_ClientOrigin a_origin) noexcept
		{
			const auto section = std::ranges::find_if(
				a_model.sections,
				[&](const auto& a_entry) {
					return a_entry.origin == a_origin;
				});
			if (section == a_model.sections.end())
				return std::nullopt;
			assert(!section->clientIndices.empty());
			const auto clientIndex = section->clientIndices.front();
			assert(clientIndex < a_model.clients.size());
			return NavigationRequest::Client(
				a_model.clients[clientIndex].handle);
		}
	}

	NavigationPresentation DestinationsNavigationPresentation::Build(
		const NavigationModel& a_model,
		const DestinationsNavigationPresentationState& a_state)
	{
		NavigationPresentation result;
		const auto selectedOrigin =
			EffectiveOrigin(a_model, a_state.selectedOrigin);
		if (HasOrigin(a_model, DMUI_CLIENT_ORIGIN_NATIVE))
		{
			result.sourceControls.push_back({
				kNativeControl,
				"Native",
				selectedOrigin == DMUI_CLIENT_ORIGIN_NATIVE
			});
		}
		if (HasOrigin(a_model, DMUI_CLIENT_ORIGIN_BRIDGED))
		{
			result.sourceControls.push_back({
				kBridgedControl,
				"Bridged",
				selectedOrigin == DMUI_CLIENT_ORIGIN_BRIDGED
			});
		}
		for (size_t index = 0; index < a_model.sections.size(); ++index)
		{
			const auto& section = a_model.sections[index];
			if (section.origin == selectedOrigin)
			{
				result.sections.push_back({
					index,
					section.origin == DMUI_CLIENT_ORIGIN_BRIDGED
				});
			}
		}
		return result;
	}

	NavigationPresentationTransition
		DestinationsNavigationPresentation::Activate(
		const NavigationModel& a_model,
		NavigationSourceControlId a_control,
		const ClientSelectionState& a_selection,
		DestinationsNavigationPresentationState& a_state) noexcept
	{
		const auto origin = ControlOrigin(a_control);
		if (!origin || !HasOrigin(a_model, *origin))
			return {};
		const auto changed = a_state.selectedOrigin != *origin;
		a_state.selectedOrigin = *origin;
		const auto* active = a_model.FindClient(a_selection.activeClient);
		return {
			true,
			changed,
			active && active->origin == *origin ?
				std::nullopt :
				FirstClientRequest(a_model, *origin)
		};
	}

	void DestinationsNavigationPresentation::RevealClient(
		const NavigationModel& a_model,
		DMUI_ClientHandle a_client,
		DestinationsNavigationPresentationState& a_state) noexcept
	{
		if (const auto* client = a_model.FindClient(a_client))
			a_state.selectedOrigin = client->origin;
	}
}

#endif
