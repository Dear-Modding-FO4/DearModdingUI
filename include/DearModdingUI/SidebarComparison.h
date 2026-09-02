#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace DearModdingUI
{
	enum class SidebarLayoutKind : uint32_t
	{
		Tree,
		TwoPane,
		DrillDown,
		IconRail
	};

	struct SidebarLayoutName
	{
		std::string_view name;
		SidebarLayoutKind kind;
	};

	inline constexpr std::array SIDEBAR_LAYOUT_NAMES{
		SidebarLayoutName{ "tree", SidebarLayoutKind::Tree },
		SidebarLayoutName{ "twopane", SidebarLayoutKind::TwoPane },
		SidebarLayoutName{ "drilldown", SidebarLayoutKind::DrillDown },
		SidebarLayoutName{ "iconrail", SidebarLayoutKind::IconRail }
	};
	inline constexpr auto DEFAULT_SIDEBAR_LAYOUT = SidebarLayoutKind::Tree;

	struct UserSidebarLayout
	{
		SidebarLayoutKind kind;
		std::string_view label;
		std::string_view description;
	};

	// Icon rail depends on third-party client icons; missing icons make the layout unusable.
	inline constexpr std::array USER_SIDEBAR_LAYOUTS{
		UserSidebarLayout{ SidebarLayoutKind::Tree, "Tree", "Browse every mod and page at once." },
		UserSidebarLayout{ SidebarLayoutKind::TwoPane, "Two-pane", "Keep a fixed mod list with the selected mod's pages." },
		UserSidebarLayout{ SidebarLayoutKind::DrillDown, "Drill-down", "Show one level at a time; well suited to many mods." }
	};

	[[nodiscard]] constexpr std::string_view SidebarLayoutKindName(
		SidebarLayoutKind a_kind) noexcept
	{
		for (const auto& layout : SIDEBAR_LAYOUT_NAMES)
		{
			if (layout.kind == a_kind)
				return layout.name;
		}
		return "unknown";
	}

	[[nodiscard]] constexpr std::optional<SidebarLayoutKind> ParseSidebarLayout(
		std::string_view a_name) noexcept
	{
		for (const auto& layout : SIDEBAR_LAYOUT_NAMES)
		{
			if (layout.name == a_name)
				return layout.kind;
		}
		return std::nullopt;
	}

	[[nodiscard]] constexpr const UserSidebarLayout* FindUserSidebarLayout(
		SidebarLayoutKind a_kind) noexcept
	{
		for (const auto& layout : USER_SIDEBAR_LAYOUTS)
		{
			if (layout.kind == a_kind)
				return &layout;
		}
		return nullptr;
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
		const auto kind = ParseUserSidebarLayout(a_name);
		return kind.value_or(DEFAULT_SIDEBAR_LAYOUT);
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
		float a_minimumPagesHeight) noexcept
	{
		const auto available = (std::max)(a_availableHeight, 0.0f);
		const auto rowStride = (std::max)(a_modRowStride, 0.0f);
		const auto minimumPages = (std::min)(
			(std::max)(a_minimumPagesHeight, 0.0f),
			available);
		const auto desiredMods =
			rowStride * static_cast<float>(a_modCount);
		const auto mods = (std::min)(
			desiredMods,
			available - minimumPages);
		return { mods, available - mods };
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

	void ConfigurePreviewSidebarComparison(
		std::optional<SidebarLayoutKind> a_layoutOverride,
		bool a_overrideExpandedClients,
		std::span<const std::string> a_expandedClients);
}
