#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/presentation/IconResolver.h>

namespace vmm_tests
{
	using namespace DearModdingUI;

	void run_asset_contract_checks(Runner& runner)
	{
		runner.test("raw icon glyph validation rejects truncating code points", [] {
			require(
				dmui::IsRepresentableIconGlyph<ImWchar>(dmui::PhosphorGlyph::kSun) &&
					!dmui::IsRepresentableIconGlyph<ImWchar>(char32_t{}) &&
					!dmui::IsRepresentableIconGlyph<ImWchar>(
						char32_t{ 0x1E472 }),
				"raw glyph validation allowed zero, invalid, or truncating values");
			const auto invalidRaw = dmui::IconResolver::Resolve({
				.explicitGlyph = char32_t{ 0x110000 }
			});
			const auto zeroRaw = dmui::IconResolver::Resolve({
				.explicitGlyph = char32_t{}
			});
			require(
				invalidRaw.status == dmui::IconSelectionStatus::kInvalidRawGlyph &&
					zeroRaw.status == dmui::IconSelectionStatus::kInvalidRawGlyph,
				"invalid raw glyphs were not flagged");
		});
	}
}
