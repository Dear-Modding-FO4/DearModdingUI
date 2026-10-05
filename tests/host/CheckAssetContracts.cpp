#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/IconGlyphs.h>

namespace vmm_tests
{
	using namespace DearModdingUI;

	void run_asset_contract_checks(Runner& runner)
	{
		runner.test("raw icon glyph validation rejects truncating code points", [] {
			require(
				IsRepresentableIconGlyph<ImWchar>(PhosphorGlyph::kSun) &&
					!IsRepresentableIconGlyph<ImWchar>(char32_t{}) &&
					!IsRepresentableIconGlyph<ImWchar>(
						char32_t{ 0x1E472 }),
				"raw glyph validation allowed zero, invalid, or truncating values");
			const auto invalidRaw = IconResolver::Resolve({
				.explicitGlyph = char32_t{ 0x110000 }
			});
			const auto zeroRaw = IconResolver::Resolve({
				.explicitGlyph = char32_t{}
			});
			require(
				invalidRaw.status == IconSelectionStatus::kInvalidRawGlyph &&
					zeroRaw.status == IconSelectionStatus::kInvalidRawGlyph,
				"invalid raw glyphs were not flagged");
		});
	}
}
