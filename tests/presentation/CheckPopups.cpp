#include "../Harness.h"
#include "../support/ImGuiTestContext.h"
#include <DearModdingUI/host/ControllerNavigation.h>
#include <DearModdingUI/host/ImGuiRecovery.h>
#include <DearModdingUI/host/MenuDismissal.h>
#include <DearModdingUI/host/ModalCoordinator.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <DearModdingUI/host/UIAdapter.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/UI.h>
#include <imgui/imgui_internal.h>

namespace vmm_tests
{
	using namespace DearModdingUI;
	namespace
	{
		class PopupFrame
		{
		public:
			PopupFrame()
			{
				ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
				ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_HasGamepad;
			}
			~PopupFrame()
			{
				ModalCoordinator::ClosePages();
				ResetMenuEscapeRequest();
				ControllerNavigation::Reset();
			}
			void Begin()
			{
				imgui.BeginWindow("Popup tests", { 20, 20 }, { 800, 550 });
			}
			void End() { imgui.EndWindow(); }
			support::ImGuiTestContext imgui{ { .disableErrorRecovery = true } };
			RenderExecution::Guard execution{ RenderExecution::Phase::kFrameDraw };
			dmui::ui::detail::ScopedContext ui{ &UI::API(), 77 };
		};

		const DMUI_DialogDescriptor kConfirm{
			DMUI_DIALOG_KIND_CONFIRM, "Host confirmation", "Harmless operation",
			"Confirm", "Cancel", nullptr, nullptr, 1
		};
	}

	void run_popup_checks(Runner& runner)
	{
		runner.test("modal arbitration preserves blocked opens and page identity", [] {
			PopupFrame frame;
			frame.Begin();
			bool firstOpen{ true }, secondOpen{ true };
			DMUI_DialogHandle dialog{};
			{
				RenderExecution::ClientGuard page{ 77, true, 101 };
				dmui::ui::OpenPopup("Same title");
				require(dmui::ui::BeginPopupModal("Same title", firstOpen),
					"first page could not acquire the chain");
				dmui::ui::EndPopup();
				require(PresentationServices::RequestDialog(77, &kConfirm, &dialog, true) ==
					DMUI_RESULT_BUSY, "host dialog stacked over a client modal");
			}
			const auto firstId = GImGui->OpenPopupStack.back().PopupId;
			{
				RenderExecution::ClientGuard page{ 77, true, 102 };
				require(!dmui::ui::IsPopupOpen("Same title"), "two pages shared a popup ID");
				dmui::ui::OpenPopup("Same title");
				require(!dmui::ui::BeginPopupModal("Same title", secondOpen) &&
					secondOpen && dmui::ui::IsPopupOpen("Same title") &&
					GImGui->OpenPopupStack.Size == 1,
					"second owner stacked or lost its pending request");
			}
			ModalCoordinator::ClosePage(101);
			{
				RenderExecution::ClientGuard page{ 77, true, 102 };
				require(dmui::ui::BeginPopupModal("Same title", secondOpen),
					"pending modal did not acquire the freed chain");
				require(GImGui->OpenPopupStack.back().PopupId != firstId,
					"modal native windows collided across pages");
				dmui::ui::CloseCurrentPopup();
				dmui::ui::EndPopup();
				require(PresentationServices::RequestDialog(77, &kConfirm, &dialog, true) ==
					DMUI_RESULT_OK, "host dialog could not acquire the released chain");
				dmui::ui::OpenPopup("Waiting for host");
				bool waiting{ true };
				require(!dmui::ui::BeginPopupModal("Waiting for host", waiting) && waiting,
					"client modal bypassed a reserved host dialog");
				require(PresentationServices::CancelDialog(77, dialog) == DMUI_RESULT_OK,
					"host cancellation failed");
				DMUI_DialogEvent event{};
				require(PresentationServices::PollDialogEvent(77, dialog, &event, nullptr, 0) ==
					DMUI_RESULT_OK, "host cancellation was not delivered");
				require(dmui::ui::BeginPopupModal("Waiting for host", waiting),
					"host completion discarded the pending client modal");
				dmui::ui::EndPopup();
			}
			require(frame.ui.Result() == DMUI_RESULT_OK, "arbitration became a UI error");
			frame.End();
		});

		runner.test("page retirement and callback failure clear popup brackets", [] {
			PopupFrame frame;
			RenderExecution::ClientGuard page{ 77, true, 104 };
			frame.Begin();
			dmui::ui::OpenPopup("Retired");
			require(dmui::ui::BeginPopup("Retired"), "non-modal popup did not open");
			dmui::ui::EndPopup();
			ModalCoordinator::ClosePage(104);
			require(!dmui::ui::IsPopupOpen("Retired") && GImGui->OpenPopupStack.empty(),
				"page retirement retained an owned popup");
			auto recovery = ImGuiRecoverySnapshot::Capture();
			dmui::ui::OpenPopup("Failed");
			bool open{ true };
			require(dmui::ui::BeginPopupModal("Failed", open), "failure fixture did not open");
			require(ModalCoordinator::HasBracket({ 77, 104 }), "unbalanced popup was not detected");
			(void)recovery->RecoverFailure();
			ModalCoordinator::ClosePage(104);
			require(!ModalCoordinator::HasBracket({ 77, 104 }) &&
				GImGui->BeginPopupStack.empty() && GImGui->OpenPopupStack.empty() &&
				GImGui->CurrentWindowStack.Size == 2,
				"failed callback retained popup state or unbalanced windows");
			require(UI::API().endPopup(77) == DMUI_RESULT_INVALID_ARGUMENT &&
				UI::API().closeCurrentPopup(77) == DMUI_RESULT_INVALID_ARGUMENT,
				"unmatched popup ends reached ImGui");
			dmui::ui::OpenPopup("Scoped failure");
			{
				dmui::ui::detail::ScopedContext failedUI{ &UI::API(), 77 };
				{
					dmui::ui::PopupScope popup{ "Scoped failure" };
					require(static_cast<bool>(popup), "RAII popup did not begin");
					dmui::ui::OpenPopup(nullptr);
					require(failedUI.Result() == DMUI_RESULT_INVALID_ARGUMENT,
						"invalid popup ID did not enter the callback failure model");
				}
				require(GImGui->BeginPopupStack.empty(),
					"RAII popup did not end after a sticky UI error");
			}
			ModalCoordinator::ClosePage(104);
			dmui::ui::OpenPopup("Absent page");
			require(dmui::ui::BeginPopup("Absent page"), "absent fixture did not open");
			dmui::ui::EndPopup();
			frame.End();
			frame.Begin();
			ModalCoordinator::FinishFrame();
			require(!dmui::ui::IsPopupOpen("Absent page"), "a page that stopped drawing retained its popup");
			dmui::ui::OpenPopup("Menu close");
			ModalCoordinator::NotifyMenuClosed();
			ModalCoordinator::BeginFrame();
			require(!dmui::ui::IsPopupOpen("Menu close"), "menu close retained a pending open");
			frame.End();
		});
	}
}
