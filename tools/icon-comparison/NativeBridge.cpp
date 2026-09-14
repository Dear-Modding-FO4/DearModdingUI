#include <Harness.h>

#include <DearModdingUI/IconGlyphs.h>
#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	using Json = nlohmann::json;
	using DearModdingUI::IconPhraseMapping;
	using DearModdingUI::IconResolutionRequest;
	using DearModdingUI::IconResolver;
	using DearModdingUI::IconSelection;
	using DearModdingUI::IconSelectionStatus;
	using DearModdingUI::kPhosphorIconAliases;
	using DearModdingUI::kPhosphorIconDomainTerms;
	using DearModdingUI::kPhosphorIconGlyphs;
	using DearModdingUI::kPhosphorIconTags;

	constexpr std::size_t kMaximumLineBytes{ 4 * 1024 * 1024 };
	constexpr std::size_t kMaximumRequests{ 4096 };
	constexpr std::size_t kMaximumHeadingBytes{ 256 };
	constexpr std::size_t kMaximumContextBytes{ 256 };
	constexpr std::size_t kMaximumExplicitNameBytes{ 128 };

	class ProtocolError final : public std::runtime_error
	{
	public:
		ProtocolError(std::string a_code, std::string a_message) :
			std::runtime_error(std::move(a_message)),
			_code(std::move(a_code))
		{}

		[[nodiscard]] const std::string& Code() const noexcept
		{
			return _code;
		}

	private:
		std::string _code;
	};

	struct BridgeRequest
	{
		std::string heading;
		std::string context;
		std::string explicitName;
		std::optional<char32_t> explicitGlyph;
	};

	struct CatalogEntry
	{
		char32_t glyph{};
		std::string_view name;
		std::vector<std::string_view> aliases;
		std::vector<std::string_view> domains;
		std::vector<std::string_view> tags;
	};

	[[nodiscard]] bool HasDisallowedControl(std::string_view a_value) noexcept
	{
		for (const auto character : a_value)
		{
			const auto byte = static_cast<unsigned char>(character);
			if ((byte < 32 && byte != '\t') || byte == 127)
				return true;
		}
		return false;
	}

	[[nodiscard]] std::string ReadBoundedString(
		const Json& a_request,
		std::string_view a_name,
		std::size_t a_maximumBytes)
	{
		const auto key = std::string{ a_name };
		const auto found = a_request.find(key);
		if (found == a_request.end())
			return {};
		if (!found->is_string())
			throw ProtocolError(
				"invalid_request",
				key + " must be a string");

		auto value = found->get<std::string>();
		if (value.size() > a_maximumBytes)
			throw ProtocolError(
				"invalid_request",
				key + " exceeds its UTF-8 byte limit");
		if (HasDisallowedControl(value))
			throw ProtocolError(
				"invalid_request",
				key + " contains a disallowed control character");
		return value;
	}

	[[nodiscard]] BridgeRequest ParseRequest(const Json& a_request)
	{
		if (!a_request.is_object())
			throw ProtocolError(
				"invalid_request",
				"each request must be an object");

		BridgeRequest request{
			.heading = ReadBoundedString(
				a_request, "heading", kMaximumHeadingBytes),
			.context = ReadBoundedString(
				a_request, "context", kMaximumContextBytes),
			.explicitName = ReadBoundedString(
				a_request, "explicit_name", kMaximumExplicitNameBytes)
		};

		const auto explicitGlyph = a_request.find("explicit_glyph");
		if (explicitGlyph == a_request.end() || explicitGlyph->is_null())
			return request;
		if (!explicitGlyph->is_number_unsigned())
			throw ProtocolError(
				"invalid_request",
				"explicit_glyph must be null or an unsigned 32-bit integer");
		const auto value = explicitGlyph->get<std::uint64_t>();
		if (value > (std::numeric_limits<std::uint32_t>::max)())
			throw ProtocolError(
				"invalid_request",
				"explicit_glyph must be null or an unsigned 32-bit integer");
		request.explicitGlyph = static_cast<char32_t>(value);
		return request;
	}

	[[nodiscard]] std::vector<BridgeRequest> ParseRequests(const Json& a_message)
	{
		const auto requests = a_message.find("requests");
		if (requests == a_message.end() || !requests->is_array())
			throw ProtocolError(
				"invalid_request",
				"resolve requires a requests array");
		if (requests->size() > kMaximumRequests)
			throw ProtocolError(
				"invalid_request",
				"requests exceeds the maximum batch size");

		std::vector<BridgeRequest> parsed;
		parsed.reserve(requests->size());
		for (const auto& request : *requests)
			parsed.push_back(ParseRequest(request));
		return parsed;
	}

	template <std::size_t Size>
	void AppendCatalogTerms(
		std::vector<CatalogEntry>& a_entries,
		const std::map<char32_t, std::size_t>& a_indices,
		const std::array<IconPhraseMapping, Size>& a_mappings,
		std::vector<std::string_view> CatalogEntry::*a_member)
	{
		for (const auto& mapping : a_mappings)
		{
			const auto entry = a_indices.find(mapping.glyph);
			if (entry == a_indices.end())
				throw ProtocolError(
					"catalog_invariant",
					"generated metadata references an unknown glyph");
			(a_entries[entry->second].*a_member).push_back(mapping.phrase);
		}
	}

	[[nodiscard]] std::vector<CatalogEntry> BuildCatalog()
	{
		std::vector<CatalogEntry> entries;
		entries.reserve(kPhosphorIconGlyphs.size());
		std::map<char32_t, std::size_t> indices;
		for (const auto& mapping : kPhosphorIconGlyphs)
		{
			if (!indices.emplace(mapping.glyph, entries.size()).second)
				throw ProtocolError(
					"catalog_invariant",
					"canonical generated glyphs are not unique");
			entries.push_back({
				.glyph = mapping.glyph,
				.name = mapping.phrase
			});
		}
		if (entries.size() != 1512)
			throw ProtocolError(
				"catalog_invariant",
				"canonical generated catalog does not contain 1512 glyphs");

		AppendCatalogTerms(
			entries, indices, kPhosphorIconAliases, &CatalogEntry::aliases);
		AppendCatalogTerms(
			entries, indices, kPhosphorIconDomainTerms, &CatalogEntry::domains);
		AppendCatalogTerms(
			entries, indices, kPhosphorIconTags, &CatalogEntry::tags);
		return entries;
	}

	[[nodiscard]] const std::vector<CatalogEntry>& Catalog()
	{
		static const auto catalog = BuildCatalog();
		return catalog;
	}

	[[nodiscard]] std::string_view CanonicalName(char32_t a_glyph)
	{
		static const auto names = [] {
			std::map<char32_t, std::string_view> result;
			for (const auto& mapping : kPhosphorIconGlyphs)
				result.emplace(mapping.glyph, mapping.phrase);
			return result;
		}();
		const auto found = names.find(a_glyph);
		return found == names.end() ? std::string_view{} : found->second;
	}

	[[nodiscard]] Json StringArray(
		const std::vector<std::string_view>& a_values)
	{
		auto result = Json::array();
		for (const auto value : a_values)
			result.push_back(std::string{ value });
		return result;
	}

	[[nodiscard]] Json CatalogResponse()
	{
		auto icons = Json::array();
		for (const auto& entry : Catalog())
		{
			icons.push_back({
				{ "glyph", static_cast<std::uint32_t>(entry.glyph) },
				{ "name", std::string{ entry.name } },
				{ "aliases", StringArray(entry.aliases) },
				{ "domains", StringArray(entry.domains) },
				{ "tags", StringArray(entry.tags) }
			});
		}
		return { { "icons", std::move(icons) } };
	}

	[[nodiscard]] std::string SelectionStatusName(
		IconSelectionStatus a_status)
	{
		switch (a_status)
		{
		case IconSelectionStatus::kSelected:
			return "selected";
		case IconSelectionStatus::kInvalidRawGlyph:
			return "invalid_raw_glyph";
		case IconSelectionStatus::kNoMatch:
			return "no_match";
		}
		throw ProtocolError(
			"resolver_invariant",
			"resolver returned an unknown selection status");
	}

	[[nodiscard]] std::string MetadataReason(
		std::string_view a_group,
		std::string_view a_value,
		char32_t a_selectedGlyph)
	{
		DearModdingUI::IconResolverDetail::BestMatch original;
		DearModdingUI::IconResolverDetail::AddMetadataMatches(
			original, a_value);
		if (original.glyph == a_selectedGlyph)
			return std::string{ a_group } + "-original";
		return std::string{ a_group } + "-word-form";
	}

	[[nodiscard]] std::string SelectionReason(
		const BridgeRequest& a_request,
		const IconSelection& a_selection)
	{
		if (a_request.explicitGlyph)
			return a_selection.status == IconSelectionStatus::kInvalidRawGlyph ?
				"explicit-glyph-invalid" :
				"explicit-glyph";
		if (DearModdingUI::IconResolverDetail::FindExactAuthoritative(
				a_request.explicitName))
			return "explicit-name";

		const std::array primary{
			std::string_view{ a_request.heading }
		};
		const auto primaryMatch =
			DearModdingUI::IconResolverDetail::EvaluateGroup(primary);
		if (primaryMatch.glyph == a_selection.glyph && primaryMatch.glyph)
			return MetadataReason(
				"primary", a_request.heading, a_selection.glyph);

		const std::array secondary{
			std::string_view{ a_request.context }
		};
		const auto secondaryMatch =
			DearModdingUI::IconResolverDetail::EvaluateGroup(secondary);
		if (secondaryMatch.glyph == a_selection.glyph && secondaryMatch.glyph)
			return MetadataReason(
				"secondary", a_request.context, a_selection.glyph);
		return "no-match";
	}

	[[nodiscard]] Json ResolveOne(const BridgeRequest& a_request)
	{
		const std::array primary{
			std::string_view{ a_request.heading }
		};
		const std::array secondary{
			std::string_view{ a_request.context }
		};
		const IconResolutionRequest request{
			.explicitGlyph = a_request.explicitGlyph,
			.explicitName = a_request.explicitName,
			.primaryMetadata = primary,
			.secondaryMetadata = secondary
		};

		const auto started = std::chrono::steady_clock::now();
		const auto selection = IconResolver::Resolve(request);
		const auto stopped = std::chrono::steady_clock::now();
		const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
			stopped - started).count();

		return {
			{ "status", SelectionStatusName(selection.status) },
			{ "glyph", static_cast<std::uint32_t>(selection.glyph) },
			{ "name", std::string{ CanonicalName(selection.glyph) } },
			{ "reason", SelectionReason(a_request, selection) },
			{ "normalized_heading",
				DearModdingUI::NormalizeIconName(a_request.heading) },
			{ "normalized_context",
				DearModdingUI::NormalizeIconName(a_request.context) },
			{ "elapsed_ns", elapsed }
		};
	}

	[[nodiscard]] Json ResolveResponse(const Json& a_message)
	{
		const auto requests = ParseRequests(a_message);
		auto results = Json::array();
		for (const auto& request : requests)
			results.push_back(ResolveOne(request));
		return { { "results", std::move(results) } };
	}

	[[nodiscard]] Json ParseMessage(std::string_view a_line)
	{
		if (a_line.empty())
			throw ProtocolError("invalid_json", "input line is empty");
		if (a_line.size() > kMaximumLineBytes)
			throw ProtocolError(
				"invalid_json",
				"input line exceeds the maximum byte size");
		try
		{
			return Json::parse(a_line.begin(), a_line.end());
		}
		catch (const Json::parse_error& error)
		{
			throw ProtocolError(
				"invalid_json",
				std::string{ "input is not valid JSON: " } + error.what());
		}
	}

	[[nodiscard]] Json Dispatch(const Json& a_message)
	{
		if (!a_message.is_object())
			throw ProtocolError(
				"invalid_message",
				"each input line must be a JSON object");
		const auto operation = a_message.find("op");
		if (operation == a_message.end() || !operation->is_string())
			throw ProtocolError(
				"invalid_message",
				"op must be a string");
		const auto name = operation->get<std::string>();
		if (name == "catalog")
			return CatalogResponse();
		if (name == "resolve")
			return ResolveResponse(a_message);
		throw ProtocolError(
			"unsupported_operation",
			"unsupported op: " + name);
	}

	[[nodiscard]] Json ErrorResponse(const ProtocolError& a_error)
	{
		return {
			{ "error", {
				{ "code", a_error.Code() },
				{ "message", a_error.what() }
			} }
		};
	}

	template <class Function>
	void RequireProtocolFailure(Function&& a_function)
	{
		bool failed = false;
		try
		{
			a_function();
		}
		catch (const ProtocolError&)
		{
			failed = true;
		}
		vmm_tests::require(failed, "invalid protocol input was accepted");
	}

	[[nodiscard]] int RunSelfTests()
	{
		using DearModdingUI::IconResolverDetail::FindExactAuthoritative;
		using DearModdingUI::PhosphorGlyph::kQuestion;
		using DearModdingUI::PhosphorGlyph::kSun;
		using vmm_tests::require;

		vmm_tests::Runner runner;
		runner.test("generated authoritative catalog resolves directly", [] {
			require(
				kPhosphorIconGlyphs.size() == 1512,
				"pinned canonical glyph count changed");
			for (const auto& mapping : kPhosphorIconGlyphs)
			{
				const auto selection = IconResolver::Resolve({
					.explicitName = mapping.phrase
				});
				require(
					selection.status == IconSelectionStatus::kSelected &&
						selection.glyph == mapping.glyph,
					"canonical icon name did not resolve to its generated glyph");
			}
			for (const auto& mapping : kPhosphorIconAliases)
				require(
					FindExactAuthoritative(mapping.phrase) == mapping.glyph,
					"generated alias did not resolve authoritatively");
			for (const auto& mapping : kPhosphorIconDomainTerms)
				require(
					FindExactAuthoritative(mapping.phrase) == mapping.glyph,
					"generated domain term did not resolve authoritatively");
		});

		runner.test("explicit glyph and name precedence remains exact", [=] {
			const std::array primary{ std::string_view{ "Hammer" } };
			const std::array secondary{ std::string_view{ "Wrench" } };
			const auto explicitGlyph = IconResolver::Resolve({
				.explicitGlyph = kSun,
				.explicitName = "wrench",
				.primaryMetadata = primary,
				.secondaryMetadata = secondary
			});
			const auto explicitName = IconResolver::Resolve({
				.explicitName = "question",
				.primaryMetadata = primary,
				.secondaryMetadata = secondary
			});
			require(
				explicitGlyph.status == IconSelectionStatus::kSelected &&
					explicitGlyph.glyph == kSun &&
					explicitName.status == IconSelectionStatus::kSelected &&
					explicitName.glyph == kQuestion,
				"explicit glyph, name, or Question precedence changed");

			for (const auto invalid :
				{ char32_t{}, char32_t{ 0xD800 }, char32_t{ 0x110000 } })
			{
				const auto selection = IconResolver::Resolve({
					.explicitGlyph = invalid,
					.explicitName = "question",
					.primaryMetadata = primary
				});
				require(
					selection.status ==
							IconSelectionStatus::kInvalidRawGlyph &&
						selection.glyph == invalid,
					"invalid raw glyph did not preserve status and value");
			}
		});

		runner.test("metadata ranking preserves production behavior", [] {
			const std::array direct{ std::string_view{ "Recent Address Book" } };
			const std::array primary{ std::string_view{ "Hammer" } };
			const std::array secondary{ std::string_view{ "Wrench" } };
			const std::array plural{ std::string_view{ "Acorns" } };
			const std::array tie{ std::string_view{ "Wrench Hammer" } };
			require(
				IconResolver::Resolve({
					.primaryMetadata = direct
				}).glyph == char32_t{ 0xE6F8 } &&
					IconResolver::Resolve({
						.primaryMetadata = primary,
						.secondaryMetadata = secondary
					}).glyph == char32_t{ 0xE80E } &&
					IconResolver::Resolve({
						.primaryMetadata = plural,
						.secondaryMetadata = secondary
					}).glyph == char32_t{ 0xEB9A } &&
					IconResolver::Resolve({
						.primaryMetadata = tie
					}).glyph == char32_t{ 0xE5D4 },
				"direct, primary, plural, or tie ranking changed");
		});

		runner.test("no-match and protocol failures remain distinct", [] {
			const std::array unknown{
				std::string_view{ "Unmapped Frobnicator" }
			};
			const auto noMatch = IconResolver::Resolve({
				.primaryMetadata = unknown
			});
			require(
				noMatch.status == IconSelectionStatus::kNoMatch &&
					noMatch.glyph == char32_t{},
				"genuine no-match did not remain a successful zero selection");

			RequireProtocolFailure([] {
				(void)ParseMessage("{");
			});
			RequireProtocolFailure([] {
				(void)ParseRequest(Json{ { "heading", 7 } });
			});
			RequireProtocolFailure([] {
				(void)ParseRequest(Json{ { "context", "bad\ncontext" } });
			});
			RequireProtocolFailure([] {
				(void)ParseRequest(Json{ { "explicit_glyph", -1 } });
			});
			RequireProtocolFailure([] {
				(void)ParseRequest(Json{
					{ "explicit_name",
						std::string(kMaximumExplicitNameBytes + 1, 'a') }
				});
			});
		});

		std::cout << "[INFO] " << runner.tests() << " tests, "
				  << runner.failures() << " failures\n";
		return runner.failures() == 0 ? 0 : 1;
	}
}

int main(int a_argumentCount, char** a_arguments)
{
	if (a_argumentCount == 2 &&
		std::string_view{ a_arguments[1] } == "--self-test")
		return RunSelfTests();
	if (a_argumentCount != 1)
	{
		std::cerr << "usage: dmui-icon-comparison [--self-test]\n";
		return 2;
	}

	bool hadError = false;
	std::string line;
	while (std::getline(std::cin, line))
	{
		try
		{
			std::cout << Dispatch(ParseMessage(line)).dump() << '\n';
		}
		catch (const ProtocolError& error)
		{
			hadError = true;
			std::cout << ErrorResponse(error).dump() << '\n';
		}
		catch (const std::exception& error)
		{
			hadError = true;
			const ProtocolError protocolError{
				"internal_error",
				std::string{ "native bridge failed: " } + error.what()
			};
			std::cout << ErrorResponse(protocolError).dump() << '\n';
		}
	}
	if (!std::cin.eof())
	{
		const ProtocolError error{
			"input_error",
			"failed while reading JSONL input"
		};
		std::cout << ErrorResponse(error).dump() << '\n';
		return 1;
	}
	return hadError ? 1 : 0;
}
