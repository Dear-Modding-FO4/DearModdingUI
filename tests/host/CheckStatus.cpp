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

			const auto clientStart = StatusClock::time_point{};
			StatusModel clients;
			require(clients.SetClient(
						2, "Second", DMUI_STATUS_SEVERITY_WARNING, "Warning",
						clientStart) == DMUI_RESULT_OK &&
					clients.SetClient(
						1, "First", DMUI_STATUS_SEVERITY_INFO, "Working",
						clientStart) == DMUI_RESULT_OK,
				"client status was rejected");
			auto statuses = clients.SnapshotClientStatuses(clientStart);
			require(
				statuses.size() == 2 &&
					statuses[0].client == 1 &&
					statuses[0].severity == DMUI_STATUS_SEVERITY_INFO &&
					statuses[1].client == 2 &&
					statuses[1].severity == DMUI_STATUS_SEVERITY_WARNING,
				"client statuses superseded another mod");
			require(clients.SetClient(
						2, "Second", DMUI_STATUS_SEVERITY_SUCCESS, "Recovered",
						clientStart) == DMUI_RESULT_OK,
				"client recovery status was rejected");
			require(clients.SnapshotClientStatuses(
						clientStart + kTransientStatusLifetime).empty(),
				"transient client statuses did not expire independently");
			require(clients.SetClient(
						DMUI_INVALID_CLIENT_HANDLE, "Invalid",
						DMUI_STATUS_SEVERITY_ERROR, "Error",
						clientStart) == DMUI_RESULT_INVALID_ARGUMENT,
				"invalid client status handle was accepted");
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
