#pragma once

#include <DearModdingUI/navigation/NavigationController.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace DearModdingUI
{
	enum class NavigationPresentationKind : uint32_t
	{
		Grouped
#if defined(DMUI_PREVIEW)
		,
		Destinations
#endif
	};

#if defined(DMUI_PREVIEW)
	struct DestinationsNavigationPresentationState
	{
		DMUI_ClientOrigin selectedOrigin{ DMUI_CLIENT_ORIGIN_NATIVE };
	};
#endif

	struct NavigationPresentationState
	{
#if defined(DMUI_PREVIEW)
		DestinationsNavigationPresentationState destinations;
#endif
	};

	struct NavigationSourceControlId
	{
		uint32_t value{ 0 };

		constexpr bool operator==(
			const NavigationSourceControlId&) const noexcept = default;
	};

	struct NavigationSourceControl
	{
		NavigationSourceControlId id;
		std::string_view label;
		bool selected{ false };
	};

	enum class ControlledNavigationControlDecision : uint32_t
	{
		None,
		Activate
	};

	[[nodiscard]] constexpr ControlledNavigationControlDecision
		ResolveControlledNavigationControlDecision(
			bool a_selected,
			bool a_userActivated) noexcept
	{
		return a_userActivated && !a_selected ?
			ControlledNavigationControlDecision::Activate :
			ControlledNavigationControlDecision::None;
	}

	struct NavigationPresentationTransition
	{
		bool accepted{ false };
		bool presentationChanged{ false };
		std::optional<NavigationRequest> navigation;
	};

	struct PresentedNavigationSection
	{
		size_t sectionIndex{ 0 };
		bool showHeading{ false };
	};

	struct NavigationPresentation
	{
		std::vector<NavigationSourceControl> sourceControls;
		std::vector<PresentedNavigationSection> sections;

		[[nodiscard]] size_t ClientCount(
			const NavigationModel& a_model) const noexcept;
		[[nodiscard]] size_t HeadingCount() const noexcept;
	};

#if defined(DMUI_PREVIEW)
	struct DestinationsNavigationPresentation
	{
		[[nodiscard]] static NavigationPresentation Build(
			const NavigationModel& a_model,
			const DestinationsNavigationPresentationState& a_state);
		[[nodiscard]] static NavigationPresentationTransition Activate(
			const NavigationModel& a_model,
			NavigationSourceControlId a_control,
			const ClientSelectionState& a_selection,
			DestinationsNavigationPresentationState& a_state) noexcept;
		static void RevealClient(
			const NavigationModel& a_model,
			DMUI_ClientHandle a_client,
			DestinationsNavigationPresentationState& a_state) noexcept;
	};
#endif

	[[nodiscard]] NavigationPresentation BuildNavigationPresentation(
		NavigationPresentationKind a_kind,
		const NavigationModel& a_model,
		const NavigationPresentationState& a_state);
	[[nodiscard]] NavigationPresentationTransition
		ActivateNavigationSourceControl(
		NavigationPresentationKind a_kind,
		const NavigationModel& a_model,
		NavigationSourceControlId a_control,
		const ClientSelectionState& a_selection,
		NavigationPresentationState& a_state) noexcept;
	void RevealNavigationClient(
		NavigationPresentationKind a_kind,
		const NavigationModel& a_model,
		DMUI_ClientHandle a_client,
		NavigationPresentationState& a_state) noexcept;
}
