#include "../Harness.h"

#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/host/HostAPIEntries.h>

#include <array>
#include <string>

namespace vmm_tests
{
	using DearModdingUI::HostAPIInternal::ApiResolveIconGlyph;

	void run_icon_resolution_checks(Runner& runner)
	{
		runner.test("host icon query delegates selection precedence to shared resolver", [] {
			const auto invoke = [](
								 const char* a_explicit,
								 const char* a_primary,
								 const char* a_secondary) {
				const DMUI_IconResolutionRequest request{
					sizeof(request),
					a_explicit,
					a_primary,
					a_secondary
				};
				uint32_t glyph{ 0xFFFFFFFFu };
				require(
					ApiResolveIconGlyph(&request, &glyph) == DMUI_RESULT_OK,
					"valid icon query failed");
				return static_cast<char32_t>(glyph);
			};

			const auto explicitGlyph =
				DearModdingUI::ResolveNamedIconGlyphOrZero("wrench");
			require(
				explicitGlyph != 0 &&
					invoke("wrench", "speaker", "display") == explicitGlyph,
				"canonical explicit name did not win");

			const auto fallbackExpected =
				DearModdingUI::ResolveIconSelection(
					"not-a-real-icon-name",
					"wrench",
					"speaker")
					.GlyphOr({});
			require(
				fallbackExpected != 0 &&
					invoke("not-a-real-icon-name", "wrench", "speaker") ==
						fallbackExpected,
				"unknown explicit name did not fall through to metadata");

			const auto precedenceExpected =
				DearModdingUI::ResolveIconSelection(
					{},
					"speaker",
					"wrench")
					.GlyphOr({});
			require(
				precedenceExpected != 0 &&
					invoke(nullptr, "speaker", "wrench") ==
						precedenceExpected,
				"primary metadata did not retain precedence");
			require(
				invoke(nullptr, "host-only-unmatched-label", nullptr) == 0,
				"genuine no-match did not return a successful zero glyph");
		});

		runner.test("host icon query validates its versioned bounded request", [] {
			DMUI_IconResolutionRequest request{
				DMUI_ICON_RESOLUTION_REQUEST_0_1_SIZE,
				nullptr,
				"wrench\tsettings",
				nullptr
			};
			uint32_t glyph{ 0xFFFFFFFFu };
			require(
				ApiResolveIconGlyph(&request, &glyph) == DMUI_RESULT_OK &&
					glyph == static_cast<uint32_t>(
						DearModdingUI::ResolveIconSelection(
							{},
							"wrench\tsettings")
							.GlyphOr({})),
				"exact request size or permitted tab was rejected");

			request.structSize += sizeof(uint64_t);
			glyph = 0xFFFFFFFFu;
			require(
				ApiResolveIconGlyph(&request, &glyph) == DMUI_RESULT_OK &&
					glyph != 0,
				"extended request was rejected");

			request.structSize =
				DMUI_ICON_RESOLUTION_REQUEST_0_1_SIZE - 1;
			glyph = 0xFFFFFFFFu;
			require(
				ApiResolveIconGlyph(&request, &glyph) ==
						DMUI_RESULT_STRUCT_TOO_SMALL &&
					glyph == 0,
				"short request did not fail with zeroed output");
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
			request.structSize = DMUI_ICON_RESOLUTION_REQUEST_0_1_SIZE;
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
