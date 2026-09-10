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
		runner.test("client descriptors reject null size and callback failures", [] {
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
			client.structSize = DMUI_CLIENT_DESCRIPTOR_0_1_SIZE - 1;
			require(registry.RegisterClient(&client, &handle) ==
					DMUI_RESULT_STRUCT_TOO_SMALL,
				"short client descriptor was accepted");
			client.structSize = sizeof(client);
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
			page.structSize = DMUI_PAGE_DESCRIPTOR_0_1_SIZE - 1;
			require(registry.RegisterPage(handle, &page, &pageHandle) ==
					DMUI_RESULT_STRUCT_TOO_SMALL,
				"short page descriptor was accepted");
			page.structSize = sizeof(page);
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

		runner.test("client descriptors require and copy the complete 0.1 shape", [] {
			CallbackState state;

			Registry registry;
			auto shortDescriptor = Client("short.mod", "Short", state);
			shortDescriptor.structSize =
				static_cast<uint32_t>(
					offsetof(DMUI_ClientDescriptor, bridgeSourceLabel));
			DMUI_ClientHandle handle{};
			require(
				registry.RegisterClient(&shortDescriptor, &handle) ==
					DMUI_RESULT_STRUCT_TOO_SMALL,
				"a partial 0.1 client descriptor was accepted");

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

		runner.test("navigation icon descriptor extensions preserve old prefixes", [] {
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

			DMUI_CategoryDescriptor oldCategory{
				DMUI_CATEGORY_DESCRIPTOR_0_1_SIZE,
				"old",
				"Lighting",
				0,
				0,
				"sun-horizon"
			};
			require(
				registry.RegisterCategory(client, &oldCategory) ==
					DMUI_RESULT_OK,
				"old category prefix was rejected");
			char categoryIcon[]{ "sun-horizon" };
			auto currentCategory = oldCategory;
			currentCategory.structSize = DMUI_CATEGORY_DESCRIPTOR_ICON_SIZE;
			currentCategory.id = "current";
			currentCategory.iconName = categoryIcon;
			require(
				registry.RegisterCategory(client, &currentCategory) ==
					DMUI_RESULT_OK,
				"current category icon was rejected");
			categoryIcon[0] = 'x';

			auto oldPage = Page(
				"old",
				"Lighting",
				"old",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state,
				"sun");
			oldPage.structSize = DMUI_PAGE_DESCRIPTOR_0_1_SIZE;
			DMUI_PageHandle oldPageHandle{};
			require(
				registry.RegisterPage(client, &oldPage, &oldPageHandle) ==
					DMUI_RESULT_OK,
				"old page prefix was rejected");
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
				"category icon prefix guards or copy lifetime changed");
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
				"page icon prefix guards or copy lifetime changed");
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

		runner.test("client service requirements fail before registration", [] {
			CallbackState state;
			Registry registry;
			auto descriptor =
				Client("required.mod", "Required", state);
			descriptor.requiredServices =
				DMUI_HOST_SERVICE_IMAGE_RESOURCES |
				DMUI_HOST_SERVICE_DIALOGS |
				DMUI_HOST_SERVICE_PIXEL_IMAGES;
			descriptor.apiVersion = DMUI_MAKE_VERSION(99u, 0u);
			DMUI_ClientHandle handle{};
			require(registry.RegisterClient(&descriptor, &handle) ==
						DMUI_RESULT_OK &&
					handle != DMUI_INVALID_CLIENT_HANDLE,
				"release metadata incorrectly gated client registration");

			Registry unavailable;
			auto unsupported =
				Client("unsupported.mod", "Unsupported", state);
			unsupported.requiredServices = UINT64_C(1) << 63u;
			handle = DMUI_INVALID_CLIENT_HANDLE;
			require(unavailable.RegisterClient(&unsupported, &handle) ==
						DMUI_RESULT_SERVICE_UNAVAILABLE &&
					handle == DMUI_INVALID_CLIENT_HANDLE &&
					unavailable.ClientCount() == 0,
				"unsupported service registered a partial client");

		});

		runner.test("official client preflight rejects incomplete host tables", [] {
			s_mockRegistrations = 0;
			s_mockServices = DMUI_HOST_SERVICE_IMAGE_RESOURCES;
			s_mockUIResult = DMUI_RESULT_OK;
			s_mockUIRevision = DMUI_UI_REVISION_CURRENT;
			s_mockUITableSize = DMUI_UI_API_CURRENT_SIZE;
			auto api = PreflightHostAPI();
			api.queryUIAPI = nullptr;
			const dmui::ClientOptions options{
				.requiredServices = DMUI_HOST_SERVICE_IMAGE_RESOURCES
			};

			api.hostAbiVersion = DMUI_HOST_ABI_CURRENT + 1;
			require(dmui::PreflightHostAPI(&api, options) ==
						DMUI_RESULT_UNSUPPORTED_ABI &&
					s_mockRegistrations == 0,
				"unknown host ABI generation reached client registration");
			api.hostAbiVersion = DMUI_HOST_ABI_CURRENT;
			api.apiVersion = DMUI_MAKE_VERSION(99u, 0u);
			require(dmui::PreflightHostAPI(&api, options) ==
						DMUI_RESULT_SERVICE_UNAVAILABLE &&
					s_mockRegistrations == 0,
				"release metadata incorrectly gated host ABI preflight");
			require(dmui::PreflightHostAPI(&api, options) ==
						DMUI_RESULT_SERVICE_UNAVAILABLE &&
					s_mockRegistrations == 0,
				"missing queryServices reached client registration");
			api.queryServices = &MockQueryServices;
			api.queryUIAPI = &MockQueryUIAPI;
			s_mockServices = DMUI_HOST_SERVICE_NONE;
			require(dmui::PreflightHostAPI(&api, options) ==
						DMUI_RESULT_SERVICE_UNAVAILABLE &&
					s_mockRegistrations == 0,
				"missing semantic service reached client registration");
			s_mockServices = DMUI_HOST_SERVICE_IMAGE_RESOURCES;
			require(dmui::PreflightHostAPI(&api, options) ==
						DMUI_RESULT_SERVICE_UNAVAILABLE &&
					s_mockRegistrations == 0,
				"advertised service with missing functions reached registration");
			api.importD3D11Image = [](DMUI_ClientHandle,
									 const DMUI_D3D11ImageDescriptor*,
									 DMUI_ImageHandle*) noexcept {
				return DMUI_RESULT_OK;
			};
			api.drawImage = &MockDrawImage;
			api.releaseImage = &MockReleaseImage;
			api.queryImage = &MockQueryImage;
			api.queryUIAPI = nullptr;
			require(dmui::PreflightHostAPI(&api, options) ==
						DMUI_RESULT_UNSUPPORTED_ABI &&
					s_mockRegistrations == 0,
				"missing stable UI query reached client registration");
			api.queryUIAPI = &MockQueryUIAPI;
			require(dmui::PreflightHostAPI(&api, options) == DMUI_RESULT_OK,
				"compatible host ABI failed with different release metadata");
			s_mockUIRevision = 0u;
			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_UNSUPPORTED_ABI,
				"old stable UI revision reached client registration");
			s_mockUIRevision = DMUI_UI_REVISION_CURRENT;
			s_mockUITableSize = DMUI_UI_API_REQUIRED_SIZE - 1u;
			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_UNSUPPORTED_ABI,
				"short required stable UI table reached client registration");
			s_mockUITableSize = DMUI_UI_API_REQUIRED_SIZE;
			require(dmui::PreflightHostAPI(&api, options) == DMUI_RESULT_OK,
				"missing optional UI tail incorrectly blocked connection");
			s_mockMissingRequiredUIOperation = true;
			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_UNSUPPORTED_ABI,
				"missing mandatory Begin/End pair operation reached registration");
			s_mockMissingRequiredUIOperation = false;
		});

		runner.test("official client preflight enforces requested UI tail operations", [] {
			s_mockUIResult = DMUI_RESULT_OK;
			s_mockUIRevision = DMUI_UI_REVISION_CURRENT;
			s_mockUITableSize = DMUI_UI_API_CURRENT_SIZE;
			s_mockMissingRequiredUIOperation = false;
			s_mockMissingPlotLines = true;
			auto api = PreflightHostAPI();

			const dmui::ClientOptions baseline{};
			require(
				dmui::PreflightHostAPI(&api, baseline) == DMUI_RESULT_OK,
				"missing optional UI tail blocked a baseline client");

			const dmui::ClientOptions plotLines{
				.minimumUIAPISize = DMUI_UI_API_PLOT_LINES_SIZE
			};
			require(
				dmui::PreflightHostAPI(&api, plotLines) ==
					DMUI_RESULT_UNSUPPORTED_ABI,
				"requested PlotLines tail accepted a null operation");

			s_mockMissingPlotLines = false;
			require(
				dmui::PreflightHostAPI(&api, plotLines) == DMUI_RESULT_OK,
				"available requested PlotLines tail failed preflight");
		});

		runner.test("navigation icon preflight requires page and category entries", [] {
			s_mockServices = DMUI_HOST_SERVICE_NAVIGATION_ICONS;
			auto api = PreflightHostAPI();
			api.queryServices = &MockQueryServices;
			const dmui::ClientOptions options{
				.requiredServices = DMUI_HOST_SERVICE_NAVIGATION_ICONS
			};
			require(
				dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_SERVICE_UNAVAILABLE,
				"navigation icon preflight accepted missing registration entries");
			api.registerPage = &MockRegisterPage;
			require(
				dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_SERVICE_UNAVAILABLE,
				"navigation icon preflight omitted category registration");
			api.registerCategory = &MockRegisterCategory;
			require(
				dmui::PreflightHostAPI(&api, options) == DMUI_RESULT_OK,
				"complete navigation icon surface failed preflight");
			api.structSize = DMUI_HOST_API_REGISTER_CATEGORY_SIZE - 1;
			require(
				dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_UNSUPPORTED_ABI,
				"truncated host table exposed the appended UI query");
		});

		runner.test("pixel-image preflight requires create update and shared entries", [] {
			s_mockServices = DMUI_HOST_SERVICE_PIXEL_IMAGES;
			auto api = PreflightHostAPI();
			api.queryServices = &MockQueryServices;
			const dmui::ClientOptions options{
				.requiredServices = DMUI_HOST_SERVICE_PIXEL_IMAGES
			};

			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_SERVICE_UNAVAILABLE,
				"advertised pixel images omitted all required entries");
			api.createImage = &MockCreateImage;
			api.updateImage = &MockUpdateImage;
			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_SERVICE_UNAVAILABLE,
				"pixel image preflight omitted shared handle entries");
			api.drawImage = &MockDrawImage;
			api.releaseImage = &MockReleaseImage;
			api.queryImage = &MockQueryImage;
			require(dmui::PreflightHostAPI(&api, options) == DMUI_RESULT_OK,
				"complete pixel image surface failed preflight");
			api.updateImage = nullptr;
			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_SERVICE_UNAVAILABLE,
				"pixel image preflight accepted a missing update entry");

			const dmui::ClientOptions unknown{
				.requiredServices = UINT64_C(1) << 63u
			};
			require(dmui::PreflightHostAPI(&api, unknown) ==
					DMUI_RESULT_SERVICE_UNAVAILABLE,
				"unknown service bit passed official preflight");
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
