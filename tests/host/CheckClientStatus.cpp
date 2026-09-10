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

	void run_client_status_checks(Runner& runner)
	{
		runner.test("client status snapshots remain independent and expire", [] {
			const auto start = StatusClock::time_point{};
			StatusModel model;
			require(model.SetClient(
						2,
						"Second",
						DMUI_STATUS_SEVERITY_WARNING,
						"Warning",
						start) == DMUI_RESULT_OK,
				"client warning status was rejected");
			require(model.SetClient(
						1,
						"First",
						DMUI_STATUS_SEVERITY_INFO,
						"Working",
						start) == DMUI_RESULT_OK,
				"client info status was rejected");
			auto statuses = model.SnapshotClientStatuses(start);
			require(
					statuses.size() == 2 &&
						statuses[0].client == 1 &&
						statuses[0].severity == DMUI_STATUS_SEVERITY_INFO &&
						statuses[1].client == 2 &&
						statuses[1].severity == DMUI_STATUS_SEVERITY_WARNING,
					"client statuses superseded another mod");
			const std::array rollupInput{
				ClientStatus{ 2, DMUI_STATUS_SEVERITY_WARNING },
				ClientStatus{ 1, DMUI_STATUS_SEVERITY_SUCCESS },
				ClientStatus{ 2, DMUI_STATUS_SEVERITY_INFO },
				ClientStatus{ 1, DMUI_STATUS_SEVERITY_ERROR },
				ClientStatus{ DMUI_INVALID_CLIENT_HANDLE,
					DMUI_STATUS_SEVERITY_ERROR }
			};
			const auto rollups = RollupClientStatuses(rollupInput);
			require(
					rollups.size() == 2 &&
						rollups[0].client == 1 &&
						rollups[0].severity == DMUI_STATUS_SEVERITY_ERROR &&
						rollups[1].client == 2 &&
						rollups[1].severity == DMUI_STATUS_SEVERITY_WARNING,
				"status rollup retained an invalid client or weaker severity");

			require(model.SetClient(
						2,
						"Second",
						DMUI_STATUS_SEVERITY_SUCCESS,
						"Recovered",
						start) == DMUI_RESULT_OK,
				"client recovery status was rejected");
			statuses = model.SnapshotClientStatuses(
				start + kTransientStatusLifetime);
			require(statuses.empty(),
				"transient client statuses did not expire independently");
			require(model.SetClient(
						DMUI_INVALID_CLIENT_HANDLE,
						"Invalid",
						DMUI_STATUS_SEVERITY_ERROR,
						"Error",
						start) == DMUI_RESULT_INVALID_ARGUMENT,
				"invalid client status handle was accepted");
		});

	}
}
