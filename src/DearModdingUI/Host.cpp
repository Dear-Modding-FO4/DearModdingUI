#define DMUI_HOST_EXPORTS
#include <DearModdingUI/Host.h>
#include <DearModdingUI/HostSettings.h>
#include <DearModdingUI/ImGuiRecovery.h>
#include <DearModdingUI/Shell.h>
#include <DearModdingUI/Theme.h>
#include <Platform/PlatformImgui.h>

#include <REX/REX.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <DearModdingUI/ImGuiFingerprint.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>

#ifndef IMGUI_HAS_DOCK
#error "DearModdingUI requires the pinned Dear ImGui docking build"
#endif

static_assert(IMGUI_VERSION_NUM == DMUI_IMGUI_VERSION_NUM);
static_assert(sizeof(DMUI_IMGUI_UPSTREAM_COMMIT) == 41);

namespace Addictol::DearModdingUI
{
	using namespace std::literals;

	namespace
	{
		struct AllocatorState
		{
			ImGuiMemAllocFunc alloc{ nullptr };
			ImGuiMemFreeFunc free{ nullptr };
			void* userData{ nullptr };
		};

		struct Service
		{
			Service() :
				registry(HostFingerprint())
			{}

			Registry registry;
			std::atomic<DMUI_HostState> state{ DMUI_HOST_STATE_NOT_INITIALIZED };
			std::atomic<DMUI_UnavailableReason> unavailableReason{ DMUI_UNAVAILABLE_NONE };
			std::atomic<DMUI_UnavailableReason> deferredUnavailableReason{ DMUI_UNAVAILABLE_NONE };
			std::atomic<bool> menuVisible{ false };
			std::atomic<DMUI_PageHandle> selectedPage{ DMUI_INVALID_PAGE_HANDLE };
			AllocatorState allocator;
			StatusModel status;
		};

		struct ClientFontPush
		{
			DMUI_ClientHandle client;
			int depth;
		};

		thread_local std::vector<ClientFontPush> s_clientFontPushes;

		[[nodiscard]] Service& GetService() noexcept
		{
			static Service service;
			return service;
		}

		void SetMenuVisibleState(Service& a_service, bool a_visible) noexcept
		{
			a_service.menuVisible.store(a_visible, std::memory_order_release);
			HostSettings::NotifyMenuVisible(a_visible);
		}

		[[nodiscard]] DMUI_Result StateResult(DMUI_HostState a_state) noexcept
		{
			switch (a_state)
			{
			case DMUI_HOST_STATE_NOT_INITIALIZED:
				return DMUI_RESULT_HOST_NOT_INITIALIZED;
			case DMUI_HOST_STATE_READY:
				return DMUI_RESULT_OK;
			case DMUI_HOST_STATE_UNAVAILABLE:
				return GetService().unavailableReason.load(std::memory_order_acquire) ==
						DMUI_UNAVAILABLE_HOST_DISABLED ?
					DMUI_RESULT_HOST_DISABLED :
					DMUI_RESULT_BACKEND_FAILED;
			default:
				return DMUI_RESULT_HOST_NOT_READY;
			}
		}

		[[nodiscard]] DMUI_Result ValidateDrawingClient(
			DMUI_ClientHandle a_client) noexcept
		{
			if (a_client == DMUI_INVALID_CLIENT_HANDLE)
				return DMUI_RESULT_INVALID_ARGUMENT;
			auto& service = GetService();
			const auto state = service.state.load(std::memory_order_acquire);
			if (state != DMUI_HOST_STATE_READY)
				return StateResult(state);
			return service.registry.ValidateClient(a_client);
		}

		[[nodiscard]] constexpr DMUI_Vec4 ToDMUIVec4(const ImVec4& a_color) noexcept
		{
			return { a_color.x, a_color.y, a_color.z, a_color.w };
		}

		[[nodiscard]] void* AllocCpp(size_t a_size, void* a_userData) noexcept
		{
			auto* allocator = static_cast<AllocatorState*>(a_userData);
			try
			{
				return allocator && allocator->alloc ?
					allocator->alloc(a_size, allocator->userData) :
					nullptr;
			}
			catch (...)
			{
				return nullptr;
			}
		}

		[[nodiscard]] void* DMUI_CALL Alloc(size_t a_size, void* a_userData) noexcept
		{
#if defined(_MSC_VER)
			__try
			{
				return AllocCpp(a_size, a_userData);
			}
			__except (1)
			{
				return nullptr;
			}
#else
			return AllocCpp(a_size, a_userData);
#endif
		}

		void FreeCpp(void* a_allocation, void* a_userData) noexcept
		{
			auto* allocator = static_cast<AllocatorState*>(a_userData);
			try
			{
				if (allocator && allocator->free)
					allocator->free(a_allocation, allocator->userData);
			}
			catch (...)
			{}
		}

		void DMUI_CALL Free(void* a_allocation, void* a_userData) noexcept
		{
#if defined(_MSC_VER)
			__try
			{
				FreeCpp(a_allocation, a_userData);
			}
			__except (1)
			{}
#else
			FreeCpp(a_allocation, a_userData);
#endif
		}

		[[nodiscard]] DMUI_Result RegisterClient(
			const DMUI_ClientDescriptor* a_descriptor,
			DMUI_ClientHandle* a_client,
			ClientOrigin a_origin) noexcept
		{
			if (!a_descriptor || !a_client)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_client = DMUI_INVALID_CLIENT_HANDLE;
			auto& service = GetService();
			const auto state = service.state.load(std::memory_order_acquire);
			if (state == DMUI_HOST_STATE_INITIALIZING ||
				state == DMUI_HOST_STATE_READY)
				return DMUI_RESULT_REGISTRATION_CLOSED;
			if (state != DMUI_HOST_STATE_WAITING_FOR_PRESENT)
				return StateResult(state);
			return service.registry.RegisterClient(a_descriptor, a_client, a_origin);
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiRegisterClientCpp(
			const DMUI_ClientDescriptor* a_descriptor,
			DMUI_ClientHandle* a_client) noexcept
		{
			return RegisterClient(a_descriptor, a_client, ClientOrigin::kExternal);
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiRegisterPageCpp(
			DMUI_ClientHandle a_client,
			const DMUI_PageDescriptor* a_descriptor,
			DMUI_PageHandle* a_page) noexcept
		{
			if (!a_descriptor || !a_page || a_client == DMUI_INVALID_CLIENT_HANDLE)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_page = DMUI_INVALID_PAGE_HANDLE;
			auto& service = GetService();
			const auto state = service.state.load(std::memory_order_acquire);
			if (state == DMUI_HOST_STATE_INITIALIZING ||
				state == DMUI_HOST_STATE_READY)
				return DMUI_RESULT_REGISTRATION_CLOSED;
			if (state != DMUI_HOST_STATE_WAITING_FOR_PRESENT)
				return StateResult(state);
			return service.registry.RegisterPage(a_client, a_descriptor, a_page);
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiRegisterActionCpp(
			DMUI_ClientHandle a_client,
			const DMUI_ActionDescriptor* a_descriptor,
			DMUI_ActionHandle* a_action) noexcept
		{
			if (!a_descriptor || !a_action || a_client == DMUI_INVALID_CLIENT_HANDLE)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_action = DMUI_INVALID_ACTION_HANDLE;
			auto& service = GetService();
			const auto state = service.state.load(std::memory_order_acquire);
			if (state == DMUI_HOST_STATE_INITIALIZING ||
				state == DMUI_HOST_STATE_READY)
				return DMUI_RESULT_REGISTRATION_CLOSED;
			if (state != DMUI_HOST_STATE_WAITING_FOR_PRESENT)
				return StateResult(state);
			return service.registry.RegisterAction(a_client, a_descriptor, a_action);
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiRegisterFrameObserverCpp(
			DMUI_ClientHandle a_client,
			const DMUI_FrameObserverDescriptor* a_descriptor,
			DMUI_FrameObserverHandle* a_observer) noexcept
		{
			if (!a_descriptor || !a_observer || a_client == DMUI_INVALID_CLIENT_HANDLE)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_observer = DMUI_INVALID_FRAME_OBSERVER_HANDLE;
			auto& service = GetService();
			const auto state = service.state.load(std::memory_order_acquire);
			if (state == DMUI_HOST_STATE_INITIALIZING ||
				state == DMUI_HOST_STATE_READY)
				return DMUI_RESULT_REGISTRATION_CLOSED;
			if (state != DMUI_HOST_STATE_WAITING_FOR_PRESENT)
				return StateResult(state);
			return service.registry.RegisterFrameObserver(
				a_client,
				a_descriptor,
				a_observer);
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiQueryStateCpp(
			DMUI_HostStateInfo* a_state) noexcept
		{
			if (!a_state)
				return DMUI_RESULT_INVALID_ARGUMENT;
			if (a_state->structSize < sizeof(DMUI_HostStateInfo))
				return DMUI_RESULT_STRUCT_TOO_SMALL;

			auto& service = GetService();
			const auto state = service.state.load(std::memory_order_acquire);
			const auto clientCount = service.registry.ClientCount();
			const auto pageCount = service.registry.PageCount();
			const auto demandedCount = service.registry.DemandedOverlayCount();
			if (clientCount > (std::numeric_limits<uint32_t>::max)() ||
				pageCount > (std::numeric_limits<uint32_t>::max)() ||
				demandedCount > (std::numeric_limits<uint32_t>::max)())
				return DMUI_RESULT_RESOURCE_EXHAUSTED;

			a_state->state = state;
			a_state->unavailableReason =
				service.unavailableReason.load(std::memory_order_acquire);
			a_state->registrationOpen =
				state == DMUI_HOST_STATE_WAITING_FOR_PRESENT && service.registry.IsOpen() ?
				1u :
				0u;
			a_state->clientCount = static_cast<uint32_t>(clientCount);
			a_state->pageCount = static_cast<uint32_t>(pageCount);
			a_state->demandedOverlayCount = static_cast<uint32_t>(demandedCount);
			return StateResult(state);
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiRequestFrameCpp(
			DMUI_ClientHandle a_client,
			DMUI_PageHandle a_page) noexcept
		{
			if (a_client == DMUI_INVALID_CLIENT_HANDLE ||
				a_page == DMUI_INVALID_PAGE_HANDLE)
				return DMUI_RESULT_INVALID_ARGUMENT;
			auto& service = GetService();
			const auto state = service.state.load(std::memory_order_acquire);
			if (state == DMUI_HOST_STATE_NOT_INITIALIZED ||
				state == DMUI_HOST_STATE_UNAVAILABLE)
				return StateResult(state);
			return service.registry.RequestFrame(a_client, a_page);
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiReleaseFrameCpp(
			DMUI_ClientHandle a_client,
			DMUI_PageHandle a_page) noexcept
		{
			if (a_client == DMUI_INVALID_CLIENT_HANDLE ||
				a_page == DMUI_INVALID_PAGE_HANDLE)
				return DMUI_RESULT_INVALID_ARGUMENT;
			auto& service = GetService();
			const auto state = service.state.load(std::memory_order_acquire);
			if (state == DMUI_HOST_STATE_NOT_INITIALIZED ||
				state == DMUI_HOST_STATE_UNAVAILABLE)
				return StateResult(state);
			return service.registry.ReleaseFrame(a_client, a_page);
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiIsMenuVisibleCpp(uint32_t* a_visible) noexcept
		{
			if (!a_visible)
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto state = GetService().state.load(std::memory_order_acquire);
			*a_visible = state == DMUI_HOST_STATE_READY && IsMenuVisible() ? 1u : 0u;
			return StateResult(state);
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiSelectPageCpp(
			DMUI_ClientHandle a_client,
			DMUI_PageHandle a_page) noexcept
		{
			if (a_client == DMUI_INVALID_CLIENT_HANDLE ||
				a_page == DMUI_INVALID_PAGE_HANDLE)
				return DMUI_RESULT_INVALID_ARGUMENT;
			auto& service = GetService();
			const auto state = service.state.load(std::memory_order_acquire);
			if (state != DMUI_HOST_STATE_READY)
				return StateResult(state);
			const auto valid = service.registry.ValidatePage(
				a_client, a_page, DMUI_PAGE_KIND_SETTINGS);
			if (valid != DMUI_RESULT_OK)
				return valid;
			service.selectedPage.store(a_page, std::memory_order_release);
			SetMenuVisibleState(service, true);
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiAttachSwapChainCpp(
			DMUI_ClientHandle a_client,
			void* a_nativeSwapChain) noexcept
		{
			if (a_client == DMUI_INVALID_CLIENT_HANDLE || !a_nativeSwapChain)
				return DMUI_RESULT_INVALID_ARGUMENT;

			auto& service = GetService();
			const auto state = service.state.load(std::memory_order_acquire);
			if (state == DMUI_HOST_STATE_INITIALIZING)
				return DMUI_RESULT_RENDERER_BUSY;
			if (state != DMUI_HOST_STATE_WAITING_FOR_PRESENT &&
				state != DMUI_HOST_STATE_READY)
				return StateResult(state);

			const auto clientResult = service.registry.ValidateSwapChainClient(a_client);
			if (clientResult != DMUI_RESULT_OK)
				return clientResult;
			return PlatformImgui::AttachSwapChain(
					   static_cast<IDXGISwapChain*>(a_nativeSwapChain)) ?
				DMUI_RESULT_OK :
				DMUI_RESULT_SWAPCHAIN_REJECTED;
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiSetStatusCpp(
			DMUI_ClientHandle a_client,
			DMUI_StatusSeverity a_severity,
			const char* a_message) noexcept
		{
			auto& service = GetService();
			std::string owner;
			const auto validation = ValidateStatusRequest(
				service.registry.CopyClientDisplayName(a_client, owner),
				a_severity,
				a_message);
			if (validation != DMUI_RESULT_OK)
				return validation;
			return service.status.Set(
				StatusOwnerKind::kClient,
				owner,
				a_severity,
				a_message);
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiGetThemeColorsCpp(
			DMUI_ClientHandle a_client,
			DMUI_ThemeColors* a_colors) noexcept
		{
			if (!a_colors)
				return DMUI_RESULT_INVALID_ARGUMENT;
			if (a_colors->structSize < DMUI_THEME_COLORS_1_0_SIZE)
				return DMUI_RESULT_STRUCT_TOO_SMALL;
			const auto validation = ValidateDrawingClient(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;

			a_colors->success = ToDMUIVec4(Theme::colors::kSuccess);
			a_colors->warning = ToDMUIVec4(Theme::colors::kWarning);
			a_colors->error = ToDMUIVec4(Theme::colors::kError);
			a_colors->info = ToDMUIVec4(Theme::colors::kInfo);
			a_colors->muted = ToDMUIVec4(Theme::colors::kMuted);
			a_colors->accent = ToDMUIVec4(Theme::colors::Accent());
			a_colors->accentMuted = ToDMUIVec4(Theme::colors::AccentMuted());
			a_colors->statusDisable = ToDMUIVec4(
				Theme::kStatusPaletteDefaults.disable);
			a_colors->statusError = ToDMUIVec4(
				Theme::kStatusPaletteDefaults.error);
			a_colors->statusWarning = ToDMUIVec4(
				Theme::kStatusPaletteDefaults.warning);
			a_colors->statusRestartNeeded = ToDMUIVec4(
				Theme::kStatusPaletteDefaults.restartNeeded);
			a_colors->statusCurrentHotkey = ToDMUIVec4(
				Theme::kStatusPaletteDefaults.currentHotkey);
			a_colors->statusSuccess = ToDMUIVec4(
				Theme::kStatusPaletteDefaults.success);
			a_colors->statusInfo = ToDMUIVec4(
				Theme::kStatusPaletteDefaults.info);
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiPushFontCpp(
			DMUI_ClientHandle a_client,
			DMUI_FontRole a_role) noexcept
		{
			if (a_role >= DMUI_FONT_ROLE_COUNT)
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto validation = ValidateDrawingClient(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			const auto* context = ImGui::GetCurrentContext();
			if (!context)
				return DMUI_RESULT_HOST_NOT_READY;

			try
			{
				s_clientFontPushes.push_back(
					{ a_client, context->FontStack.Size + 1 });
			}
			catch (const std::bad_alloc&)
			{
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			}
			catch (...)
			{
				return DMUI_RESULT_CALLBACK_FAILED;
			}
			if (!Theme::PushFont(static_cast<Theme::FontRole>(a_role)))
			{
				s_clientFontPushes.pop_back();
				return DMUI_RESULT_HOST_NOT_READY;
			}
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiPopFontCpp(
			DMUI_ClientHandle a_client) noexcept
		{
			const auto validation = ValidateDrawingClient(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			const auto* context = ImGui::GetCurrentContext();
			if (!context ||
				s_clientFontPushes.empty() ||
				s_clientFontPushes.back().client != a_client ||
				s_clientFontPushes.back().depth != context->FontStack.Size)
				return DMUI_RESULT_INVALID_ARGUMENT;

			Theme::PopFont();
			s_clientFontPushes.pop_back();
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiDrawSectionHeaderCpp(
			DMUI_ClientHandle a_client,
			const char* a_text,
			uint32_t a_glyph) noexcept
		{
			if (!a_text)
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto validation = ValidateDrawingClient(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			DrawSectionHeader(a_text, static_cast<char32_t>(a_glyph));
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiDrawSearchInputCpp(
			DMUI_ClientHandle a_client,
			const char* a_id,
			const char* a_hint,
			char* a_buffer,
			size_t a_capacity,
			uint32_t* a_changed) noexcept
		{
			if (!a_id || !a_hint || !a_buffer || !a_capacity || !a_changed)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_changed = 0u;

			size_t length = 0;
			while (length < a_capacity && a_buffer[length])
				++length;
			if (length == a_capacity)
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto validation = ValidateDrawingClient(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;

			try
			{
				std::string search{ a_buffer, length };
				DrawSearchInput(a_id, a_hint, search);
				*a_changed = search.size() != length ||
						std::memcmp(search.data(), a_buffer, length) != 0 ?
					1u :
					0u;
				const auto outputLength = (std::min)(search.size(), a_capacity - 1);
				std::memcpy(a_buffer, search.data(), outputLength);
				a_buffer[outputLength] = '\0';
				return DMUI_RESULT_OK;
			}
			catch (const std::bad_alloc&)
			{
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			}
			catch (...)
			{
				return DMUI_RESULT_CALLBACK_FAILED;
			}
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiDrawCollapsingSectionHeaderCpp(
			DMUI_ClientHandle a_client,
			const char* a_key,
			const char* a_text,
			uint32_t a_glyph,
			uint32_t* a_expanded,
			size_t a_count) noexcept
		{
			if (!a_key || !a_text || !a_expanded)
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto validation = ValidateDrawingClient(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;

			auto expanded = *a_expanded != 0;
			DrawCollapsingSectionHeader(
				a_key,
				a_text,
				static_cast<char32_t>(a_glyph),
				expanded,
				a_count);
			*a_expanded = expanded ? 1u : 0u;
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiDrawSettingsActionButtonCpp(
			DMUI_ClientHandle a_client,
			const char* a_id,
			DMUI_Vec2 a_origin,
			DMUI_Vec2 a_size,
			DMUI_SettingsAction a_action,
			const char* a_fallbackLabel,
			const char* a_tooltip,
			uint32_t a_enabled,
			uint32_t* a_pressed) noexcept
		{
			if (!a_id || !a_pressed || a_action > DMUI_SETTINGS_ACTION_APPLY)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_pressed = 0u;
			const auto validation = ValidateDrawingClient(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;

			*a_pressed = DrawSettingsActionButton(
							 a_id,
							 { a_origin.x, a_origin.y },
							 { a_size.x, a_size.y },
							 static_cast<SettingsAction>(a_action),
							 a_fallbackLabel,
							 a_tooltip,
							 a_enabled != 0) ?
				1u :
				0u;
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiSettingsActionButtonWidthCpp(
			DMUI_ClientHandle a_client,
			DMUI_SettingsAction a_action,
			const char* a_fallbackLabel,
			float a_buttonExtent,
			float* a_width) noexcept
		{
			if (!a_width || a_action > DMUI_SETTINGS_ACTION_APPLY)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_width = 0.0f;
			const auto validation = ValidateDrawingClient(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;

			*a_width = SettingsActionButtonWidth(
				static_cast<SettingsAction>(a_action),
				a_fallbackLabel,
				a_buttonExtent);
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiSettingsActionButtonExtentCpp(
			DMUI_ClientHandle a_client,
			float* a_extent) noexcept
		{
			if (!a_extent)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_extent = 0.0f;
			const auto validation = ValidateDrawingClient(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;

			*a_extent = SettingsActionButtonExtent();
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiQueryVideoMemoryCpp(
			DMUI_ClientHandle a_client,
			uint64_t* a_used,
			uint64_t* a_budget) noexcept
		{
			if (!a_used || !a_budget)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_used = 0;
			*a_budget = 0;
			const auto validation = ValidateDrawingClient(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			return PlatformImgui::QueryVideoMemory(*a_used, *a_budget) ?
				DMUI_RESULT_OK :
				DMUI_RESULT_BACKEND_FAILED;
		}

		template <class Function>
		[[nodiscard]] DMUI_Result GuardApiCall(Function&& a_function) noexcept
		{
#if defined(_MSC_VER)
			__try
			{
				return a_function();
			}
			__except (1)
			{
				return DMUI_RESULT_INVALID_ARGUMENT;
			}
#else
			return a_function();
#endif
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiRegisterClient(
			const DMUI_ClientDescriptor* a_descriptor,
			DMUI_ClientHandle* a_client) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiRegisterClientCpp(a_descriptor, a_client);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiRegisterPage(
			DMUI_ClientHandle a_client,
			const DMUI_PageDescriptor* a_descriptor,
			DMUI_PageHandle* a_page) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiRegisterPageCpp(a_client, a_descriptor, a_page);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiRegisterAction(
			DMUI_ClientHandle a_client,
			const DMUI_ActionDescriptor* a_descriptor,
			DMUI_ActionHandle* a_action) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiRegisterActionCpp(a_client, a_descriptor, a_action);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiRegisterFrameObserver(
			DMUI_ClientHandle a_client,
			const DMUI_FrameObserverDescriptor* a_descriptor,
			DMUI_FrameObserverHandle* a_observer) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiRegisterFrameObserverCpp(a_client, a_descriptor, a_observer);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiQueryState(DMUI_HostStateInfo* a_state) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiQueryStateCpp(a_state);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiRequestFrame(
			DMUI_ClientHandle a_client,
			DMUI_PageHandle a_page) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiRequestFrameCpp(a_client, a_page);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiReleaseFrame(
			DMUI_ClientHandle a_client,
			DMUI_PageHandle a_page) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiReleaseFrameCpp(a_client, a_page);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiIsMenuVisible(uint32_t* a_visible) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiIsMenuVisibleCpp(a_visible);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiSelectPage(
			DMUI_ClientHandle a_client,
			DMUI_PageHandle a_page) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiSelectPageCpp(a_client, a_page);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiAttachSwapChain(
			DMUI_ClientHandle a_client,
			void* a_nativeSwapChain) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiAttachSwapChainCpp(a_client, a_nativeSwapChain);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiSetStatus(
			DMUI_ClientHandle a_client,
			DMUI_StatusSeverity a_severity,
			const char* a_message) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiSetStatusCpp(a_client, a_severity, a_message);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiGetThemeColors(
			DMUI_ClientHandle a_client,
			DMUI_ThemeColors* a_colors) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiGetThemeColorsCpp(a_client, a_colors);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiPushFont(
			DMUI_ClientHandle a_client,
			DMUI_FontRole a_role) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiPushFontCpp(a_client, a_role);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiPopFont(
			DMUI_ClientHandle a_client) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiPopFontCpp(a_client);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiDrawSectionHeader(
			DMUI_ClientHandle a_client,
			const char* a_text,
			uint32_t a_glyph) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiDrawSectionHeaderCpp(a_client, a_text, a_glyph);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiDrawSearchInput(
			DMUI_ClientHandle a_client,
			const char* a_id,
			const char* a_hint,
			char* a_buffer,
			size_t a_capacity,
			uint32_t* a_changed) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiDrawSearchInputCpp(
					a_client,
					a_id,
					a_hint,
					a_buffer,
					a_capacity,
					a_changed);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiDrawCollapsingSectionHeader(
			DMUI_ClientHandle a_client,
			const char* a_key,
			const char* a_text,
			uint32_t a_glyph,
			uint32_t* a_expanded,
			size_t a_count) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiDrawCollapsingSectionHeaderCpp(
					a_client,
					a_key,
					a_text,
					a_glyph,
					a_expanded,
					a_count);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiDrawSettingsActionButton(
			DMUI_ClientHandle a_client,
			const char* a_id,
			DMUI_Vec2 a_origin,
			DMUI_Vec2 a_size,
			DMUI_SettingsAction a_action,
			const char* a_fallbackLabel,
			const char* a_tooltip,
			uint32_t a_enabled,
			uint32_t* a_pressed) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiDrawSettingsActionButtonCpp(
					a_client,
					a_id,
					a_origin,
					a_size,
					a_action,
					a_fallbackLabel,
					a_tooltip,
					a_enabled,
					a_pressed);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiSettingsActionButtonWidth(
			DMUI_ClientHandle a_client,
			DMUI_SettingsAction a_action,
			const char* a_fallbackLabel,
			float a_buttonExtent,
			float* a_width) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiSettingsActionButtonWidthCpp(
					a_client,
					a_action,
					a_fallbackLabel,
					a_buttonExtent,
					a_width);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiSettingsActionButtonExtent(
			DMUI_ClientHandle a_client,
			float* a_extent) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiSettingsActionButtonExtentCpp(a_client, a_extent);
			});
		}

		[[nodiscard]] DMUI_Result DMUI_CALL ApiQueryVideoMemory(
			DMUI_ClientHandle a_client,
			uint64_t* a_used,
			uint64_t* a_budget) noexcept
		{
			return GuardApiCall([&]() noexcept {
				return ApiQueryVideoMemoryCpp(a_client, a_used, a_budget);
			});
		}

		template <class InvokeCallback, class DisableCallback>
		[[nodiscard]] bool InvokeClientCallback(
			const char* a_kind,
			uint64_t a_handle,
			InvokeCallback&& a_invoke,
			DisableCallback&& a_disable) noexcept
		{
			auto recovery = ImGuiRecoverySnapshot::Capture();
			if (!recovery)
			{
				s_clientFontPushes.clear();
				a_disable();
				REX::ERROR(
					"DearModdingUI: {} callback {} could not be isolated and was disabled"sv,
					a_kind,
					a_handle);
				return false;
			}
			const auto result = a_invoke();
			if (result == DMUI_RESULT_CALLBACK_FAILED)
			{
				recovery->RecoverFailure();
				s_clientFontPushes.clear();
				REX::ERROR(
					"DearModdingUI: {} callback {} failed and was disabled"sv,
					a_kind,
					a_handle);
				return false;
			}
			recovery->RecoverAfterCallback();
			s_clientFontPushes.clear();
			return result == DMUI_RESULT_OK;
		}
	}

	const DMUI_ImGuiFingerprint& HostFingerprint() noexcept
	{
		static const DMUI_ImGuiFingerprint fingerprint = DMUI_MakeImGuiFingerprint();
		return fingerprint;
	}

	const DMUI_HostAPI& HostAPI() noexcept
	{
		static const DMUI_HostAPI api{
			sizeof(DMUI_HostAPI),
			DMUI_API_VERSION_CURRENT,
			&HostFingerprint(),
			&ApiRegisterClient,
			&ApiRegisterPage,
			&ApiQueryState,
			&ApiRequestFrame,
			&ApiReleaseFrame,
			&ApiIsMenuVisible,
			&ApiSelectPage,
			&ApiAttachSwapChain,
			&ApiRegisterAction,
			&ApiSetStatus,
			&ApiGetThemeColors,
			&ApiPushFont,
			&ApiPopFont,
			&ApiDrawSectionHeader,
			&ApiDrawSearchInput,
			&ApiDrawCollapsingSectionHeader,
			&ApiDrawSettingsActionButton,
			&ApiSettingsActionButtonWidth,
			&ApiSettingsActionButtonExtent,
			&ApiRegisterFrameObserver,
			&ApiQueryVideoMemory
		};
		return api;
	}

	void Initialize() noexcept
	{
		auto& service = GetService();
		auto expected = DMUI_HOST_STATE_NOT_INITIALIZED;
		service.state.compare_exchange_strong(
			expected,
			DMUI_HOST_STATE_WAITING_FOR_PRESENT,
			std::memory_order_acq_rel);
	}

	void SetBackendUnavailable(DMUI_UnavailableReason a_reason) noexcept
	{
		auto& service = GetService();
		const auto state = service.state.load(std::memory_order_acquire);
		if (state == DMUI_HOST_STATE_UNAVAILABLE)
			return;
		service.unavailableReason.store(a_reason, std::memory_order_release);
		service.state.store(DMUI_HOST_STATE_UNAVAILABLE, std::memory_order_release);
		SetMenuVisibleState(service, false);
	}

	void DeferBackendUnavailable(DMUI_UnavailableReason a_reason) noexcept
	{
		GetService().deferredUnavailableReason.store(a_reason, std::memory_order_release);
	}

	bool BeginBackendInitialization() noexcept
	{
		auto& service = GetService();
		auto expected = DMUI_HOST_STATE_WAITING_FOR_PRESENT;
		if (!service.state.compare_exchange_strong(
				expected,
				DMUI_HOST_STATE_INITIALIZING,
				std::memory_order_acq_rel))
			return expected == DMUI_HOST_STATE_INITIALIZING ||
				expected == DMUI_HOST_STATE_READY;
		if (!service.registry.Freeze())
		{
			FailBackendInitialization();
			return false;
		}
		if (service.deferredUnavailableReason.load(std::memory_order_acquire) !=
			DMUI_UNAVAILABLE_NONE)
		{
			FailBackendInitialization();
			return false;
		}
		return true;
	}

	void CompleteBackendInitialization(void* a_imguiContext) noexcept
	{
		auto& service = GetService();
		auto expected = DMUI_HOST_STATE_INITIALIZING;
		if (!service.state.compare_exchange_strong(
				expected,
				DMUI_HOST_STATE_READY,
				std::memory_order_acq_rel))
			return;

		ImGui::GetAllocatorFunctions(
			&service.allocator.alloc,
			&service.allocator.free,
			&service.allocator.userData);
		const DMUI_HostReadyInfo info{
			sizeof(DMUI_HostReadyInfo),
			DMUI_API_VERSION_CURRENT,
			a_imguiContext,
			&Alloc,
			&Free,
			&service.allocator
		};
		service.registry.NotifyReady(info);
	}

	void FailBackendInitialization() noexcept
	{
		auto& service = GetService();
		if (service.state.exchange(
				DMUI_HOST_STATE_UNAVAILABLE,
				std::memory_order_acq_rel) == DMUI_HOST_STATE_UNAVAILABLE)
			return;
		service.unavailableReason.store(
			DMUI_UNAVAILABLE_BACKEND_FAILED,
			std::memory_order_release);
		SetMenuVisibleState(service, false);
		if (service.registry.IsOpen())
			(void)service.registry.Freeze();
		service.registry.NotifyUnavailable(DMUI_UNAVAILABLE_BACKEND_FAILED);
	}

	bool NeedsFrame() noexcept
	{
		auto& service = GetService();
		return service.menuVisible.load(std::memory_order_acquire) ||
			service.registry.DemandedOverlayCount() != 0;
	}

	bool HasSettingsPages() noexcept
	{
		return GetService().registry.HasSettingsPages();
	}

	bool IsMenuVisible() noexcept
	{
		return GetService().menuVisible.load(std::memory_order_acquire);
	}

	DMUI_Result SetMenuVisible(bool a_visible) noexcept
	{
		auto& service = GetService();
		const auto state = service.state.load(std::memory_order_acquire);
		if (state != DMUI_HOST_STATE_READY)
			return StateResult(state);
		SetMenuVisibleState(service, a_visible);
		return DMUI_RESULT_OK;
	}

	void CloseMenu() noexcept
	{
		SetMenuVisibleState(GetService(), false);
	}

	DMUI_PageHandle SelectedPage() noexcept
	{
		return GetService().selectedPage.load(std::memory_order_acquire);
	}

	void ClearPageSelection(DMUI_PageHandle a_page) noexcept
	{
		auto& selected = GetService().selectedPage;
		selected.compare_exchange_strong(
			a_page,
			DMUI_INVALID_PAGE_HANDLE,
			std::memory_order_acq_rel);
	}

	bool DrawPage(DMUI_PageHandle a_page) noexcept
	{
		auto& service = GetService();
		return InvokeClientCallback(
			"page",
			a_page,
			[&]() noexcept {
				return service.registry.InvokePage(a_page);
			},
			[&]() noexcept {
				service.registry.MarkPageFailed(a_page);
			});
	}

	bool PageFailed(DMUI_PageHandle a_page) noexcept
	{
		return GetService().registry.PageFailed(a_page);
	}

	bool InvokeAction(DMUI_ActionHandle a_action) noexcept
	{
		auto& service = GetService();
		return InvokeClientCallback(
			"action",
			a_action,
			[&]() noexcept {
				return service.registry.InvokeAction(a_action);
			},
			[&]() noexcept {
				service.registry.MarkActionFailed(a_action);
			});
	}

	bool ActionFailed(DMUI_ActionHandle a_action) noexcept
	{
		return GetService().registry.ActionFailed(a_action);
	}

	void ObserveFrame() noexcept
	{
		auto& service = GetService();
		auto& registry = service.registry;
		if (!registry.HasActiveFrameObservers())
			return;
		if (service.state.load(std::memory_order_acquire) != DMUI_HOST_STATE_READY)
			return;
		for (const auto& observer : registry.OrderedFrameObservers())
		{
			if (observer.callbackFailed)
				continue;
			(void)InvokeClientCallback(
				"frame observer",
				observer.handle,
				[&]() noexcept {
					return registry.InvokeFrameObserver(observer.handle);
				},
				[&]() noexcept {
					registry.MarkFrameObserverFailed(observer.handle);
				});
		}
	}

	void DrawDemandedOverlays() noexcept
	{
		auto& registry = GetService().registry;
		for (const auto& page : registry.OrderedPages())
		{
			if (page.kind == DMUI_PAGE_KIND_OVERLAY &&
				registry.IsFrameDemanded(page.handle))
				(void)DrawPage(page.handle);
		}
	}

	const std::vector<RegisteredPage>& OrderedPages() noexcept
	{
		return GetService().registry.OrderedPages();
	}

	const std::vector<RegisteredAction>& OrderedActions() noexcept
	{
		return GetService().registry.OrderedActions();
	}

	const NavigationModel& Navigation() noexcept
	{
		return GetService().registry.Navigation();
	}

	DMUI_Result SetHostStatus(
		DMUI_StatusSeverity a_severity,
		std::string_view a_message) noexcept
	{
		return GetService().status.Set(
			StatusOwnerKind::kHost,
			kHostDisplayName,
			a_severity,
			a_message);
	}

	std::optional<StatusMessage> CurrentStatus() noexcept
	{
		return GetService().status.Snapshot();
	}

	bool DismissStatus(uint64_t a_generation) noexcept
	{
		return GetService().status.Dismiss(a_generation);
	}

	DMUI_Result RegisterInternalClient(
		const DMUI_ClientDescriptor* a_descriptor,
		DMUI_ClientHandle* a_client) noexcept
	{
		return RegisterClient(a_descriptor, a_client, ClientOrigin::kHost);
	}
}

DMUI_EXPORT const DMUI_HostAPI* DMUI_CALL DMUI_GetHostAPI(
	uint32_t a_requestedVersion) noexcept
{
	return Addictol::DearModdingUI::Registry::SupportsVersion(a_requestedVersion) ?
		&Addictol::DearModdingUI::HostAPI() :
		nullptr;
}
