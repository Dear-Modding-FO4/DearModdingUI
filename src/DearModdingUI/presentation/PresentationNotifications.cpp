#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/presentation/Theme.h>
#include <DearModdingUI/controls/FieldFeedback.h>
#include <DearModdingUI/host/Registry.h>
#include <Support/BoundedString.h>
#include "PresentationServiceOwners.h"
#include "notifications/Queue.h"

#include <imgui/imgui.h>
#include <REX/REX.h>

#include <array>
#include <cstdio>
#include <cmath>
#include <mutex>
#include <ranges>

namespace DearModdingUI::PresentationServices
{
	namespace
	{
		struct NotificationService
		{
			std::mutex mutex;
			Notifications::Queue queue;
		};

		[[nodiscard]] NotificationService& GetNotificationService() noexcept
		{
			static NotificationService service;
			return service;
		}

		struct ToastLayout
		{
			float height;
			float nameHeight;
			float titleHeight;
			float messageHeight;
			float nameWidth;
		};

		[[nodiscard]] ToastLayout Measure(
			const Notifications::Toast& a_toast, float a_width, float a_maximumHeight)
		{
			const auto gap = ImGui::GetStyle().ItemSpacing.y;
			const auto fontSize = ImGui::GetFontSize();
			const auto nameWidth = a_width - (a_toast.count > 1 ? fontSize * 7.0f : 0.0f);
			float name{};
			{
				const Theme::FontGuard font{ Theme::FontRole::kSubtext };
				name = (std::min)(
					ImGui::CalcTextSize(a_toast.clientName.c_str(), nullptr, false, nameWidth).y,
					fontSize * 3.0f);
			}
			float title{};
			if (!a_toast.title.empty())
			{
				const Theme::FontGuard font{ Theme::FontRole::kSubheading };
				title = (std::min)(
					ImGui::CalcTextSize(a_toast.title.c_str(), nullptr, false, a_width).y,
					ImGui::GetFontSize() * 3.0f) + gap;
			}
			const auto available = (std::max)(fontSize, a_maximumHeight - name - gap - title);
			const auto message = (std::min)(
				ImGui::CalcTextSize(a_toast.message.c_str(), nullptr, false, a_width).y, available);
			return { name + gap + title + message, name, title, message, nameWidth };
		}

		template <class Callback>
		void ForEachToast(const Notifications::Queue& a_queue, Callback&& a_callback)
		{
			const auto* viewport = ImGui::GetMainViewport();
			const auto fontSize = ImGui::GetFontSize();
			const auto padding = fontSize * 0.75f;
			const auto width = (std::min)(fontSize * 25.0f, viewport->WorkSize.x - fontSize * 2.0f);
			const auto textWidth = width - padding * 2.0f;
			const auto right = viewport->WorkPos.x + viewport->WorkSize.x - fontSize;
			auto y = viewport->WorkPos.y + viewport->WorkSize.y - fontSize;
			for (const auto& toast : a_queue.Visible() | std::views::reverse)
			{
				const auto layout = Measure(toast, textWidth,
					viewport->WorkSize.y - fontSize * 2.0f - padding * 2.0f);
				const auto height = layout.height + padding * 2.0f;
				const ImVec2 minimum{ right - width, y - height };
				const ImVec2 maximum{ right, y };
				if (minimum.y < viewport->WorkPos.y + fontSize)
					continue;
				a_callback(toast, layout, minimum, maximum, textWidth, padding);
				y -= height + ImGui::GetStyle().ItemSpacing.y;
			}
		}

		void DrawText(
			ImDrawList& a_draw, const std::string& a_text, ImVec2 a_position,
			ImU32 a_color, float a_width, float a_height)
		{
			const auto fontSize = ImGui::GetFontSize();
			const auto overflow = ImGui::CalcTextSize(a_text.c_str(), nullptr, false, a_width).y > a_height;
			const auto height = overflow ?
				(std::max)(0.0f, std::floor(a_height / fontSize) - 1.0f) * fontSize : a_height;
			a_draw.PushClipRect(a_position, { a_position.x + a_width, a_position.y + height }, true);
			a_draw.AddText(ImGui::GetFont(), fontSize, a_position, a_color, a_text.c_str(), nullptr, a_width);
			a_draw.PopClipRect();
			if (overflow)
				a_draw.AddText({ a_position.x, a_position.y + height }, a_color, "...");
		}

		[[nodiscard]] ImVec4 SeverityColor(DMUI_StatusSeverity a_severity) noexcept
		{
			switch (a_severity)
			{
			case DMUI_STATUS_SEVERITY_INFO:
				return FieldFeedback::SeverityColor(DMUI_FIELD_FEEDBACK_SEVERITY_INFO);
			case DMUI_STATUS_SEVERITY_WARNING:
				return FieldFeedback::SeverityColor(DMUI_FIELD_FEEDBACK_SEVERITY_WARNING);
			case DMUI_STATUS_SEVERITY_ERROR:
				return FieldFeedback::SeverityColor(DMUI_FIELD_FEEDBACK_SEVERITY_ERROR);
			default:
				return Theme::StatusTextColor(a_severity);
			}
		}
	}

	DMUI_Result PostNotification(
		const Registry& a_registry,
		DMUI_ClientHandle a_client,
		const DMUI_NotificationDescriptor* a_descriptor) noexcept
	{
		if (!a_descriptor || a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_descriptor->severity > DMUI_STATUS_SEVERITY_ERROR)
			return DMUI_RESULT_INVALID_ARGUMENT;
		try
		{
			Notifications::Toast toast;
			const auto ownerResult = a_registry.CopyClientDisplayName(a_client, toast.clientName);
			if (ownerResult != DMUI_RESULT_OK)
				return ownerResult;
			toast.owner = a_client;
			toast.severity = a_descriptor->severity;
			if (!Internal::CopyBoundedString(a_descriptor->message, 1024, false, toast.message) ||
				!Internal::CopyBoundedString(a_descriptor->title, 256, true, toast.title))
				return DMUI_RESULT_INVALID_DESCRIPTOR;
			toast.duration = std::chrono::milliseconds{
				(std::clamp)(a_descriptor->durationMilliseconds ?
					a_descriptor->durationMilliseconds : 5000u, 250u, 30000u)
			};
			auto& service = GetNotificationService();
			const std::scoped_lock lock{ service.mutex };
			if (service.queue.Post(std::move(toast)))
				REX::DEBUG("DearModdingUI: notification queue full; dropped oldest queued toast");
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	void DrawNotifications(bool a_cursorActive) noexcept
	{
		try
		{
			auto& service = GetNotificationService();
			const std::scoped_lock lock{ service.mutex };
			const auto now = Notifications::Clock::now();
			const auto& style = ImGui::GetStyle();
			const auto fontSize = ImGui::GetFontSize();
			uint64_t hovered{};
			if (a_cursorActive)
				ForEachToast(service.queue, [&](const auto& a_toast, const auto&, ImVec2 a_minimum,
					ImVec2 a_maximum, float, float) {
					if (ImGui::IsMouseHoveringRect(a_minimum, a_maximum, false))
						hovered = a_toast.id;
				});
			service.queue.Advance(now, hovered);
			auto* draw = ImGui::GetForegroundDrawList();
			ForEachToast(service.queue, [&](const auto& toast, const auto& layout, ImVec2 minimum,
				ImVec2 maximum, float textWidth, float padding) {
				auto background = style.Colors[ImGuiCol_WindowBg];
				// Keep underlying menu text from competing with passive toast text.
				background.w = (std::max)(background.w, 0.96f);
				draw->AddRectFilled(minimum, maximum, ImGui::GetColorU32(background), style.WindowRounding);
				draw->AddRect(minimum, maximum, ImGui::GetColorU32(ImGuiCol_Border), style.WindowRounding,
					0, style.WindowBorderSize);
				const auto accent = ImGui::GetColorU32(SeverityColor(toast.severity));
				draw->AddRectFilled({ minimum.x, minimum.y + padding },
					{ minimum.x + fontSize * 0.18f, maximum.y - padding }, accent, fontSize * 0.09f);
				ImVec2 position{ minimum.x + padding, minimum.y + padding };
				{
					const Theme::FontGuard font{ Theme::FontRole::kSubtext };
					DrawText(*draw, toast.clientName, position, accent, layout.nameWidth, layout.nameHeight);
				}
				position.y += layout.nameHeight + style.ItemSpacing.y;
				if (!toast.title.empty())
				{
					const Theme::FontGuard font{ Theme::FontRole::kSubheading };
					DrawText(*draw, toast.title, position, ImGui::GetColorU32(ImGuiCol_Text),
						textWidth, layout.titleHeight - style.ItemSpacing.y);
					position.y += layout.titleHeight;
				}
				// A dimmer body keeps the title as the toast's first read.
				DrawText(*draw, toast.message, position,
					ImGui::GetColorU32(ImGuiCol_Text, toast.title.empty() ? 1.0f : 0.78f),
					textWidth, layout.messageHeight);
				if (toast.count > 1)
				{
					std::array<char, 16> count{};
					std::snprintf(count.data(), count.size(), "x%u", toast.count);
					const auto size = ImGui::CalcTextSize(count.data());
					const ImVec2 badge{ maximum.x - padding - size.x, minimum.y + padding };
					draw->AddRectFilled({ badge.x - 4.0f, badge.y - 2.0f },
						{ maximum.x - padding + 4.0f, badge.y + size.y + 2.0f },
						ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);
					draw->AddText(badge, ImGui::GetColorU32(ImGuiCol_Text), count.data());
				}
				service.queue.Presented(toast.id, now);
			});
		}
		catch (...)
		{}
	}

	namespace Notifications
	{
		void Clear() noexcept
		{
			auto& service = GetNotificationService();
			const std::scoped_lock lock{ service.mutex };
			service.queue = {};
		}

		bool HasFrameDemand() noexcept
		{
			auto& service = GetNotificationService();
			const std::scoped_lock lock{ service.mutex };
			return !service.queue.Empty();
		}

#if defined(DMUI_UI_TESTING)
		Queue Snapshot()
		{
			auto& service = GetNotificationService();
			const std::scoped_lock lock{ service.mutex };
			return service.queue;
		}
#endif
	}
}
