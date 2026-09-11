#include <DearModdingUI/host/Registry.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <Support/BoundedString.h>

#include <algorithm>
#include <new>

namespace DearModdingUI
{
	namespace
	{
		inline constexpr size_t kIdCapacity{ 128 };
		inline constexpr size_t kDisplayNameCapacity{ 256 };
		inline constexpr size_t kCategoryIdCapacity{ 128 };
		inline constexpr size_t kSummaryCapacity{ 1024 };
		inline constexpr size_t kIconNameCapacity{ 128 };
		inline constexpr size_t kBridgeSourceLabelCapacity{ 128 };

		[[nodiscard]] bool ValidId(std::string_view a_id) noexcept
		{
			if (a_id.empty())
				return false;
			for (const auto character : a_id)
			{
				const auto alpha =
					(character >= 'a' && character <= 'z') ||
					(character >= 'A' && character <= 'Z');
				const auto digit = character >= '0' && character <= '9';
				if (!alpha && !digit && character != '.' && character != '_' && character != '-')
					return false;
			}
			return true;
		}
	}

	DMUI_Result Registry::RegisterClient(
		const DMUI_ClientDescriptor* a_descriptor,
		DMUI_ClientHandle* a_client) noexcept
	{
		if (!a_descriptor || !a_client)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_client = DMUI_INVALID_CLIENT_HANDLE;
		if (a_descriptor->structSize < DMUI_CLIENT_DESCRIPTOR_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (!a_descriptor->onHostReady || !a_descriptor->onHostUnavailable)
			return DMUI_RESULT_INVALID_DESCRIPTOR;
		if ((a_descriptor->capabilities &
				~DMUI_CLIENT_CAPABILITY_RENDERER_REPLACEMENT) != 0)
			return DMUI_RESULT_INVALID_DESCRIPTOR;
		if (a_descriptor->origin != DMUI_CLIENT_ORIGIN_NATIVE &&
			a_descriptor->origin != DMUI_CLIENT_ORIGIN_BRIDGED)
			return DMUI_RESULT_INVALID_DESCRIPTOR;
		const auto hasServiceRequirements =
			a_descriptor->structSize >= DMUI_CLIENT_DESCRIPTOR_SERVICES_SIZE;
		if (hasServiceRequirements &&
			(a_descriptor->requiredServices &
				~PresentationServices::kSupportedServices) != 0)
			return DMUI_RESULT_SERVICE_UNAVAILABLE;

		try
		{
			RegisteredClient client{};
			client.version = a_descriptor->version;
			client.capabilities = a_descriptor->capabilities;
			client.requiredServices = hasServiceRequirements ?
				a_descriptor->requiredServices :
				DMUI_HOST_SERVICE_NONE;
			client.origin = a_descriptor->origin;
			client.onHostReady = a_descriptor->onHostReady;
			client.onHostUnavailable = a_descriptor->onHostUnavailable;
			client.userData = a_descriptor->userData;
			if (!Internal::CopyBoundedString(
					a_descriptor->id, kIdCapacity, false, client.id) ||
				!Internal::CopyBoundedString(
					a_descriptor->displayName,
					kDisplayNameCapacity,
					false,
					client.displayName) ||
				!Internal::CopyBoundedString(
					a_descriptor->iconName,
					kIconNameCapacity,
					true,
					client.iconName) ||
				!Internal::CopyBoundedString(
					a_descriptor->bridgeSourceLabel,
					kBridgeSourceLabelCapacity,
					true,
					client.bridgeSourceLabel) ||
				!ValidId(client.id) ||
				!Internal::ValidText(client.displayName, false) ||
				!Internal::ValidText(client.iconName, true) ||
				!Internal::ValidText(client.bridgeSourceLabel, true) ||
				(client.origin == DMUI_CLIENT_ORIGIN_NATIVE &&
					!client.bridgeSourceLabel.empty()))
				return DMUI_RESULT_INVALID_DESCRIPTOR;

			const std::scoped_lock lock{ m_mutex };
			if (!m_open)
				return DMUI_RESULT_REGISTRATION_CLOSED;
			if (std::ranges::any_of(m_clients, [&](const auto& a_existing) {
					return a_existing.id == client.id;
				}))
				return DMUI_RESULT_DUPLICATE_CLIENT_ID;
			if (m_nextClient == DMUI_INVALID_CLIENT_HANDLE)
				return DMUI_RESULT_RESOURCE_EXHAUSTED;

			client.handle = m_nextClient++;
			m_clients.push_back(std::move(client));
			*a_client = m_clients.back().handle;
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	DMUI_Result Registry::RegisterPage(
		DMUI_ClientHandle a_client,
		const DMUI_PageDescriptor* a_descriptor,
		DMUI_PageHandle* a_page) noexcept
	{
		if (!a_descriptor || !a_page || a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_page = DMUI_INVALID_PAGE_HANDLE;
		if (a_descriptor->structSize < DMUI_PAGE_DESCRIPTOR_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (!a_descriptor->draw)
			return DMUI_RESULT_INVALID_DESCRIPTOR;
		if (a_descriptor->kind != DMUI_PAGE_KIND_SETTINGS &&
			a_descriptor->kind != DMUI_PAGE_KIND_OVERLAY)
			return DMUI_RESULT_INVALID_PAGE_KIND;

		try
		{
			RegisteredPage page{};
			page.client = a_client;
			page.sortKey = a_descriptor->sortKey;
			page.kind = a_descriptor->kind;
			page.draw = a_descriptor->draw;
			page.userData = a_descriptor->userData;
			const auto* iconName =
				a_descriptor->structSize >= DMUI_PAGE_DESCRIPTOR_ICON_SIZE ?
					a_descriptor->iconName :
					nullptr;
			if (!Internal::CopyBoundedString(
					a_descriptor->id, kIdCapacity, false, page.id) ||
				!Internal::CopyBoundedString(
					a_descriptor->displayName,
					kDisplayNameCapacity,
					false,
					page.displayName) ||
				!Internal::CopyBoundedString(
					a_descriptor->categoryId,
					kCategoryIdCapacity,
					true,
					page.categoryId) ||
				!Internal::CopyBoundedString(
					a_descriptor->summary,
					kSummaryCapacity,
					true,
					page.summary) ||
				!Internal::CopyBoundedString(
					iconName, kIconNameCapacity, true, page.iconName) ||
				!ValidId(page.id) ||
				!Internal::ValidText(page.displayName, false) ||
				(!page.categoryId.empty() && !ValidId(page.categoryId)) ||
				!Internal::ValidText(page.summary, true) ||
				!Internal::ValidText(page.iconName, true))
				return DMUI_RESULT_INVALID_DESCRIPTOR;

			const std::scoped_lock lock{ m_mutex };
			if (!m_open)
				return DMUI_RESULT_REGISTRATION_CLOSED;
			const auto* client = FindClient(a_client);
			if (!client)
				return DMUI_RESULT_CLIENT_NOT_FOUND;
			if (!page.categoryId.empty() &&
				!FindCategory(a_client, page.categoryId))
				return DMUI_RESULT_CATEGORY_NOT_FOUND;
			if (std::ranges::any_of(m_pages, [&](const auto& a_existing) {
					return a_existing.client == a_client && a_existing.id == page.id;
				}))
				return DMUI_RESULT_DUPLICATE_PAGE_ID;
			if (m_nextPage == DMUI_INVALID_PAGE_HANDLE)
				return DMUI_RESULT_RESOURCE_EXHAUSTED;

			page.handle = m_nextPage++;
			page.clientId = client->id;
			page.clientDisplayName = client->displayName;
			page.imguiLabel = page.displayName + "###" + page.clientId + "/" + page.id;
			m_pages.push_back(std::move(page));
			*a_page = m_pages.back().handle;
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	DMUI_Result Registry::RegisterCategory(
		DMUI_ClientHandle a_client,
		const DMUI_CategoryDescriptor* a_descriptor) noexcept
	{
		if (!a_descriptor || a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_descriptor->structSize < DMUI_CATEGORY_DESCRIPTOR_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (a_descriptor->reserved != 0)
			return DMUI_RESULT_INVALID_DESCRIPTOR;

		try
		{
			RegisteredCategory category{};
			category.client = a_client;
			category.sortKey = a_descriptor->sortKey;
			const auto* iconName =
				a_descriptor->structSize >= DMUI_CATEGORY_DESCRIPTOR_ICON_SIZE ?
					a_descriptor->iconName :
					nullptr;
			if (!Internal::CopyBoundedString(
					a_descriptor->id,
					kCategoryIdCapacity,
					false,
					category.id) ||
				!Internal::CopyBoundedString(
					a_descriptor->displayName,
					kDisplayNameCapacity,
					false,
					category.displayName) ||
				!Internal::CopyBoundedString(
					iconName,
					kIconNameCapacity,
					true,
					category.iconName) ||
				!ValidId(category.id) ||
				!Internal::ValidText(category.displayName, false) ||
				!Internal::ValidText(category.iconName, true))
				return DMUI_RESULT_INVALID_DESCRIPTOR;

			const std::scoped_lock lock{ m_mutex };
			if (!m_open)
				return DMUI_RESULT_REGISTRATION_CLOSED;
			const auto* client = FindClient(a_client);
			if (!client)
				return DMUI_RESULT_CLIENT_NOT_FOUND;
			if (FindCategory(a_client, category.id))
				return DMUI_RESULT_DUPLICATE_CATEGORY_ID;

			category.clientId = client->id;
			m_categories.push_back(std::move(category));
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	DMUI_Result Registry::RegisterAction(
		DMUI_ClientHandle a_client,
		const DMUI_ActionDescriptor* a_descriptor,
		DMUI_ActionHandle* a_action) noexcept
	{
		if (!a_descriptor || !a_action || a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_action = DMUI_INVALID_ACTION_HANDLE;
		if (a_descriptor->structSize < DMUI_ACTION_DESCRIPTOR_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (!a_descriptor->callback)
			return DMUI_RESULT_INVALID_DESCRIPTOR;

		try
		{
			RegisteredAction action{};
			action.client = a_client;
			action.sortKey = a_descriptor->sortKey;
			action.callback = a_descriptor->callback;
			action.userData = a_descriptor->userData;
			if (!Internal::CopyBoundedString(
					a_descriptor->id, kIdCapacity, false, action.id) ||
				!Internal::CopyBoundedString(
					a_descriptor->displayLabel,
					kDisplayNameCapacity,
					false,
					action.displayLabel) ||
				!Internal::CopyBoundedString(
					a_descriptor->iconName,
					kIconNameCapacity,
					true,
					action.iconName) ||
				!Internal::CopyBoundedString(
					a_descriptor->tooltip,
					kSummaryCapacity,
					true,
					action.tooltip) ||
				!ValidId(action.id) ||
				!Internal::ValidText(action.displayLabel, false) ||
				!Internal::ValidText(action.iconName, true) ||
				!Internal::ValidText(action.tooltip, true))
				return DMUI_RESULT_INVALID_DESCRIPTOR;

			const std::scoped_lock lock{ m_mutex };
			if (!m_open)
				return DMUI_RESULT_REGISTRATION_CLOSED;
			const auto* client = FindClient(a_client);
			if (!client)
				return DMUI_RESULT_CLIENT_NOT_FOUND;
			if (std::ranges::any_of(m_actions, [&](const auto& a_existing) {
					return a_existing.client == a_client &&
						a_existing.id == action.id;
				}))
				return DMUI_RESULT_DUPLICATE_ACTION_ID;
			if (m_nextAction == DMUI_INVALID_ACTION_HANDLE)
				return DMUI_RESULT_RESOURCE_EXHAUSTED;

			action.handle = m_nextAction++;
			action.clientId = client->id;
			action.clientDisplayName = client->displayName;
			action.iconSelection = ResolveIconSelection(
				action.iconName,
				action.displayLabel);
			m_actions.push_back(std::move(action));
			*a_action = m_actions.back().handle;
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	DMUI_Result Registry::RegisterFrameObserver(
		DMUI_ClientHandle a_client,
		const DMUI_FrameObserverDescriptor* a_descriptor,
		DMUI_FrameObserverHandle* a_observer) noexcept
	{
		if (!a_descriptor || !a_observer || a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_observer = DMUI_INVALID_FRAME_OBSERVER_HANDLE;
		if (a_descriptor->structSize < DMUI_FRAME_OBSERVER_DESCRIPTOR_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (!a_descriptor->callback)
			return DMUI_RESULT_INVALID_DESCRIPTOR;

		try
		{
			RegisteredFrameObserver observer{};
			observer.client = a_client;
			observer.callback = a_descriptor->callback;
			observer.userData = a_descriptor->userData;

			const std::scoped_lock lock{ m_mutex };
			if (!m_open)
				return DMUI_RESULT_REGISTRATION_CLOSED;
			if (!FindClient(a_client))
				return DMUI_RESULT_CLIENT_NOT_FOUND;
			if (m_nextFrameObserver == DMUI_INVALID_FRAME_OBSERVER_HANDLE)
				return DMUI_RESULT_RESOURCE_EXHAUSTED;

			observer.handle = m_nextFrameObserver++;
			m_frameObservers.push_back(observer);
			m_activeFrameObserverCount.fetch_add(1, std::memory_order_release);
			*a_observer = observer.handle;
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	DMUI_Result Registry::RegisterPageActivityObserver(
		DMUI_ClientHandle a_client,
		const DMUI_PageActivityObserverDescriptor* a_descriptor,
		DMUI_PageActivityObserverHandle* a_observer) noexcept
	{
		if (!a_descriptor ||
			!a_observer ||
			a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_observer = DMUI_INVALID_PAGE_ACTIVITY_OBSERVER_HANDLE;
		if (a_descriptor->structSize <
			DMUI_PAGE_ACTIVITY_OBSERVER_DESCRIPTOR_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (!a_descriptor->callback)
			return DMUI_RESULT_INVALID_DESCRIPTOR;

		try
		{
			RegisteredPageActivityObserver observer{};
			observer.client = a_client;
			observer.callback = a_descriptor->callback;
			observer.userData = a_descriptor->userData;

			const std::scoped_lock lock{ m_mutex };
			if (!m_open)
				return DMUI_RESULT_REGISTRATION_CLOSED;
			if (!FindClient(a_client))
				return DMUI_RESULT_CLIENT_NOT_FOUND;
			if (m_nextPageActivityObserver ==
				DMUI_INVALID_PAGE_ACTIVITY_OBSERVER_HANDLE)
				return DMUI_RESULT_RESOURCE_EXHAUSTED;

			observer.handle = m_nextPageActivityObserver++;
			m_pageActivityObservers.push_back(observer);
			*a_observer = observer.handle;
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}
}
