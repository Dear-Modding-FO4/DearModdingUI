#include <DearModdingUI/host/Registry.h>
#include "RegistryCallbackDispatch.h"

#include <algorithm>
#include <limits>
#include <new>
#include <tuple>

namespace DearModdingUI
{

	bool Registry::Freeze() noexcept
	{
		try
		{
			const std::scoped_lock lock{ m_mutex };
			if (!m_open)
				return false;
			std::ranges::sort(m_pages, [&](const auto& a_left, const auto& a_right) {
				if (std::tie(a_left.clientDisplayName, a_left.clientId) !=
					std::tie(a_right.clientDisplayName, a_right.clientId))
					return std::tie(a_left.clientDisplayName, a_left.clientId) <
						std::tie(a_right.clientDisplayName, a_right.clientId);
				if (a_left.categoryId.empty() != a_right.categoryId.empty())
					return a_left.categoryId.empty();
				if (!a_left.categoryId.empty())
				{
					const auto* leftCategory =
						FindCategory(a_left.client, a_left.categoryId);
					const auto* rightCategory =
						FindCategory(a_right.client, a_right.categoryId);
					if (std::tie(
							leftCategory->sortKey,
							leftCategory->displayName,
							leftCategory->id) !=
						std::tie(
							rightCategory->sortKey,
							rightCategory->displayName,
							rightCategory->id))
						return std::tie(
								   leftCategory->sortKey,
								   leftCategory->displayName,
								   leftCategory->id) <
							std::tie(
								   rightCategory->sortKey,
								   rightCategory->displayName,
								   rightCategory->id);
				}
				return std::tie(
						   a_left.sortKey,
						   a_left.displayName,
						   a_left.id) <
					std::tie(
						   a_right.sortKey,
						   a_right.displayName,
						   a_right.id);
			});
			std::ranges::sort(m_actions, [](const auto& a_left, const auto& a_right) {
				return std::tie(a_left.client, a_left.sortKey, a_left.id) <
					std::tie(a_right.client, a_right.sortKey, a_right.id);
			});
			m_navigation = BuildNavigationModel(
				m_clients,
				m_categories,
				m_pages,
				m_actions);
			m_open = false;
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool Registry::IsOpen() const noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		return m_open;
	}

	bool Registry::Empty() const noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		return m_clients.empty();
	}

	size_t Registry::ClientCount() const noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		return m_clients.size();
	}

	size_t Registry::PageCount() const noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		return m_pages.size();
	}

	size_t Registry::DemandedOverlayCount() const noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		return static_cast<size_t>(std::ranges::count_if(m_pages, [](const auto& a_page) {
			return a_page.kind == DMUI_PAGE_KIND_OVERLAY &&
				a_page.frameDemand != 0 &&
				!a_page.callbackFailed;
		}));
	}

	bool Registry::HasSettingsPages() const noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		return std::ranges::any_of(m_pages, [](const auto& a_page) {
			return a_page.kind == DMUI_PAGE_KIND_SETTINGS;
		});
	}

	const std::vector<RegisteredClient>& Registry::RegisteredClients() const noexcept
	{
		return m_clients;
	}

	const std::vector<RegisteredCategory>&
		Registry::RegisteredCategories() const noexcept
	{
		return m_categories;
	}

	const std::vector<RegisteredPage>& Registry::OrderedPages() const noexcept
	{
		return m_pages;
	}

	const std::vector<RegisteredAction>& Registry::OrderedActions() const noexcept
	{
		return m_actions;
	}

	const std::vector<RegisteredFrameObserver>&
		Registry::OrderedFrameObservers() const noexcept
	{
		return m_frameObservers;
	}

	bool Registry::HasActiveFrameObservers() const noexcept
	{
		return m_activeFrameObserverCount.load(std::memory_order_acquire) != 0;
	}

	const NavigationModel& Registry::Navigation() const noexcept
	{
		return m_navigation;
	}

	DMUI_Result Registry::RequestFrame(
		DMUI_ClientHandle a_client,
		DMUI_PageHandle a_page) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		if (!OwnsPage(a_client, a_page))
			return FindClient(a_client) ? DMUI_RESULT_PAGE_NOT_FOUND : DMUI_RESULT_CLIENT_NOT_FOUND;
		auto* page = FindPage(a_page);
		if (page->kind != DMUI_PAGE_KIND_OVERLAY)
			return DMUI_RESULT_INVALID_PAGE_KIND;
		if (page->frameDemand == (std::numeric_limits<uint32_t>::max)())
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		++page->frameDemand;
		return DMUI_RESULT_OK;
	}

	DMUI_Result Registry::ReleaseFrame(
		DMUI_ClientHandle a_client,
		DMUI_PageHandle a_page) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		if (!OwnsPage(a_client, a_page))
			return FindClient(a_client) ? DMUI_RESULT_PAGE_NOT_FOUND : DMUI_RESULT_CLIENT_NOT_FOUND;
		auto* page = FindPage(a_page);
		if (page->kind != DMUI_PAGE_KIND_OVERLAY)
			return DMUI_RESULT_INVALID_PAGE_KIND;
		if (!page->frameDemand)
			return DMUI_RESULT_NO_FRAME_DEMAND;
		--page->frameDemand;
		return DMUI_RESULT_OK;
	}

	bool Registry::IsFrameDemanded(DMUI_PageHandle a_page) const noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		const auto* page = FindPage(a_page);
		return page &&
			page->kind == DMUI_PAGE_KIND_OVERLAY &&
			page->frameDemand != 0 &&
			!page->callbackFailed;
	}

	DMUI_Result Registry::ValidatePage(
		DMUI_ClientHandle a_client,
		DMUI_PageHandle a_page,
		DMUI_PageKind a_kind) const noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		if (!FindClient(a_client))
			return DMUI_RESULT_CLIENT_NOT_FOUND;
		const auto* page = FindPage(a_page);
		if (!page || page->client != a_client)
			return DMUI_RESULT_PAGE_NOT_FOUND;
		return page->kind == a_kind ? DMUI_RESULT_OK : DMUI_RESULT_INVALID_PAGE_KIND;
	}

	DMUI_Result Registry::ValidateSwapChainClient(DMUI_ClientHandle a_client) const noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		const auto* client = FindClient(a_client);
		if (!client)
			return DMUI_RESULT_CLIENT_NOT_FOUND;
		return (client->capabilities & DMUI_CLIENT_CAPABILITY_RENDERER_REPLACEMENT) != 0 ?
			DMUI_RESULT_OK :
			DMUI_RESULT_CLIENT_CAPABILITY_REQUIRED;
	}

	DMUI_Result Registry::ValidateClient(DMUI_ClientHandle a_client) const noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		return FindClient(a_client) ? DMUI_RESULT_OK : DMUI_RESULT_CLIENT_NOT_FOUND;
	}

	DMUI_Result Registry::CopyClientDisplayName(
		DMUI_ClientHandle a_client,
		std::string& a_displayName) const noexcept
	{
		try
		{
			const std::scoped_lock lock{ m_mutex };
			const auto* client = FindClient(a_client);
			if (!client)
				return DMUI_RESULT_CLIENT_NOT_FOUND;
			a_displayName = client->displayName;
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	DMUI_Result Registry::InvokePage(DMUI_PageHandle a_page) noexcept
	{
		DMUI_PageDrawCallback callback{ nullptr };
		void* userData{ nullptr };
		{
			const std::scoped_lock lock{ m_mutex };
			auto* page = FindPage(a_page);
			if (!page)
				return DMUI_RESULT_PAGE_NOT_FOUND;
			if (page->callbackFailed)
				return DMUI_RESULT_CALLBACK_FAILED;
			callback = page->draw;
			userData = page->userData;
		}

		const auto result =
			RegistryCallbackDispatch::InvokePage(callback, userData);
		if (result == DMUI_RESULT_OK)
			return DMUI_RESULT_OK;

		const std::scoped_lock lock{ m_mutex };
		if (auto* page = FindPage(a_page))
			page->callbackFailed = true;
		return result;
	}

	bool Registry::PageFailed(DMUI_PageHandle a_page) const noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		const auto* page = FindPage(a_page);
		return !page || page->callbackFailed;
	}

	void Registry::MarkPageFailed(DMUI_PageHandle a_page) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		if (auto* page = FindPage(a_page))
			page->callbackFailed = true;
	}

	DMUI_Result Registry::InvokeAction(DMUI_ActionHandle a_action) noexcept
	{
		DMUI_ActionCallback callback{ nullptr };
		void* userData{ nullptr };
		{
			const std::scoped_lock lock{ m_mutex };
			auto* action = FindAction(a_action);
			if (!action)
				return DMUI_RESULT_ACTION_NOT_FOUND;
			if (action->callbackFailed)
				return DMUI_RESULT_CALLBACK_FAILED;
			callback = action->callback;
			userData = action->userData;
		}

		if (RegistryCallbackDispatch::InvokeAction(callback, userData))
			return DMUI_RESULT_OK;

		const std::scoped_lock lock{ m_mutex };
		if (auto* action = FindAction(a_action))
			action->callbackFailed = true;
		return DMUI_RESULT_CALLBACK_FAILED;
	}

	bool Registry::ActionFailed(DMUI_ActionHandle a_action) const noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		const auto* action = FindAction(a_action);
		return !action || action->callbackFailed;
	}

	void Registry::MarkActionFailed(DMUI_ActionHandle a_action) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		if (auto* action = FindAction(a_action))
			action->callbackFailed = true;
	}

	DMUI_Result Registry::InvokeFrameObserver(
		DMUI_FrameObserverHandle a_observer) noexcept
	{
		DMUI_FrameCallback callback{ nullptr };
		void* userData{ nullptr };
		{
			const std::scoped_lock lock{ m_mutex };
			auto* observer = FindFrameObserver(a_observer);
			if (!observer)
				return DMUI_RESULT_INVALID_ARGUMENT;
			if (observer->callbackFailed)
				return DMUI_RESULT_CALLBACK_FAILED;
			callback = observer->callback;
			userData = observer->userData;
		}

		if (RegistryCallbackDispatch::InvokeAction(callback, userData))
			return DMUI_RESULT_OK;

		MarkFrameObserverFailed(a_observer);
		return DMUI_RESULT_CALLBACK_FAILED;
	}

	void Registry::MarkFrameObserverFailed(
		DMUI_FrameObserverHandle a_observer) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		auto* observer = FindFrameObserver(a_observer);
		if (!observer || observer->callbackFailed)
			return;
		observer->callbackFailed = true;
		m_activeFrameObserverCount.fetch_sub(1, std::memory_order_release);
	}

	void Registry::NotifyPageActivity(
		DMUI_PageHandle a_previousPage,
		DMUI_PageHandle a_activePage) noexcept
	{
		if (a_previousPage == a_activePage)
			return;

		DMUI_ClientHandle previousClient{ DMUI_INVALID_CLIENT_HANDLE };
		DMUI_ClientHandle activeClient{ DMUI_INVALID_CLIENT_HANDLE };
		{
			const std::scoped_lock lock{ m_mutex };
			if (const auto* page = FindPage(a_previousPage))
				previousClient = page->client;
			if (const auto* page = FindPage(a_activePage))
				activeClient = page->client;
		}

		const auto notify = [&](
			DMUI_ClientHandle a_client,
			DMUI_PageActivityKind a_kind,
			DMUI_PageHandle a_previous,
			DMUI_PageHandle a_active) {
			const DMUI_PageActivityInfo info{
				sizeof(DMUI_PageActivityInfo),
				a_kind,
				a_previous,
				a_active
			};
			for (size_t index = 0;; ++index)
			{
				DMUI_PageActivityObserverHandle handle{
					DMUI_INVALID_PAGE_ACTIVITY_OBSERVER_HANDLE
				};
				DMUI_PageActivityCallback callback{ nullptr };
				void* userData{ nullptr };
				{
					const std::scoped_lock lock{ m_mutex };
					if (index >= m_pageActivityObservers.size())
						break;
					const auto& observer = m_pageActivityObservers[index];
					if (observer.client != a_client ||
						observer.callbackFailed)
						continue;
					handle = observer.handle;
					callback = observer.callback;
					userData = observer.userData;
				}
				if (RegistryCallbackDispatch::InvokePageActivity(
						callback, &info, userData))
					continue;
				const std::scoped_lock lock{ m_mutex };
				if (auto* observer = FindPageActivityObserver(handle))
					observer->callbackFailed = true;
			}
		};

		if (previousClient != DMUI_INVALID_CLIENT_HANDLE &&
			previousClient == activeClient)
		{
			notify(
				previousClient,
				DMUI_PAGE_ACTIVITY_CHANGED,
				a_previousPage,
				a_activePage);
			return;
		}
		if (previousClient != DMUI_INVALID_CLIENT_HANDLE)
		{
			notify(
				previousClient,
				DMUI_PAGE_ACTIVITY_DEACTIVATED,
				a_previousPage,
				DMUI_INVALID_PAGE_HANDLE);
		}
		if (activeClient != DMUI_INVALID_CLIENT_HANDLE)
		{
			notify(
				activeClient,
				DMUI_PAGE_ACTIVITY_ACTIVATED,
				DMUI_INVALID_PAGE_HANDLE,
				a_activePage);
		}
	}

	void Registry::NotifyReady(const DMUI_HostReadyInfo& a_info) noexcept
	{
		{
			const std::scoped_lock lock{ m_mutex };
			if (m_notification != Notification::kNone)
				return;
			m_notification = Notification::kReady;
		}

		for (;;)
		{
			DMUI_HostReadyCallback callback{ nullptr };
			void* userData{ nullptr };
			DMUI_ClientHandle handle{ DMUI_INVALID_CLIENT_HANDLE };
			{
				const std::scoped_lock lock{ m_mutex };
				const auto found = std::ranges::find_if(m_clients, [](const auto& a_client) {
					return !a_client.notified;
				});
				if (found == m_clients.end())
					return;
				found->notified = true;
				callback = found->onHostReady;
				userData = found->userData;
				handle = found->handle;
			}

			if (!RegistryCallbackDispatch::InvokeReady(
					callback, &a_info, userData))
			{
				const std::scoped_lock lock{ m_mutex };
				if (auto* client = FindClient(handle))
					client->callbackFailed = true;
				for (auto& page : m_pages)
				{
					if (page.client == handle)
						page.callbackFailed = true;
				}
				for (auto& action : m_actions)
				{
					if (action.client == handle)
						action.callbackFailed = true;
				}
				for (auto& observer : m_frameObservers)
				{
					if (observer.client == handle && !observer.callbackFailed)
					{
						observer.callbackFailed = true;
						m_activeFrameObserverCount.fetch_sub(1, std::memory_order_release);
					}
				}
				for (auto& observer : m_pageActivityObservers)
				{
					if (observer.client == handle)
						observer.callbackFailed = true;
				}
			}
		}
	}

	void Registry::NotifyUnavailable(DMUI_UnavailableReason a_reason) noexcept
	{
		{
			const std::scoped_lock lock{ m_mutex };
			if (m_notification != Notification::kNone)
				return;
			m_notification = Notification::kUnavailable;
		}

		for (;;)
		{
			DMUI_HostUnavailableCallback callback{ nullptr };
			void* userData{ nullptr };
			DMUI_ClientHandle handle{ DMUI_INVALID_CLIENT_HANDLE };
			{
				const std::scoped_lock lock{ m_mutex };
				const auto found = std::ranges::find_if(m_clients, [](const auto& a_client) {
					return !a_client.notified;
				});
				if (found == m_clients.end())
					return;
				found->notified = true;
				callback = found->onHostUnavailable;
				userData = found->userData;
				handle = found->handle;
			}

			if (!RegistryCallbackDispatch::InvokeUnavailable(
					callback, a_reason, userData))
			{
				const std::scoped_lock lock{ m_mutex };
				if (auto* client = FindClient(handle))
					client->callbackFailed = true;
			}
		}
	}

	RegisteredClient* Registry::FindClient(DMUI_ClientHandle a_client) noexcept
	{
		const auto found = std::ranges::find_if(m_clients, [&](const auto& a_existing) {
			return a_existing.handle == a_client;
		});
		return found != m_clients.end() ? &*found : nullptr;
	}

	const RegisteredClient* Registry::FindClient(DMUI_ClientHandle a_client) const noexcept
	{
		const auto found = std::ranges::find_if(m_clients, [&](const auto& a_existing) {
			return a_existing.handle == a_client;
		});
		return found != m_clients.end() ? &*found : nullptr;
	}

	RegisteredPage* Registry::FindPage(DMUI_PageHandle a_page) noexcept
	{
		const auto found = std::ranges::find_if(m_pages, [&](const auto& a_existing) {
			return a_existing.handle == a_page;
		});
		return found != m_pages.end() ? &*found : nullptr;
	}

	const RegisteredPage* Registry::FindPage(DMUI_PageHandle a_page) const noexcept
	{
		const auto found = std::ranges::find_if(m_pages, [&](const auto& a_existing) {
			return a_existing.handle == a_page;
		});
		return found != m_pages.end() ? &*found : nullptr;
	}

	const RegisteredCategory* Registry::FindCategory(
		DMUI_ClientHandle a_client,
		std::string_view a_id) const noexcept
	{
		const auto found = std::ranges::find_if(
			m_categories,
			[&](const auto& a_existing) {
				return a_existing.client == a_client &&
					a_existing.id == a_id;
			});
		return found != m_categories.end() ? &*found : nullptr;
	}

	RegisteredAction* Registry::FindAction(DMUI_ActionHandle a_action) noexcept
	{
		const auto found = std::ranges::find_if(m_actions, [&](const auto& a_existing) {
			return a_existing.handle == a_action;
		});
		return found != m_actions.end() ? &*found : nullptr;
	}

	const RegisteredAction* Registry::FindAction(DMUI_ActionHandle a_action) const noexcept
	{
		const auto found = std::ranges::find_if(m_actions, [&](const auto& a_existing) {
			return a_existing.handle == a_action;
		});
		return found != m_actions.end() ? &*found : nullptr;
	}

	RegisteredFrameObserver* Registry::FindFrameObserver(
		DMUI_FrameObserverHandle a_observer) noexcept
	{
		const auto found = std::ranges::find_if(
			m_frameObservers,
			[&](const auto& a_existing) {
				return a_existing.handle == a_observer;
			});
		return found != m_frameObservers.end() ? &*found : nullptr;
	}

	RegisteredPageActivityObserver* Registry::FindPageActivityObserver(
		DMUI_PageActivityObserverHandle a_observer) noexcept
	{
		const auto found = std::ranges::find_if(
			m_pageActivityObservers,
			[&](const auto& a_existing) {
				return a_existing.handle == a_observer;
			});
		return found != m_pageActivityObservers.end() ? &*found : nullptr;
	}

	bool Registry::OwnsPage(
		DMUI_ClientHandle a_client,
		DMUI_PageHandle a_page) const noexcept
	{
		const auto* page = FindPage(a_page);
		return page && page->client == a_client;
	}
}
