#include "Harness.h"

#include <GeneralTestFixtures.h>
#include <DearModdingUI/UIAdapter.h>

#include <memory>
#include <string>
#include <vector>

namespace
{
	struct CapturedPage
	{
		DMUI_PageDrawCallback draw{};
		void* userData{};
	};

	struct FixtureHost
	{
		std::vector<CapturedPage> pages;
		DMUI_ClientHandle clientCount{};
		size_t registerPageAttempts{};
		size_t failPageRegistrationAt{};
		size_t drawCalls{};
	};

	FixtureHost s_fixtureHost;

	DMUI_Result DMUI_CALL RegisterClient(
		const DMUI_ClientDescriptor* a_descriptor,
		DMUI_ClientHandle* a_client) noexcept
	{
		if (!a_descriptor || !a_client)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_client = ++s_fixtureHost.clientCount;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL RegisterCategory(
		DMUI_ClientHandle,
		const DMUI_CategoryDescriptor* a_descriptor) noexcept
	{
		return a_descriptor ? DMUI_RESULT_OK : DMUI_RESULT_INVALID_ARGUMENT;
	}

	DMUI_Result DMUI_CALL RegisterPage(
		DMUI_ClientHandle,
		const DMUI_PageDescriptor* a_descriptor,
		DMUI_PageHandle* a_page) noexcept
	{
		if (!a_descriptor || !a_page)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (++s_fixtureHost.registerPageAttempts ==
			s_fixtureHost.failPageRegistrationAt)
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		s_fixtureHost.pages.push_back({
			a_descriptor->draw,
			a_descriptor->userData
		});
		*a_page = s_fixtureHost.pages.size();
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL DrawSectionHeader(
		DMUI_ClientHandle,
		const char* a_text,
		uint32_t) noexcept
	{
		if (!a_text)
			return DMUI_RESULT_INVALID_ARGUMENT;
		++s_fixtureHost.drawCalls;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL DrawBulletText(
		DMUI_ClientHandle,
		const char* a_text) noexcept
	{
		return DrawSectionHeader(DMUI_INVALID_CLIENT_HANDLE, a_text, 0);
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
			result.registerCategory = &RegisterCategory;
			result.drawSectionHeader = &DrawSectionHeader;
			result.drawBulletText = &DrawBulletText;
			result.queryUIAPI = [](
				uint32_t a_abi,
				uint32_t a_revision,
				uint32_t a_size,
				DMUI_UIAPIInfo* a_info) noexcept -> DMUI_Result {
				return DearModdingUI::UI::Query(a_abi, a_revision, a_size, a_info);
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
		runner.test("synthetic registration failure retains callback owners", [] {
			s_fixtureHost = {};
			s_fixtureHost.failPageRegistrationAt = 2;
			DmuiTestFixtures::SyntheticSettingsState state;
			std::vector<std::unique_ptr<dmui::Client>> clients;
			std::string error;

			require(
				!DmuiTestFixtures::RegisterSyntheticClients(clients, state, error) &&
					error.find("RESOURCE_EXHAUSTED") != std::string::npos,
				"partial registration did not report its host failure");
			require(!clients.empty() &&
					clients.size() == s_fixtureHost.clientCount &&
					s_fixtureHost.pages.size() == 1,
				"partial registration lost its successful page or client owner");

			const auto& callback = s_fixtureHost.pages.front();
			require(callback.draw && callback.userData,
				"retained navigation callback is incomplete");
			callback.draw(callback.userData);
			require(s_fixtureHost.drawCalls > 0,
				"retained callback owner was not usable after registration failure");
			s_fixtureHost = {};
		});
	}
}
