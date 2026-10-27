#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace DearModdingUI::FontCatalog
{
	enum class FontWeight : uint16_t
	{
		kThin = 100,
		kExtraLight = 200,
		kLight = 300,
		kRegular = 400,
		kMedium = 500,
		kSemiBold = 600,
		kBold = 700,
		kExtraBold = 800,
		kBlack = 900
	};

	// Names the built-in vector font instead of a bundled family.
	inline constexpr std::string_view kBuiltinFamily{ "ImGui" };

	struct FontFace
	{
		FontWeight weight{ FontWeight::kRegular };
		bool italic{ false };
		// Relative to the catalog root.
		std::string file;

		[[nodiscard]] bool operator==(const FontFace&) const noexcept = default;
	};

	struct FontFamily
	{
		std::string name;
		std::vector<FontFace> faces;

		[[nodiscard]] bool operator==(const FontFamily&) const noexcept = default;
	};

	struct ResolvedFace
	{
		const FontFace* face{ nullptr };
		bool exact{ false };
	};

	[[nodiscard]] bool IsBuiltin(std::string_view a_family) noexcept;
	[[nodiscard]] std::vector<FontFamily> Enumerate(
		const std::filesystem::path& a_root) noexcept;
	[[nodiscard]] const FontFamily* Find(
		std::string_view a_requested,
		const std::vector<FontFamily>& a_families) noexcept;
	[[nodiscard]] const FontFamily* Resolve(
		std::string_view a_requested,
		const std::vector<FontFamily>& a_families,
		std::string_view a_fallback) noexcept;
	// Exact face, else the nearest weight of the same slant, then the other.
	[[nodiscard]] ResolvedFace ResolveFace(
		const FontFamily& a_family,
		FontWeight a_weight,
		bool a_italic) noexcept;
}
