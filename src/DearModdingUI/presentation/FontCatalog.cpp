#include <DearModdingUI/presentation/FontCatalog.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <system_error>
#include <utility>

namespace DearModdingUI::FontCatalog
{
	namespace
	{
		[[nodiscard]] std::string Lower(std::string_view a_value)
		{
			std::string result{ a_value };
			std::ranges::transform(result, result.begin(), [](unsigned char a_character) {
				return static_cast<char>(std::tolower(a_character));
			});
			return result;
		}

		[[nodiscard]] bool IsFontFile(
			const std::filesystem::path& a_path)
		{
			const auto extension = Lower(a_path.extension().string());
			return extension == ".ttf" ||
				extension == ".otf" ||
				extension == ".ttc";
		}

		[[nodiscard]] bool ConsumeSuffix(
			std::string& a_style,
			std::string_view a_suffix)
		{
			if (!a_style.ends_with(a_suffix))
				return false;
			a_style.resize(a_style.size() - a_suffix.size());
			return true;
		}

		struct ParsedFace
		{
			FontFace face;
			// False when no weight, slant or "regular" suffix named the face.
			bool named{ false };
		};

		// "Jost-SemiBold" and "Font BoldItalic" name the face by stem suffix.
		[[nodiscard]] ParsedFace ParseFace(const std::filesystem::path& a_path)
		{
			auto style = Lower(a_path.stem().string());
			std::erase_if(style, [](char a_character) {
				return a_character == ' ' || a_character == '_' ||
					a_character == '-';
			});
			ParsedFace parsed;
			auto& face = parsed.face;
			face.italic = ConsumeSuffix(style, "italic") ||
				ConsumeSuffix(style, "oblique");
			parsed.named = face.italic;
			constexpr std::pair<std::string_view, FontWeight> weights[]{
				{ "extralight", FontWeight::kExtraLight },
				{ "ultralight", FontWeight::kExtraLight },
				{ "semibold", FontWeight::kSemiBold },
				{ "demibold", FontWeight::kSemiBold },
				{ "extrabold", FontWeight::kExtraBold },
				{ "ultrabold", FontWeight::kExtraBold },
				{ "thin", FontWeight::kThin },
				{ "light", FontWeight::kLight },
				{ "medium", FontWeight::kMedium },
				{ "bold", FontWeight::kBold },
				{ "black", FontWeight::kBlack },
				{ "heavy", FontWeight::kBlack }
			};
			for (const auto& [name, weight] : weights)
			{
				if (style.ends_with(name))
				{
					face.weight = weight;
					parsed.named = true;
					return parsed;
				}
			}
			parsed.named = parsed.named || style.ends_with("regular");
			return parsed;
		}

		[[nodiscard]] bool SameName(
			std::string_view a_left,
			std::string_view a_right)
		{
			return Lower(a_left) == Lower(a_right);
		}
	}

	bool IsBuiltin(std::string_view a_family) noexcept
	{
		return a_family.empty() || SameName(a_family, kBuiltinFamily);
	}

	std::vector<FontFamily> Enumerate(
		const std::filesystem::path& a_root) noexcept
	{
		std::vector<FontFamily> result;
		std::error_code error;
		if (!std::filesystem::is_directory(a_root, error))
			return result;

		for (std::filesystem::directory_iterator iterator{ a_root, error }, end;
			iterator != end && !error;
			iterator.increment(error))
		{
			const auto& entry = *iterator;
			if (!entry.is_directory(error))
				continue;
			const auto familyName = entry.path().filename().string();
			if (SameName(familyName, "Phosphor"))
				continue;

			std::vector<std::filesystem::path> files;
			std::error_code familyError;
			for (std::filesystem::recursive_directory_iterator family{
					 entry.path(),
					 std::filesystem::directory_options::skip_permission_denied,
					 familyError
				 }, familyEnd;
				family != familyEnd && !familyError;
				family.increment(familyError))
			{
				if (family->is_regular_file(familyError) &&
					IsFontFile(family->path()))
					files.push_back(family->path());
			}
			std::vector<std::pair<std::filesystem::path, ParsedFace>> parsed;
			parsed.reserve(files.size());
			for (auto& file : files)
			{
				auto face = ParseFace(file);
				parsed.emplace_back(std::move(file), std::move(face));
			}
			// Named faces claim their slot before files whose suffix said nothing.
			std::ranges::sort(parsed, [](const auto& a_left, const auto& a_right) {
				return std::pair{
					!a_left.second.named,
					Lower(a_left.first.generic_string())
				} <
					std::pair{
						!a_right.second.named,
						Lower(a_right.first.generic_string())
					};
			});

			FontFamily family{ familyName, {} };
			for (auto& [file, entry] : parsed)
			{
				auto& face = entry.face;
				if (std::ranges::any_of(family.faces, [&](const auto& a_known) {
						return a_known.weight == face.weight &&
							a_known.italic == face.italic;
					}))
					continue;
				face.file = file.lexically_relative(a_root).generic_string();
				family.faces.push_back(std::move(face));
			}
			if (!family.faces.empty())
				result.push_back(std::move(family));
		}

		std::ranges::sort(result, [](const auto& a_left, const auto& a_right) {
			return Lower(a_left.name) < Lower(a_right.name);
		});
		return result;
	}

	const FontFamily* Find(
		std::string_view a_requested,
		const std::vector<FontFamily>& a_families) noexcept
	{
		const auto requested = std::ranges::find_if(
			a_families,
			[&](const auto& a_family) {
				return SameName(a_family.name, a_requested);
			});
		return requested == a_families.end() ?
			nullptr :
			std::addressof(*requested);
	}

	const FontFamily* Resolve(
		std::string_view a_requested,
		const std::vector<FontFamily>& a_families,
		std::string_view a_fallback) noexcept
	{
		if (const auto* requested = Find(a_requested, a_families))
			return requested;
		if (const auto* fallback = Find(a_fallback, a_families))
			return fallback;
		return a_families.empty() ? nullptr : std::addressof(a_families.front());
	}

	ResolvedFace ResolveFace(
		const FontFamily& a_family,
		FontWeight a_weight,
		bool a_italic) noexcept
	{
		const auto exact = std::ranges::find_if(a_family.faces, [&](const auto& a_face) {
			return a_face.weight == a_weight && a_face.italic == a_italic;
		});
		if (exact != a_family.faces.end())
			return { std::addressof(*exact), true };

		const auto target = static_cast<int>(a_weight);
		const auto nearest = [&](bool a_slant) -> const FontFace* {
			const FontFace* best = nullptr;
			for (const auto& face : a_family.faces)
			{
				if (face.italic != a_slant)
					continue;
				if (!best)
				{
					best = std::addressof(face);
					continue;
				}
				const auto distance = std::abs(static_cast<int>(face.weight) - target);
				const auto bestDistance = std::abs(static_cast<int>(best->weight) - target);
				const auto heavier = face.weight > best->weight;
				if (distance < bestDistance ||
					(distance == bestDistance && heavier == (target >= 500)))
					best = std::addressof(face);
			}
			return best;
		};
		if (const auto* face = nearest(a_italic))
			return { face, false };
		return { nearest(!a_italic), false };
	}
}
