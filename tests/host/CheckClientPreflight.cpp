#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/UIAdapter.h>
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

	void run_client_preflight_checks(Runner& runner)
	{
		runner.test("client descriptors reject null arguments and callbacks", [] {
			Registry registry;
			CallbackState state;
			DMUI_ClientHandle handle{};
			auto client = Client("sample.mod", "Sample", state);

			require(registry.RegisterClient(nullptr, &handle) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null descriptor was accepted");
			require(registry.RegisterClient(&client, nullptr) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null output was accepted");
			client.onHostReady = nullptr;
			require(registry.RegisterClient(&client, &handle) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"null ready callback was accepted");

			client.onHostReady = &Ready;
			require(registry.RegisterClient(&client, &handle) ==
					DMUI_RESULT_OK,
				"valid client was rejected");
			AddCategory(registry, handle, "general", "General");
			auto page = Page("settings", "Settings", "general", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			DMUI_PageHandle pageHandle{};
			require(registry.RegisterPage(handle, nullptr, &pageHandle) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null page descriptor was accepted");
			require(registry.RegisterPage(handle, &page, nullptr) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null page output was accepted");
			page.kind = 3;
			require(registry.RegisterPage(handle, &page, &pageHandle) ==
					DMUI_RESULT_INVALID_PAGE_KIND,
				"removed page kind was accepted");
			page.kind = DMUI_PAGE_KIND_SETTINGS;
			page.draw = nullptr;
			require(registry.RegisterPage(handle, &page, &pageHandle) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"null page callback was accepted");
		});

		runner.test("client registration copies icon metadata", [] {
			CallbackState state;

			Registry registry;
			DMUI_ClientHandle handle{};

			char iconName[]{ "gauge" };
			auto client = Client("owned.mod", "Owned", state);
			client.iconName = iconName;
			require(registry.RegisterClient(&client, &handle) == DMUI_RESULT_OK,
				"client icon registration failed");
			iconName[0] = 'x';
			AddCategory(registry, handle, "general", "General");
			(void)AddPage(
				registry,
				handle,
				"settings",
				"Settings",
				"general",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state);
			require(registry.Freeze(), "registry with a client icon did not freeze");
			require(
				registry.Navigation().clients.size() == 1 &&
					registry.Navigation().clients.front().iconName == "gauge",
				"the client icon name was not deep-copied");
		});

		runner.test("navigation icons validate and copy descriptor metadata", [] {
			Registry registry;
			CallbackState state;
			auto clientDescriptor =
				Client("icons.mod", "Icon Metadata", state);
			clientDescriptor.iconName = "cloud-sun";
			DMUI_ClientHandle client{};
			require(
				registry.RegisterClient(&clientDescriptor, &client) ==
					DMUI_RESULT_OK,
				"navigation icon client registration failed");

			DMUI_CategoryDescriptor noIconCategory{
				"old",
				"Lighting",
				0,
				0,
				nullptr
			};
			require(
				registry.RegisterCategory(client, &noIconCategory) ==
					DMUI_RESULT_OK,
				"category without an explicit icon was rejected");
			char categoryIcon[]{ "sun-horizon" };
			auto currentCategory = noIconCategory;
			currentCategory.id = "current";
			currentCategory.iconName = categoryIcon;
			require(
				registry.RegisterCategory(client, &currentCategory) ==
					DMUI_RESULT_OK,
				"current category icon was rejected");
			categoryIcon[0] = 'x';

			auto noIconPage = Page(
				"old",
				"Lighting",
				"old",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state,
				nullptr);
			DMUI_PageHandle oldPageHandle{};
			require(
				registry.RegisterPage(client, &noIconPage, &oldPageHandle) ==
					DMUI_RESULT_OK,
				"page without an explicit icon was rejected");
			char pageIcon[]{ "sliders-horizontal" };
			auto currentPage = Page(
				"current",
				"Current",
				"current",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state,
				pageIcon);
			DMUI_PageHandle currentPageHandle{};
			require(
				registry.RegisterPage(
					client,
					&currentPage,
					&currentPageHandle) == DMUI_RESULT_OK,
				"current page icon was rejected");
			pageIcon[0] = 'x';

			std::string oversized(129, 'x');
			auto invalidCategory = currentCategory;
			invalidCategory.id = "oversized";
			invalidCategory.iconName = oversized.c_str();
			require(
				registry.RegisterCategory(client, &invalidCategory) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"oversized category icon name was accepted");
			const char malformed[]{ '\x01', '\0' };
			auto invalidPage = Page(
				"malformed",
				"Malformed",
				"current",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state,
				malformed);
			DMUI_PageHandle invalidPageHandle{};
			require(
				registry.RegisterPage(
					client,
					&invalidPage,
					&invalidPageHandle) == DMUI_RESULT_INVALID_DESCRIPTOR,
				"malformed page icon name was accepted");
			require(registry.Freeze(), "navigation icon registry did not freeze");
			const auto& categories = registry.RegisteredCategories();
			require(
				categories.size() == 2 &&
					categories[0].iconName.empty() &&
					categories[1].iconName == "sun-horizon",
				"category icon default or copy lifetime changed");
			const auto& pages = registry.OrderedPages();
			const auto oldRegisteredPage = std::ranges::find(
				pages,
				"old",
				&RegisteredPage::id);
			const auto currentRegisteredPage = std::ranges::find(
				pages,
				"current",
				&RegisteredPage::id);
			require(
				pages.size() == 2 &&
					oldRegisteredPage != pages.end() &&
					oldRegisteredPage->iconName.empty() &&
					currentRegisteredPage != pages.end() &&
					currentRegisteredPage->iconName ==
						"sliders-horizontal",
				"page icon default or copy lifetime changed");
			const auto& navigation = registry.Navigation().clients.front();
			const auto navigationCategory = std::ranges::find(
				navigation.categories,
				"current",
				&NavigationCategory::id);
			require(
				navigation.iconName == "cloud-sun" &&
					navigationCategory != navigation.categories.end() &&
					navigationCategory->iconName == "sun-horizon" &&
					navigationCategory->pages.front().handle ==
						currentPageHandle &&
					navigationCategory->pages.front().iconName ==
						"sliders-horizontal",
				"navigation model dropped copied icon metadata");
		});













		runner.test("bridged clients carry copied source labels", [] {
			Registry registry;
			CallbackState state;
			char sourceLabel[]{ "MCM" };
			auto client = Client("bridged.mod", "Bridged", state);
			client.origin = DMUI_CLIENT_ORIGIN_BRIDGED;
			client.bridgeSourceLabel = sourceLabel;
			DMUI_ClientHandle handle{};

			require(registry.RegisterClient(&client, &handle) == DMUI_RESULT_OK,
				"a bridged client was rejected");
			sourceLabel[0] = 'X';
			const auto& registered = registry.RegisteredClients().front();
			require(
				registered.origin == DMUI_CLIENT_ORIGIN_BRIDGED &&
					registered.bridgeSourceLabel == "MCM",
				"the bridge source label was not copied");

			auto contradictory =
				Client("native.source", "Native Source", state);
			contradictory.bridgeSourceLabel = "MCM";
			require(
				registry.RegisterClient(&contradictory, &handle) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"a native client carried a bridge source label");
		});

	}
}
