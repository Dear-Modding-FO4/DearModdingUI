#pragma once

#include <DearModdingUI/navigation/Navigation.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace DearModdingUI
{
	enum class SidebarLayoutKind : uint32_t
	{
		Tree,
		TwoPane,
		DrillDown
#if defined(DMUI_PREVIEW)
		,
		IconRail
#endif
	};

	struct SidebarLayoutDescriptor
	{
		SidebarLayoutKind kind;
		std::string_view id;
		std::string_view label;
		std::string_view description;
		bool production;
		bool preview;
	};

	inline constexpr std::array SIDEBAR_LAYOUTS{
		SidebarLayoutDescriptor{
			SidebarLayoutKind::Tree,
			"tree",
			"Tree",
			"Browse every mod and page at once.",
			true,
			true
		},
		SidebarLayoutDescriptor{
			SidebarLayoutKind::TwoPane,
			"twopane",
			"Two-pane",
			"Keep a fixed mod list with the selected mod's pages.",
			true,
			true
		},
		SidebarLayoutDescriptor{
			SidebarLayoutKind::DrillDown,
			"drilldown",
			"Drill-down",
			"Show one level at a time; well suited to many mods.",
			true,
			true
		}
#if defined(DMUI_PREVIEW)
		,
		SidebarLayoutDescriptor{
			SidebarLayoutKind::IconRail,
			"iconrail",
			"Icon rail",
			"Browse mods from a compact icon rail.",
			false,
			true
		}
#endif
	};
	inline constexpr auto DEFAULT_SIDEBAR_LAYOUT = SidebarLayoutKind::Tree;

	[[nodiscard]] constexpr const SidebarLayoutDescriptor*
		FindSidebarLayout(SidebarLayoutKind a_kind) noexcept
	{
		for (const auto& layout : SIDEBAR_LAYOUTS)
		{
			if (layout.kind == a_kind)
				return &layout;
		}
		return nullptr;
	}

	[[nodiscard]] constexpr std::string_view SidebarLayoutKindName(
		SidebarLayoutKind a_kind) noexcept
	{
		const auto* layout = FindSidebarLayout(a_kind);
		return layout ? layout->id : "unknown";
	}

	[[nodiscard]] constexpr std::optional<SidebarLayoutKind> ParseSidebarLayout(
		std::string_view a_name) noexcept
	{
		for (const auto& layout : SIDEBAR_LAYOUTS)
		{
			if (layout.id == a_name)
				return layout.kind;
		}
		return std::nullopt;
	}

	[[nodiscard]] constexpr const SidebarLayoutDescriptor*
		FindUserSidebarLayout(SidebarLayoutKind a_kind) noexcept
	{
		const auto* layout = FindSidebarLayout(a_kind);
		return layout && layout->production ? layout : nullptr;
	}

	[[nodiscard]] constexpr std::optional<SidebarLayoutKind>
		ParseUserSidebarLayout(std::string_view a_name) noexcept
	{
		const auto kind = ParseSidebarLayout(a_name);
		return kind && FindUserSidebarLayout(*kind) ?
			kind :
			std::nullopt;
	}

	[[nodiscard]] constexpr SidebarLayoutKind DecodeUserSidebarLayout(
		std::string_view a_name) noexcept
	{
		return ParseUserSidebarLayout(a_name).value_or(DEFAULT_SIDEBAR_LAYOUT);
	}

	[[nodiscard]] constexpr SidebarLayoutKind NormalizeUserSidebarLayout(
		SidebarLayoutKind a_kind) noexcept
	{
		return FindUserSidebarLayout(a_kind) ?
			a_kind :
			DEFAULT_SIDEBAR_LAYOUT;
	}

	[[nodiscard]] constexpr SidebarLayoutKind ResolveSidebarLayout(
		SidebarLayoutKind a_setting,
		std::optional<SidebarLayoutKind> a_previewOverride) noexcept
	{
		return a_previewOverride.value_or(a_setting);
	}

	struct SidebarPaneHeights
	{
		float mods{ 0.0f };
		float pages{ 0.0f };

		constexpr bool operator==(const SidebarPaneHeights&) const noexcept = default;
	};

	[[nodiscard]] constexpr SidebarPaneHeights ResolveSidebarPaneHeights(
		float a_availableHeight,
		float a_modRowStride,
		size_t a_modCount,
		float a_headingRowStride,
		size_t a_headingCount,
		float a_sourceControlHeight,
		float a_minimumPagesHeight) noexcept
	{
		const auto available = (std::max)(a_availableHeight, 0.0f);
		const auto minimumPages = (std::min)(
			(std::max)(a_minimumPagesHeight, 0.0f),
			available);
		const auto desiredMods =
			(std::max)(a_modRowStride, 0.0f) *
				static_cast<float>(a_modCount) +
			(std::max)(a_headingRowStride, 0.0f) *
				static_cast<float>(a_headingCount) +
			(std::max)(a_sourceControlHeight, 0.0f);
		const auto mods = (std::min)(
			desiredMods,
			available - minimumPages);
		return { mods, available - mods };
	}

	[[nodiscard]] constexpr SidebarPaneHeights ResolveSidebarPaneHeights(
		float a_availableHeight,
		float a_modRowStride,
		size_t a_modCount,
		float a_headingRowStride,
		size_t a_headingCount,
		float a_minimumPagesHeight) noexcept
	{
		return ResolveSidebarPaneHeights(
			a_availableHeight,
			a_modRowStride,
			a_modCount,
			a_headingRowStride,
			a_headingCount,
			0.0f,
			a_minimumPagesHeight);
	}

	[[nodiscard]] constexpr SidebarPaneHeights ResolveSidebarPaneHeights(
		float a_availableHeight,
		float a_modRowStride,
		size_t a_modCount,
		float a_minimumPagesHeight) noexcept
	{
		return ResolveSidebarPaneHeights(
			a_availableHeight,
			a_modRowStride,
			a_modCount,
			0.0f,
			0,
			0.0f,
			a_minimumPagesHeight);
	}

	enum class DrillDownLevel : uint32_t
	{
		Mods,
		Pages
	};

	enum class DrillDownEvent : uint32_t
	{
		Open,
		SelectClient,
		Back
	};

	struct DrillDownState
	{
		DrillDownLevel level{ DrillDownLevel::Mods };
		uint64_t client{ 0 };

		constexpr bool operator==(const DrillDownState&) const noexcept = default;
	};

	[[nodiscard]] constexpr DrillDownState TransitionDrillDown(
		DrillDownState a_state,
		DrillDownEvent a_event,
		uint64_t a_client = 0) noexcept
	{
		switch (a_event)
		{
		case DrillDownEvent::Open:
		case DrillDownEvent::SelectClient:
			return a_client ?
				DrillDownState{ DrillDownLevel::Pages, a_client } :
				DrillDownState{};
		case DrillDownEvent::Back:
			return {};
		default:
			return a_state;
		}
	}

	struct SidebarBrowsingState
	{
		DrillDownState drillDown;
		std::map<std::string, bool> categoryExpansion;
		std::map<std::string, bool> modExpansion;
	};

	[[nodiscard]] inline std::string SidebarCategoryKey(
		const NavigationClient& a_client,
		std::string_view a_categoryId)
	{
		return a_client.id + "/" + std::string{ a_categoryId };
	}

	inline void RevealSidebarCategory(
		const NavigationModel& a_model,
		const ClientSelectionState& a_selection,
		SidebarBrowsingState& a_state)
	{
		const auto* page = a_model.FindPage(a_selection.activePage);
		const auto* client = page ?
			a_model.FindClient(page->client) :
			nullptr;
		if (!page || !client)
			return;
		a_state.categoryExpansion[
			SidebarCategoryKey(*client, page->categoryId)] = true;
	}

	struct TreeSidebarLayout
	{
		inline static constexpr auto kind = SidebarLayoutKind::Tree;

		static void RevealSelection(
			const NavigationModel& a_model,
			const ClientSelectionState& a_selection,
			SidebarBrowsingState& a_state)
		{
			if (const auto* client =
					a_model.FindClient(a_selection.activeClient))
				a_state.modExpansion[client->id] = true;
			RevealSidebarCategory(a_model, a_selection, a_state);
		}

	};

	struct TwoPaneSidebarLayout
	{
		inline static constexpr auto kind = SidebarLayoutKind::TwoPane;

		static void RevealSelection(
			const NavigationModel& a_model,
			const ClientSelectionState& a_selection,
			SidebarBrowsingState& a_state)
		{
			RevealSidebarCategory(a_model, a_selection, a_state);
		}

	};

	struct DrillDownSidebarLayout
	{
		inline static constexpr auto kind = SidebarLayoutKind::DrillDown;

		static void RevealSelection(
			const NavigationModel& a_model,
			const ClientSelectionState& a_selection,
			SidebarBrowsingState& a_state)
		{
			RevealSidebarCategory(a_model, a_selection, a_state);
			a_state.drillDown = TransitionDrillDown(
				a_state.drillDown,
				DrillDownEvent::SelectClient,
				a_model.FindClient(a_selection.activeClient) ?
					a_selection.activeClient :
					DMUI_INVALID_CLIENT_HANDLE);
		}

	};

#if defined(DMUI_PREVIEW)
	struct IconRailSidebarLayout
	{
		inline static constexpr auto kind = SidebarLayoutKind::IconRail;

		static void RevealSelection(
			const NavigationModel& a_model,
			const ClientSelectionState& a_selection,
			SidebarBrowsingState& a_state)
		{
			RevealSidebarCategory(a_model, a_selection, a_state);
		}

	};
#endif

	template<class F>
	decltype(auto) VisitSidebarLayout(
		SidebarLayoutKind a_kind,
		F&& a_fn)
	{
		switch (a_kind)
		{
		case SidebarLayoutKind::TwoPane:
			return std::forward<F>(a_fn)
				.template operator()<TwoPaneSidebarLayout>();
		case SidebarLayoutKind::DrillDown:
			return std::forward<F>(a_fn)
				.template operator()<DrillDownSidebarLayout>();
#if defined(DMUI_PREVIEW)
		case SidebarLayoutKind::IconRail:
			return std::forward<F>(a_fn)
				.template operator()<IconRailSidebarLayout>();
#endif
		default:
			return std::forward<F>(a_fn)
				.template operator()<TreeSidebarLayout>();
		}
	}

	inline void RevealSidebarSelection(
		SidebarLayoutKind a_kind,
		const NavigationModel& a_model,
		const ClientSelectionState& a_selection,
		SidebarBrowsingState& a_state)
	{
		VisitSidebarLayout(
			a_kind,
			[&]<class Layout>() {
				Layout::RevealSelection(a_model, a_selection, a_state);
			});
	}

	inline void ActivateSidebarLayout(
		SidebarLayoutKind a_kind,
		const NavigationModel& a_model,
		const ClientSelectionState& a_selection,
		SidebarBrowsingState& a_state)
	{
		RevealSidebarSelection(a_kind, a_model, a_selection, a_state);
	}

#if defined(DMUI_PREVIEW)
	struct IconRailGeometry
	{
		float railWidth{ 0.0f };
		float panelWidth{ 0.0f };
		float gap{ 0.0f };

		constexpr bool operator==(const IconRailGeometry&) const noexcept = default;
	};

	[[nodiscard]] constexpr IconRailGeometry ResolveIconRailGeometry(
		float a_availableWidth,
		float a_fontSize,
		float a_framePaddingX,
		float a_itemSpacingX) noexcept
	{
		const auto available = (std::max)(a_availableWidth, 0.0f);
		const auto desiredRail =
			(std::max)(a_fontSize, 0.0f) +
			(std::max)(a_framePaddingX, 0.0f) * 2.0f;
		const auto rail = (std::min)(desiredRail, available);
		const auto remaining = available - rail;
		const auto gap = (std::min)(
			(std::max)(a_itemSpacingX, 0.0f),
			remaining);
		return { rail, remaining - gap, gap };
	}
#endif
}
