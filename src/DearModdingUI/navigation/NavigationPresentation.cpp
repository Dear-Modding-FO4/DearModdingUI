#include <DearModdingUI/navigation/NavigationPresentation.h>

#include <algorithm>
#include <cassert>

namespace DearModdingUI
{
	size_t NavigationPresentation::ClientCount(
		const NavigationModel& a_model) const noexcept
	{
		size_t result{};
		for (const auto& section : sections)
		{
			assert(section.sectionIndex < a_model.sections.size());
			result +=
				a_model.sections[section.sectionIndex].clientIndices.size();
		}
		return result;
	}

	size_t NavigationPresentation::HeadingCount() const noexcept
	{
		return static_cast<size_t>(std::ranges::count(
			sections,
			true,
			&PresentedNavigationSection::showHeading));
	}

	NavigationPresentation BuildNavigationPresentation(
		[[maybe_unused]] NavigationPresentationKind a_kind,
		const NavigationModel& a_model,
		[[maybe_unused]] const NavigationPresentationState& a_state)
	{
#if defined(DMUI_PREVIEW)
		if (a_kind == NavigationPresentationKind::Destinations)
			return DestinationsNavigationPresentation::Build(
				a_model,
				a_state.destinations);
#endif
		NavigationPresentation result;
		result.sections.reserve(a_model.sections.size());
		for (size_t index = 0; index < a_model.sections.size(); ++index)
			result.sections.push_back({ index, true });
		return result;
	}

	NavigationPresentationTransition ActivateNavigationSourceControl(
		[[maybe_unused]] NavigationPresentationKind a_kind,
		[[maybe_unused]] const NavigationModel& a_model,
		[[maybe_unused]] NavigationSourceControlId a_control,
		[[maybe_unused]] const ClientSelectionState& a_selection,
		[[maybe_unused]] NavigationPresentationState& a_state) noexcept
	{
#if defined(DMUI_PREVIEW)
		if (a_kind == NavigationPresentationKind::Destinations)
			return DestinationsNavigationPresentation::Activate(
				a_model,
				a_control,
				a_selection,
				a_state.destinations);
#endif
		return {};
	}

	void RevealNavigationClient(
		[[maybe_unused]] NavigationPresentationKind a_kind,
		[[maybe_unused]] const NavigationModel& a_model,
		[[maybe_unused]] DMUI_ClientHandle a_client,
		[[maybe_unused]] NavigationPresentationState& a_state) noexcept
	{
#if defined(DMUI_PREVIEW)
		if (a_kind == NavigationPresentationKind::Destinations)
			DestinationsNavigationPresentation::RevealClient(
				a_model,
				a_client,
				a_state.destinations);
#endif
	}
}
