#include "../Harness.h"

#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/host/HostAPIEntries.h>

#include <array>
#include <string>

namespace vmm_tests
{
	using DearModdingUI::HostAPIInternal::ApiResolveIconGlyph;

	namespace
	{
		char32_t QueryIconGlyph(
			const char* a_explicit,
			const char* a_primary,
			const char* a_secondary = nullptr)
		{
			const DMUI_IconResolutionRequest request{
				a_explicit,
				a_primary,
				a_secondary
			};
			uint32_t glyph{ 0xFFFFFFFFu };
			require(
				ApiResolveIconGlyph(&request, &glyph) == DMUI_RESULT_OK,
				"valid icon query failed");
			return static_cast<char32_t>(glyph);
		}
	}

	void run_icon_resolution_checks(Runner& runner)
	{
		runner.test("host icon query preserves resolver precedence and fallback", [] {
			const auto wrench =
				DearModdingUI::FindPhosphorIconGlyphOrZero("wrench");
			const auto hammer =
				DearModdingUI::FindPhosphorIconGlyphOrZero("hammer");
			require(
				wrench != 0 && hammer != 0,
				"representative resolver glyphs were unavailable");
			require(
				QueryIconGlyph("Wrench", "hammer", "gear") == wrench,
				"canonical explicit name did not win");
			require(
				QueryIconGlyph("not-a-real-icon-name", "wrench", "hammer") ==
						wrench,
				"unknown explicit name did not fall through to metadata");
			require(
				QueryIconGlyph(nullptr, "hammer", "wrench") == hammer,
				"primary metadata did not retain precedence");
			require(
				QueryIconGlyph(
					nullptr,
					"host-only-unmatched-label",
					"gear") == DearModdingUI::PhosphorGlyph::kGear,
				"secondary metadata was not used after a primary miss");
			require(
				QueryIconGlyph(nullptr, "host-only-unmatched-label") == 0,
				"genuine no-match did not return a successful zero glyph");
		});

		runner.test("host icon query validates bounded request strings", [] {
			DMUI_IconResolutionRequest request{
				nullptr,
				"wrench\tsettings",
				nullptr
			};
			uint32_t glyph{ 0xFFFFFFFFu };
			require(
				ApiResolveIconGlyph(&request, &glyph) == DMUI_RESULT_OK &&
					glyph == static_cast<uint32_t>(
						DearModdingUI::FindPhosphorIconGlyphOrZero("wrench")),
				"permitted tab was rejected");

			glyph = 0xFFFFFFFFu;
			require(
				ApiResolveIconGlyph(nullptr, &glyph) ==
						DMUI_RESULT_INVALID_ARGUMENT &&
					glyph == 0 &&
					ApiResolveIconGlyph(&request, nullptr) ==
						DMUI_RESULT_INVALID_ARGUMENT,
				"null request or output validation changed");

			std::array<char, 129> overlongName{};
			overlongName.fill('a');
			std::array<char, 257> overlongMetadata{};
			overlongMetadata.fill('b');
			request.explicitName = overlongName.data();
			request.primaryMetadata = nullptr;
			glyph = 0xFFFFFFFFu;
			require(
				ApiResolveIconGlyph(&request, &glyph) ==
						DMUI_RESULT_INVALID_ARGUMENT &&
					glyph == 0,
				"nonterminated explicit name was accepted");
			request.explicitName = nullptr;
			request.primaryMetadata = overlongMetadata.data();
			glyph = 0xFFFFFFFFu;
			require(
				ApiResolveIconGlyph(&request, &glyph) ==
						DMUI_RESULT_INVALID_ARGUMENT &&
					glyph == 0,
				"nonterminated metadata was accepted");

			const std::string controlText{ "wrench\nsettings" };
			request.primaryMetadata = controlText.c_str();
			glyph = 0xFFFFFFFFu;
			require(
				ApiResolveIconGlyph(&request, &glyph) ==
						DMUI_RESULT_INVALID_ARGUMENT &&
					glyph == 0,
				"disallowed control text was accepted");
		});
	}
}
