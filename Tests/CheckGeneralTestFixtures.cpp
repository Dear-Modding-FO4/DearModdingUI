#include "Harness.h"

#include <GeneralTestFixtures.h>
#include <DearModdingUI/UIAdapter.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace
{
	struct CapturedClient
	{
		DMUI_ClientHandle handle{};
		std::string id;
		std::string displayName;
		DMUI_ClientOrigin origin{};
		std::string bridgeSourceLabel;
	};

	struct CapturedCategory
	{
		DMUI_ClientHandle client{};
		std::string id;
		std::string displayName;
		int32_t sortKey{};
	};

	struct CapturedPage
	{
		DMUI_ClientHandle client{};
		DMUI_PageHandle handle{};
		std::string clientId;
		std::string id;
		std::string displayName;
		std::string categoryId;
		std::string summary;
		int32_t sortKey{};
		DMUI_PageKind kind{};
		DMUI_PageDrawCallback draw{};
		void* userData{};
	};

	struct CapturedStatus
	{
		DMUI_ClientHandle client{};
		DMUI_StatusSeverity severity{};
		std::string text;
	};

	struct CapturedDiagnostic
	{
		DMUI_ClientHandle client{};
		DMUI_StatusSeverity severity{};
		std::string scope;
		std::string summary;
		std::string detail;
	};

	struct FixtureHost
	{
		std::vector<CapturedClient> clients;
		std::vector<CapturedCategory> categories;
		std::vector<CapturedPage> pages;
		std::vector<CapturedStatus> statuses;
		std::vector<CapturedDiagnostic> diagnostics;
		DMUI_ClientHandle nextClient{ 1 };
		DMUI_PageHandle nextPage{ 1 };
		size_t registerPageAttempts{};
		std::optional<size_t> failPageRegistrationAt;
		size_t sectionHeaderDraws{};
		size_t bulletTextDraws{};

		void Reset()
		{
			clients.clear();
			categories.clear();
			pages.clear();
			statuses.clear();
			diagnostics.clear();
			nextClient = 1;
			nextPage = 1;
			registerPageAttempts = 0;
			failPageRegistrationAt.reset();
			sectionHeaderDraws = 0;
			bulletTextDraws = 0;
		}
	};

	FixtureHost s_fixtureHost;

	DMUI_Result DMUI_CALL RegisterClient(
		const DMUI_ClientDescriptor* a_descriptor,
		DMUI_ClientHandle* a_client) noexcept
	{
		if (!a_descriptor || !a_client)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_client = s_fixtureHost.nextClient++;
		s_fixtureHost.clients.push_back({
			*a_client,
			a_descriptor->id ? a_descriptor->id : "",
			a_descriptor->displayName ? a_descriptor->displayName : "",
			a_descriptor->origin,
			a_descriptor->bridgeSourceLabel ?
				a_descriptor->bridgeSourceLabel :
				""
		});
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL RegisterCategory(
		DMUI_ClientHandle a_client,
		const DMUI_CategoryDescriptor* a_descriptor) noexcept
	{
		if (!a_descriptor)
			return DMUI_RESULT_INVALID_ARGUMENT;
		s_fixtureHost.categories.push_back({
			a_client,
			a_descriptor->id ? a_descriptor->id : "",
			a_descriptor->displayName ? a_descriptor->displayName : "",
			a_descriptor->sortKey
		});
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL RegisterPage(
		DMUI_ClientHandle a_client,
		const DMUI_PageDescriptor* a_descriptor,
		DMUI_PageHandle* a_page) noexcept
	{
		if (!a_descriptor || !a_page)
			return DMUI_RESULT_INVALID_ARGUMENT;
		++s_fixtureHost.registerPageAttempts;
		if (s_fixtureHost.failPageRegistrationAt ==
			s_fixtureHost.registerPageAttempts)
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		*a_page = s_fixtureHost.nextPage++;
		const auto owner = std::ranges::find(
			s_fixtureHost.clients,
			a_client,
			&CapturedClient::handle);
		s_fixtureHost.pages.push_back({
			a_client,
			*a_page,
			owner != s_fixtureHost.clients.end() ? owner->id : "",
			a_descriptor->id ? a_descriptor->id : "",
			a_descriptor->displayName ? a_descriptor->displayName : "",
			a_descriptor->categoryId ? a_descriptor->categoryId : "",
			a_descriptor->summary ? a_descriptor->summary : "",
			a_descriptor->sortKey,
			a_descriptor->kind,
			a_descriptor->draw,
			a_descriptor->userData
		});
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL DrawSectionHeader(
		DMUI_ClientHandle,
		const char* a_text,
		uint32_t) noexcept
	{
		if (!a_text)
			return DMUI_RESULT_INVALID_ARGUMENT;
		++s_fixtureHost.sectionHeaderDraws;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL DrawBulletText(
		DMUI_ClientHandle,
		const char* a_text) noexcept
	{
		if (!a_text)
			return DMUI_RESULT_INVALID_ARGUMENT;
		++s_fixtureHost.bulletTextDraws;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL SetStatus(
		DMUI_ClientHandle a_client,
		DMUI_StatusSeverity a_severity,
		const char* a_text) noexcept
	{
		if (!a_text)
			return DMUI_RESULT_INVALID_ARGUMENT;
		s_fixtureHost.statuses.push_back({
			a_client,
			a_severity,
			a_text
		});
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL ReportDiagnostic(
		DMUI_ClientHandle a_client,
		const DMUI_DiagnosticDescriptor* a_descriptor) noexcept
	{
		if (!a_descriptor)
			return DMUI_RESULT_INVALID_ARGUMENT;
		s_fixtureHost.diagnostics.push_back({
			a_client,
			a_descriptor->severity,
			a_descriptor->scope ? a_descriptor->scope : "",
			a_descriptor->summary ? a_descriptor->summary : "",
			a_descriptor->detail ? a_descriptor->detail : ""
		});
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] const DMUI_HostAPI& FixtureAPI() noexcept
	{
		static const auto api = [] {
			DMUI_HostAPI result{};
			result.structSize = sizeof(result);
			result.hostAbiVersion = DMUI_HOST_ABI_CURRENT;
			result.apiVersion = DMUI_API_VERSION_CURRENT;
			result.registerClient = &RegisterClient;
			result.registerPage = &RegisterPage;
			result.setStatus = &SetStatus;
			result.reportDiagnostic = &ReportDiagnostic;
			result.registerCategory = &RegisterCategory;
			result.drawSectionHeader = &DrawSectionHeader;
			result.drawBulletText = &DrawBulletText;
			result.queryUIAPI = [](
				uint32_t a_abi,
				uint32_t a_revision,
				uint32_t a_size,
				DMUI_UIAPIInfo* a_info) noexcept -> DMUI_Result {
				static const DMUI_UIAPI ui = DearModdingUI::UI::API();
				if (!a_info ||
					a_info->structSize < DMUI_UI_API_INFO_1_SIZE)
					return DMUI_RESULT_STRUCT_TOO_SMALL;
				a_info->abiVersion = ui.abiVersion;
				a_info->revision = ui.revision;
				a_info->tableSize = ui.structSize;
				a_info->api = nullptr;
				if (a_abi != ui.abiVersion ||
					a_revision > ui.revision ||
					a_size > ui.structSize)
					return DMUI_RESULT_UNSUPPORTED_ABI;
				a_info->api = &ui;
				return DMUI_RESULT_OK;
			};
			return result;
		}();
		return api;
	}
}

const DMUI_HostAPI* DMUI_CALL DMUI_GetAPI(
	uint32_t a_requestedHostAbi) noexcept
{
	return a_requestedHostAbi == DMUI_HOST_ABI_CURRENT ?
		&FixtureAPI() :
		nullptr;
}

namespace vmm_tests
{
	void run_general_test_fixture_checks(Runner& runner)
	{
		runner.test("general test fixture uses a stable synthetic identity", [] {
			require(
				DmuiTestFixtures::kClientId == "dearmodding.tests.general",
				"general test client id changed");
			require(
				DmuiTestFixtures::kClientDisplayName == "DMUI Tests",
				"general test client label changed");

			constexpr std::array realClientIds{
				"dearmodding.addictol",
				"dearmodding.communityshaders",
				"buffout4",
				"fallui"
			};
			for (const auto id : realClientIds)
				require(
					DmuiTestFixtures::kClientId != id,
					"fixture id collides with a real client");

			std::set<std::string> syntheticIds;
			for (const auto& fixture : DmuiTestFixtures::kSyntheticClients)
			{
				const std::string_view id{ fixture.id };
				const std::string_view label{ fixture.displayName };
				require(
					id.starts_with("dearmodding.tests.synthetic."),
					"synthetic fixture id left the test namespace");
				require(
					label.starts_with("[Fixture]"),
					"synthetic fixture label is not obvious");
				require(
					syntheticIds.emplace(fixture.id).second,
					"synthetic fixture ids must be unique");
				for (const auto realId : realClientIds)
					require(id != realId, "synthetic fixture collides with a real client");
			}
		});

		runner.test("general test exercise catalog is complete and explicit", [] {
			std::set<std::string> ids;
			for (const auto& page : DmuiTestFixtures::kExercisePages)
			{
				require(page.id && *page.id, "exercise id is empty");
				require(page.displayName && *page.displayName, "exercise label is empty");
				require(page.summary && *page.summary, "exercise summary is empty");
				require(page.howTo && *page.howTo, "exercise instructions are empty");
				require(page.expected && *page.expected, "exercise expectation is empty");
				require(ids.emplace(page.id).second, "exercise ids must be unique");
			}
			require(
				DmuiTestFixtures::kExercisePages.size() == 6,
				"general test exercise coverage changed");
		});

		runner.test("general test outcomes never infer pass", [] {
			require(
				std::string_view{ DmuiTestFixtures::OutcomeName(
					DmuiTestFixtures::Outcome::kUnexercised) } == "unexercised",
				"unexercised outcome changed");
			require(
				std::string_view{ DmuiTestFixtures::OutcomeName(
					DmuiTestFixtures::Outcome::kObserved) } == "observed",
				"observed outcome changed");
			require(
				std::string_view{ DmuiTestFixtures::OutcomeName(
					DmuiTestFixtures::Outcome::kFailed) } == "failed",
				"failed outcome changed");
		});

		runner.test("shared exercises register through the public host API", [] {
			s_fixtureHost.Reset();
			size_t callbackCount{};

			dmui::Client client(
				"dearmodding.tests.registration-probe",
				"[Fixture] Registration Probe",
				dmui::Version{ 0, 1 },
				"test-tube");
			require(client.Connect(), "fixture client did not connect through host discovery");
			const auto registered = DmuiTestFixtures::RegisterExercises(
				client,
				[&callbackCount](DmuiTestFixtures::ExerciseKind) {
					++callbackCount;
				});
			require(registered.has_value(), "exercise registration failed");

			const auto& clients = s_fixtureHost.clients;
			require(clients.size() == 1, "fixture client was not registered");
			require(
				clients.back().id == "dearmodding.tests.registration-probe",
				"registered fixture identity changed");

			const auto& pages = s_fixtureHost.pages;
			require(
				pages.size() == DmuiTestFixtures::kExercisePages.size(),
				"exercise page count did not reach the host registry");
			require(
				s_fixtureHost.categories.size() == 1 &&
					s_fixtureHost.categories.front().id ==
						DmuiTestFixtures::kCategoryId,
				"exercise category descriptor did not reach the host registry");
			for (size_t index = 0; index < DmuiTestFixtures::kExercisePages.size(); ++index)
			{
				const auto& actual = pages[index];
				const auto& expected = DmuiTestFixtures::kExercisePages[index];
				require(actual.id == expected.id, "registered exercise id changed");
				require(actual.categoryId == DmuiTestFixtures::kCategoryId,
					"registered exercise category changed");
				require(actual.displayName == expected.displayName,
					"registered exercise label changed");
				require(actual.summary == expected.summary,
					"registered exercise summary changed");
				require(actual.sortKey == static_cast<int32_t>(index * 10),
					"registered exercise ordering changed");
				require(actual.kind == expected.pageKind, "registered exercise kind changed");
				require(actual.draw && actual.userData, "registered exercise callback missing");
				actual.draw(actual.userData);
			}
			require(
				callbackCount == DmuiTestFixtures::kExercisePages.size(),
				"exercise callbacks did not invoke the shared dispatcher");
			s_fixtureHost.Reset();
		});

		runner.test("synthetic clients share navigation status and configuration", [] {
			s_fixtureHost.Reset();
			DmuiTestFixtures::SyntheticSettingsState state;
			std::vector<std::unique_ptr<dmui::Client>> clients;
			std::string error;
			require(
				DmuiTestFixtures::RegisterSyntheticClients(clients, state, error),
				"synthetic client registration failed: " + error);
			require(
				clients.size() == DmuiTestFixtures::kSyntheticClients.size(),
				"synthetic client ownership is incomplete");

			const auto& registeredClients = s_fixtureHost.clients;
			require(
				registeredClients.size() == DmuiTestFixtures::kSyntheticClients.size(),
				"synthetic clients did not reach the host registry");
			for (size_t index = 0; index < DmuiTestFixtures::kSyntheticClients.size(); ++index)
			{
				const auto& actual = registeredClients[index];
				const auto& expected = DmuiTestFixtures::kSyntheticClients[index];
				require(actual.id == expected.id, "synthetic client id changed");
				require(actual.displayName == expected.displayName,
					"synthetic client label changed");
			}
			require(
				registeredClients[1].origin ==
					DMUI_CLIENT_ORIGIN_BRIDGED,
				"synthetic status client is not bridged");
			require(
				registeredClients[1].bridgeSourceLabel ==
					"Synthetic Test Bridge",
				"synthetic bridge attribution changed");
			require(
				registeredClients[0].origin == DMUI_CLIENT_ORIGIN_NATIVE &&
					registeredClients[2].origin == DMUI_CLIENT_ORIGIN_NATIVE,
				"native synthetic attribution changed");

			const auto& pages = s_fixtureHost.pages;
			require(pages.size() == 5,
				"synthetic navigation/configuration pages are incomplete");
			const auto navigationPages = std::ranges::count_if(
				pages,
				[](const auto& page) {
					return page.clientId ==
						"dearmodding.tests.synthetic.navigation";
				});
			require(navigationPages == 3,
				"synthetic navigation does not expose all shared pages");
			require(
				s_fixtureHost.categories.size() == 2 &&
					s_fixtureHost.categories[0].id == "general" &&
					s_fixtureHost.categories[0].sortKey == 0 &&
					s_fixtureHost.categories[1].id == "advanced" &&
					s_fixtureHost.categories[1].sortKey == 10,
				"synthetic navigation categories changed");
			require(
				std::ranges::any_of(
					pages,
					[](const auto& page) {
						return page.categoryId == "advanced" &&
							page.id == "diagnostics";
					}),
				"synthetic advanced navigation category is missing");
			require(
				std::ranges::any_of(
					pages,
					[](const auto& page) {
						return page.clientId ==
								"dearmodding.tests.synthetic.configuration" &&
							page.id == "configuration" &&
							page.displayName == "Configuration fixture" &&
							page.summary ==
								"In-memory synthetic configuration with no persistence." &&
							page.kind == DMUI_PAGE_KIND_SETTINGS &&
							page.draw && page.userData;
					}),
				"synthetic editable configuration page is missing");

			require(
				s_fixtureHost.statuses.size() == 1 &&
					s_fixtureHost.statuses.front().client ==
						registeredClients[1].handle &&
					s_fixtureHost.statuses.front().severity ==
						DMUI_STATUS_SEVERITY_INFO &&
					s_fixtureHost.statuses.front().text ==
						"[Fixture] Synthetic status observation.",
				"synthetic status observation was not registered");
			require(
				s_fixtureHost.diagnostics.size() == 1 &&
					s_fixtureHost.diagnostics.front().client ==
						registeredClients[1].handle &&
					s_fixtureHost.diagnostics.front().severity ==
						DMUI_STATUS_SEVERITY_WARNING &&
					s_fixtureHost.diagnostics.front().scope ==
						"synthetic-status.toml" &&
					s_fixtureHost.diagnostics.front().summary ==
						"[Fixture] Deliberate diagnostic warning." &&
					s_fixtureHost.diagnostics.front().detail ==
						"Expected only in the DMUI test suite.",
				"synthetic diagnostic observation was not registered");

			const auto navigation = std::ranges::find_if(
				pages,
				[](const auto& page) {
					return page.clientId ==
						"dearmodding.tests.synthetic.navigation";
				});
			require(navigation != pages.end(), "synthetic navigation callback is missing");
			navigation->draw(navigation->userData);
			require(
				s_fixtureHost.sectionHeaderDraws == 1 &&
					s_fixtureHost.bulletTextDraws == 2,
				"synthetic navigation callback did not use public host drawing calls");
			s_fixtureHost.Reset();
		});

		runner.test("synthetic registration failure retains callback owners", [] {
			s_fixtureHost.Reset();
			s_fixtureHost.failPageRegistrationAt = 5;
			DmuiTestFixtures::SyntheticSettingsState state;
			std::vector<std::unique_ptr<dmui::Client>> clients;
			std::string error;

			require(
				!DmuiTestFixtures::RegisterSyntheticClients(clients, state, error),
				"synthetic registration failure was not reported");
			require(
				error.find("RESOURCE_EXHAUSTED") != std::string::npos,
				"synthetic registration failure lost its host result");
			require(
				clients.size() == 3,
				"partially registered synthetic clients lost their owners");
			require(
				s_fixtureHost.pages.size() == 4,
				"failure injection did not stop at the configuration page");

			const auto& callback = s_fixtureHost.pages.front();
			require(callback.draw && callback.userData,
				"retained navigation callback is incomplete");
			callback.draw(callback.userData);
			require(
				s_fixtureHost.sectionHeaderDraws == 1 &&
					s_fixtureHost.bulletTextDraws == 2,
				"retained callback owner was not usable after registration failure");
			s_fixtureHost.Reset();
		});

		runner.test("synthetic settings actions remain in memory", [] {
			DmuiTestFixtures::SyntheticSettingsState state;
			auto page = DmuiTestFixtures::MakeSyntheticSettingsPage(&state);
			require(page.groups.size() == 2, "synthetic settings groups changed");
			require(page.groups[0].settings.size() == 3,
				"synthetic general settings changed");
			require(page.groups[1].settings.size() == 3,
				"synthetic performance settings changed");
			require(
				page.actions.reset && page.actions.revert && page.actions.apply,
				"synthetic settings actions are incomplete");

			auto& enabled = page.groups[0].settings[0];
			require(
				std::get<bool>(enabled.binding.get()),
				"synthetic Boolean did not read its draft value");
			const auto applied = enabled.binding.set(dmui::SettingValue{ false });
			require(!std::get<bool>(applied), "synthetic Boolean write was not returned");
			require(!state.draft.enabled, "synthetic Boolean did not update memory");
			require(enabled.isDirty(), "synthetic Boolean did not become dirty");
			require(enabled.isModified(), "synthetic Boolean did not become modified");

			auto& preset = page.groups[0].settings[1];
			const auto selected =
				preset.binding.set(dmui::SettingValue{ std::string{ "Performance" } });
			require(
				std::get<std::string>(selected) == "Performance" &&
					state.draft.preset == "Performance",
				"synthetic choice did not update memory");

			auto& workers = page.groups[1].settings[0];
			const auto workerCount =
				workers.binding.set(dmui::SettingValue{ int64_t{ 12 } });
			require(
				std::get<int64_t>(workerCount) == 12 &&
					state.draft.workerThreads == 12,
				"synthetic integer did not update memory");

			page.actions.revert();
			require(state.draft == state.committed,
				"synthetic Revert did not restore committed values");
			page.actions.reset();
			require(state.draft == state.defaults,
				"synthetic Reset did not restore defaults");
			page.actions.apply();
			require(state.committed == state.defaults,
				"synthetic Apply did not commit in-memory values");
		});
	}
}
