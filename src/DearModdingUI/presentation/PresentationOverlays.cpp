#include <DearModdingUI/presentation/PresentationServices.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <REX/REX.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <cstdio>
#include <mutex>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace DearModdingUI::PresentationServices
{
	namespace
	{
		[[nodiscard]] bool IsResizingOverlay(ImGuiWindow* a_window) noexcept
		{
			if (!a_window || GImGui->ActiveIdWindow != a_window || GImGui->ActiveId == 0 ||
				!ImGui::IsMouseDown(ImGuiMouseButton_Left))
				return false;
			for (int index = 0; index < 4; ++index)
				if (GImGui->ActiveId == ImGui::GetWindowResizeCornerID(a_window, index) ||
					GImGui->ActiveId == ImGui::GetWindowResizeBorderID(a_window, static_cast<ImGuiDir>(index)))
					return true;
			return false;
		}

		struct OverlayEntry
		{
			std::string key;
			DMUI_ClientHandle owner{ DMUI_INVALID_CLIENT_HANDLE };
			DMUI_PageHandle page{ DMUI_INVALID_PAGE_HANDLE };
			DMUI_ManagedOverlayOptions options{};
			DMUI_Vec2 position{};
			DMUI_Vec2 pivot{};
			DMUI_ManagedOverlayPlacement placement{};
			DMUI_Vec2 pendingSize{};
			bool configured{};
			bool positionPending{};
			bool sizePending{};
			bool arrangementInProgress{};
			bool focused{};
		};

		struct OverlayService
		{
			struct SavedPlacement
			{
				DMUI_OverlayAnchor anchor{ DMUI_OVERLAY_ANCHOR_TOP_LEFT };
				DMUI_Vec2 offset{};
				DMUI_Vec2 size{};
				bool hasOffset{};
				bool hasSize{};
			};
			std::mutex mutex;
			std::vector<OverlayEntry> overlays;
			std::map<std::string, SavedPlacement> saved;
		};

		[[nodiscard]] OverlayService& GetOverlayService() noexcept
		{
			static OverlayService service;
			return service;
		}

		[[nodiscard]] OverlayEntry* FindOverlay(
			OverlayService& a_service,
			DMUI_PageHandle a_page) noexcept
		{
			const auto found = std::ranges::find(
				a_service.overlays, a_page, &OverlayEntry::page);
			return found == a_service.overlays.end() ? nullptr : &*found;
		}

		[[nodiscard]] std::string OverlayKey(std::string_view a_client, std::string_view a_page)
		{
			if (a_client.empty() || a_page.empty())
				return {};
			constexpr char hex[]{ "0123456789abcdef" };
			std::string key;
			for (const auto part : { a_client, a_page })
			{
				if (!key.empty())
					key += '/';
				for (const unsigned char byte : part)
				{
					key += hex[byte >> 4];
					key += hex[byte & 15];
				}
			}
			return key;
		}

		void ApplyOverlayDefaults(OverlayEntry& a_overlay) noexcept
		{
			a_overlay.placement.anchor = a_overlay.options.anchor;
			a_overlay.placement.offset = a_overlay.options.offset;
			a_overlay.pendingSize = a_overlay.options.size;
			a_overlay.positionPending = a_overlay.sizePending = true;
			a_overlay.arrangementInProgress = false;
		}

		void ApplySavedPlacement(const OverlayService& a_service, OverlayEntry& a_overlay) noexcept
		{
			const auto saved = a_service.saved.find(a_overlay.key);
			if (saved == a_service.saved.end() || !saved->second.hasSize)
				return;
			if (saved->second.anchor == DMUI_OVERLAY_ANCHOR_FREE &&
				a_overlay.options.anchor == DMUI_OVERLAY_ANCHOR_FREE &&
				saved->second.hasOffset)
			{
				a_overlay.placement.offset = saved->second.offset;
				a_overlay.positionPending = true;
			}
			a_overlay.pendingSize = saved->second.size;
			a_overlay.sizePending = true;
		}

		[[nodiscard]] ImVec2 ResolveOverlaySize(
			const DMUI_ManagedOverlayOptions& a_options,
			float a_scale) noexcept
		{
			const auto resolve = [a_scale](float a_size, float a_minimum) {
				return a_size > 0.0f ? a_size : a_minimum * a_scale;
			};
			return {
				resolve(a_options.size.x, a_options.minimumSize.x),
				resolve(a_options.size.y, a_options.minimumSize.y)
			};
		}

		[[nodiscard]] constexpr bool IsOverlayAnchor(DMUI_OverlayAnchor a_anchor) noexcept
		{
			return a_anchor <= DMUI_OVERLAY_ANCHOR_NORMALIZED;
		}

		struct OverlayPosition
		{
			ImVec2 position;
			ImVec2 pivot;
		};

		// Corners are normalized placements whose offset insets from the edges they touch.
		[[nodiscard]] OverlayPosition ResolveOverlayPosition(
			const DMUI_ManagedOverlayOptions& a_options,
			DMUI_Vec2 a_position,
			DMUI_Vec2 a_pivot,
			ImVec2 a_viewport,
			float a_scale) noexcept
		{
			constexpr DMUI_Vec2 corners[]{ { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 }, { 0, 0 } };
			auto offset = a_options.offset;
			if (a_options.anchor != DMUI_OVERLAY_ANCHOR_NORMALIZED)
			{
				a_position = a_pivot = corners[a_options.anchor];
				offset = {
					a_pivot.x == 1.0f ? -offset.x : offset.x,
					a_pivot.y == 1.0f ? -offset.y : offset.y
				};
			}
			return {
				{ a_viewport.x * a_position.x + offset.x * a_scale,
					a_viewport.y * a_position.y + offset.y * a_scale },
				{ a_pivot.x, a_pivot.y }
			};
		}
	}

	void RegisterOverlaySettings() noexcept
	{
		if (ImGui::FindSettingsHandler("DMUIOverlay"))
			return;
		auto& service = GetOverlayService();
		{
			const std::scoped_lock lock{ service.mutex };
			service.saved.clear();
			for (auto& overlay : service.overlays)
			{
				overlay.placement = {};
				ApplyOverlayDefaults(overlay);
			}
		}
		ImGuiSettingsHandler handler{};
		handler.TypeName = "DMUIOverlay";
		handler.TypeHash = ImHashStr(handler.TypeName);
		handler.ReadInitFn = [](ImGuiContext*, ImGuiSettingsHandler*) {
			auto& service = GetOverlayService();
			const std::scoped_lock lock{ service.mutex };
			service.saved.clear();
		};
		handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler*, const char* a_name) -> void* {
			try
			{
				auto& service = GetOverlayService();
				const std::scoped_lock lock{ service.mutex };
				return &service.saved[a_name];
			}
			catch (...)
			{
				REX::ERROR("DearModdingUI: overlay settings could not be read");
				return nullptr;
			}
		};
		handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void* a_entry, const char* a_line) {
			auto& entry = *static_cast<OverlayService::SavedPlacement*>(a_entry);
			unsigned int anchor{};
			if (sscanf_s(a_line, "Anchor=%u", &anchor) == 1 && IsOverlayAnchor(anchor))
				entry.anchor = anchor;
			DMUI_Vec2 value{};
			if (sscanf_s(a_line, "Offset=%f,%f", &value.x, &value.y) == 2 &&
				std::isfinite(value.x) && std::isfinite(value.y))
			{
				entry.offset = value;
				entry.hasOffset = true;
			}
			if (sscanf_s(a_line, "Size=%f,%f", &value.x, &value.y) == 2 &&
				std::isfinite(value.x) && std::isfinite(value.y) && value.x > 0 && value.y > 0)
			{
				entry.size = value;
				entry.hasSize = true;
			}
		};
		handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler*, ImGuiTextBuffer* a_buffer) {
			auto& service = GetOverlayService();
			const std::scoped_lock lock{ service.mutex };
			for (const auto& [key, entry] : service.saved)
				if (entry.hasSize)
				{
					a_buffer->appendf("[DMUIOverlay][%s]\nAnchor=%u\n",
						key.c_str(), entry.anchor);
					if (entry.anchor == DMUI_OVERLAY_ANCHOR_FREE && entry.hasOffset)
						a_buffer->appendf("Offset=%.9g,%.9g\n", entry.offset.x, entry.offset.y);
					a_buffer->appendf("Size=%.9g,%.9g\n\n", entry.size.x, entry.size.y);
				}
		};
		handler.ApplyAllFn = [](ImGuiContext*, ImGuiSettingsHandler*) {
			auto& service = GetOverlayService();
			const std::scoped_lock lock{ service.mutex };
			for (auto& overlay : service.overlays)
				ApplySavedPlacement(service, overlay);
		};
		ImGui::AddSettingsHandler(&handler);
		// Ready callbacks configure overlays before ImGui's first NewFrame loads settings.
		if (!GImGui->SettingsLoaded && ImGui::GetIO().IniFilename)
			ImGui::LoadIniSettingsFromDisk(ImGui::GetIO().IniFilename);
	}

	DMUI_Result ConfigureOverlay(
		DMUI_ClientHandle a_client,
		DMUI_PageHandle a_page,
		const DMUI_ManagedOverlayOptions* a_options,
		std::string_view a_clientId,
		std::string_view a_pageId) noexcept
	{
		if (!a_options || a_options->anchor == DMUI_OVERLAY_ANCHOR_NORMALIZED)
			return DMUI_RESULT_INVALID_ARGUMENT;
		const DMUI_ManagedOverlayNormalizedOptions options{ .options = *a_options };
		return ConfigureOverlay(a_client, a_page, &options, a_clientId, a_pageId);
	}

	DMUI_Result ConfigureOverlay(
		DMUI_ClientHandle a_client,
		DMUI_PageHandle a_page,
		const DMUI_ManagedOverlayNormalizedOptions* a_options,
		std::string_view a_clientId,
		std::string_view a_pageId) noexcept
	{
		if (!a_options || a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_page == DMUI_INVALID_PAGE_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto& options = a_options->options;
		const auto normalized = options.anchor == DMUI_OVERLAY_ANCHOR_NORMALIZED;
		const auto position = normalized ? a_options->position : DMUI_Vec2{};
		const auto pivot = normalized ? a_options->pivot : DMUI_Vec2{};
		// Range checks also reject NaN.
		const auto fraction = [](DMUI_Vec2 a_value) {
			return a_value.x >= 0.0f && a_value.x <= 1.0f && a_value.y >= 0.0f && a_value.y <= 1.0f;
		};
		if (!fraction(position) || !fraction(pivot) ||
			!IsOverlayAnchor(options.anchor) ||
			!std::isfinite(options.offset.x) ||
			!std::isfinite(options.offset.y) ||
			!std::isfinite(options.size.x) ||
			!std::isfinite(options.size.y) ||
			!std::isfinite(options.minimumSize.x) ||
			!std::isfinite(options.minimumSize.y) ||
			!std::isfinite(options.maximumSize.x) ||
			!std::isfinite(options.maximumSize.y) ||
			!std::isfinite(options.opacity) ||
			!std::isfinite(options.contentScale) ||
			options.opacity < 0.0f ||
			options.opacity > 1.0f ||
			options.contentScale < 0.5f ||
			options.contentScale > 3.0f ||
			options.size.x < 0.0f ||
			options.size.y < 0.0f ||
			options.minimumSize.x < 0.0f ||
			options.minimumSize.y < 0.0f ||
			(options.maximumSize.x > 0.0f &&
				options.maximumSize.x < options.minimumSize.x) ||
			(options.maximumSize.y > 0.0f &&
				options.maximumSize.y < options.minimumSize.y))
			return DMUI_RESULT_INVALID_ARGUMENT;
		try
		{
			auto& service = GetOverlayService();
			const std::scoped_lock lock{ service.mutex };
			auto* overlay = FindOverlay(service, a_page);
			if (overlay && overlay->owner != a_client)
				return DMUI_RESULT_PAGE_NOT_FOUND;
			if (!overlay)
			{
				const auto key = OverlayKey(a_clientId, a_pageId);
				service.overlays.push_back({});
				overlay = &service.overlays.back();
				overlay->owner = a_client;
				overlay->page = a_page;
				overlay->key = key;
			}
			const auto positionChanged = !overlay->configured ||
				overlay->options.anchor != options.anchor ||
				overlay->position.x != position.x || overlay->position.y != position.y ||
				overlay->pivot.x != pivot.x || overlay->pivot.y != pivot.y ||
				overlay->options.offset.x != options.offset.x ||
				overlay->options.offset.y != options.offset.y;
			const auto sizeChanged = !overlay->configured ||
				overlay->options.size.x != options.size.x ||
				overlay->options.size.y != options.size.y;
			if (positionChanged)
				overlay->placement.offset = options.offset;
			if (sizeChanged)
				overlay->pendingSize = options.size;
			const auto firstConfigure = !overlay->configured;
			overlay->options = options;
			overlay->position = position;
			overlay->pivot = pivot;
			if (firstConfigure)
				ApplySavedPlacement(service, *overlay);
			overlay->positionPending |= positionChanged;
			overlay->sizePending |= sizeChanged;
			overlay->configured = true;
			overlay->placement.anchor = options.anchor;
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	DMUI_Result ResetOverlay(
		DMUI_ClientHandle a_client,
		DMUI_PageHandle a_page,
		std::string_view a_clientId,
		std::string_view a_pageId) noexcept
	{
		if (a_client == DMUI_INVALID_CLIENT_HANDLE || a_page == DMUI_INVALID_PAGE_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		try
		{
			auto& service = GetOverlayService();
			const std::scoped_lock lock{ service.mutex };
			auto* overlay = FindOverlay(service, a_page);
			if (overlay && overlay->owner != a_client)
				return DMUI_RESULT_PAGE_NOT_FOUND;
			const auto key = overlay ? overlay->key : OverlayKey(a_clientId, a_pageId);
			if (service.saved.erase(key) && ImGui::GetCurrentContext())
				ImGui::MarkIniSettingsDirty();
			if (overlay)
				ApplyOverlayDefaults(*overlay);
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	DMUI_Result QueryOverlay(
		DMUI_ClientHandle a_client,
		DMUI_PageHandle a_page,
		DMUI_ManagedOverlayPlacement* a_placement) noexcept
	{
		if (!a_placement || a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_page == DMUI_INVALID_PAGE_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		auto& service = GetOverlayService();
		const std::scoped_lock lock{ service.mutex };
		const auto* overlay = FindOverlay(service, a_page);
		if (!overlay || overlay->owner != a_client || !overlay->configured)
			return DMUI_RESULT_PAGE_NOT_FOUND;
		*a_placement = overlay->placement;
		return DMUI_RESULT_OK;
	}

	ManagedOverlayBeginResult BeginManagedOverlay(
		DMUI_ClientHandle a_client,
		DMUI_PageHandle a_page,
		std::string_view a_label,
		ManagedOverlayInput a_input) noexcept
	{
		DMUI_ManagedOverlayOptions options{};
		DMUI_Vec2 normalizedPosition{};
		DMUI_Vec2 normalizedPivot{};
		DMUI_ManagedOverlayPlacement previous{};
		bool positionPending{};
		bool sizePending{};
		bool wasFocused{};
		const auto focused = a_input == ManagedOverlayInput::kFocused ||
			a_input == ManagedOverlayInput::kFocusGranted;
		{
			auto& service = GetOverlayService();
			const std::scoped_lock lock{ service.mutex };
			auto* overlay = FindOverlay(service, a_page);
			if (!overlay || overlay->owner != a_client || !overlay->configured)
				return ManagedOverlayBeginResult::kNotConfigured;
			options = overlay->options;
			normalizedPosition = overlay->position;
			normalizedPivot = overlay->pivot;
			previous = overlay->placement;
			options.offset = previous.offset;
			positionPending = std::exchange(overlay->positionPending, false);
			sizePending = std::exchange(overlay->sizePending, false);
			if (sizePending)
				options.size = overlay->pendingSize;
			wasFocused = std::exchange(overlay->focused, focused);
		}

		const auto& io = ImGui::GetIO();
		const auto hostScale = io.FontGlobalScale > 0.0f ?
			io.FontGlobalScale :
			1.0f;
		const auto scale = hostScale * options.contentScale;
		const auto viewport = io.DisplaySize;
		const ImVec2 minimum{
			options.minimumSize.x > 0.0f ?
				options.minimumSize.x * scale :
				0.0f,
			options.minimumSize.y > 0.0f ?
				options.minimumSize.y * scale :
				0.0f
		};
		const ImVec2 maximum{
			options.maximumSize.x > 0.0f ?
				options.maximumSize.x * scale :
				(std::numeric_limits<float>::max)(),
			options.maximumSize.y > 0.0f ?
				options.maximumSize.y * scale :
				(std::numeric_limits<float>::max)()
		};
		auto expectedSize = sizePending || previous.changeGeneration == 0 ?
			ResolveOverlaySize(options, scale) :
			ImVec2{ previous.size.x, previous.size.y };
		const auto clamp = [](float a_value, float a_minimum, float a_maximum) {
			return a_value > 0.0f ? std::clamp(a_value, a_minimum, a_maximum) : 0.0f;
		};
		expectedSize = {
			clamp(expectedSize.x, minimum.x, maximum.x),
			clamp(expectedSize.y, minimum.y, maximum.y)
		};
		const auto [position, pivot] = ResolveOverlayPosition(
			options, normalizedPosition, normalizedPivot, viewport, hostScale);
		const auto anchored = options.anchor != DMUI_OVERLAY_ANCHOR_FREE;
		const std::string windowLabel{
			a_label.empty() ? "Managed overlay" : a_label
		};
		// Let ImGui own resize geometry until release, then restore the final anchor.
		const auto resizing = IsResizingOverlay(ImGui::FindWindowByName(windowLabel.c_str()));
		if ((anchored && !resizing) || (!anchored && positionPending) || previous.changeGeneration == 0)
			ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
		if (sizePending)
			ImGui::SetNextWindowSize(expectedSize, ImGuiCond_Always);
		else if (options.minimumSize.x > 0.0f || options.minimumSize.y > 0.0f)
			ImGui::SetNextWindowSize(ResolveOverlaySize(options, scale), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(minimum, maximum);
		ImGui::SetNextWindowBgAlpha(options.opacity);
		const auto arrangementEnabled =
			a_input != ManagedOverlayInput::kPassive && options.allowArrangement;
		auto flags =
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav;
		if (!arrangementEnabled)
			flags |= focused ?
				ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize :
				ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
		// A click on empty space clears window focus; keep keyboard input with the focused overlay.
		if (a_input == ManagedOverlayInput::kFocusGranted || (focused && !GImGui->NavWindow))
			ImGui::SetNextWindowFocus();
		if (anchored)
			flags |= ImGuiWindowFlags_NoMove;
		if (!options.backgroundVisible)
			flags |= ImGuiWindowFlags_NoBackground;
		if (!options.borderVisible)
			flags |= ImGuiWindowFlags_NoDecoration;
		const auto opened = ImGui::Begin(windowLabel.c_str(), nullptr, flags);
		ImGui::SetWindowFontScale(options.contentScale);
		if (wasFocused && !focused)
		{
			// A shell opened over a lost focus must not keep typing into this overlay.
			auto* window = ImGui::GetCurrentWindow();
			if (GImGui->ActiveIdWindow && GImGui->ActiveIdWindow->RootWindow == window)
				ImGui::ClearActiveID();
			if (GImGui->NavWindow && GImGui->NavWindow->RootWindow == window)
				ImGui::FocusWindow(nullptr);
		}

		const auto currentPosition = ImGui::GetWindowPos();
		const auto currentSize = ImGui::GetWindowSize();
		auto& service = GetOverlayService();
		const std::scoped_lock lock{ service.mutex };
		auto* overlay = FindOverlay(service, a_page);
		if (overlay)
		{
			const auto changed =
				overlay->placement.position.x != currentPosition.x ||
				overlay->placement.position.y != currentPosition.y ||
				overlay->placement.size.x != currentSize.x ||
				overlay->placement.size.y != currentSize.y;
			overlay->placement.position = {
				currentPosition.x,
				currentPosition.y
			};
			overlay->placement.size = { currentSize.x, currentSize.y };
			if (!anchored)
				overlay->placement.offset = {
					currentPosition.x / hostScale,
					currentPosition.y / hostScale
				};
			overlay->placement.visible = 1u;
			if (changed)
				++overlay->placement.changeGeneration;
			if (arrangementEnabled && changed &&
				ImGui::IsMouseDown(ImGuiMouseButton_Left))
				overlay->arrangementInProgress = true;
			overlay->placement.arrangementCompleted =
				arrangementEnabled &&
				overlay->arrangementInProgress &&
				ImGui::IsMouseReleased(ImGuiMouseButton_Left) ?
				1u :
				0u;
			if (overlay->placement.arrangementCompleted)
			{
				overlay->arrangementInProgress = false;
				if (!overlay->key.empty())
				{
					try
					{
						service.saved[overlay->key] = {
							options.anchor, anchored ? DMUI_Vec2{} : overlay->placement.offset,
							overlay->placement.size, !anchored, true
						};
						ImGui::MarkIniSettingsDirty();
					}
					catch (...)
					{
						REX::ERROR("DearModdingUI: overlay arrangement could not be retained");
					}
				}
			}
		}
		return opened ?
			ManagedOverlayBeginResult::kVisible :
			ManagedOverlayBeginResult::kHidden;
	}

	void EndManagedOverlay() noexcept
	{
		ImGui::End();
	}
}
