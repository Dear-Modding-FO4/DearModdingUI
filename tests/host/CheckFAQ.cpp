#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/controls/Faq.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;
	using namespace support::host;

	void run_faq_checks(Runner& runner)
	{
		runner.test("FAQ API arguments reject malformed entries", [] {
			const DMUI_DrawFaqFn drawFaq = &ValidateFaqArguments;
			DMUI_FaqEntry entry{
				DMUI_FAQ_ENTRY_0_1_SIZE,
				"How do I open the menu?",
				"Press End."
			};
			require(
				drawFaq(
					DMUI_INVALID_CLIENT_HANDLE,
					nullptr,
					&entry,
					1) == DMUI_RESULT_INVALID_ARGUMENT,
				"a null FAQ ID was accepted");
			require(
				drawFaq(
					DMUI_INVALID_CLIENT_HANDLE,
					"faq",
					nullptr,
					1) == DMUI_RESULT_INVALID_ARGUMENT,
				"a null non-empty FAQ array was accepted");
			entry.question = "";
			require(
				drawFaq(
					DMUI_INVALID_CLIENT_HANDLE,
					"faq",
					&entry,
					1) == DMUI_RESULT_INVALID_ARGUMENT,
				"an empty FAQ question was accepted");
			entry.question = "How do I open the menu?";
			entry.answer = "";
			require(
				drawFaq(
					DMUI_INVALID_CLIENT_HANDLE,
					"faq",
					&entry,
					1) == DMUI_RESULT_INVALID_ARGUMENT,
				"an empty FAQ answer was accepted");
			entry.answer = "Press End.";
			entry.structSize = DMUI_FAQ_ENTRY_0_1_SIZE - 1;
			require(
				drawFaq(
					DMUI_INVALID_CLIENT_HANDLE,
					"faq",
					&entry,
					1) == DMUI_RESULT_INVALID_ARGUMENT,
				"a short FAQ entry was accepted");
			require(
				drawFaq(
					DMUI_INVALID_CLIENT_HANDLE,
					"faq",
					nullptr,
					0) == DMUI_RESULT_OK,
				"an empty FAQ was rejected");
		});

	}
}
