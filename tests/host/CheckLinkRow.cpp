#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/controls/LinkRow.h>
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

	void run_link_row_checks(Runner& runner)
	{
		runner.test("link-row API arguments reject malformed descriptors", [] {
			const DMUI_DrawLinkRowFn drawLinkRow =
				&ValidateLinkRowArguments;
			DMUI_ExternalOpenDescriptor external{
				DMUI_EXTERNAL_OPEN_DESCRIPTOR_0_1_SIZE,
				DMUI_EXTERNAL_TARGET_URI,
				"https://github.com/Dear-Modding-FO4/DearModdingUI"
			};
			DMUI_LinkDescriptor link{
				DMUI_LINK_DESCRIPTOR_0_1_SIZE,
				"GitHub",
				nullptr,
				0,
				1,
				DMUI_LINK_ACTION_COPY_TARGET,
				0,
				&external
			};
			require(
				drawLinkRow(
					DMUI_INVALID_CLIENT_HANDLE,
					nullptr,
					&link,
					1) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"a null link-row ID was accepted");
			require(
				drawLinkRow(
					DMUI_INVALID_CLIENT_HANDLE,
					"links",
					nullptr,
					1) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"a null non-empty link array was accepted");
			link.label = "";
			require(
				drawLinkRow(
					DMUI_INVALID_CLIENT_HANDLE,
					"links",
					&link,
					1) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"an empty link label was accepted");
			link.label = "GitHub";
			external.target = "";
			require(
				drawLinkRow(
					DMUI_INVALID_CLIENT_HANDLE,
					"links",
					&link,
					1) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"an enabled link without a target was accepted");
			external.target = "https://github.com/Dear-Modding-FO4/DearModdingUI";
			link.structSize = DMUI_LINK_DESCRIPTOR_0_1_SIZE - 1;
			require(
				drawLinkRow(
					DMUI_INVALID_CLIENT_HANDLE,
					"links",
					&link,
					1) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"a short link descriptor was accepted");
			link.structSize = DMUI_LINK_DESCRIPTOR_0_1_SIZE;
			require(
				drawLinkRow(
					DMUI_INVALID_CLIENT_HANDLE,
					"links",
					nullptr,
					0) ==
					DMUI_RESULT_OK,
				"an empty link row was rejected");
			link.enabled = 0;
			link.external = nullptr;
			require(
				drawLinkRow(
					DMUI_INVALID_CLIENT_HANDLE,
					"links",
					&link,
					1) == DMUI_RESULT_OK,
				"a disabled link required an executable action");
		});

	}
}
