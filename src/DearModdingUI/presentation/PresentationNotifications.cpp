#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/presentation/Theme.h>
#include <Support/BoundedString.h>
#include "PresentationServiceOwners.h"

#include <imgui/imgui.h>

#include <chrono>
#include <mutex>
#include <string>

namespace DearModdingUI::PresentationServices
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		inline constexpr size_t kNotificationCapacity{ 1024 };

		struct Notification
		{
			DMUI_ClientHandle owner{ DMUI_INVALID_CLIENT_HANDLE };
			DMUI_StatusSeverity severity{ DMUI_STATUS_SEVERITY_INFO };
			std::string message;
			Clock::time_point expiresAt{};
			uint64_t generation{};
		};

		struct NotificationService
		{
			std::mutex mutex;
			uint64_t nextGeneration{ 1 };
			Notification notification;
		};

		[[nodiscard]] NotificationService& GetNotificationService() noexcept
		{
			static NotificationService service;
			return service;
		}

		[[nodiscard]] bool ValidSeverity(DMUI_StatusSeverity a_severity) noexcept
		{
			return a_severity <= DMUI_STATUS_SEVERITY_ERROR;
		}

	}

	DMUI_Result PostNotification(
		DMUI_ClientHandle a_client,
		const DMUI_NotificationDescriptor* a_descriptor) noexcept
	{
		if (!a_descriptor || a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_descriptor->structSize < DMUI_NOTIFICATION_DESCRIPTOR_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (!ValidSeverity(a_descriptor->severity))
			return DMUI_RESULT_INVALID_ARGUMENT;
		try
		{
			std::string message;
			if (!Internal::CopyBoundedString(
					a_descriptor->message,
					kNotificationCapacity,
					false,
					message))
				return DMUI_RESULT_INVALID_DESCRIPTOR;
			const auto duration = std::chrono::milliseconds{
				(std::clamp)(
					a_descriptor->durationMilliseconds ?
						a_descriptor->durationMilliseconds :
						4000u,
					250u,
					30000u)
			};
			auto& service = GetNotificationService();
			const std::scoped_lock lock{ service.mutex };
			service.notification = {
				a_client,
				a_descriptor->severity,
				std::move(message),
				Clock::now() + duration,
				service.nextGeneration++
			};
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	void DrawNotification() noexcept
	{
		Notification notification;
		{
			auto& service = GetNotificationService();
			const std::scoped_lock lock{ service.mutex };
			if (service.notification.message.empty())
				return;
			if (Clock::now() >= service.notification.expiresAt)
			{
				service.notification = {};
				return;
			}
			try
			{
				notification = service.notification;
			}
			catch (...)
			{
				return;
			}
		}
		const auto& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(
			{ io.DisplaySize.x * 0.5f, 24.0f },
			ImGuiCond_Always,
			{ 0.5f, 0.0f });
		ImGui::SetNextWindowBgAlpha(0.94f);
		const auto flags =
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoSavedSettings;
		if (ImGui::Begin("Notification###dmui.notification", nullptr, flags))
			ImGui::TextColored(
				Theme::StatusTextColor(notification.severity),
				"%s",
				notification.message.c_str());
		ImGui::End();
	}

	namespace Notifications
	{
		void Clear() noexcept
		{
			auto& service = GetNotificationService();
			const std::scoped_lock lock{ service.mutex };
			service.notification = {};
		}

		bool HasFrameDemand() noexcept
		{
			auto& service = GetNotificationService();
			const std::scoped_lock lock{ service.mutex };
			return !service.notification.message.empty() &&
				Clock::now() < service.notification.expiresAt;
		}
	}
}
