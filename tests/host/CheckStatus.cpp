#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/host/Status.h>
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

	void run_status_checks(Runner& runner)
	{
		runner.test("status severity controls expiry and persistence", [] {
			const auto start = StatusClock::time_point{};
			StatusModel model;
			require(model.Set(
						StatusOwnerKind::kHost,
						"Evil Modding",
						DMUI_STATUS_SEVERITY_INFO,
						"Working",
						start) == DMUI_RESULT_OK,
				"info status was rejected");
			require(model.Snapshot(
						start +
						kTransientStatusLifetime -
						std::chrono::milliseconds{ 1 }).has_value(),
				"info status expired early");
			require(!model.Snapshot(start + kTransientStatusLifetime),
				"info status did not expire");

			require(model.Set(
						StatusOwnerKind::kHost,
						"Evil Modding",
						DMUI_STATUS_SEVERITY_SUCCESS,
						"Saved",
						start) == DMUI_RESULT_OK,
				"success status was rejected");
			require(!model.Snapshot(start + kTransientStatusLifetime),
				"success status did not expire");

			require(model.Set(
						StatusOwnerKind::kHost,
						"Evil Modding",
						DMUI_STATUS_SEVERITY_WARNING,
						"Warning",
						start) == DMUI_RESULT_OK,
				"warning status was rejected");
			require(model.Snapshot(start + std::chrono::hours{ 24 }).has_value(),
				"warning status expired");

			require(model.Set(
						StatusOwnerKind::kHost,
						"Evil Modding",
						DMUI_STATUS_SEVERITY_ERROR,
						"Error",
						start) == DMUI_RESULT_OK,
				"error status was rejected");
			require(model.Snapshot(start + std::chrono::hours{ 24 }).has_value(),
				"error status expired");
		});

		runner.test("persistent status can be dismissed without clearing a replacement", [] {
			const auto start = StatusClock::time_point{};
			StatusModel model;
			require(model.Set(
						StatusOwnerKind::kClient,
						"Client",
						DMUI_STATUS_SEVERITY_ERROR,
						"Persistent",
						start) == DMUI_RESULT_OK,
				"persistent status was rejected");
			const auto persistent = model.Snapshot(start);
			require(persistent && model.Dismiss(persistent->generation),
				"persistent status was not dismissed");
			require(!model.Snapshot(start), "dismissed status remained visible");

			require(model.Set(
						StatusOwnerKind::kClient,
						"Client",
						DMUI_STATUS_SEVERITY_WARNING,
						"Older",
						start) == DMUI_RESULT_OK,
				"older persistent status was rejected");
			const auto older = model.Snapshot(start);
			require(model.Set(
						StatusOwnerKind::kClient,
						"Replacement owner",
						DMUI_STATUS_SEVERITY_ERROR,
						"Replacement",
						start + std::chrono::milliseconds{ 1 }) == DMUI_RESULT_OK,
				"replacement status was rejected");
			const auto replacement =
				model.Snapshot(start + std::chrono::milliseconds{ 1 });
			require(older && !model.Dismiss(older->generation),
				"stale dismissal cleared a replacement");
			require(
					replacement &&
						replacement->generation > older->generation &&
						replacement->owner == "Replacement owner" &&
						replacement->message == "Replacement",
				"newest status did not supersede an older persistent status");
			require(
				model.Snapshot(start + std::chrono::milliseconds{ 1 })
						->message == "Replacement",
				"replacement was lost after stale dismissal");
		});

		runner.test("status truncation preserves text and UTF-8 boundaries", [] {
			const auto measure = [](std::string_view a_text) {
				return static_cast<float>(a_text.size());
			};
			const auto full = std::string{ "Community Shaders: A detailed failure message" };
			const auto truncated = FitStatusText(full, 24.0f, measure);
			require(
					truncated.truncated &&
						truncated.full == full &&
						truncated.visible != full &&
						truncated.visible.ends_with("\xE2\x80\xA6") &&
						measure(truncated.visible) <= 24.0f,
					"truncated status lost its full tooltip text");
			const auto fitting = FitStatusText(full, measure(full), measure);
			require(
					!fitting.truncated &&
						fitting.visible == full &&
						fitting.full == full,
					"fitting status was truncated");

			const std::string utf8{
				"Buffout \xF0\x9F\xA7\xAA status"
			};
			const auto utf8Truncated = FitStatusText(utf8, 11.0f, measure);
			require(
					utf8Truncated.truncated &&
						utf8Truncated.visible ==
							"Buffout \xE2\x80\xA6" &&
						utf8Truncated.full == utf8,
				"status truncation split a UTF-8 character");

			const auto clipped =
				FitStatusText("Buffout 4: Error", 0.0f, measure);
			require(
					clipped.truncated &&
						clipped.visible == "\xE2\x80\xA6" &&
						clipped.full == "Buffout 4: Error",
				"fully clipped status lost its overflow presentation");
		});

		runner.test("status validation rejects invalid clients and messages", [] {
			Registry registry;
			CallbackState state;
			const auto client = AddClient(
				registry, "status.mod", "Status Mod", state);
			std::string owner;
			require(
					ValidateStatusRequest(
						registry.CopyClientDisplayName(9999, owner),
						DMUI_STATUS_SEVERITY_INFO,
						"Message") == DMUI_RESULT_CLIENT_NOT_FOUND,
					"unaccepted status client was not rejected");
			const auto accepted = registry.CopyClientDisplayName(client, owner);
			require(
					accepted == DMUI_RESULT_OK && owner == "Status Mod",
					"accepted status owner was not copied");
			require(
					ValidateStatusRequest(
						accepted,
						DMUI_STATUS_SEVERITY_INFO,
						nullptr) == DMUI_RESULT_INVALID_ARGUMENT,
					"null status message was not rejected");
			require(
					ValidateStatusRequest(
						accepted,
						DMUI_STATUS_SEVERITY_INFO,
						"") == DMUI_RESULT_INVALID_ARGUMENT,
					"empty status message was not rejected");
			require(
					ValidateStatusRequest(
						accepted,
						99,
						"Message") == DMUI_RESULT_INVALID_ARGUMENT,
					"unknown status severity was not rejected");
		});

	}
}
