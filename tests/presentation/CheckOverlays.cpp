#include "../support/D3DTestResources.h"
#include "../support/PresentationTestSupport.h"
#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>

namespace vmm_tests
{
	using namespace DearModdingUI;
	using support::CreateImageResources;
	using support::presentation::ImGuiFrame;

	void run_presentation_overlay_notification_plot_checks(Runner& runner)
	{
		runner.test("managed overlays validate and report consumer-owned placement", [] {
			ImGuiFrame frame;
			const DMUI_ManagedOverlayOptions options{
				sizeof(DMUI_ManagedOverlayOptions),
				DMUI_OVERLAY_ANCHOR_TOP_RIGHT,
				{ 10.0f, 10.0f },
				{ 440.0f, 40.0f },
				{ 440.0f, 160.0f },
				0.75f,
				1.25f,
				1,
				1,
				1,
				0
			};
			require(PresentationServices::ConfigureOverlay(9, 11, &options) ==
					DMUI_RESULT_OK,
				"valid managed overlay configuration failed");
			DMUI_ManagedOverlayPlacement placement{};
			placement.structSize = sizeof(placement);
			require(PresentationServices::QueryOverlay(9, 11, &placement) ==
						DMUI_RESULT_OK &&
					placement.anchor == DMUI_OVERLAY_ANCHOR_TOP_RIGHT &&
					placement.offset.x == 10.0f,
				"managed overlay placement did not preserve requested coordinates");
			require(PresentationServices::QueryOverlay(10, 11, &placement) ==
					DMUI_RESULT_PAGE_NOT_FOUND,
				"another owner queried managed placement");
			require(PresentationServices::BeginManagedOverlay(
						9, 11, "Scaled overlay", false) ==
					PresentationServices::ManagedOverlayBeginResult::kVisible,
				"configured managed overlay did not open");
			const auto* window = ImGui::GetCurrentWindow();
			require(
				(window->Flags & ImGuiWindowFlags_NoInputs) != 0 &&
					(window->Flags & ImGuiWindowFlags_NoMove) != 0,
				"passive managed overlay accepted gameplay input");
			require(std::abs(window->FontWindowScale - 1.25f) < 0.001f,
				"managed overlay content scale was not applied exactly once");
			PresentationServices::EndManagedOverlay();
			placement.structSize = sizeof(placement);
			require(PresentationServices::QueryOverlay(9, 11, &placement) ==
						DMUI_RESULT_OK &&
					std::abs(placement.size.x - 550.0f) < 1.0f &&
					std::abs(placement.size.y - 50.0f) < 1.0f &&
					std::abs(placement.position.x - 720.0f) < 1.0f &&
					std::abs(placement.position.y - 10.0f) < 1.0f,
				"managed overlay scaled dimensions or host-scale offset twice");
		});

		runner.test("latest notification survives an older expiry", [] {
			auto resources = CreateImageResources();
			PresentationServices::SetDevice(resources.device.Get());
			const DMUI_NotificationDescriptor first{
				sizeof(DMUI_NotificationDescriptor),
				DMUI_STATUS_SEVERITY_INFO,
				"first",
				250
			};
			const DMUI_NotificationDescriptor second{
				sizeof(DMUI_NotificationDescriptor),
				DMUI_STATUS_SEVERITY_WARNING,
				"second",
				1000
			};
			require(PresentationServices::PostNotification(1, &first) ==
					DMUI_RESULT_OK,
				"first notification failed");
			std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });
			DMUI_Result postResult{};
			std::thread poster{ [&] {
				postResult =
					PresentationServices::PostNotification(2, &second);
			} };
			poster.join();
			require(postResult == DMUI_RESULT_OK,
				"worker notification failed");
			std::this_thread::sleep_for(std::chrono::milliseconds{ 175 });
			require(PresentationServices::HasFrameDemand(),
				"older expiry erased the newer notification");
			const DMUI_NotificationDescriptor expiring{
				sizeof(DMUI_NotificationDescriptor),
				DMUI_STATUS_SEVERITY_INFO,
				"expires",
				250
			};
			require(PresentationServices::PostNotification(1, &expiring) ==
					DMUI_RESULT_OK,
				"expiring notification failed");
			std::this_thread::sleep_for(std::chrono::milliseconds{ 275 });
			require(!PresentationServices::HasFrameDemand(),
				"expired passive notification retained frame demand");

			const DMUI_NotificationDescriptor teardownNotification{
				sizeof(DMUI_NotificationDescriptor),
				DMUI_STATUS_SEVERITY_INFO,
				"teardown",
				5000
			};
			require(PresentationServices::PostNotification(
						9, &teardownNotification) == DMUI_RESULT_OK &&
					PresentationServices::HasFrameDemand(),
				"active notification did not demand a frame");
			PresentationServices::InvalidateDevice();
			require(!PresentationServices::HasFrameDemand(),
				"backend teardown retained presentation demand");
			PresentationServices::SetDevice(resources.device.Get());
			require(!PresentationServices::HasFrameDemand(),
				"backend recovery revived a discarded notification");
			PresentationServices::InvalidateDevice();
		});

		runner.test("annotated plots clip references to the visible frame", [] {
			ImGuiFrame frame;
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameDraw
			};
			(void)execution.NoteBinding(1);
			const RenderExecution::ClientGuard callback{ 12, true };
			const float values[]{ 4.0f, 8.0f, 12.0f };
			const DMUI_PlotReferenceLine lines[]{
				{ 8.0f, { 0.123f, 0.456f, 0.789f, 0.654f } }
			};
			DMUI_AnnotatedPlotDescriptor descriptor{
				sizeof(DMUI_AnnotatedPlotDescriptor),
				values,
				3,
				0,
				0.0f,
				16.0f,
				{ 300.0f, 80.0f },
				"8 ms",
				lines,
				1
			};
			const auto origin = ImGui::GetCursorScreenPos();
			const auto expectedMaximumX =
				origin.x + descriptor.size.x -
				ImGui::GetStyle().FramePadding.x;
			require(PresentationServices::DrawAnnotatedPlot(
						12, "Visible plot label", &descriptor) ==
					DMUI_RESULT_OK,
				"valid annotated plot failed");
			require(ImGui::GetItemRectMax().x > origin.x + descriptor.size.x,
				"visible plot label did not extend the total item bounds");
			const auto referenceColor =
				ImGui::ColorConvertFloat4ToU32({
					lines[0].color.x,
					lines[0].color.y,
					lines[0].color.z,
					lines[0].color.w
				});
			auto foundReference = false;
			auto maximumReferenceX = -(std::numeric_limits<float>::max)();
			for (const auto& vertex : ImGui::GetWindowDrawList()->VtxBuffer)
			{
				if (vertex.col != referenceColor)
					continue;
				foundReference = true;
				maximumReferenceX = (std::max)(maximumReferenceX, vertex.pos.x);
			}
			require(foundReference &&
					maximumReferenceX <= expectedMaximumX + 2.0f,
				"reference line extended into the visible plot label");
			const float invalid[]{ std::numeric_limits<float>::quiet_NaN() };
			descriptor.samples = invalid;
			descriptor.sampleCount = 1;
			require(PresentationServices::DrawAnnotatedPlot(
						12, "invalid", &descriptor) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"non-finite plot sample was accepted");
		});

	}
}
