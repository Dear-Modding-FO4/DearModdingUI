#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace DearModdingUI::Theme
{
	inline constexpr float kDefaultScreenHeight{ 1080.0f };
	inline constexpr float kDefaultFontRatio{ 7.0f / 360.0f };
	inline constexpr float kBaselineFontSize{ kDefaultScreenHeight * kDefaultFontRatio };
	inline constexpr float kSearchBaselineFontSize{ 1440.0f * kDefaultFontRatio };
	inline constexpr float kMinFontSize{ 16.0f };
	inline constexpr float kMaxFontSize{ 108.0f };
	inline constexpr float kDefaultGlobalScale{ 0.0f };
	inline constexpr float kMinUserScale{ 0.75f };
	inline constexpr float kMaxUserScale{ 2.0f };
	inline constexpr float kDefaultUserScale{ 1.0f };

	enum class FontRole : uint32_t
	{
		kBody,
		kTitle,
		kHeading,
		kSubheading,
		kSubtext,
		kMonospace,
		kCount
	};

	inline constexpr std::size_t kFontRoleCount{ static_cast<std::size_t>(FontRole::kCount) };

	inline constexpr std::array<std::string_view, kFontRoleCount> kFontRoleKeys{
		"body", "title", "heading", "subheading", "subtext", "monospace"
	};

	[[nodiscard]] inline float ResolveFontSize(
		uint32_t a_backBufferHeight,
		float a_baseFontRatio = kDefaultFontRatio) noexcept
	{
		const auto height = a_backBufferHeight ?
			static_cast<float>(a_backBufferHeight) :
			kDefaultScreenHeight;
		return std::clamp(height * a_baseFontRatio, kMinFontSize, kMaxFontSize);
	}

	[[nodiscard]] inline float ResolveRoleFontSize(
		float a_sizeScale,
		float a_baseFontRatio,
		uint32_t a_backBufferHeight,
		float a_userScale = kDefaultUserScale) noexcept
	{
		const auto userScale = std::isfinite(a_userScale) ?
			std::clamp(a_userScale, kMinUserScale, kMaxUserScale) :
			kDefaultUserScale;
		return std::round(std::clamp(
			ResolveFontSize(a_backBufferHeight, a_baseFontRatio) *
				userScale * a_sizeScale,
			kMinFontSize * kMinUserScale,
			kMaxFontSize * kMaxUserScale));
	}

	[[nodiscard]] inline float ResolveStyleScale(
		float a_bodyFontSize,
		float a_globalScale = kDefaultGlobalScale) noexcept
	{
		const auto body = std::isfinite(a_bodyFontSize) && a_bodyFontSize > 0.0f ?
			a_bodyFontSize :
			kBaselineFontSize;
		const auto global = std::isfinite(a_globalScale) ?
			a_globalScale :
			kDefaultGlobalScale;
		return body / kBaselineFontSize * std::exp2(global);
	}
}
