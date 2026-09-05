#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace DearModdingUI::MCM
{
	enum class TextAlignment : uint8_t
	{
		kLeft,
		kCenter,
		kRight
	};

	struct TextPresentation
	{
		std::string text;
		TextAlignment alignment{ TextAlignment::kLeft };
	};

	[[nodiscard]] TextPresentation ResolveTextPresentation(
		std::string_view a_text,
		bool a_html,
		std::optional<std::string_view> a_alignment = std::nullopt);
}
