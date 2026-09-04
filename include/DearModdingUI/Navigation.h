#pragma once

#include <DearModdingUI/API.h>

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace DearModdingUI
{
	struct RegisteredClient;
	struct RegisteredPage;
	struct RegisteredAction;

	struct NavigationPage
	{
		DMUI_PageHandle handle{ DMUI_INVALID_PAGE_HANDLE };
		DMUI_ClientHandle client{ DMUI_INVALID_CLIENT_HANDLE };
		std::string id;
		std::string displayName;
		std::string category;
		std::string summary;
		int32_t sortKey{ 0 };
	};

	struct NavigationCategory
	{
		std::string displayName;
		std::vector<NavigationPage> pages;
	};

	struct NavigationClient
	{
		DMUI_ClientHandle handle{ DMUI_INVALID_CLIENT_HANDLE };
		std::string id;
		std::string displayName;
		uint32_t version{ 0 };
		std::vector<NavigationCategory> categories;
		std::string iconName;
	};

	[[nodiscard]] char32_t ResolveNavigationClientIconGlyph(
		const NavigationClient& a_client) noexcept;

	struct NavigationModel
	{
		std::vector<NavigationClient> clients;

		[[nodiscard]] const NavigationClient* FindClient(DMUI_ClientHandle a_client) const noexcept;
		[[nodiscard]] const NavigationClient* FindClientForPage(DMUI_PageHandle a_page) const noexcept;
		[[nodiscard]] const NavigationPage* FindPage(DMUI_PageHandle a_page) const noexcept;
		[[nodiscard]] DMUI_PageHandle FirstPage() const noexcept;
	};

	enum class NavigationItemKind : uint32_t
	{
		kPage,
		kAction,
		kClient
	};

	enum class NavigationMatchQuality : uint32_t
	{
		kSummary = 1,
		kId,
		kCategory,
		kClientDisplayName,
		kDisplayNameSubstring,
		kDisplayNamePrefix,
		kDisplayNameExact
	};

	struct NavigationSearchEntry
	{
		NavigationItemKind kind{ NavigationItemKind::kPage };
		DMUI_ClientHandle client{ DMUI_INVALID_CLIENT_HANDLE };
		DMUI_PageHandle page{ DMUI_INVALID_PAGE_HANDLE };
		DMUI_ActionHandle action{ DMUI_INVALID_ACTION_HANDLE };
		std::string clientId;
		std::string clientDisplayName;
		std::string id;
		std::string displayName;
		std::string iconName;
		std::string category;
		std::string summary;
		int32_t sortKey{ 0 };
	};

	struct NavigationSearchHit
	{
		NavigationSearchEntry entry;
		NavigationMatchQuality match{ NavigationMatchQuality::kSummary };
	};

	inline constexpr size_t kRecentPageCapacity{ 8 };

	enum class HostPageKind : uint32_t
	{
		kHome
	};

	struct HostNavigationPage
	{
		HostPageKind kind{ HostPageKind::kHome };
		std::string_view id;
		std::string_view displayName;
		std::string_view summary;
	};

	inline constexpr HostNavigationPage kHostHomePage{
		HostPageKind::kHome,
		"home",
		"Home",
		"Session overview for the shared menu host and its registered mods."
	};

	struct ClientSelectionState
	{
		DMUI_ClientHandle activeClient{ DMUI_INVALID_CLIENT_HANDLE };
		DMUI_PageHandle activePage{ DMUI_INVALID_PAGE_HANDLE };
		std::string search;
		std::vector<DMUI_PageHandle> recentPages;
		std::optional<HostPageKind> activeHostPage{ HostPageKind::kHome };
	};

	inline void SelectHostPage(
		HostPageKind a_page,
		ClientSelectionState& a_state) noexcept
	{
		a_state.activeHostPage = a_page;
		a_state.activeClient = DMUI_INVALID_CLIENT_HANDLE;
		a_state.activePage = DMUI_INVALID_PAGE_HANDLE;
		a_state.search.clear();
	}

	[[nodiscard]] constexpr size_t ResolvePaletteSelectionIndex(
		size_t a_index,
		size_t a_resultCount,
		bool a_queryChanged) noexcept
	{
		if (a_queryChanged || a_resultCount == 0)
			return 0;
		return a_index < a_resultCount ? a_index : a_resultCount - 1;
	}

	enum class PagePresentation : uint32_t
	{
		kEmpty,
		kContent,
		kFailure
	};

	[[nodiscard]] NavigationModel BuildNavigationModel(
		const std::vector<RegisteredClient>& a_clients,
		const std::vector<RegisteredPage>& a_pages);
	[[nodiscard]] std::vector<NavigationSearchEntry> BuildNavigationSearchIndex(
		const NavigationModel& a_model,
		const std::vector<RegisteredAction>& a_actions);
	[[nodiscard]] std::vector<NavigationSearchHit> SearchNavigation(
		const NavigationModel& a_model,
		const std::vector<RegisteredAction>& a_actions,
		std::string_view a_query);
	[[nodiscard]] DMUI_PageHandle ResolvePageSelection(
		const NavigationModel& a_model,
		DMUI_PageHandle a_requested,
		DMUI_PageHandle a_current,
		bool a_hostPageActive = false) noexcept;
	[[nodiscard]] bool SelectClient(
		const NavigationModel& a_model,
		DMUI_ClientHandle a_client,
		ClientSelectionState& a_state) noexcept;
	void RecordRecentPage(
		const NavigationModel& a_model,
		DMUI_PageHandle a_page,
		ClientSelectionState& a_state,
		size_t a_capacity = kRecentPageCapacity);
	void PruneRecentPages(
		const NavigationModel& a_model,
		ClientSelectionState& a_state);
	[[nodiscard]] DMUI_PageHandle ResolveLandingPage(
		const NavigationClient& a_client) noexcept;
	[[nodiscard]] std::string PageRowLabel(
		const NavigationClient& a_client,
		const NavigationPage& a_page);
	[[nodiscard]] PagePresentation DecidePagePresentation(
		const NavigationPage* a_page,
		bool a_callbackFailed) noexcept;
}
