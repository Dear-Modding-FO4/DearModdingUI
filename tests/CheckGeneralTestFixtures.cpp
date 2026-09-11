#include "Harness.h"

#include <GeneralTestFixtures.h>
#include <DearModdingUI/host/UIAdapter.h>

#include <cstdint>
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
		size_t resolveCalls{};
		size_t collapsingHeaderCalls{};
		size_t separatorCalls{};
		size_t settingsTableCalls{};
		DMUI_Result resolveResult{ DMUI_RESULT_OK };
		uint32_t resolvedGlyph{};
		std::string resolvedExplicitName;
		std::string resolvedPrimaryMetadata;
		std::string resolvedSecondaryMetadata;
		std::vector<uint32_t> drawnGlyphs;
		std::vector<std::string> drawnLabels;
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

	DMUI_Result DMUI_CALL ResolveIconGlyph(
		const DMUI_IconResolutionRequest* a_request,
		uint32_t* a_glyph) noexcept
	{
		++s_fixtureHost.resolveCalls;
		if (!a_request || !a_glyph)
			return DMUI_RESULT_INVALID_ARGUMENT;
		s_fixtureHost.resolvedExplicitName =
			a_request->explicitName ? a_request->explicitName : "";
		s_fixtureHost.resolvedPrimaryMetadata =
			a_request->primaryMetadata ? a_request->primaryMetadata : "";
		s_fixtureHost.resolvedSecondaryMetadata =
			a_request->secondaryMetadata ? a_request->secondaryMetadata : "";
		*a_glyph = s_fixtureHost.resolveResult == DMUI_RESULT_OK ?
			s_fixtureHost.resolvedGlyph :
			0;
		return s_fixtureHost.resolveResult;
	}

	DMUI_Result DMUI_CALL DrawCollapsingSectionHeader(
		DMUI_ClientHandle,
		const char*,
		const char* a_text,
		uint32_t a_glyph,
		uint32_t* a_expanded,
		size_t) noexcept
	{
		if (!a_text || !a_expanded)
			return DMUI_RESULT_INVALID_ARGUMENT;
		++s_fixtureHost.collapsingHeaderCalls;
		s_fixtureHost.drawnGlyphs.push_back(a_glyph);
		s_fixtureHost.drawnLabels.emplace_back(a_text);
		*a_expanded = 0;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL BeginSettingsTable(
		DMUI_ClientHandle,
		const char* a_id,
		uint32_t* a_visible) noexcept
	{
		if (!a_id || !a_visible)
			return DMUI_RESULT_INVALID_ARGUMENT;
		++s_fixtureHost.settingsTableCalls;
		*a_visible = 0;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL EndSettingsTable(DMUI_ClientHandle) noexcept
	{
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL Separator(DMUI_ClientHandle) noexcept
	{
		++s_fixtureHost.separatorCalls;
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] DMUI_UIAPI& FixtureUIAPI() noexcept
	{
		static auto api = [] {
			auto result = DearModdingUI::UI::API();
			result.separator = &Separator;
			return result;
		}();
		return api;
	}

	DMUI_Result DMUI_CALL QueryUIAPI(
		uint32_t a_abi,
		uint32_t a_revision,
		uint32_t a_size,
		DMUI_UIAPIInfo* a_info) noexcept
	{
		const auto result =
			DearModdingUI::UI::Query(a_abi, a_revision, a_size, a_info);
		if (result == DMUI_RESULT_OK)
			a_info->api = &FixtureUIAPI();
		return result;
	}

	[[nodiscard]] DMUI_HostAPI& FixtureAPI() noexcept
	{
		static auto api = [] {
			DMUI_HostAPI result{};
			result.structSize = sizeof(result);
			result.hostAbiVersion = DMUI_HOST_ABI_CURRENT;
			result.apiVersion = DMUI_API_VERSION_CURRENT;
			result.registerClient = &RegisterClient;
			result.registerPage = &RegisterPage;
			result.registerCategory = &RegisterCategory;
			result.drawSectionHeader = &DrawSectionHeader;
			result.drawBulletText = &DrawBulletText;
			result.drawCollapsingSectionHeader =
				&DrawCollapsingSectionHeader;
			result.beginSettingsTable = &BeginSettingsTable;
			result.endSettingsTable = &EndSettingsTable;
			result.queryUIAPI = &QueryUIAPI;
			result.resolveIconGlyph = &ResolveIconGlyph;
			return result;
		}();
		return api;
	}

	void ResetFixture() noexcept
	{
		s_fixtureHost = {};
		auto& api = FixtureAPI();
		api.structSize = sizeof(api);
		api.resolveIconGlyph = &ResolveIconGlyph;
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
			ResetFixture();
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
			ResetFixture();
		});

		runner.test("client forwards host icon selection without local inference", [] {
			ResetFixture();
			constexpr auto label = "HostOnlyUnmappedLabel";
			require(
				DearModdingUI::ResolveIconSelection({}, label).GlyphOr({}) == 0,
				"test label unexpectedly entered the local icon vocabulary");
			s_fixtureHost.resolvedGlyph = 0x2605u;
			dmui::Client client{
				"icon-forwarding",
				"Icon Forwarding",
				{ 1, 0 }
			};
			require(client.Connect(), "fixture client did not connect");

			const auto glyph =
				client.ResolveIconGlyph(label, "unknown-explicit", "secondary");
			require(
				glyph && *glyph == U'\u2605' &&
					client.LastResult() == DMUI_RESULT_OK &&
					s_fixtureHost.resolveCalls == 1 &&
					s_fixtureHost.resolvedExplicitName == "unknown-explicit" &&
					s_fixtureHost.resolvedPrimaryMetadata == label &&
					s_fixtureHost.resolvedSecondaryMetadata == "secondary",
				"client did not preserve host-selected glyph or request fields");
		});

		runner.test("declarative groups resolve current labels at draw time", [] {
			ResetFixture();
			std::string currentLabel{ "HostOnlyUnmappedFirst" };
			s_fixtureHost.resolvedGlyph = 0x2605u;
			dmui::Client client{
				"declarative-host-icons",
				"Declarative Host Icons",
				{ 1, 0 }
			};
			require(client.Connect(), "fixture client did not connect");
			dmui::SettingsPage settings{
				.groups = {
					{
						.id = "dynamic",
						.label = currentLabel,
						.settings = { { .id = "setting", .label = "Setting" } },
						.expanded = false
					}
				},
				.filterOptions = {
					.showSearch = false,
					.showModifiedOnly = false
				},
				.prepareView = [&](dmui::SettingsPage& a_page) {
					a_page.groups.front().label = currentLabel;
				}
			};
			const auto page = client.AddSettingsPage(
				{ .id = "settings", .displayName = "Settings" },
				std::move(settings));
			require(page && s_fixtureHost.pages.size() == 1,
				"declarative fixture page did not register");

			auto& callback = s_fixtureHost.pages.front();
			require(
				callback.draw(callback.userData) == DMUI_RESULT_OK &&
					s_fixtureHost.resolvedPrimaryMetadata == currentLabel &&
					s_fixtureHost.drawnGlyphs == std::vector<uint32_t>{ 0x2605u },
				"first current label or host glyph was not used");

			currentLabel = "HostOnlyUnmappedSecond";
			s_fixtureHost.resolvedGlyph = 0x2666u;
			require(
				callback.draw(callback.userData) == DMUI_RESULT_OK &&
					s_fixtureHost.resolveCalls == 2 &&
					s_fixtureHost.resolvedPrimaryMetadata == currentLabel &&
					s_fixtureHost.drawnLabels.back() == currentLabel &&
					s_fixtureHost.drawnGlyphs.back() == 0x2666u,
				"changed label did not invoke the current host resolver");
		});

		runner.test("declarative icon fallback and explicit modes preserve intent", [] {
			ResetFixture();
			char32_t currentGlyph{};
			auto headingMode = dmui::SettingGroup::HeadingMode::kAutomatic;
			dmui::Client client{
				"declarative-icon-modes",
				"Declarative Icon Modes",
				{ 1, 0 }
			};
			require(client.Connect(), "fixture client did not connect");
			dmui::SettingsPage settings{
				.groups = {
					{
						.id = "modes",
						.label = "HostOnlyUnmappedModes",
						.settings = { { .id = "setting", .label = "Setting" } },
						.expanded = false
					}
				},
				.filterOptions = {
					.showSearch = false,
					.showModifiedOnly = false
				},
				.prepareView = [&](dmui::SettingsPage& a_page) {
					a_page.groups.front().glyph = currentGlyph;
					a_page.groups.front().headingMode = headingMode;
				}
			};
			require(
				client.AddSettingsPage(
					{ .id = "settings", .displayName = "Settings" },
					std::move(settings))
					.has_value(),
				"declarative fixture page did not register");
			auto& callback = s_fixtureHost.pages.front();

			s_fixtureHost.resolvedGlyph = 0;
			require(
				callback.draw(callback.userData) == DMUI_RESULT_OK &&
					s_fixtureHost.drawnGlyphs.back() ==
						static_cast<uint32_t>(
							DearModdingUI::PhosphorGlyph::kQuestion),
				"successful host no-match did not use Question");

			const std::vector<char32_t> explicitGlyphs{
				U'\u2605',
				DearModdingUI::PhosphorGlyph::kQuestion,
				static_cast<char32_t>(0x110000u)
			};
			const auto resolverCalls = s_fixtureHost.resolveCalls;
			for (const auto glyph : explicitGlyphs)
			{
				currentGlyph = glyph;
				require(
					callback.draw(callback.userData) == DMUI_RESULT_OK &&
						s_fixtureHost.drawnGlyphs.back() ==
							static_cast<uint32_t>(glyph),
					"explicit raw glyph was not forwarded unchanged");
			}
			require(s_fixtureHost.resolveCalls == resolverCalls,
				"explicit glyph invoked automatic resolution");

			currentGlyph = 0;
			headingMode = dmui::SettingGroup::HeadingMode::kDivider;
			const auto headerCalls = s_fixtureHost.collapsingHeaderCalls;
			const auto dividerResult = callback.draw(callback.userData);
			require(
				dividerResult == DMUI_RESULT_OK,
				std::string{ "divider draw failed with " } +
					DMUI_ResultToString(dividerResult));
			require(
				s_fixtureHost.resolveCalls == resolverCalls &&
					s_fixtureHost.collapsingHeaderCalls == headerCalls,
				"divider did not bypass icon resolution and header drawing");
			require(
				s_fixtureHost.separatorCalls == 1,
				"divider did not use the iconless separator path");
			require(
				s_fixtureHost.settingsTableCalls == 1,
				"divider did not continue through the invisible settings table");
		});

		runner.test("declarative icon resolution fails closed on unavailable hosts", [] {
			ResetFixture();
			dmui::Client client{
				"declarative-icon-errors",
				"Declarative Icon Errors",
				{ 1, 0 }
			};
			require(client.Connect(), "fixture client did not connect");
			dmui::SettingsPage settings{
				.groups = {
					{
						.id = "errors",
						.label = "HostOnlyUnmappedErrors",
						.settings = { { .id = "setting", .label = "Setting" } },
						.expanded = false
					}
				},
				.filterOptions = {
					.showSearch = false,
					.showModifiedOnly = false
				}
			};
			require(
				client.AddSettingsPage(
					{ .id = "settings", .displayName = "Settings" },
					std::move(settings))
					.has_value(),
				"declarative fixture page did not register");
			auto& callback = s_fixtureHost.pages.front();

			FixtureAPI().structSize = DMUI_HOST_API_QUERY_UI_API_SIZE;
			require(
				callback.draw(callback.userData) == DMUI_RESULT_UNSUPPORTED_ABI &&
					client.LastResult() == DMUI_RESULT_UNSUPPORTED_ABI &&
					s_fixtureHost.resolveCalls == 0 &&
					s_fixtureHost.collapsingHeaderCalls == 0,
				"short host table read or drew after negotiation failure");

			FixtureAPI().structSize = sizeof(DMUI_HostAPI);
			FixtureAPI().resolveIconGlyph = nullptr;
			require(
				callback.draw(callback.userData) == DMUI_RESULT_UNSUPPORTED_ABI &&
					client.LastResult() == DMUI_RESULT_UNSUPPORTED_ABI &&
					s_fixtureHost.resolveCalls == 0 &&
					s_fixtureHost.collapsingHeaderCalls == 0,
				"null host entry invoked or drew after negotiation failure");

			FixtureAPI().resolveIconGlyph = &ResolveIconGlyph;
			s_fixtureHost.resolveResult = DMUI_RESULT_CALLBACK_FAILED;
			require(
				callback.draw(callback.userData) == DMUI_RESULT_CALLBACK_FAILED &&
					client.LastResult() == DMUI_RESULT_CALLBACK_FAILED &&
					s_fixtureHost.resolveCalls == 1 &&
					s_fixtureHost.collapsingHeaderCalls == 0,
				"host resolver failure was hidden by a fake-success draw");
			ResetFixture();
		});
	}
}
