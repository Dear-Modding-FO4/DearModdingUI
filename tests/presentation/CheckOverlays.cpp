#include "../support/D3DTestResources.h"
#include "../support/PresentationTestSupport.h"
#include "../support/DearModdingUITestSupport.h"
#include "../../src/DearModdingUI/presentation/notifications/Queue.h"
#include <DearModdingUI/presentation/PresentationServices.h>
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
			for (int frame = 0; frame < 3; ++frame)
			{
				imgui.BeginWindow("##OverlayRestoreTest");
				if (frame == 1)
				{
					ImGui::SetWindowSize("Restore overlay", { 560, 300 });
					ImGui::SetWindowPos("Restore overlay", { 210, 180 });
				}
				const auto placement = draw(12, "Restore overlay");
				if (frame == 2)
				{
					options.size = placement.size;
					options.offset = placement.offset;
					configure(13);
					const auto restored = draw(13, "Round-trip overlay");
					require(sizeEquals(restored.size, placement.size) &&
							sizeEquals(restored.position, placement.position),
						"persisted geometry did not round-trip with content and host scaling");
				}
				imgui.EndWindow();
			}

			options.allowArrangement = 0;
			configure(14);
			DMUI_ManagedOverlayPlacement foreign{};
			require(PresentationServices::QueryOverlay(10, 14, &foreign) == DMUI_RESULT_PAGE_NOT_FOUND,
				"another owner queried managed placement");
			imgui.BeginWindow("##OverlayPassiveTest");
			require(PresentationServices::BeginManagedOverlay(9, 14, "Passive overlay", false) ==
					PresentationServices::ManagedOverlayBeginResult::kVisible,
				"passive managed overlay did not open");
			const auto flags = ImGui::GetCurrentWindow()->Flags;
			PresentationServices::EndManagedOverlay();
			imgui.EndWindow();
			require((flags & ImGuiWindowFlags_NoInputs) != 0 && (flags & ImGuiWindowFlags_NoMove) != 0,
				"passive managed overlay accepted gameplay input");

			options.size.x = -1;
			require(PresentationServices::ConfigureOverlay(9, 12, &options) ==
					DMUI_RESULT_INVALID_ARGUMENT, "negative desired size was accepted");
			options.size.x = std::numeric_limits<float>::quiet_NaN();
			require(PresentationServices::ConfigureOverlay(9, 12, &options) ==
					DMUI_RESULT_INVALID_ARGUMENT, "non-finite desired size was accepted");
		});

		runner.test("managed overlay settings restore defaults reset and retain absent mods", [] {
			{
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
			}
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
	}
}
