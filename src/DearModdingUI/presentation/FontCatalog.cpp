#include <DearModdingUI/presentation/FontCatalog.h>
#include <Support/Utf8.h>

#include <REX/REX.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <system_error>
#include <tuple>
#include <utility>

namespace DearModdingUI::FontCatalog
{
	using namespace std::literals;

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

		[[nodiscard]] bool IsFontFile(std::string_view a_name)
		{
			const auto dot = a_name.rfind('.');
			if (dot == std::string_view::npos)
				return false;
			const auto extension = Lower(a_name.substr(dot));
			return extension == ".ttf" ||
				extension == ".otf" ||
				extension == ".ttc";
		}

		[[nodiscard]] int CandidatePriority(std::string_view a_name)
		{
			const auto stem = Lower(a_name.substr(0, a_name.rfind('.')));
			if (stem.contains("regular"))
				return 0;
			constexpr std::string_view variants[]{
				"bold", "italic", "light", "medium", "semibold",
				"thin", "black", "icon", "fill"
			};
			for (const auto variant : variants)
			{
				if (stem.contains(variant))
					return 2;
			}
			return 1;
		}

		struct Candidate
		{
			int priority;
			std::string name;
			std::string relative;
		};

		[[nodiscard]] std::optional<Candidate> MakeCandidate(
			const std::filesystem::path& a_path,
			const std::filesystem::path& a_root)
		{
			auto name = Support::PathToUtf8(a_path.filename());
			if (!name || !IsFontFile(*name))
				return std::nullopt;
			auto relative = Support::PathToUtf8(a_path.lexically_relative(a_root));
			if (!relative)
			{
				REX::WARN("DearModdingUI: skipped a font file with an unreadable name"sv);
				return std::nullopt;
			}
			std::ranges::replace(*relative, '\\', '/');
			return Candidate{ CandidatePriority(*name), Lower(*name), std::move(*relative) };
		}

		[[nodiscard]] bool SameName(
			std::string_view a_left,
			std::string_view a_right)
		{
			return Lower(a_left) == Lower(a_right);
		}
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
			const auto familyName = Support::PathToUtf8(entry.path().filename());
			if (!familyName)
			{
				REX::WARN("DearModdingUI: skipped a font folder with an unreadable name"sv);
				continue;
			}
			if (SameName(*familyName, "Phosphor"))
				continue;

			std::vector<Candidate> candidates;
			std::error_code familyError;
			for (std::filesystem::recursive_directory_iterator family{
					 entry.path(),
					 std::filesystem::directory_options::skip_permission_denied,
					 familyError
				 }, familyEnd;
				family != familyEnd && !familyError;
				family.increment(familyError))
			{
				if (!family->is_regular_file(familyError))
					continue;
				if (auto candidate = MakeCandidate(family->path(), a_root))
					candidates.push_back(std::move(*candidate));
			}
			if (candidates.empty())
				continue;

			std::ranges::sort(candidates, [](const auto& a_left, const auto& a_right) {
				return std::tie(a_left.priority, a_left.name) <
					std::tie(a_right.priority, a_right.name);
			});
			result.push_back({ *familyName, std::move(candidates.front().relative) });
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
}
