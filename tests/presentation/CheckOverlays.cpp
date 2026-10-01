#include "../support/D3DTestResources.h"
#include "../support/PresentationTestSupport.h"
#include "../support/DearModdingUITestSupport.h"
#include "../../src/DearModdingUI/presentation/notifications/Queue.h"
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
			require(PresentationServices::QueryOverlay(9, 11, &placement) ==
						DMUI_RESULT_OK &&
					std::abs(placement.size.x - 550.0f) < 1.0f &&
					std::abs(placement.size.y - 50.0f) < 1.0f &&
					std::abs(placement.position.x - 720.0f) < 1.0f &&
					std::abs(placement.position.y - 10.0f) < 1.0f,
				"managed overlay scaled dimensions or host-scale offset twice");
		});

		runner.test("toast expiry starts at presentation and hover pauses it", [] {
			using namespace PresentationServices::Notifications;
			using namespace std::chrono_literals;
			Queue queue;
			const Clock::time_point start{};
			(void)queue.Post({ .owner = 1, .message = "Waiting", .duration = 250ms });
			queue.Advance(start + 10s);
			const auto id = queue.Visible().front().id;
			queue.Advance(start + 20s);
			require(queue.Visible().size() == 1, "undrawn toast expired");
			queue.Presented(id, start + 20s);
			queue.Advance(start + 20100ms);
			queue.Advance(start + 30s, id);
			require(queue.Visible().size() == 1, "hover did not pause expiry");
			queue.Advance(start + 30149ms);
			require(!queue.Empty(), "toast expired before its remaining lifetime");
			queue.Advance(start + 30150ms);
			require(queue.Empty(), "toast did not expire after presentation and hover");
		});

		runner.test("toast queue caps visible entries and drops oldest waiting entries", [] {
			using namespace PresentationServices::Notifications;
			using namespace std::chrono_literals;
			Queue queue;
			const Clock::time_point start{};
			for (int index = 0; index < 4; ++index)
				(void)queue.Post({ .owner = 1, .message = std::to_string(index), .duration = 250ms });
			queue.Advance(start);
			for (const auto& toast : queue.Visible())
				queue.Presented(toast.id, start);
			size_t dropped{};
			for (int index = 4; index < 40; ++index)
				dropped += queue.Post({ .owner = 1, .message = std::to_string(index), .duration = 250ms });
			require(queue.Visible().size() == 4 && queue.Pending().size() == 32 &&
					dropped == 4 && queue.Pending().front().message == "8" &&
					queue.Visible().front().message == "0",
				"overflow dropped a visible toast or retained the wrong queued toast");
			queue.Advance(start + 1s);
			require(queue.Visible().size() == 4 && queue.Visible().front().message == "8",
				"queued toasts expired or did not promote FIFO");
		});

		runner.test("consecutive identical toasts coalesce and renew presentation lifetime", [] {
			using namespace PresentationServices::Notifications;
			using namespace std::chrono_literals;
			Queue queue;
			const Clock::time_point start{};
			Toast toast{ .owner = 1, .title = "Title", .message = "Duplicate", .duration = 250ms };
			(void)queue.Post(toast);
			(void)queue.Post(toast);
			queue.Advance(start);
			queue.Presented(queue.Visible().front().id, start);
			queue.Advance(start + 200ms);
			(void)queue.Post(toast);
			queue.Advance(start + 10s);
			require(queue.Visible().size() == 1 && queue.Visible().front().count == 3,
				"duplicates did not coalesce across promotion or renew lifetime");
			toast.owner = 2;
			(void)queue.Post(toast);
			toast.owner = 1;
			(void)queue.Post(toast);
			toast.title = "Other title";
			(void)queue.Post(toast);
			queue.Advance(start + 10s);
			require(queue.Visible().size() == 4 && queue.Visible().back().count == 1,
				"nonconsecutive, foreign, or differently titled posts coalesced");
		});

		runner.test("worker toasts copy registered attribution and retain demand until drawn", [] {
			using namespace support::host;
			Registry registry;
			CallbackState state;
			const auto client = AddClient(registry, "toast.mod", "Registered Mod", state);
			auto resources = CreateImageResources();
			PresentationServices::SetDevice(resources.device.Get());
			const std::string message = std::string(1000, '\n') + "Worker message";
			const DMUI_NotificationDescriptor descriptor{
				DMUI_STATUS_SEVERITY_INFO,
				message.c_str(),
				250,
				"Optional title"
			};
			std::atomic<size_t> failures{};
			std::vector<std::jthread> workers;
			for (int index = 0; index < 4; ++index)
				workers.emplace_back([&] {
					for (int post = 0; post < 25; ++post)
					{
						if (PresentationServices::PostNotification(registry, client, &descriptor) != DMUI_RESULT_OK)
							++failures;
						(void)PresentationServices::HasFrameDemand();
					}
				});
			workers.clear();
			auto snapshot = PresentationServices::Notifications::Snapshot();
			require(failures == 0 && snapshot.Pending().size() == 1 &&
					snapshot.Pending().front().count == 100 &&
					snapshot.Pending().front().clientName == "Registered Mod" &&
					snapshot.Pending().front().title == "Optional title" &&
					snapshot.Pending().front().message == message,
				"concurrent posting lost entries or registered attribution");
			require(PresentationServices::PostNotification(registry, 9999, &descriptor) ==
					DMUI_RESULT_CLIENT_NOT_FOUND,
				"unregistered client posted a toast");
			{
				ImGuiFrame frame;
				const auto* focused = ImGui::GetCurrentContext()->NavWindow;
				dmui::ui::ForegroundDrawList().AddRectFilled({ 10, 10 }, { 60, 60 }, 0x123456FF);
				auto* foreground = ImGui::GetForegroundDrawList();
				const auto clientEnd = foreground->VtxBuffer.Size;
				PresentationServices::DrawNotifications(false);
				require(ImGui::GetCurrentContext()->NavWindow == focused &&
						clientEnd > 0 && foreground->VtxBuffer.Size > clientEnd &&
						foreground->VtxBuffer[0].col == IM_COL32(0x12, 0x34, 0x56, 255) &&
						PresentationServices::Notifications::Snapshot().Visible().front().lastUpdate.has_value(),
					"toasts did not append after client foreground geometry without taking focus");
			}
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
