#include <DearModdingUI/presentation/PresentationServices.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <limits>
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
			DMUI_ClientHandle owner{ DMUI_INVALID_CLIENT_HANDLE };
			DMUI_PageHandle page{ DMUI_INVALID_PAGE_HANDLE };
			DMUI_ManagedOverlayOptions options{};
			DMUI_ManagedOverlayPlacement placement{};
			bool configured{};
			bool positionPending{};
			bool sizePending{};
			bool arrangementInProgress{};
		};

		struct OverlayService
		{
			std::mutex mutex;
			std::vector<OverlayEntry> overlays;
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

		[[nodiscard]] ImVec2 ResolveOverlayPosition(
			const DMUI_ManagedOverlayOptions& a_options,
			ImVec2 a_viewport,
			ImVec2 a_size,
			float a_scale) noexcept
		{
			const ImVec2 offset{
				a_options.offset.x * a_scale,
				a_options.offset.y * a_scale
			};
			switch (a_options.anchor)
			{
			case DMUI_OVERLAY_ANCHOR_TOP_RIGHT:
				return { a_viewport.x - a_size.x - offset.x, offset.y };
			case DMUI_OVERLAY_ANCHOR_BOTTOM_LEFT:
				return { offset.x, a_viewport.y - a_size.y - offset.y };
			case DMUI_OVERLAY_ANCHOR_BOTTOM_RIGHT:
				return {
					a_viewport.x - a_size.x - offset.x,
					a_viewport.y - a_size.y - offset.y
				};
			default:
				return offset;
			}
		}
	}

	DMUI_Result ConfigureOverlay(
		DMUI_ClientHandle a_client,
		DMUI_PageHandle a_page,
		const DMUI_ManagedOverlayOptions* a_options) noexcept
	{
		if (!a_options || a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_page == DMUI_INVALID_PAGE_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_options->anchor > DMUI_OVERLAY_ANCHOR_FREE ||
			!std::isfinite(a_options->offset.x) ||
			!std::isfinite(a_options->offset.y) ||
			!std::isfinite(a_options->size.x) ||
			!std::isfinite(a_options->size.y) ||
			!std::isfinite(a_options->minimumSize.x) ||
			!std::isfinite(a_options->minimumSize.y) ||
			!std::isfinite(a_options->maximumSize.x) ||
			!std::isfinite(a_options->maximumSize.y) ||
			!std::isfinite(a_options->opacity) ||
			!std::isfinite(a_options->contentScale) ||
			a_options->opacity < 0.0f ||
			a_options->opacity > 1.0f ||
			a_options->contentScale < 0.5f ||
			a_options->contentScale > 3.0f ||
			a_options->size.x < 0.0f ||
			a_options->size.y < 0.0f ||
			a_options->minimumSize.x < 0.0f ||
			a_options->minimumSize.y < 0.0f ||
			(a_options->maximumSize.x > 0.0f &&
				a_options->maximumSize.x < a_options->minimumSize.x) ||
			(a_options->maximumSize.y > 0.0f &&
				a_options->maximumSize.y < a_options->minimumSize.y))
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
				service.overlays.push_back({});
				overlay = &service.overlays.back();
				overlay->owner = a_client;
				overlay->page = a_page;
			}
			overlay->positionPending |= !overlay->configured ||
				overlay->options.anchor != a_options->anchor ||
				overlay->options.offset.x != a_options->offset.x ||
				overlay->options.offset.y != a_options->offset.y;
			overlay->sizePending |= !overlay->configured ||
				overlay->options.size.x != a_options->size.x ||
				overlay->options.size.y != a_options->size.y;
			overlay->options = *a_options;
			overlay->configured = true;
			overlay->placement.anchor = a_options->anchor;
			if (overlay->positionPending)
				overlay->placement.offset = a_options->offset;
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
		bool a_menuVisible) noexcept
	{
		DMUI_ManagedOverlayOptions options{};
		DMUI_ManagedOverlayPlacement previous{};
		bool positionPending{};
		bool sizePending{};
		{
			auto& service = GetOverlayService();
			const std::scoped_lock lock{ service.mutex };
			auto* overlay = FindOverlay(service, a_page);
			if (!overlay || overlay->owner != a_client || !overlay->configured)
				return ManagedOverlayBeginResult::kNotConfigured;
			options = overlay->options;
			previous = overlay->placement;
			positionPending = std::exchange(overlay->positionPending, false);
			sizePending = std::exchange(overlay->sizePending, false);
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
		const auto position = ResolveOverlayPosition(options, viewport, expectedSize, hostScale);
		const auto anchored = options.anchor != DMUI_OVERLAY_ANCHOR_FREE;
		const std::string windowLabel{
			a_label.empty() ? "Managed overlay" : a_label
		};
		// Let ImGui own resize geometry until release, then restore the final anchor.
		const auto resizing = IsResizingOverlay(ImGui::FindWindowByName(windowLabel.c_str()));
		if ((anchored && !resizing) || (!anchored && positionPending) || previous.changeGeneration == 0)
			ImGui::SetNextWindowPos(position, ImGuiCond_Always);
		if (sizePending && (options.size.x > 0.0f || options.size.y > 0.0f))
			ImGui::SetNextWindowSize(expectedSize, ImGuiCond_Always);
		else if (options.minimumSize.x > 0.0f || options.minimumSize.y > 0.0f)
			ImGui::SetNextWindowSize(ResolveOverlaySize(options, scale), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(minimum, maximum);
		ImGui::SetNextWindowBgAlpha(options.opacity);
		auto flags =
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav;
		if (!a_menuVisible || !options.allowArrangement)
			flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
		if (anchored)
			flags |= ImGuiWindowFlags_NoMove;
		if (!options.backgroundVisible)
			flags |= ImGuiWindowFlags_NoBackground;
		if (!options.borderVisible)
			flags |= ImGuiWindowFlags_NoDecoration;
		const auto opened = ImGui::Begin(windowLabel.c_str(), nullptr, flags);
		ImGui::SetWindowFontScale(options.contentScale);

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
			const auto arrangementEnabled =
				a_menuVisible &&
				options.allowArrangement;
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
				overlay->arrangementInProgress = false;
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
