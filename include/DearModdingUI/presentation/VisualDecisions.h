#pragma once

#include <DearModdingUI/Presentation/Layout.h>
#include <DearModdingUI/presentation/RowGeometry.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace DearModdingUI
{
	struct CursorPresentation
	{
		bool captureInput{ false };
		bool hideOperatingSystemCursor{ false };
		bool drawSoftwareCursor{ false };
		bool drawCustomCursor{ false };
	};

	[[nodiscard]] constexpr CursorPresentation DecideCursorPresentation(
		bool a_modalVisible) noexcept
	{
		if (!a_modalVisible)
			return {};
		return {
			true,
			true,
			true,
			false
		};
	}

	enum class CursorOwnershipTransition : uint32_t
	{
		kNone,
		kAcquire,
		kRelease
	};

	[[nodiscard]] constexpr CursorOwnershipTransition DecideCursorTransition(
		bool a_owned,
		bool a_modalVisible) noexcept
	{
		if (a_owned == a_modalVisible)
			return CursorOwnershipTransition::kNone;
		return a_modalVisible ?
			CursorOwnershipTransition::kAcquire :
			CursorOwnershipTransition::kRelease;
	}

	[[nodiscard]] constexpr float FooterRowAdjustmentY(
		float a_verticalSpacing,
		float a_windowPadding) noexcept
	{
		const auto verticalSpacing = a_verticalSpacing > 0.0f ?
			a_verticalSpacing :
			0.0f;
		const auto windowPadding = a_windowPadding > 0.0f ?
			a_windowPadding :
			0.0f;
		return windowPadding - verticalSpacing;
	}

	[[nodiscard]] inline std::string BuildHostBreadcrumb(
		std::string_view a_hostName,
		std::string_view a_clientName)
	{
		std::string breadcrumb{ a_hostName };
		if (!a_clientName.empty())
		{
			breadcrumb.append(" > ");
			breadcrumb.append(a_clientName);
		}
		return breadcrumb;
	}

	[[nodiscard]] constexpr bool ShouldDrawHeaderClose(
		bool a_docked,
		bool a_noTitleBar) noexcept
	{
		return !a_docked && a_noTitleBar;
	}

	[[nodiscard]] constexpr float ReservedFooterHeight(
		float a_rowHeight,
		float a_verticalSpacing,
		float a_windowPadding,
		float a_separatorThickness) noexcept
	{
		const auto rowHeight = a_rowHeight > 0.0f ? a_rowHeight : 0.0f;
		const auto verticalSpacing = a_verticalSpacing > 0.0f ?
			a_verticalSpacing :
			0.0f;
		const auto windowPadding = a_windowPadding > 0.0f ?
			a_windowPadding :
			0.0f;
		const auto separator = a_separatorThickness > 0.0f ?
			a_separatorThickness :
			0.0f;
		return rowHeight +
			verticalSpacing * 2.0f +
			windowPadding +
			separator;
	}

	struct FooterControlsLayout
	{
		float runMaxX{ 0.0f };
		float dismissMinX{ 0.0f };
		float dismissMaxX{ 0.0f };
		float settingsMinX{ 0.0f };
		float settingsMaxX{ 0.0f };

		constexpr bool operator==(const FooterControlsLayout&) const noexcept = default;
	};

	[[nodiscard]] constexpr FooterControlsLayout ResolveFooterControlsLayout(
		float a_contentMinX,
		float a_contentMaxX,
		float a_settingsWidth,
		float a_dismissWidth,
		float a_horizontalSpacing) noexcept
	{
		const auto settings = ResolveTrailingControlLayout(
			a_contentMinX,
			a_contentMaxX,
			a_settingsWidth,
			a_horizontalSpacing);
		const auto dismiss = ResolveTrailingControlLayout(
			a_contentMinX,
			settings.adjacentMaxX,
			a_dismissWidth,
			a_dismissWidth > 0.0f ? a_horizontalSpacing : 0.0f);
		return {
			dismiss.adjacentMaxX,
			dismiss.controlMinX,
			dismiss.controlMaxX,
			settings.controlMinX,
			settings.controlMaxX
		};
	}

	struct HostSettingsTitleRowLayout
	{
		float titleMinX{ 0.0f };
		float titleMaxX{ 0.0f };
		float actionsMinX{ 0.0f };
		float actionsMaxX{ 0.0f };
		float closeMinX{ 0.0f };
		float closeMaxX{ 0.0f };
		float reservedWidth{ 0.0f };
	};

	[[nodiscard]] constexpr HostSettingsTitleRowLayout
		ResolveHostSettingsTitleRowLayout(
			float a_contentMinX,
			float a_contentMaxX,
			float a_actionButtonWidthSum,
			size_t a_actionCount,
			float a_closeButtonWidth,
			float a_spacing) noexcept
	{
		const auto close = ResolveTrailingControlLayout(
			a_contentMinX,
			a_contentMaxX,
			a_closeButtonWidth,
			a_spacing);
		const auto actions = dmui::ResolvePageActionRowLayout(
			a_contentMinX,
			close.adjacentMaxX,
			a_actionButtonWidthSum,
			a_actionCount,
			a_spacing);
		const auto controlsMinX = a_actionCount > 0 ?
			actions.actionsMinX :
			close.controlMinX;
		return {
			a_contentMinX,
			actions.titleMaxX,
			actions.actionsMinX,
			actions.actionsMaxX,
			close.controlMinX,
			close.controlMaxX,
			close.controlMaxX - controlsMinX
		};
	}
}
