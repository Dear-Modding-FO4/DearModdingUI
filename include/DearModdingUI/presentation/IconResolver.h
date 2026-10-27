#pragma once

#include <DearModdingUI/IconGlyphs.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace dmui
{
	struct IconPhraseMapping
	{
		std::string_view phrase;
		char32_t glyph;
	};
}

#include "PhosphorGlyphs.generated.h"

namespace dmui
{
	enum class IconSelectionStatus : uint32_t
	{
		kNoMatch,
		kSelected,
		kInvalidRawGlyph
	};

	struct IconSelection
	{
		IconSelectionStatus status{ IconSelectionStatus::kNoMatch };
		char32_t glyph{};

		[[nodiscard]] constexpr bool HasSelection() const noexcept
		{
			return status == IconSelectionStatus::kSelected;
		}

		[[nodiscard]] constexpr char32_t GlyphOr(
			char32_t a_fallback) const noexcept
		{
			return status == IconSelectionStatus::kSelected ||
					status == IconSelectionStatus::kInvalidRawGlyph ?
				glyph :
				a_fallback;
		}
	};

	struct IconResolutionRequest
	{
		std::optional<char32_t> explicitGlyph;
		std::string_view explicitName;
		std::span<const std::string_view> primaryMetadata;
		std::span<const std::string_view> secondaryMetadata;
	};

	[[nodiscard]] inline std::string NormalizeIconName(std::string_view a_name)
	{
		std::string normalized;
		normalized.reserve(a_name.size());
		bool separatorPending = false;
		bool previousLowerOrDigit = false;
		bool previousUpper = false;
		for (size_t index = 0; index < a_name.size(); ++index)
		{
			const auto character = static_cast<unsigned char>(a_name[index]);
			const auto next = index + 1 < a_name.size() ?
				static_cast<unsigned char>(a_name[index + 1]) :
				static_cast<unsigned char>(0);
			const auto nextLower = next >= 'a' && next <= 'z';
			if ((character >= 'a' && character <= 'z') ||
				(character >= '0' && character <= '9'))
			{
				if (separatorPending && !normalized.empty())
					normalized.push_back('-');
				normalized.push_back(static_cast<char>(character));
				separatorPending = false;
				previousLowerOrDigit = true;
				previousUpper = false;
			}
			else if (character >= 'A' && character <= 'Z')
			{
				if ((separatorPending || previousLowerOrDigit ||
						(previousUpper && nextLower)) &&
					!normalized.empty() && normalized.back() != '-')
					normalized.push_back('-');
				normalized.push_back(
					static_cast<char>(character - 'A' + 'a'));
				separatorPending = false;
				previousLowerOrDigit = false;
				previousUpper = true;
			}
			else
			{
				separatorPending = !normalized.empty();
				previousLowerOrDigit = false;
				previousUpper = false;
			}
		}
		return normalized;
	}

	namespace IconResolverDetail
	{
		struct MatchRank
		{
			uint32_t tier{};
			size_t wordCount{};
			uint32_t authority{};

			[[nodiscard]] friend constexpr bool operator<(
				const MatchRank& a_left,
				const MatchRank& a_right) noexcept
			{
				if (a_left.tier != a_right.tier)
					return a_left.tier < a_right.tier;
				if (a_left.wordCount != a_right.wordCount)
					return a_left.wordCount < a_right.wordCount;
				return a_left.authority < a_right.authority;
			}
		};

		struct BestMatch
		{
			MatchRank rank;
			char32_t glyph{};

			void Add(char32_t a_glyph, MatchRank a_rank)
			{
				if (!a_glyph)
					return;
				if (!glyph || rank < a_rank)
				{
					rank = a_rank;
					glyph = a_glyph;
				}
				else if (!(a_rank < rank) && a_glyph < glyph)
					glyph = a_glyph;
			}
		};

		template <size_t Size>
		[[nodiscard]] inline std::pair<size_t, size_t> EqualRange(
			const std::array<IconPhraseMapping, Size>& a_mappings,
			std::string_view a_phrase) noexcept
		{
			const auto first = std::ranges::lower_bound(
				a_mappings,
				a_phrase,
				{},
				&IconPhraseMapping::phrase);
			const auto last = std::ranges::upper_bound(
				first,
				a_mappings.end(),
				a_phrase,
				{},
				&IconPhraseMapping::phrase);
			return {
				static_cast<size_t>(first - a_mappings.begin()),
				static_cast<size_t>(last - a_mappings.begin())
			};
		}

		template <size_t Size>
		void AddMatches(
			BestMatch& a_match,
			const std::array<IconPhraseMapping, Size>& a_mappings,
			std::string_view a_phrase,
			MatchRank a_rank)
		{
			const auto [first, last] = EqualRange(a_mappings, a_phrase);
			for (auto index = first; index < last; ++index)
				a_match.Add(a_mappings[index].glyph, a_rank);
		}

		[[nodiscard]] inline size_t WordCount(
			std::string_view a_phrase) noexcept
		{
			return a_phrase.empty() ?
				0 :
				static_cast<size_t>(
					std::ranges::count(a_phrase, '-')) +
					1;
		}

		[[nodiscard]] inline bool HasMetadataTerm(
			std::string_view a_term) noexcept
		{
			for (const auto& mappings : {
					std::span<const IconPhraseMapping>{ kPhosphorIconGlyphs },
					std::span<const IconPhraseMapping>{ kPhosphorIconAliases },
					std::span<const IconPhraseMapping>{
						kPhosphorIconDomainTerms },
					std::span<const IconPhraseMapping>{ kPhosphorIconTags } })
			{
				const auto first = std::ranges::lower_bound(
					mappings,
					a_term,
					{},
					&IconPhraseMapping::phrase);
				if (first != mappings.end() && first->phrase == a_term)
					return true;
			}
			return false;
		}

		[[nodiscard]] inline std::string SingularMetadataWord(
			std::string_view a_word)
		{
			if (a_word.size() <= 3 ||
				a_word.ends_with("ss") ||
				a_word.ends_with("us") ||
				a_word.ends_with("is"))
				return {};

			std::string candidate;
			candidate.reserve(a_word.size());
			const auto tryCandidate =
				[&](size_t a_removed, char a_replacement = '\0') {
				candidate.assign(
					a_word.substr(0, a_word.size() - a_removed));
				if (a_replacement)
					candidate.push_back(a_replacement);
				return HasMetadataTerm(candidate);
			};
			if (a_word.ends_with("ies") && tryCandidate(3, 'y'))
				return candidate;
			if ((a_word.ends_with("sses") ||
				a_word.ends_with("zzes") ||
				a_word.ends_with("ches") ||
				a_word.ends_with("shes") ||
				a_word.ends_with("xes")) &&
				tryCandidate(2))
				return candidate;
			if (a_word.ends_with('s') && tryCandidate(1))
				return candidate;
			if (a_word.ends_with("es") && tryCandidate(2))
				return candidate;
			return {};
		}

		[[nodiscard]] inline std::string SingularizeMetadataWords(
			std::string_view a_normalized)
		{
			std::string singular;
			singular.reserve(a_normalized.size());
			bool changed = false;
			for (size_t begin = 0; begin < a_normalized.size();)
			{
				const auto separator = a_normalized.find('-', begin);
				const auto end = separator == std::string::npos ?
					a_normalized.size() :
					separator;
				const auto word = a_normalized.substr(begin, end - begin);
				auto wordForm = SingularMetadataWord(word);
				if (wordForm.empty())
					singular.append(word);
				else
				{
					singular.append(wordForm);
					changed = true;
				}
				if (separator == std::string::npos)
					break;
				singular.push_back('-');
				begin = separator + 1;
			}
			return changed ? singular : std::string{};
		}

		inline void AddNormalizedMetadataMatches(
			BestMatch& a_match,
			std::string_view a_normalized)
		{
			if (a_normalized.empty())
				return;

			const auto fullWordCount = WordCount(a_normalized);
			AddMatches(
				a_match,
				kPhosphorIconGlyphs,
				a_normalized,
				{ 3, fullWordCount, 2 });
			AddMatches(
				a_match,
				kPhosphorIconAliases,
				a_normalized,
				{ 3, fullWordCount, 2 });
			AddMatches(
				a_match,
				kPhosphorIconDomainTerms,
				a_normalized,
				{ 3, fullWordCount, 1 });

			for (size_t begin = 0; begin < a_normalized.size();)
			{
				size_t end = begin;
				size_t words{};
				while (end < a_normalized.size())
				{
					const auto separator = a_normalized.find('-', end);
					end = separator == std::string::npos ?
						a_normalized.size() :
						separator;
					++words;
					const auto phrase =
						a_normalized.substr(begin, end - begin);
					if (phrase.size() > 1 || a_normalized.size() == 1)
					{
						AddMatches(
							a_match,
							kPhosphorIconGlyphs,
							phrase,
							{ 2, words, 2 });
						AddMatches(
							a_match,
							kPhosphorIconAliases,
							phrase,
							{ 2, words, 2 });
						AddMatches(
							a_match,
							kPhosphorIconDomainTerms,
							phrase,
							{ 2, words, 1 });
						AddMatches(
							a_match,
							kPhosphorIconTags,
							phrase,
							{ 1, words, 0 });
					}
					if (separator == std::string::npos)
						break;
					end = separator + 1;
				}
				const auto next = a_normalized.find('-', begin);
				if (next == std::string::npos)
					break;
				begin = next + 1;
			}
		}

		inline void AddMetadataMatches(
			BestMatch& a_match,
			std::string_view a_metadata)
		{
			AddNormalizedMetadataMatches(
				a_match,
				NormalizeIconName(a_metadata));
		}

		[[nodiscard]] inline BestMatch EvaluateGroup(
			std::span<const std::string_view> a_metadata)
		{
			BestMatch match;
			for (const auto value : a_metadata)
				AddMetadataMatches(match, value);
			if (match.glyph)
				return match;

			for (const auto value : a_metadata)
			{
				const auto normalized = NormalizeIconName(value);
				if (const auto singular =
						SingularizeMetadataWords(normalized);
					!singular.empty())
					AddNormalizedMetadataMatches(match, singular);
			}
			return match;
		}

		[[nodiscard]] inline char32_t FindExactAuthoritative(
			std::string_view a_name)
		{
			const auto normalized = NormalizeIconName(a_name);
			if (normalized.empty())
				return {};
			for (const auto& mappings : {
					std::span<const IconPhraseMapping>{ kPhosphorIconGlyphs },
					std::span<const IconPhraseMapping>{ kPhosphorIconAliases },
					std::span<const IconPhraseMapping>{
						kPhosphorIconDomainTerms } })
			{
				const auto first = std::ranges::lower_bound(
					mappings,
					normalized,
					{},
					&IconPhraseMapping::phrase);
				if (first != mappings.end() && first->phrase == normalized)
					return first->glyph;
			}
			return {};
		}
	}

	class IconResolver
	{
	public:
		[[nodiscard]] static IconSelection Resolve(
			const IconResolutionRequest& a_request)
		{
			if (a_request.explicitGlyph)
			{
				return {
					IsValidUnicodeScalar(*a_request.explicitGlyph) ?
						IconSelectionStatus::kSelected :
						IconSelectionStatus::kInvalidRawGlyph,
					*a_request.explicitGlyph
				};
			}
			if (const auto glyph =
					IconResolverDetail::FindExactAuthoritative(
						a_request.explicitName))
				return { IconSelectionStatus::kSelected, glyph };

			auto primary =
				IconResolverDetail::EvaluateGroup(a_request.primaryMetadata);
			if (primary.glyph)
				return { IconSelectionStatus::kSelected, primary.glyph };

			auto secondary =
				IconResolverDetail::EvaluateGroup(a_request.secondaryMetadata);
			if (secondary.glyph)
				return {
					IconSelectionStatus::kSelected,
					secondary.glyph
				};
			return { IconSelectionStatus::kNoMatch, {} };
		}
	};

	[[nodiscard]] inline char32_t FindPhosphorSlugGlyphOrZero(
		std::string_view a_slug) noexcept
	{
		const auto [first, last] =
			IconResolverDetail::EqualRange(kPhosphorIconGlyphs, a_slug);
		return first != last ? kPhosphorIconGlyphs[first].glyph : char32_t{};
	}

	[[nodiscard]] inline char32_t FindPhosphorIconGlyphOrZero(
		std::string_view a_name)
	{
		return FindPhosphorSlugGlyphOrZero(NormalizeIconName(a_name));
	}

	[[nodiscard]] inline IconSelection ResolveIconSelection(
		std::string_view a_explicitName,
		std::string_view a_primaryMetadata = {},
		std::string_view a_secondaryMetadata = {})
	{
		const std::array primary{ a_primaryMetadata };
		const std::array secondary{ a_secondaryMetadata };
		return IconResolver::Resolve({
			.explicitName = a_explicitName,
			.primaryMetadata = a_primaryMetadata.empty() ?
				std::span<const std::string_view>{} :
				std::span<const std::string_view>{ primary },
			.secondaryMetadata = a_secondaryMetadata.empty() ?
				std::span<const std::string_view>{} :
				std::span<const std::string_view>{ secondary }
		});
	}

	[[nodiscard]] inline char32_t ResolveNamedIconGlyphOrZero(
		std::string_view a_name)
	{
		return IconResolverDetail::FindExactAuthoritative(a_name);
	}
}
