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
		runner.test("managed overlays validate and report host-owned placement", [] {
			ImGuiFrame frame;
			const DMUI_ManagedOverlayOptions options{
				DMUI_OVERLAY_ANCHOR_TOP_RIGHT,
				{ 10.0f, 10.0f },
				{},
				{ 440.0f, 40.0f },
				{ 440.0f, 160.0f },
				0.75f,
				1.25f,
				1,
				1,
				1
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

		runner.test("managed overlay geometry restores once in placement units", [] {
			support::ImGuiTestContext imgui;
			ImGui::GetIO().FontGlobalScale = 1.5f;
			DMUI_ManagedOverlayOptions options{
				.anchor = DMUI_OVERLAY_ANCHOR_FREE,
				.offset = { 60.0f, 80.0f },
				.size = { 520.0f, 260.0f },
				.minimumSize = { 80.0f, 80.0f },
				.maximumSize = { 800.0f, 400.0f },
				.opacity = 1.0f,
				.contentScale = 1.25f,
				.backgroundVisible = 1,
				.borderVisible = 1,
				.allowArrangement = 1
			};
			const auto configure = [&](DMUI_PageHandle a_page) {
				require(PresentationServices::ConfigureOverlay(9, a_page, &options) ==
						DMUI_RESULT_OK, "overlay reconfiguration failed");
			};
			const auto draw = [&](DMUI_PageHandle a_page, const char* a_label) {
				require(PresentationServices::BeginManagedOverlay(9, a_page, a_label, true) ==
						PresentationServices::ManagedOverlayBeginResult::kVisible,
					"restored overlay did not open");
				PresentationServices::EndManagedOverlay();
				DMUI_ManagedOverlayPlacement placement{};
				require(PresentationServices::QueryOverlay(9, a_page, &placement) == DMUI_RESULT_OK,
					"restored overlay placement was unavailable");
				return placement;
			};
			const auto sizeEquals = [](DMUI_Vec2 a_size, DMUI_Vec2 a_expected) {
				return std::abs(a_size.x - a_expected.x) < 0.01f &&
					std::abs(a_size.y - a_expected.y) < 0.01f;
			};
			configure(12);
			for (int frame = 0; frame < 10; ++frame)
			{
				if (frame == 8 || frame == 9)
					ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, frame == 8);
				imgui.BeginWindow("##OverlayRestoreTest");
				if (frame == 1)
				{
					ImGui::SetWindowSize("Restore overlay", { 560, 300 });
					ImGui::SetWindowPos("Restore overlay", { 210, 180 });
				}
				if (frame == 2)
					configure(12);
				if (frame == 4)
				{
					options.size = { 640, 340 };
					options.offset = { 100, 100 };
					configure(12);
				}
				if (frame == 5)
				{
					options.size = { 20, 5000 };
					configure(12);
				}
				if (frame == 6)
				{
					options.size = { 400, 0 };
					configure(12);
				}
				if (frame == 7)
				{
					options.anchor = DMUI_OVERLAY_ANCHOR_TOP_RIGHT;
					configure(12);
				}
				if (frame == 8)
					ImGui::SetWindowSize("Restore overlay", { 460, 220 });
				const auto placement = draw(12, "Restore overlay");
				if (frame == 0)
					require(sizeEquals(placement.size, options.size) &&
							sizeEquals(placement.position, { 90, 120 }),
						"restored size was scaled or initial offset was not host-scaled");
				if (frame == 1 || frame == 2)
					require(sizeEquals(placement.size, { 560, 300 }) &&
							sizeEquals(placement.offset, { 140, 120 }),
						"unchanged configuration snapped user geometry or failed to report free offset");
				if (frame == 3)
				{
					options.size = placement.size;
					options.offset = placement.offset;
					configure(13);
					const auto restored = draw(13, "Round-trip overlay");
					require(sizeEquals(restored.size, placement.size) &&
							sizeEquals(restored.position, placement.position),
						"persisted geometry did not round-trip with content and host scaling");
				}
				if (frame == 4)
					require(sizeEquals(placement.size, options.size) &&
							sizeEquals(placement.position, { 150, 150 }),
						"changed size or free offset was not applied on the next frame");
				if (frame == 5)
					require(sizeEquals(placement.size, { 150, 750 }),
						"desired size bypassed scaled min/max constraints");
				if (frame == 6)
					require(sizeEquals(placement.size, { 400, 150 }),
						"zero size component did not use its scaled minimum");
				if (frame == 9)
					require(sizeEquals(placement.size, { 460, 220 }) &&
							placement.arrangementCompleted,
						"anchored resize did not retain user size or report arrangement completion");
				imgui.EndWindow();
			}
			options.size.x = -1;
			require(PresentationServices::ConfigureOverlay(9, 12, &options) ==
					DMUI_RESULT_INVALID_ARGUMENT, "negative desired size was accepted");
			options.size.x = std::numeric_limits<float>::quiet_NaN();
			require(PresentationServices::ConfigureOverlay(9, 12, &options) ==
					DMUI_RESULT_INVALID_ARGUMENT, "non-finite desired size was accepted");
		});

		runner.test("managed overlay settings restore defaults reset and retain absent mods", [] {
			constexpr auto settings =
				"[DMUIOverlay][6d6f64/687564]\nAnchor=4\nOffset=140,120\nSize=560,300\n\n"
				"[DMUIOverlay][616273656e74/687564]\nOffset=20,30\nSize=200,100\n\n";
			std::string written;
			{
				support::ImGuiTestContext imgui;
				PresentationServices::RegisterOverlaySettings();
				ImGui::LoadIniSettingsFromMemory(settings);
				written = ImGui::SaveIniSettingsToMemory();
			}
			support::ImGuiTestContext imgui;
			PresentationServices::RegisterOverlaySettings();
			ImGui::LoadIniSettingsFromMemory(written.c_str());
			ImGui::GetIO().FontGlobalScale = 1.5f;
			DMUI_ManagedOverlayOptions options{
				.anchor = DMUI_OVERLAY_ANCHOR_FREE,
				.offset = { 60, 80 },
				.size = { 520, 260 },
				.minimumSize = { 80, 80 },
				.opacity = 1,
				.contentScale = 1.25f,
				.backgroundVisible = 1,
				.borderVisible = 1,
				.allowArrangement = 1
			};
			const auto configure = [&] {
				require(PresentationServices::ConfigureOverlay(90, 110, &options, "mod", "hud") ==
					DMUI_RESULT_OK, "persisted overlay configure failed");
			};
			configure();
			for (int frame = 0; frame < 6; ++frame)
			{
				imgui.BeginWindow("##SavedOverlayTest");
				if (frame == 1)
				{
					options.offset = { 100, 100 };
					options.size = { 640, 340 };
					configure();
					DMUI_ManagedOverlayPlacement beforeDraw{};
					require(PresentationServices::QueryOverlay(90, 110, &beforeDraw) == DMUI_RESULT_OK &&
						beforeDraw.size.x == 560 && beforeDraw.size.y == 300,
						"configuration replaced reported geometry before presentation");
				}
				if (frame == 2)
				{
					ImGui::SetWindowPos("Saved overlay", { 240, 210 });
					ImGui::SetWindowSize("Saved overlay", { 600, 320 });
					configure();
				}
				if (frame == 3)
				{
					require(PresentationServices::ResetOverlay(91, 110) == DMUI_RESULT_PAGE_NOT_FOUND,
						"foreign owner reset placement");
					require(PresentationServices::ResetOverlay(90, 110) == DMUI_RESULT_OK,
						"overlay reset failed");
					require(GImGui->SettingsDirtyTimer > 0, "reset did not dirty settings");
				}
				if (frame == 4)
				{
					ImGui::SetWindowSize("Saved overlay", { 600, 320 });
					configure();
				}
				if (frame == 5)
				{
					options.size = {};
					configure();
					require(PresentationServices::ResetOverlay(90, 110) == DMUI_RESULT_OK,
						"automatic-size reset failed");
				}
				require(PresentationServices::BeginManagedOverlay(90, 110, "Saved overlay", true) ==
					PresentationServices::ManagedOverlayBeginResult::kVisible, "saved overlay did not open");
				PresentationServices::EndManagedOverlay();
				DMUI_ManagedOverlayPlacement placement{};
				require(PresentationServices::QueryOverlay(90, 110, &placement) == DMUI_RESULT_OK,
					"saved overlay query failed");
				const auto expectedPosition = frame == 0 ? DMUI_Vec2{ 210, 180 } :
					frame == 2 ? DMUI_Vec2{ 240, 210 } : DMUI_Vec2{ 150, 150 };
				const auto expectedSize = frame == 0 ? DMUI_Vec2{ 560, 300 } :
					(frame == 2 || frame == 4) ? DMUI_Vec2{ 600, 320 } :
					frame == 5 ? DMUI_Vec2{ 150, 150 } : options.size;
				require(placement.position.x == expectedPosition.x && placement.position.y == expectedPosition.y &&
					placement.size.x == expectedSize.x && placement.size.y == expectedSize.y,
					"saved/default geometry did not apply once");
				imgui.EndWindow();
			}
			written = ImGui::SaveIniSettingsToMemory();
			require(written.find("[DMUIOverlay][6d6f64/687564]") == std::string::npos &&
				written.find("[DMUIOverlay][616273656e74/687564]\nAnchor=0\nSize=200,100") != std::string::npos,
				"reset retained its record or write dropped an absent mod");
		});

		runner.test("saved overlay anchors never reinterpret offsets or freeze author insets", [] {
			support::ImGuiTestContext imgui;
			PresentationServices::RegisterOverlaySettings();
			ImGui::LoadIniSettingsFromMemory(
				"[DMUIOverlay][6d6f64/66726565]\nAnchor=4\nOffset=900,600\nSize=320,200\n\n"
				"[DMUIOverlay][6d6f64/636f726e6572]\nAnchor=3\nOffset=400,300\nSize=320,200\n\n"
				"[DMUIOverlay][6d6f64/6c6567616379]\nOffset=900,600\nSize=320,200\n\n");
			const struct
			{
				const char* id;
				DMUI_OverlayAnchor anchor;
				DMUI_Vec2 expected;
			} cases[]{
				{ "free", DMUI_OVERLAY_ANCHOR_BOTTOM_RIGHT, { 935, 485 } },
				{ "corner", DMUI_OVERLAY_ANCHOR_FREE, { 25, 35 } },
				{ "corner", DMUI_OVERLAY_ANCHOR_BOTTOM_RIGHT, { 935, 485 } },
				{ "legacy", DMUI_OVERLAY_ANCHOR_FREE, { 25, 35 } }
			};
			imgui.BeginWindow("##SavedAnchorTest");
			for (size_t index = 0; index < std::size(cases); ++index)
			{
				const auto& test = cases[index];
				const DMUI_ManagedOverlayOptions options{
					.anchor = test.anchor,
					.offset = { 25, 35 },
					.size = { 200, 100 },
					.opacity = 1,
					.contentScale = 1
				};
				const auto page = static_cast<DMUI_PageHandle>(120 + index);
				require(PresentationServices::ConfigureOverlay(90, page, &options, "mod", test.id) ==
					DMUI_RESULT_OK, "anchor overlay configure failed");
				const auto label = std::string{ "Anchor overlay " } + std::to_string(index);
				(void)PresentationServices::BeginManagedOverlay(90, page, label, false);
				PresentationServices::EndManagedOverlay();
				DMUI_ManagedOverlayPlacement placement{};
				require(PresentationServices::QueryOverlay(90, page, &placement) == DMUI_RESULT_OK &&
					placement.size.x == 320 && placement.size.y == 200 &&
					placement.position.x == test.expected.x && placement.position.y == test.expected.y &&
					placement.offset.x == 25 && placement.offset.y == 35,
					"saved anchor changed the author's inset or discarded arranged size");
			}
			imgui.EndWindow();
		});

		runner.test("anchored manual resize stays stable with a stationary mouse", [] {
			support::ImGuiTestContext imgui{ { .disableInputTrickle = true } };
			PresentationServices::RegisterOverlaySettings();
			const DMUI_ManagedOverlayOptions options{
				.anchor = DMUI_OVERLAY_ANCHOR_BOTTOM_RIGHT,
				.offset = { 100, 100 },
				.size = { 320, 200 },
				.minimumSize = { 80, 80 },
				.opacity = 1,
				.contentScale = 1,
				.backgroundVisible = 1,
				.borderVisible = 1,
				.allowArrangement = 1
			};
			require(PresentationServices::ConfigureOverlay(9, 14, &options, "resize", "hud") == DMUI_RESULT_OK,
				"resize overlay configuration failed");
			ImVec2 grip{};
			DMUI_Vec2 resized{};
			for (int frame = 0; frame < 9; ++frame)
			{
				auto& io = ImGui::GetIO();
				if (frame == 2)
					io.AddMousePosEvent(grip.x, grip.y);
				if (frame == 3)
					io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
				if (frame == 4)
					io.AddMousePosEvent(grip.x - 40, grip.y - 30);
				if (frame == 8)
					io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
				imgui.BeginWindow("##OverlayManualResizeTest");
				require(PresentationServices::BeginManagedOverlay(9, 14, "Manual resize overlay", true) ==
						PresentationServices::ManagedOverlayBeginResult::kVisible,
					"resize overlay did not open");
				auto* window = ImGui::GetCurrentWindow();
				if (frame == 1)
					grip = { window->Pos.x + window->Size.x - 2, window->Pos.y + window->Size.y - 2 };
				if (frame >= 3 && frame < 8)
					require(GImGui->ActiveId == ImGui::GetWindowResizeCornerID(window, 0),
						"mouse did not engage the manual resize grip");
				PresentationServices::EndManagedOverlay();
				DMUI_ManagedOverlayPlacement placement{};
				require(PresentationServices::QueryOverlay(9, 14, &placement) == DMUI_RESULT_OK,
					"resize placement was unavailable");
				if (frame == 4)
				{
					resized = placement.size;
					require(resized.x < options.size.x && resized.y < options.size.y,
						"manual grip drag did not resize both axes");
				}
				if (frame > 4)
					require(placement.size.x == resized.x && placement.size.y == resized.y,
						"anchored overlay compounded resize while the mouse stayed still");
				if (frame == 8)
					require(placement.arrangementCompleted &&
							placement.position.x + placement.size.x == io.DisplaySize.x - options.offset.x &&
							placement.position.y + placement.size.y == io.DisplaySize.y - options.offset.y,
						"resize release did not restore the final bottom-right anchor");
				imgui.EndWindow();
			}
			const std::string saved = ImGui::SaveIniSettingsToMemory();
			require(saved.find("[DMUIOverlay][726573697a65/687564]\nAnchor=3\nSize=") != std::string::npos,
				"completed resize was not written through ImGui settings");
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
			support::ImGuiTestContext frame;
			frame.BeginWindow("Annotated plot", { 20, 20 }, { 640, 480 });
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
			const auto* draw = ImGui::GetWindowDrawList();
			int lastReference{ -1 }, firstCurve{ -1 }, lastText{ -1 };
			for (int index = 0; index < draw->IdxBuffer.Size; ++index)
			{
				const auto color = draw->VtxBuffer[draw->IdxBuffer[index]].col;
				if (color == referenceColor)
					lastReference = index;
				if (color == ImGui::GetColorU32(ImGuiCol_PlotLines) && firstCurve < 0)
					firstCurve = index;
				if (color == ImGui::GetColorU32(ImGuiCol_Text))
					lastText = index;
			}
			require(lastReference >= 0 && firstCurve > lastReference && lastText > firstCurve,
				"plot references covered the curve or overlay: reference=" + std::to_string(lastReference) +
					", curve=" + std::to_string(firstCurve) + ", text=" + std::to_string(lastText));
			const float invalid[]{ std::numeric_limits<float>::quiet_NaN() };
			descriptor.samples = invalid;
			descriptor.sampleCount = 1;
			require(PresentationServices::DrawAnnotatedPlot(
						12, "invalid", &descriptor) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"non-finite plot sample was accepted");
			frame.EndWindow();
		});

	}
}
