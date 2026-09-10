#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <DearModdingUI/presentation/Theme.h>
#include <Support/BoundedString.h>
#include <DearModdingUI/host/MenuDismissal.h>
#include "PresentationServiceOwners.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace DearModdingUI::PresentationServices
{
	namespace
	{
		inline constexpr size_t kDialogTextLimit{ 4096 };

		inline constexpr size_t kDialogStringLimit{ 1024 };

		struct Dialog
		{
			DMUI_DialogHandle handle{ DMUI_INVALID_DIALOG_HANDLE };
			DMUI_ClientHandle owner{ DMUI_INVALID_CLIENT_HANDLE };
			DMUI_DialogKind kind{ DMUI_DIALOG_KIND_CONFIRM };
			std::string title;
			std::string body;
			std::string acceptLabel;
			std::string cancelLabel;
			std::string hint;
			std::vector<char> text;
			std::string error;
			DMUI_DialogEventKind event{ DMUI_DIALOG_EVENT_PENDING };
			uint64_t submissionId{};
			bool popupOpened{};
			uint32_t popupId{};
		};

		struct DialogService
		{
			std::mutex mutex;
			DMUI_DialogHandle nextDialog{ 1 };
			uint64_t nextSubmission{ 1 };
			Dialog dialog;
		};

		[[nodiscard]] DialogService& GetDialogService() noexcept
		{
			static DialogService service;
			return service;
		}

		[[nodiscard]] DMUI_Result SubmitDialogLocked(
			DialogService& a_service,
			Dialog& a_dialog,
			DMUI_DialogHandle a_handle) noexcept
		{
			if (a_dialog.handle != a_handle)
				return DMUI_RESULT_STALE_HANDLE;
			if (a_dialog.event != DMUI_DIALOG_EVENT_PENDING)
				return a_dialog.event == DMUI_DIALOG_EVENT_SUBMITTED ?
					DMUI_RESULT_BUSY :
					DMUI_RESULT_STALE_HANDLE;
			if (a_service.nextSubmission == 0)
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			a_dialog.event = DMUI_DIALOG_EVENT_SUBMITTED;
			a_dialog.submissionId = a_service.nextSubmission++;
			a_dialog.error.clear();
			return DMUI_RESULT_OK;
		}
	}

	DMUI_Result RequestDialog(
		DMUI_ClientHandle a_client,
		const DMUI_DialogDescriptor* a_descriptor,
		DMUI_DialogHandle* a_dialog,
		bool a_menuVisible) noexcept
	{
		if (!a_descriptor || !a_dialog ||
			a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_dialog = DMUI_INVALID_DIALOG_HANDLE;
		if (a_descriptor->structSize < DMUI_DIALOG_DESCRIPTOR_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (!a_menuVisible)
			return DMUI_RESULT_NOT_VISIBLE;
		if (!RenderExecution::IsActiveClient(a_client, false))
			return DMUI_RESULT_WRONG_THREAD;
		if (a_descriptor->kind != DMUI_DIALOG_KIND_CONFIRM &&
			a_descriptor->kind != DMUI_DIALOG_KIND_TEXT_ENTRY)
			return DMUI_RESULT_INVALID_ARGUMENT;
		try
		{
			Dialog dialog;
			if (!Internal::CopyBoundedString(
					a_descriptor->title,
					kDialogStringLimit,
					false,
					dialog.title) ||
				!Internal::CopyBoundedString(
					a_descriptor->body,
					kDialogStringLimit,
					true,
					dialog.body) ||
				!Internal::CopyBoundedString(
					a_descriptor->acceptLabel,
					64,
					false,
					dialog.acceptLabel) ||
				!Internal::CopyBoundedString(
					a_descriptor->cancelLabel,
					64,
					false,
					dialog.cancelLabel) ||
				!Internal::CopyBoundedString(
					a_descriptor->hint,
					kDialogStringLimit,
					true,
					dialog.hint))
				return DMUI_RESULT_INVALID_DESCRIPTOR;
			const auto maximum =
				a_descriptor->kind == DMUI_DIALOG_KIND_TEXT_ENTRY ?
					a_descriptor->maximumTextBytes :
					1u;
			if (!maximum || maximum > kDialogTextLimit)
				return DMUI_RESULT_INVALID_ARGUMENT;
			std::string initial;
			if (!Internal::CopyBoundedString(
					a_descriptor->initialText,
					maximum - 1u,
					true,
					initial))
				return DMUI_RESULT_INVALID_DESCRIPTOR;
			dialog.text.assign(maximum, '\0');
			std::copy(initial.begin(), initial.end(), dialog.text.begin());
			dialog.owner = a_client;
			dialog.kind = a_descriptor->kind;
			auto& service = GetDialogService();
			const std::scoped_lock lock{ service.mutex };
			if (service.dialog.handle != DMUI_INVALID_DIALOG_HANDLE)
				return DMUI_RESULT_BUSY;
			if (service.nextDialog == DMUI_INVALID_DIALOG_HANDLE)
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			dialog.handle = service.nextDialog++;
			service.dialog = std::move(dialog);
			*a_dialog = service.dialog.handle;
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	DMUI_Result PollDialogEvent(
		DMUI_ClientHandle a_client,
		DMUI_DialogHandle a_dialog,
		DMUI_DialogEvent* a_event,
		char* a_textBuffer,
		uint32_t a_textCapacity) noexcept
	{
		if (!a_event || a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_dialog == DMUI_INVALID_DIALOG_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_event->structSize < DMUI_DIALOG_EVENT_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (!RenderExecution::IsActiveClient(a_client, false))
			return DMUI_RESULT_WRONG_THREAD;
		auto& service = GetDialogService();
		const std::scoped_lock lock{ service.mutex };
		auto& dialog = service.dialog;
		if (dialog.handle != a_dialog || dialog.owner != a_client)
			return DMUI_RESULT_STALE_HANDLE;
		const auto textLength = dialog.text.empty() ?
			0u :
			std::strlen(dialog.text.data());
		const auto required = static_cast<uint32_t>(textLength + 1u);
		a_event->kind = dialog.event;
		a_event->submissionId = dialog.submissionId;
		a_event->requiredTextCapacity = required;
		if ((dialog.event == DMUI_DIALOG_EVENT_SUBMITTED ||
				dialog.kind == DMUI_DIALOG_KIND_TEXT_ENTRY) &&
			(!a_textBuffer || a_textCapacity < required))
			return DMUI_RESULT_BUFFER_TOO_SMALL;
		if (a_textBuffer && a_textCapacity)
			std::memcpy(a_textBuffer, dialog.text.data(), required);
		if (dialog.event == DMUI_DIALOG_EVENT_CANCELLED ||
			dialog.event == DMUI_DIALOG_EVENT_COMPLETED)
			dialog = {};
		return DMUI_RESULT_OK;
	}

	DMUI_Result ResolveDialogSubmission(
		DMUI_ClientHandle a_client,
		DMUI_DialogHandle a_dialog,
		uint64_t a_submissionId,
		uint32_t a_accepted,
		const char* a_error) noexcept
	{
		if (a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_dialog == DMUI_INVALID_DIALOG_HANDLE ||
			!a_submissionId)
			return DMUI_RESULT_INVALID_ARGUMENT;
		try
		{
			std::string error;
			if (!a_accepted &&
				!Internal::CopyBoundedString(
					a_error, kDialogStringLimit, true, error))
				return DMUI_RESULT_INVALID_DESCRIPTOR;
			auto& service = GetDialogService();
			const std::scoped_lock lock{ service.mutex };
			auto& dialog = service.dialog;
			if (dialog.handle != a_dialog || dialog.owner != a_client)
				return DMUI_RESULT_STALE_HANDLE;
			if (dialog.event != DMUI_DIALOG_EVENT_SUBMITTED ||
				dialog.submissionId != a_submissionId)
				return DMUI_RESULT_STALE_SUBMISSION;
			if (a_accepted)
			{
				dialog.event = DMUI_DIALOG_EVENT_COMPLETED;
				dialog.error.clear();
			}
			else
			{
				dialog.event = DMUI_DIALOG_EVENT_PENDING;
				dialog.error = std::move(error);
			}
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	DMUI_Result CancelDialog(
		DMUI_ClientHandle a_client,
		DMUI_DialogHandle a_dialog) noexcept
	{
		if (a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_dialog == DMUI_INVALID_DIALOG_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		auto& service = GetDialogService();
		const std::scoped_lock lock{ service.mutex };
		auto& dialog = service.dialog;
		if (dialog.handle != a_dialog || dialog.owner != a_client)
			return DMUI_RESULT_STALE_HANDLE;
		if (dialog.event == DMUI_DIALOG_EVENT_SUBMITTED)
			return DMUI_RESULT_BUSY;
		dialog.event = DMUI_DIALOG_EVENT_CANCELLED;
		return DMUI_RESULT_OK;
	}

	DMUI_Result SubmitDialog(DMUI_DialogHandle a_dialog) noexcept
	{
		if (a_dialog == DMUI_INVALID_DIALOG_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		auto& service = GetDialogService();
		const std::scoped_lock lock{ service.mutex };
		return SubmitDialogLocked(service, service.dialog, a_dialog);
	}

	void DrawDialog(bool a_menuVisible) noexcept
	{
		try
		{
			const auto escapeDismissed =
				DismissCapturedMenuDialog();
			auto& service = GetDialogService();
			Dialog snapshot;
			bool openPopup{};
			{
				const std::scoped_lock lock{ service.mutex };
				auto& dialog = service.dialog;
				if (dialog.handle == DMUI_INVALID_DIALOG_HANDLE)
					return;
				if (!a_menuVisible)
				{
					if (dialog.event == DMUI_DIALOG_EVENT_PENDING)
						dialog.event = DMUI_DIALOG_EVENT_CANCELLED;
					return;
				}
				if (dialog.event == DMUI_DIALOG_EVENT_COMPLETED ||
					dialog.event == DMUI_DIALOG_EVENT_CANCELLED)
					return;
				snapshot = dialog;
				openPopup = !dialog.popupOpened;
				dialog.popupOpened = true;
			}

			const auto popupId = snapshot.title + "###dmui.dialog";
			const auto popupImGuiId = ImGui::GetID(popupId.c_str());
			{
				const std::scoped_lock lock{ service.mutex };
				if (service.dialog.handle == snapshot.handle)
					service.dialog.popupId = popupImGuiId;
			}
			if (openPopup)
				ImGui::OpenPopup(popupId.c_str());
			bool open{ true };
			if (!ImGui::BeginPopupModal(
					popupId.c_str(),
					snapshot.event == DMUI_DIALOG_EVENT_PENDING ?
						&open :
						nullptr,
					ImGuiWindowFlags_AlwaysAutoResize))
			{
				if (escapeDismissed &&
					snapshot.event == DMUI_DIALOG_EVENT_PENDING)
				{
					const std::scoped_lock lock{ service.mutex };
					if (service.dialog.handle == snapshot.handle &&
						service.dialog.event == DMUI_DIALOG_EVENT_PENDING)
						service.dialog.event = DMUI_DIALOG_EVENT_CANCELLED;
				}
				return;
			}
			if (!snapshot.body.empty())
				ImGui::TextWrapped("%s", snapshot.body.c_str());
			if (snapshot.kind == DMUI_DIALOG_KIND_TEXT_ENTRY)
			{
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 24.0f);
				(void)ImGui::InputTextWithHint(
					"##dmui.dialog.text",
					snapshot.hint.c_str(),
					snapshot.text.data(),
					snapshot.text.size());
			}
			if (!snapshot.error.empty())
				ImGui::TextColored(
					Theme::TextColor(dmui::TextTone::kStatusError),
					"%s",
					snapshot.error.c_str());

			enum class DialogAction
			{
				kNone,
				kSubmit,
				kCancel
			};
			auto action = DialogAction::kNone;
			if (snapshot.event == DMUI_DIALOG_EVENT_SUBMITTED)
				ImGui::TextDisabled("Working...");
			else
			{
				if (ImGui::Button(snapshot.acceptLabel.c_str()))
					action = DialogAction::kSubmit;
				ImGui::SameLine();
				if (ImGui::Button(snapshot.cancelLabel.c_str()))
					action = DialogAction::kCancel;
			}
			if (!open && snapshot.event == DMUI_DIALOG_EVENT_PENDING)
				action = DialogAction::kCancel;
			if (escapeDismissed)
			{
				if (snapshot.event == DMUI_DIALOG_EVENT_PENDING)
					action = DialogAction::kCancel;
			}
			else if (action == DialogAction::kCancel)
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();

			const std::scoped_lock lock{ service.mutex };
			auto& dialog = service.dialog;
			if (dialog.handle != snapshot.handle ||
				dialog.event != snapshot.event)
				return;
			if (dialog.kind == DMUI_DIALOG_KIND_TEXT_ENTRY &&
				dialog.event == DMUI_DIALOG_EVENT_PENDING)
				dialog.text = std::move(snapshot.text);
			if (action == DialogAction::kSubmit &&
				dialog.event == DMUI_DIALOG_EVENT_PENDING)
				(void)SubmitDialogLocked(service, dialog, dialog.handle);
			else if (action == DialogAction::kCancel &&
				dialog.event == DMUI_DIALOG_EVENT_PENDING)
				dialog.event = DMUI_DIALOG_EVENT_CANCELLED;
		}
		catch (...)
		{
			// A presentation allocation failure leaves the dialog pending.
		}
	}

	void NotifyMenuClosed() noexcept
	{
		auto& service = GetDialogService();
		const std::scoped_lock lock{ service.mutex };
		if (service.dialog.handle != DMUI_INVALID_DIALOG_HANDLE &&
			service.dialog.event == DMUI_DIALOG_EVENT_PENDING)
			service.dialog.event = DMUI_DIALOG_EVENT_CANCELLED;
	}

	bool HasActiveDialog() noexcept
	{
		auto& service = GetDialogService();
		const std::scoped_lock lock{ service.mutex };
		return service.dialog.handle != DMUI_INVALID_DIALOG_HANDLE &&
			service.dialog.event != DMUI_DIALOG_EVENT_CANCELLED &&
			service.dialog.event != DMUI_DIALOG_EVENT_COMPLETED;
	}

	uint32_t ActiveDialogPopupId() noexcept
	{
		auto& service = GetDialogService();
		const std::scoped_lock lock{ service.mutex };
		if (service.dialog.handle == DMUI_INVALID_DIALOG_HANDLE ||
			service.dialog.event == DMUI_DIALOG_EVENT_CANCELLED ||
			service.dialog.event == DMUI_DIALOG_EVENT_COMPLETED)
			return 0;
		return service.dialog.popupId;
	}

	namespace Dialogs
	{
		bool HasFrameDemand() noexcept
		{
			auto& service = GetDialogService();
			const std::scoped_lock lock{ service.mutex };
			return service.dialog.handle != DMUI_INVALID_DIALOG_HANDLE &&
				service.dialog.event != DMUI_DIALOG_EVENT_CANCELLED &&
				service.dialog.event != DMUI_DIALOG_EVENT_COMPLETED;
		}
	}
}
