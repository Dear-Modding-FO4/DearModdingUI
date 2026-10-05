#include "../support/D3DTestResources.h"
#include "../support/PresentationTestSupport.h"
#include <DearModdingUI/host/ControllerNavigation.h>
#include <DearModdingUI/host/MenuDismissal.h>
#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <DearModdingUI/Client.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <string>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;
	using support::CreateImageResources;
	using support::presentation::InteractiveImGui;

	void run_presentation_dialog_interaction_checks(Runner& runner)
	{
		runner.test("dialog session rejects completes cancels and isolates callbacks", [] {
			RenderExecution::Guard execution{ RenderExecution::Phase::kFrameObservation };
			(void)execution.NoteBinding(1);
			const RenderExecution::ClientGuard callback{ 18, false };
			static DMUI_DialogHandle requested{};
			DMUI_HostAPI api{};
			api.requestDialog = [](DMUI_ClientHandle a_client, const DMUI_DialogDescriptor* a_descriptor,
				DMUI_DialogHandle* a_dialog) noexcept -> DMUI_Result {
				const auto result = PresentationServices::RequestDialog(a_client, a_descriptor, a_dialog, true);
				requested = *a_dialog;
				return result;
			};
			api.pollDialogEvent = &PresentationServices::PollDialogEvent;
			api.resolveDialogSubmission = &PresentationServices::ResolveDialogSubmission;
			api.cancelDialog = &PresentationServices::CancelDialog;
			dmui::Client client{ "dialogs.test", "Dialog tests", { 1, 0 } };
			dmui::detail::ClientTestAccess::Bind(client, api, 18);
			const std::string initial(100, 'x');
			const DMUI_DialogDescriptor descriptor{
				DMUI_DIALOG_KIND_TEXT_ENTRY, "Name", nullptr, "Save", "Cancel",
				nullptr, initial.c_str(), 256
			};
			int submissions{};
			dmui::DialogSession session;
			const auto submit = [&](std::string_view a_text) -> std::optional<std::string> {
				require(a_text == initial, "session truncated submission text");
				return ++submissions == 1 ? std::optional<std::string>{ "Try again" } : std::nullopt;
			};
			require(session.Open(client, descriptor, submit), "session open failed");
			require(!session.Open(client, descriptor, submit) && session.LastResult() == DMUI_RESULT_BUSY,
				"session replaced an active dialog");
			require(PresentationServices::SubmitDialog(requested) == DMUI_RESULT_OK, "session submit failed");
			session.Poll();
			require(session.Active() && session.LastResult() == DMUI_RESULT_OK && submissions == 1,
				"rejected session did not remain open");
			require(PresentationServices::SubmitDialog(requested) == DMUI_RESULT_OK, "session retry failed");
			session.Poll();
			require(!session.Active() && session.LastResult() == DMUI_RESULT_OK && submissions == 2,
				"successful session did not drain completion");
			require(session.Open(client, descriptor, submit), "completion retained host ownership");
			require(PresentationServices::SubmitDialog(requested) == DMUI_RESULT_OK, "cancel submit failed");
			session.Cancel();
			require(!session.Active() && session.LastResult() == DMUI_RESULT_OK && submissions == 2,
				"cancel did not reject unresolved work without invoking submit");
			require(session.Open(client, descriptor,
				[](std::string_view) -> std::optional<std::string> { throw 1; }), "exception session open failed");
			require(PresentationServices::SubmitDialog(requested) == DMUI_RESULT_OK, "exception submit failed");
			session.Poll();
			require(!session.Active() && session.LastResult() == DMUI_RESULT_CALLBACK_FAILED,
				"callback exception escaped or retained host ownership");
			{
				dmui::DialogSession scoped;
				require(scoped.Open(client, descriptor, submit), "scoped session open failed");
			}
			require(session.Open(client, descriptor, submit), "destructor did not drain cancellation");
			session.Cancel();
			require(!session.Active(), "pending session cancel did not reset");
		});

		runner.test("declarative edits report live completion", [] {
			InteractiveImGui frame;
			size_t textSets{};
			std::vector<dmui::SettingEditEvent> textEvents;
			dmui::SettingDescriptor textSetting{
				.id = "comment",
				.control = dmui::TextSettingControl{
					.bufferCapacity = 64,
					.multiline = false
				},
				.defaultValue = std::string{},
				.binding = dmui::BindSetting(
					[] { return std::string{ "base" }; },
					[&](std::string a_value) {
						++textSets;
						return a_value;
					}),
				.onEdit = [&](const dmui::SettingEditEvent& a_event) {
					textEvents.push_back(a_event);
				}
			};

			frame.Begin({ 500.0f, 400.0f }, false);
			auto text = dmui::setting_detail::DrawBoundSetting(
				textSetting, std::string{ "base" });
			dmui::setting_detail::NotifySettingEdit(textSetting, text);
			frame.End();
			require(!text.changed && textEvents.empty(),
				"initial text draw emitted an edit callback");

			frame.Begin({ 40.0f, 28.0f }, true);
			text = dmui::setting_detail::DrawBoundSetting(
				textSetting, std::move(text.value));
			dmui::setting_detail::NotifySettingEdit(textSetting, text);
			const auto textActive = ImGui::IsItemActive();
			frame.End();
			require(textActive && !text.changed && textEvents.empty(),
				"activating text emitted an unchanged edit callback");

			frame.Begin({ 40.0f, 28.0f }, false, "X");
			text = dmui::setting_detail::DrawBoundSetting(
				textSetting, std::move(text.value));
			dmui::setting_detail::NotifySettingEdit(textSetting, text);
			require(text.changed && !text.completed && textSets == 1 &&
					textEvents.size() == 1 &&
					textEvents.back().changed &&
					!textEvents.back().completed,
				"live text edit did not report changed before completion");
			frame.End();

			frame.Begin({ 500.0f, 400.0f }, true);
			text = dmui::setting_detail::DrawBoundSetting(
				textSetting, std::move(text.value));
			dmui::setting_detail::NotifySettingEdit(textSetting, text);
			frame.End();
			require(!text.changed && text.completed &&
					textEvents.size() == 2 &&
					!textEvents.back().changed &&
					textEvents.back().completed,
				"text deactivation did not report a separate completion");

			frame.Begin({ 500.0f, 400.0f }, false);
			text = dmui::setting_detail::DrawBoundSetting(
				textSetting, std::move(text.value));
			dmui::setting_detail::NotifySettingEdit(textSetting, text);
			frame.End();
			require(textEvents.size() == 2,
				"unchanged text emitted another edit callback");

			size_t sliderSets{};
			std::vector<dmui::SettingEditEvent> sliderEvents;
			dmui::SettingDescriptor sliderSetting{
				.id = "scale",
				.control = dmui::DoubleSettingControl{
					.range = dmui::NumericSettingRange<double>{ 0.0, 10.0 }
				},
				.defaultValue = 0.0,
				.binding = dmui::BindSetting(
					[] { return 0.0; },
					[&](double a_value) {
						++sliderSets;
						return a_value;
					}),
				.onEdit = [&](const dmui::SettingEditEvent& a_event) {
					sliderEvents.push_back(a_event);
				}
			};
			frame.Begin({ 170.0f, 28.0f }, true);
			auto slider = dmui::setting_detail::DrawBoundSetting(
				sliderSetting, 0.0);
			dmui::setting_detail::NotifySettingEdit(sliderSetting, slider);
			frame.End();
			require(slider.changed && sliderSets == 1 &&
					sliderEvents.size() == 1 &&
					sliderEvents.back().changed,
				"live slider edit did not apply immediately");

			frame.Begin({ 170.0f, 28.0f }, false);
			slider = dmui::setting_detail::DrawBoundSetting(
				sliderSetting, std::move(slider.value));
			dmui::setting_detail::NotifySettingEdit(sliderSetting, slider);
			frame.End();
			require(slider.completed && sliderEvents.size() == 2 &&
					sliderEvents.back().completed,
				"slider release did not report completion");

			size_t multilineEvents{};
			dmui::SettingDescriptor multilineSetting{
				.id = "multiline",
				.control = dmui::TextSettingControl{
					.bufferCapacity = 128,
					.multiline = true
				},
				.defaultValue = std::string{},
				.binding = dmui::BindSetting(
					[] { return std::string{ "line one\nline two" }; },
					[](std::string a_value) { return a_value; }),
				.onEdit = [&](const dmui::SettingEditEvent&) {
					++multilineEvents;
				}
			};
			frame.Begin({ 500.0f, 400.0f }, false);
			const auto multiline = dmui::setting_detail::DrawBoundSetting(
				multilineSetting,
				std::string{ "line one\nline two" });
			dmui::setting_detail::NotifySettingEdit(
				multilineSetting, multiline);
			frame.End();
			require(!multiline.changed &&
					!multiline.completed &&
					multilineEvents == 0,
				"unchanged multiline text emitted an edit callback");
		});

		runner.test("Escape dismisses one active UI level per press", [] {
			InteractiveImGui frame;
			ResetMenuEscapeRequest();
			frame.Begin({ -100.0f, -100.0f }, false);

			auto* context = ImGui::GetCurrentContext();
			auto* window = ImGui::GetCurrentWindow();
			const auto interactionId =
				window->GetID("##EscapeActiveInteraction");
			ImGui::OpenPopup("##EscapeOuter");
			require(ImGui::BeginPopup("##EscapeOuter"),
				"outer popup did not open");
			const auto outerId = context->OpenPopupStack.back().PopupId;
			ImGui::SetActiveID(interactionId, window);

			Hotkeys::BeginCapture();
			CaptureMenuEscapePress(HostInputMode::kShell, true, outerId);
			require(!DismissCapturedMenuDialog() && !DismissCapturedMenuPopup() &&
					ConsumeMenuEscapeTarget(MenuEscapeTarget::kKeyCapture) &&
					Hotkeys::CancelCapture() &&
					!ConsumeMenuEscapeTarget(MenuEscapeTarget::kHost) &&
					!ConsumeMenuEscapeTarget(MenuEscapeTarget::kInteraction),
				"capture Escape also reached a dialog, popup, interaction, or host");

			CaptureMenuEscapePress(HostInputMode::kShell, false, 0);
			require(
				ConsumeMenuEscapeTarget(MenuEscapeTarget::kInteraction),
				"an active interaction did not own the first Escape");
			require(context->OpenPopupStack.Size == 1 &&
					context->OpenPopupStack.back().PopupId == outerId,
				"active-interaction Escape also dismissed a popup");

			ImGui::ClearActiveID();
			CaptureMenuEscapePress(HostInputMode::kShell, false, 0);
			ImGui::CloseCurrentPopup();
			require(DismissCapturedMenuPopup(),
				"the second Escape did not retain its popup ownership");
			require(context->OpenPopupStack.empty(),
				"popup coordination dismissed another UI level");

			ImGui::OpenPopup("##ControllerB");
			(void)ControllerNavigation::RouteButton(
				KeyCatalog::kPadB, true, false, 1.0f, { HostInputMode::kShell });
			require(DismissCapturedMenuPopup() &&
					!ConsumeMenuEscapeTarget(MenuEscapeTarget::kHost),
				"controller B did not use the single-level popup dismissal");
			ImGui::ClosePopupToLevel(0, false);

			context->NavId = interactionId;
			CaptureMenuEscapePress(HostInputMode::kShell, false, 0);
			require(ConsumeMenuEscapeTarget(MenuEscapeTarget::kHost),
				"idle keyboard focus incorrectly trapped Escape");
			require(!ConsumeMenuEscapeTarget(MenuEscapeTarget::kHost),
				"one Escape generated more than one host dismissal");

			CaptureMenuEscapePress(HostInputMode::kGameplay, false, 0);
			require(!ConsumeMenuEscapeTarget(MenuEscapeTarget::kHost),
				"closed or overlay-only state captured Escape");
			require(
				DecideMenuEscapeTarget({
					HostInputMode::kShell,
					false,
					true,
					0x22,
					0x11,
					2
				}) == MenuEscapeTarget::kPopup &&
				DecideMenuEscapeTarget({
					HostInputMode::kShell,
					false,
					true,
					0x11,
					0x11,
					1
				}) == MenuEscapeTarget::kDialog,
				"a nested popup did not outrank its owning dialog");
			require(
				DecideMenuEscapeTarget({
					HostInputMode::kShell,
					false,
					true,
					0,
					0x11,
					0
				}) == MenuEscapeTarget::kHost,
				"a hidden logical dialog trapped shell dismissal");
			ResetMenuEscapeRequest();
			ImGui::EndPopup();
			frame.End();
		});

		runner.test("dialog submissions reject retry and complete deterministically", [] {
			auto resources = CreateImageResources();
			PresentationServices::SetDevice(resources.device.Get());
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameObservation
			};
			(void)execution.NoteBinding(1);
			const RenderExecution::ClientGuard callback{ 16, false };
			const DMUI_DialogDescriptor descriptor{
				DMUI_DIALOG_KIND_TEXT_ENTRY,
				"Save preset",
				"Choose a preset name.",
				"Save",
				"Cancel",
				"Preset name",
				"Commonwealth",
				65
			};
			DMUI_DialogHandle dialog{};
			require(PresentationServices::RequestDialog(
						16, &descriptor, &dialog, false) ==
					DMUI_RESULT_NOT_VISIBLE,
				"hidden menu accepted a dialog request");
			require(PresentationServices::RequestDialog(
						16, &descriptor, &dialog, true) == DMUI_RESULT_OK,
				"submission dialog request failed");
			DMUI_DialogEvent event{};
			char smallBuffer[2]{};
			require(PresentationServices::PollDialogEvent(
						16, dialog, &event, smallBuffer, sizeof(smallBuffer)) ==
						DMUI_RESULT_BUFFER_TOO_SMALL &&
					event.requiredTextCapacity > sizeof(smallBuffer),
				"small dialog buffer was truncated or consumed");
			require(PresentationServices::SubmitDialog(dialog) == DMUI_RESULT_OK,
				"first dialog submission failed");
			require(PresentationServices::SubmitDialog(dialog) == DMUI_RESULT_BUSY,
				"duplicate pending submission was accepted");

			char text[65]{};
			require(PresentationServices::PollDialogEvent(
						16, dialog, &event, text, sizeof(text)) ==
						DMUI_RESULT_OK &&
					event.kind == DMUI_DIALOG_EVENT_SUBMITTED &&
					event.submissionId != 0,
				"submitted dialog event was not pollable");
			const auto firstSubmission = event.submissionId;
			PresentationServices::NotifyMenuClosed();
			PresentationServices::InvalidateDevice();
			require(!PresentationServices::HasFrameDemand(),
				"backend teardown retained unavailable dialog demand");
			require(PresentationServices::CancelDialog(16, dialog) ==
					DMUI_RESULT_BUSY,
				"menu close or cancel discarded submitted client work");
			PresentationServices::SetDevice(resources.device.Get());
			require(PresentationServices::HasFrameDemand(),
				"backend recovery did not resume submitted dialog demand");
			require(PresentationServices::ResolveDialogSubmission(
						16,
						dialog,
						firstSubmission + 1,
						0,
						"duplicate") ==
					DMUI_RESULT_STALE_SUBMISSION,
				"stale dialog resolution was accepted");
			require(PresentationServices::ResolveDialogSubmission(
						16,
						dialog,
						firstSubmission,
						0,
						"Name already exists.") ==
					DMUI_RESULT_OK,
				"dialog rejection failed");

			require(PresentationServices::PollDialogEvent(
						16, dialog, &event, text, sizeof(text)) ==
						DMUI_RESULT_OK &&
					event.kind == DMUI_DIALOG_EVENT_PENDING &&
					std::string_view{ text } == "Commonwealth",
				"rejected dialog did not preserve text and pending state");
			require(PresentationServices::SubmitDialog(dialog) == DMUI_RESULT_OK,
				"dialog resubmission after explicit error failed");
			require(PresentationServices::PollDialogEvent(
						16, dialog, &event, text, sizeof(text)) ==
						DMUI_RESULT_OK &&
					event.kind == DMUI_DIALOG_EVENT_SUBMITTED &&
					event.submissionId > firstSubmission,
				"resubmission did not allocate a unique submission id");
			const auto secondSubmission = event.submissionId;
			require(PresentationServices::ResolveDialogSubmission(
						16, dialog, firstSubmission, 1, nullptr) ==
					DMUI_RESULT_STALE_SUBMISSION,
				"superseded submission resolution was accepted");
			require(PresentationServices::ResolveDialogSubmission(
						16, dialog, secondSubmission, 1, nullptr) ==
					DMUI_RESULT_OK,
				"accepted dialog resolution failed");
			require(PresentationServices::PollDialogEvent(
						16, dialog, &event, text, sizeof(text)) ==
						DMUI_RESULT_OK &&
					event.kind == DMUI_DIALOG_EVENT_COMPLETED,
				"completed dialog event was not delivered");
			require(PresentationServices::PollDialogEvent(
						16, dialog, &event, text, sizeof(text)) ==
					DMUI_RESULT_STALE_HANDLE,
				"completed dialog handle remained live");
			PresentationServices::InvalidateDevice();
		});

		runner.test("Escape cancels pending dialogs but only hides submitted work", [] {
			InteractiveImGui frame;
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameObservation
			};
			(void)execution.NoteBinding(1);
			const RenderExecution::ClientGuard callback{ 17, false };
			const DMUI_DialogDescriptor descriptor{
				DMUI_DIALOG_KIND_CONFIRM,
				"Confirm sample",
				"Confirm a harmless operation.",
				"Confirm",
				"Cancel",
				nullptr,
				nullptr,
				0
			};

			DMUI_DialogHandle pending{};
			require(PresentationServices::RequestDialog(
						17, &descriptor, &pending, true) == DMUI_RESULT_OK,
				"pending Escape dialog request failed");
			frame.Begin({ -100.0f, -100.0f }, false);
			PresentationServices::DrawDialog(true);
			frame.End();
			CaptureMenuEscapePress(
				HostInputMode::kShell,
				PresentationServices::HasActiveDialog(),
				PresentationServices::ActiveDialogPopupId());
			frame.Begin({ -100.0f, -100.0f }, false);
			PresentationServices::DrawDialog(true);
			frame.End();

			DMUI_DialogEvent event{};
			char text[2]{};
			require(PresentationServices::PollDialogEvent(
						17, pending, &event, text, sizeof(text)) ==
						DMUI_RESULT_OK &&
					event.kind == DMUI_DIALOG_EVENT_CANCELLED,
				"Escape did not cancel a pending dialog");

			DMUI_DialogHandle submitted{};
			require(PresentationServices::RequestDialog(
						17, &descriptor, &submitted, true) == DMUI_RESULT_OK &&
					PresentationServices::SubmitDialog(submitted) ==
						DMUI_RESULT_OK,
				"submitted Escape dialog setup failed");
			frame.Begin({ -100.0f, -100.0f }, false);
			PresentationServices::DrawDialog(true);
			frame.End();
			CaptureMenuEscapePress(
				HostInputMode::kShell,
				PresentationServices::HasActiveDialog(),
				PresentationServices::ActiveDialogPopupId());
			frame.Begin({ -100.0f, -100.0f }, false);
			PresentationServices::DrawDialog(true);
			const auto popupId =
				PresentationServices::ActiveDialogPopupId();
			require(popupId != 0 &&
					!ImGui::IsPopupOpen(
						popupId,
						ImGuiPopupFlags_AnyPopupLevel),
				"submitted dialog popup remained visible after Escape");
			frame.End();

			event = {};
			require(PresentationServices::PollDialogEvent(
						17, submitted, &event, text, sizeof(text)) ==
						DMUI_RESULT_OK &&
					event.kind == DMUI_DIALOG_EVENT_SUBMITTED &&
					event.submissionId != 0,
				"Escape cancelled submitted client work");
			const auto submissionId = event.submissionId;

			CaptureMenuEscapePress(
				HostInputMode::kShell,
				PresentationServices::HasActiveDialog(),
				PresentationServices::ActiveDialogPopupId());
			require(ConsumeMenuEscapeTarget(MenuEscapeTarget::kHost),
				"hidden submitted dialog trapped the next fresh Escape");
			require(!ConsumeMenuEscapeTarget(MenuEscapeTarget::kDialog) &&
					!ConsumeMenuEscapeTarget(MenuEscapeTarget::kHost),
				"hidden submitted dialog produced repeated dismissal");

			frame.Begin({ -100.0f, -100.0f }, false);
			PresentationServices::DrawDialog(true);
			require(!ImGui::IsPopupOpen(
						popupId,
						ImGuiPopupFlags_AnyPopupLevel),
				"dismissed submitted dialog reopened");
			frame.End();

			require(PresentationServices::ResolveDialogSubmission(
						17, submitted, submissionId, 1, nullptr) ==
					DMUI_RESULT_OK,
				"dismissed submitted work could not complete");
			event = {};
			require(PresentationServices::PollDialogEvent(
						17, submitted, &event, text, sizeof(text)) ==
						DMUI_RESULT_OK &&
					event.kind == DMUI_DIALOG_EVENT_COMPLETED,
				"dismissed submitted work did not deliver completion");

			DMUI_DialogHandle completedBetweenFrames{};
			require(PresentationServices::RequestDialog(
						17,
						&descriptor,
						&completedBetweenFrames,
						true) == DMUI_RESULT_OK &&
					PresentationServices::SubmitDialog(
						completedBetweenFrames) == DMUI_RESULT_OK,
				"completion-race dialog setup failed");
			frame.Begin({ -100.0f, -100.0f }, false);
			PresentationServices::DrawDialog(true);
			const auto completionPopupId =
				PresentationServices::ActiveDialogPopupId();
			frame.End();
			CaptureMenuEscapePress(
				HostInputMode::kShell,
				PresentationServices::HasActiveDialog(),
				completionPopupId);
			event = {};
			require(PresentationServices::PollDialogEvent(
						17,
						completedBetweenFrames,
						&event,
						text,
						sizeof(text)) == DMUI_RESULT_OK &&
					event.kind == DMUI_DIALOG_EVENT_SUBMITTED,
				"completion-race submission was not observable");
			require(PresentationServices::ResolveDialogSubmission(
						17,
						completedBetweenFrames,
						event.submissionId,
						1,
						nullptr) == DMUI_RESULT_OK,
				"completion-race resolution failed");
			frame.Begin({ -100.0f, -100.0f }, false);
			PresentationServices::DrawDialog(true);
			require(!ImGui::IsPopupOpen(
						completionPopupId,
						ImGuiPopupFlags_AnyPopupLevel) &&
					!ConsumeMenuEscapeTarget(MenuEscapeTarget::kHost),
				"completion between capture and draw closed the parent");
			frame.End();
			event = {};
			require(PresentationServices::PollDialogEvent(
						17,
						completedBetweenFrames,
						&event,
						text,
						sizeof(text)) == DMUI_RESULT_OK &&
					event.kind == DMUI_DIALOG_EVENT_COMPLETED,
				"completion-race result was not delivered");
			ResetMenuEscapeRequest();
		});
	}
}
