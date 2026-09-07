#include <DearModdingUI/PresentationServices.h>
#include <DearModdingUI/MenuDismissal.h>
#include "Harness.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <DearModdingUI/Client.h>

#include <chrono>
#include <cmath>
#include <limits>
#include <thread>

namespace vmm_tests
{
	namespace
	{
		using Microsoft::WRL::ComPtr;
		using namespace DearModdingUI;

		class ImGuiFrame
		{
		public:
			ImGuiFrame()
			{
				m_context = ImGui::CreateContext();
				auto& io = ImGui::GetIO();
				io.DisplaySize = { 1280.0f, 720.0f };
				io.DeltaTime = 1.0f / 60.0f;
				io.IniFilename = nullptr;
				io.ConfigErrorRecoveryEnableAssert = false;
				io.ConfigErrorRecoveryEnableDebugLog = false;
				io.ConfigErrorRecoveryEnableTooltip = false;
				(void)io.Fonts->Build();
				ImGui::NewFrame();
				(void)ImGui::Begin("##PresentationServicesTest");
			}

			~ImGuiFrame()
			{
				ImGui::End();
				ImGui::EndFrame();
				ImGui::DestroyContext(m_context);
			}

		private:
			ImGuiContext* m_context{};
		};

		class InteractiveImGui
		{
		public:
			InteractiveImGui()
			{
				m_context = ImGui::CreateContext();
				auto& io = ImGui::GetIO();
				io.DisplaySize = { 640.0f, 480.0f };
				io.DeltaTime = 1.0f / 60.0f;
				io.IniFilename = nullptr;
				io.ConfigInputTrickleEventQueue = false;
				io.ConfigErrorRecoveryEnableAssert = false;
				io.ConfigErrorRecoveryEnableDebugLog = false;
				io.ConfigErrorRecoveryEnableTooltip = false;
				(void)io.Fonts->Build();
			}

			~InteractiveImGui()
			{
				ImGui::DestroyContext(m_context);
			}

			void Begin(
				ImVec2 a_mouse,
				bool a_mouseDown,
				const char* a_input = nullptr)
			{
				auto& io = ImGui::GetIO();
				io.AddMousePosEvent(a_mouse.x, a_mouse.y);
				io.AddMouseButtonEvent(ImGuiMouseButton_Left, a_mouseDown);
				if (a_input)
					io.AddInputCharactersUTF8(a_input);
				ImGui::NewFrame();
				ImGui::SetNextWindowPos({ 0.0f, 0.0f }, ImGuiCond_Always);
				ImGui::SetNextWindowSize({ 640.0f, 480.0f }, ImGuiCond_Always);
				(void)ImGui::Begin(
					"##InteractivePresentationTest",
					nullptr,
					ImGuiWindowFlags_NoDecoration |
						ImGuiWindowFlags_NoSavedSettings);
				ImGui::SetCursorScreenPos({ 20.0f, 20.0f });
				ImGui::SetNextItemWidth(200.0f);
			}

			void End()
			{
				ImGui::End();
				ImGui::Render();
			}

			void Key(ImGuiKey a_key, bool a_down)
			{
				ImGui::GetIO().AddKeyEvent(a_key, a_down);
			}

		private:
			ImGuiContext* m_context{};
		};

		struct DeviceResources
		{
			ComPtr<ID3D11Device> device;
			ComPtr<ID3D11DeviceContext> context;
			ComPtr<ID3D11Texture2D> texture;
			ComPtr<ID3D11ShaderResourceView> view;
		};

		[[nodiscard]] DeviceResources CreateImageResources()
		{
			DeviceResources resources;
			D3D_FEATURE_LEVEL level{};
			require(SUCCEEDED(D3D11CreateDevice(
						nullptr,
						D3D_DRIVER_TYPE_WARP,
						nullptr,
						0,
						nullptr,
						0,
						D3D11_SDK_VERSION,
						&resources.device,
						&level,
						&resources.context)),
				"WARP D3D11 device creation failed");
			const D3D11_TEXTURE2D_DESC textureDescription{
				64,
				32,
				1,
				1,
				DXGI_FORMAT_R8G8B8A8_UNORM,
				{ 1, 0 },
				D3D11_USAGE_DEFAULT,
				D3D11_BIND_SHADER_RESOURCE,
				0,
				0
			};
			require(SUCCEEDED(resources.device->CreateTexture2D(
						&textureDescription,
						nullptr,
						&resources.texture)),
				"test texture creation failed");
			require(SUCCEEDED(resources.device->CreateShaderResourceView(
						resources.texture.Get(),
						nullptr,
						&resources.view)),
				"test SRV creation failed");
			return resources;
		}

		[[nodiscard]] ULONG ReferenceCount(IUnknown* a_object) noexcept
		{
			const auto incremented = a_object->AddRef();
			(void)a_object->Release();
			return incremented - 1;
		}
	}

	void run_presentation_service_checks(Runner& runner)
	{
		runner.test("presentation service flags describe implemented services", [] {
			require(
				PresentationServices::kSupportedServices ==
					(DMUI_HOST_SERVICE_FRAME_CONTROL |
						DMUI_HOST_SERVICE_EDIT_LIFECYCLE |
						DMUI_HOST_SERVICE_CONTEXTUAL_HOTKEYS |
						DMUI_HOST_SERVICE_IMAGE_RESOURCES |
						DMUI_HOST_SERVICE_MANAGED_OVERLAYS |
						DMUI_HOST_SERVICE_NOTIFICATIONS |
						DMUI_HOST_SERVICE_ANNOTATED_PLOTS |
						DMUI_HOST_SERVICE_DIALOGS),
				"advertised presentation services drifted");
		});

		runner.test("cold frame observers can import before UI demand", [] {
			auto resources = CreateImageResources();
			PresentationServices::BindRenderer(resources.device.Get());
			const PresentationServices::ClientExecutionGuard observer{ 6, false };
			const DMUI_D3D11ImageDescriptor descriptor{
				sizeof(DMUI_D3D11ImageDescriptor),
				resources.view.Get(),
				0,
				0
			};
			DMUI_ImageHandle image{};
			require(PresentationServices::ImportD3D11Image(
						6, &descriptor, &image) == DMUI_RESULT_OK,
				"first ready observer could not import without UI demand");
			require(PresentationServices::ReleaseImage(6, image) ==
					DMUI_RESULT_OK,
				"observer image release failed");
			PresentationServices::InvalidateDevice();
		});

		runner.test("image handles retain queued draws and invalidate by device generation", [] {
			auto resources = CreateImageResources();
			ImGuiFrame frame;
			PresentationServices::BindRenderThread();
			PresentationServices::SetDevice(resources.device.Get());
			PresentationServices::BeginFrame();
			const DMUI_D3D11ImageDescriptor descriptor{
				sizeof(DMUI_D3D11ImageDescriptor),
				resources.view.Get(),
				0,
				0
			};
			auto* const queuedView = resources.view.Get();
			const auto consumerReferenceCount = ReferenceCount(queuedView);
			DMUI_ImageHandle image{};
			require(PresentationServices::ImportD3D11Image(
						7, &descriptor, &image) == DMUI_RESULT_OK,
				"valid same-device image import failed");
			require(ReferenceCount(queuedView) == consumerReferenceCount + 1,
				"image import did not retain the SRV");
			DMUI_ImageInfo info{};
			info.structSize = sizeof(info);
			require(PresentationServices::QueryImage(7, image, &info) ==
						DMUI_RESULT_OK &&
					info.contentWidth == 64 &&
					info.contentHeight == 32,
				"derived image dimensions were not reported");
			const DMUI_ImageDrawOptions options{
				sizeof(DMUI_ImageDrawOptions),
				{ 128.0f, 128.0f },
				{ 0.0f, 0.0f },
				{ 1.0f, 1.0f },
				{ 1.0f, 1.0f, 1.0f, 1.0f },
				1,
				0
			};
			{
				const PresentationServices::ClientExecutionGuard callback{ 7, true };
				require(PresentationServices::DrawImage(7, image, &options) ==
						DMUI_RESULT_OK,
					"queued image draw failed");
			}
			require(ReferenceCount(queuedView) == consumerReferenceCount + 2,
				"queued draw did not acquire a submission lease");
			require(PresentationServices::ReleaseImage(7, image) ==
					DMUI_RESULT_OK,
				"release after a queued draw failed");
			require(ReferenceCount(queuedView) == consumerReferenceCount + 1,
				"handle release dropped the queued submission lease");
			require(PresentationServices::QueryImage(7, image, &info) ==
						DMUI_RESULT_OK &&
					info.status == DMUI_IMAGE_STATUS_RELEASED,
				"released image status was not retained");
			resources.view.Reset();
			D3D11_SHADER_RESOURCE_VIEW_DESC queuedDescription{};
			queuedView->GetDesc(&queuedDescription);
			require(
				queuedDescription.ViewDimension ==
					D3D11_SRV_DIMENSION_TEXTURE2D,
				"consumer release destroyed the SRV before submission");
			PresentationServices::CompleteRenderSubmission();

			auto replacementResources = CreateImageResources();
			PresentationServices::SetDevice(replacementResources.device.Get());
			const DMUI_D3D11ImageDescriptor replacementDescriptor{
				sizeof(DMUI_D3D11ImageDescriptor),
				replacementResources.view.Get(),
				0,
				0
			};
			DMUI_ImageHandle replacement{};
			require(PresentationServices::ImportD3D11Image(
						7, &replacementDescriptor, &replacement) ==
					DMUI_RESULT_OK,
				"replacement image import failed");
			PresentationServices::InvalidateDevice();
			info.structSize = sizeof(info);
			require(PresentationServices::QueryImage(7, replacement, &info) ==
						DMUI_RESULT_OK &&
					info.status == DMUI_IMAGE_STATUS_INVALIDATED,
				"device loss did not invalidate the image handle");

			PresentationServices::SetDevice(replacementResources.device.Get());
			const auto stableSlotCount =
				PresentationServices::ImageSlotCount();
			auto stale = replacement;
			for (size_t cycle = 0; cycle < 4096; ++cycle)
			{
				DMUI_ImageHandle reused{};
				require(PresentationServices::ImportD3D11Image(
							7, &replacementDescriptor, &reused) ==
						DMUI_RESULT_OK,
					"reused image import failed");
				info.structSize = sizeof(info);
				require(PresentationServices::QueryImage(7, stale, &info) ==
						DMUI_RESULT_STALE_HANDLE,
					"reused image slot aliased an older generation");
				require(PresentationServices::QueryImage(8, reused, &info) ==
						DMUI_RESULT_STALE_HANDLE,
					"image query ignored owner isolation");
				require(PresentationServices::ReleaseImage(8, reused) ==
						DMUI_RESULT_STALE_HANDLE,
					"image release ignored owner isolation");
				require(PresentationServices::ReleaseImage(7, reused) ==
						DMUI_RESULT_OK,
					"reused image release failed");
				info.structSize = sizeof(info);
				require(PresentationServices::QueryImage(7, reused, &info) ==
							DMUI_RESULT_OK &&
						info.status == DMUI_IMAGE_STATUS_RELEASED,
					"released slot did not expose its transient status");
				stale = reused;
			}
			require(PresentationServices::ImageSlotCount() == stableSlotCount,
				"image slot storage grew across import/release cycles");
			PresentationServices::InvalidateDevice();
		});

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
			PresentationServices::BindRenderThread();
			const PresentationServices::ClientExecutionGuard callback{ 12, true };
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

		runner.test("declarative edits report live completion and multiline height", [] {
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
			const auto lineHeight = ImGui::GetTextLineHeightWithSpacing();
			const auto multiline = dmui::setting_detail::DrawBoundSetting(
				multilineSetting,
				std::string{ "line one\nline two" });
			const auto multilineHeight = ImGui::GetItemRectSize().y;
			dmui::setting_detail::NotifySettingEdit(
				multilineSetting, multiline);
			frame.End();
			require(multilineHeight >= lineHeight * 3.0f &&
					!multiline.changed &&
					!multiline.completed &&
					multilineEvents == 0,
				"multiline height or unchanged callback contract regressed");
		});

		runner.test("dialogs reject hidden requests and preserve small-buffer events", [] {
			PresentationServices::BindRenderThread();
			const PresentationServices::ClientExecutionGuard callback{ 15, false };
			const DMUI_DialogDescriptor descriptor{
				sizeof(DMUI_DialogDescriptor),
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
						15, &descriptor, &dialog, false) ==
					DMUI_RESULT_NOT_VISIBLE,
				"hidden menu accepted a dialog request");
			require(PresentationServices::RequestDialog(
						15, &descriptor, &dialog, true) ==
					DMUI_RESULT_OK,
				"visible menu rejected a dialog request");
			DMUI_DialogEvent event{};
			event.structSize = sizeof(event);
			char smallBuffer[2]{};
			require(PresentationServices::PollDialogEvent(
						15,
						dialog,
						&event,
						smallBuffer,
						sizeof(smallBuffer)) ==
						DMUI_RESULT_BUFFER_TOO_SMALL &&
					event.requiredTextCapacity > sizeof(smallBuffer),
				"small dialog buffer was truncated or consumed");
			require(PresentationServices::CancelDialog(15, dialog) ==
					DMUI_RESULT_OK,
				"pending dialog cancellation failed");
			char text[65]{};
			event.structSize = sizeof(event);
			require(PresentationServices::PollDialogEvent(
						15, dialog, &event, text, sizeof(text)) ==
						DMUI_RESULT_OK &&
					event.kind == DMUI_DIALOG_EVENT_CANCELLED &&
					std::string_view{ text } == "Commonwealth",
				"cancelled dialog did not preserve entered text");
			event.structSize = sizeof(event);
			require(PresentationServices::PollDialogEvent(
						15, dialog, &event, text, sizeof(text)) ==
					DMUI_RESULT_STALE_HANDLE,
				"observed cancellation did not retire the dialog");
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

			CaptureMenuEscapePress(true, false, 0);
			require(
				ConsumeMenuEscapeTarget(MenuEscapeTarget::kInteraction),
				"an active interaction did not own the first Escape");
			require(context->OpenPopupStack.Size == 1 &&
					context->OpenPopupStack.back().PopupId == outerId,
				"active-interaction Escape also dismissed a popup");

			ImGui::ClearActiveID();
			CaptureMenuEscapePress(true, false, 0);
			ImGui::CloseCurrentPopup();
			require(DismissCapturedMenuPopup(),
				"the second Escape did not retain its popup ownership");
			require(context->OpenPopupStack.empty(),
				"popup coordination dismissed another UI level");

			context->NavId = interactionId;
			CaptureMenuEscapePress(true, false, 0);
			require(ConsumeMenuEscapeTarget(MenuEscapeTarget::kHost),
				"idle keyboard focus incorrectly trapped Escape");
			require(!ConsumeMenuEscapeTarget(MenuEscapeTarget::kHost),
				"one Escape generated more than one host dismissal");

			CaptureMenuEscapePress(false, false, 0);
			require(!ConsumeMenuEscapeTarget(MenuEscapeTarget::kHost),
				"closed or overlay-only state captured Escape");
			require(
				DecideMenuEscapeTarget({
					true,
					false,
					true,
					0x22,
					0x11,
					2
				}) == MenuEscapeTarget::kPopup &&
				DecideMenuEscapeTarget({
					true,
					false,
					true,
					0x11,
					0x11,
					1
				}) == MenuEscapeTarget::kDialog,
				"a nested popup did not outrank its owning dialog");
			require(
				DecideMenuEscapeTarget({
					true,
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

		runner.test("Escape lets an active text edit cancel before its surface", [] {
			InteractiveImGui frame;
			ResetMenuEscapeRequest();
			char value[32]{ "Baseline" };

			frame.Begin({ 500.0f, 400.0f }, false);
			(void)ImGui::InputText("##EscapeEdit", value, sizeof(value));
			frame.End();
			frame.Begin({ 40.0f, 28.0f }, true);
			(void)ImGui::InputText("##EscapeEdit", value, sizeof(value));
			frame.End();
			frame.Begin({ 40.0f, 28.0f }, false, " changed");
			(void)ImGui::InputText("##EscapeEdit", value, sizeof(value));
			frame.End();
			require(std::string_view{ value } != "Baseline",
				"text edit did not become active and change");

			CaptureMenuEscapePress(true, false, 0);
			frame.Key(ImGuiKey_Escape, true);
			frame.Begin({ -100.0f, -100.0f }, false);
			(void)ImGui::InputText("##EscapeEdit", value, sizeof(value));
			require(
				ConsumeMenuEscapeTarget(MenuEscapeTarget::kInteraction),
				"active text edit did not retain the first Escape");
			require(!ConsumeMenuEscapeTarget(MenuEscapeTarget::kHost),
				"active text edit Escape also closed its surface");
			frame.End();
			require(std::string_view{ value } == "Baseline",
				"Escape did not use the input control's rollback semantics");

			frame.Key(ImGuiKey_Escape, false);
			frame.Begin({ -100.0f, -100.0f }, false);
			(void)ImGui::InputText("##EscapeEdit", value, sizeof(value));
			frame.End();
			CaptureMenuEscapePress(true, false, 0);
			require(ConsumeMenuEscapeTarget(MenuEscapeTarget::kHost),
				"the next deliberate Escape did not reach the surface");
			ResetMenuEscapeRequest();
		});

		runner.test("dialog submissions reject retry and complete deterministically", [] {
			auto resources = CreateImageResources();
			PresentationServices::SetDevice(resources.device.Get());
			PresentationServices::BindRenderThread();
			const PresentationServices::ClientExecutionGuard callback{ 16, false };
			const DMUI_DialogDescriptor descriptor{
				sizeof(DMUI_DialogDescriptor),
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
						16, &descriptor, &dialog, true) == DMUI_RESULT_OK,
				"submission dialog request failed");
			require(PresentationServices::SubmitDialog(dialog) == DMUI_RESULT_OK,
				"first dialog submission failed");
			require(PresentationServices::SubmitDialog(dialog) == DMUI_RESULT_BUSY,
				"duplicate pending submission was accepted");

			DMUI_DialogEvent event{};
			event.structSize = sizeof(event);
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

			event.structSize = sizeof(event);
			require(PresentationServices::PollDialogEvent(
						16, dialog, &event, text, sizeof(text)) ==
						DMUI_RESULT_OK &&
					event.kind == DMUI_DIALOG_EVENT_PENDING &&
					std::string_view{ text } == "Commonwealth",
				"rejected dialog did not preserve text and pending state");
			require(PresentationServices::SubmitDialog(dialog) == DMUI_RESULT_OK,
				"dialog resubmission after explicit error failed");
			event.structSize = sizeof(event);
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
						16, dialog, secondSubmission, 0, nullptr) ==
					DMUI_RESULT_OK,
				"default null rejection failed");
			event.structSize = sizeof(event);
			require(PresentationServices::PollDialogEvent(
						16, dialog, &event, text, sizeof(text)) ==
						DMUI_RESULT_OK &&
					event.kind == DMUI_DIALOG_EVENT_PENDING &&
					std::string_view{ text } == "Commonwealth",
				"null rejection did not preserve pending text");
			require(PresentationServices::SubmitDialog(dialog) == DMUI_RESULT_OK,
				"dialog resubmission after null rejection failed");
			event.structSize = sizeof(event);
			require(PresentationServices::PollDialogEvent(
						16, dialog, &event, text, sizeof(text)) ==
						DMUI_RESULT_OK &&
					event.kind == DMUI_DIALOG_EVENT_SUBMITTED &&
					event.submissionId > secondSubmission,
				"third submission was not delivered");
			const auto thirdSubmission = event.submissionId;
			require(PresentationServices::ResolveDialogSubmission(
						16, dialog, thirdSubmission, 0, "") ==
					DMUI_RESULT_OK,
				"empty rejection failed");
			event.structSize = sizeof(event);
			require(PresentationServices::PollDialogEvent(
						16, dialog, &event, text, sizeof(text)) ==
						DMUI_RESULT_OK &&
					event.kind == DMUI_DIALOG_EVENT_PENDING &&
					std::string_view{ text } == "Commonwealth",
				"empty rejection did not preserve pending text");
			require(PresentationServices::SubmitDialog(dialog) == DMUI_RESULT_OK,
				"dialog resubmission after empty rejection failed");
			event.structSize = sizeof(event);
			require(PresentationServices::PollDialogEvent(
						16, dialog, &event, text, sizeof(text)) ==
						DMUI_RESULT_OK &&
					event.kind == DMUI_DIALOG_EVENT_SUBMITTED &&
					event.submissionId > thirdSubmission,
				"final submission was not delivered");
			require(PresentationServices::ResolveDialogSubmission(
						16, dialog, event.submissionId, 1, nullptr) ==
					DMUI_RESULT_OK,
				"accepted dialog resolution failed");
			event.structSize = sizeof(event);
			require(PresentationServices::PollDialogEvent(
						16, dialog, &event, text, sizeof(text)) ==
						DMUI_RESULT_OK &&
					event.kind == DMUI_DIALOG_EVENT_COMPLETED,
				"completed dialog event was not delivered");
			event.structSize = sizeof(event);
			require(PresentationServices::PollDialogEvent(
						16, dialog, &event, text, sizeof(text)) ==
					DMUI_RESULT_STALE_HANDLE,
				"completed dialog handle remained live");
			PresentationServices::InvalidateDevice();
		});

		runner.test("Escape cancels pending dialogs but only hides submitted work", [] {
			InteractiveImGui frame;
			PresentationServices::BindRenderThread();
			const PresentationServices::ClientExecutionGuard callback{ 17, false };
			const DMUI_DialogDescriptor descriptor{
				sizeof(DMUI_DialogDescriptor),
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
				true,
				PresentationServices::HasActiveDialog(),
				PresentationServices::ActiveDialogPopupId());
			frame.Begin({ -100.0f, -100.0f }, false);
			PresentationServices::DrawDialog(true);
			frame.End();

			DMUI_DialogEvent event{};
			event.structSize = sizeof(event);
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
				true,
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
			event.structSize = sizeof(event);
			require(PresentationServices::PollDialogEvent(
						17, submitted, &event, text, sizeof(text)) ==
						DMUI_RESULT_OK &&
					event.kind == DMUI_DIALOG_EVENT_SUBMITTED &&
					event.submissionId != 0,
				"Escape cancelled submitted client work");
			const auto submissionId = event.submissionId;

			CaptureMenuEscapePress(
				true,
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
			event.structSize = sizeof(event);
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
				true,
				PresentationServices::HasActiveDialog(),
				completionPopupId);
			event = {};
			event.structSize = sizeof(event);
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
			event.structSize = sizeof(event);
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
