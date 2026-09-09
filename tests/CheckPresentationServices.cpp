#include <DearModdingUI/PresentationServices.h>
#include <DearModdingUI/RenderExecution.h>
#include <DearModdingUI/MenuDismissal.h>
#include <DearModdingUI/UIAdapter.h>
#include "Harness.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <DearModdingUI/Client.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace vmm_tests
{
	namespace
	{
		using Microsoft::WRL::ComPtr;
		using namespace DearModdingUI;

		[[nodiscard]] DMUI_Result AcceptUIClient(
			DMUI_ClientHandle) noexcept
		{
			return DMUI_RESULT_OK;
		}

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
			UI::Testing::ValidationOverride m_uiValidation{ &AcceptUIClient };
			dmui::ui::detail::ScopedContext m_uiContext{
				&UI::API(),
				1u
			};
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
			UI::Testing::ValidationOverride m_uiValidation{ &AcceptUIClient };
			dmui::ui::detail::ScopedContext m_uiContext{
				&UI::API(),
				1u
			};
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

		[[nodiscard]] std::vector<uint8_t> ReadPixels(
			ID3D11Device* a_device,
			ID3D11DeviceContext* a_context,
			ID3D11ShaderResourceView* a_view,
			uint32_t& a_width,
			uint32_t& a_height)
		{
			require(a_view != nullptr, "queued image view was null");
			ComPtr<ID3D11Resource> resource;
			a_view->GetResource(&resource);
			ComPtr<ID3D11Texture2D> texture;
			require(SUCCEEDED(resource.As(&texture)),
				"queued image resource was not Texture2D");
			D3D11_TEXTURE2D_DESC description{};
			texture->GetDesc(&description);
			require(description.Format == DXGI_FORMAT_R8G8B8A8_UNORM,
				"CPU image did not use RGBA8 UNORM");
			a_width = description.Width;
			a_height = description.Height;
			description.Usage = D3D11_USAGE_STAGING;
			description.BindFlags = 0;
			description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			description.MiscFlags = 0;
			ComPtr<ID3D11Texture2D> staging;
			require(SUCCEEDED(a_device->CreateTexture2D(
						&description, nullptr, &staging)),
				"staging texture creation failed");
			a_context->CopyResource(staging.Get(), texture.Get());
			D3D11_MAPPED_SUBRESOURCE mapped{};
			require(SUCCEEDED(a_context->Map(
						staging.Get(),
						0,
						D3D11_MAP_READ,
						0,
						&mapped)),
				"staging texture map failed");
			std::vector<uint8_t> result(
				static_cast<size_t>(a_width) * a_height * 4u);
			for (uint32_t row = 0; row < a_height; ++row)
			{
				std::memcpy(
					result.data() + static_cast<size_t>(row) * a_width * 4u,
					static_cast<const uint8_t*>(mapped.pData) +
						static_cast<size_t>(row) * mapped.RowPitch,
					static_cast<size_t>(a_width) * 4u);
			}
			a_context->Unmap(staging.Get(), 0);
			return result;
		}
	}

	void run_presentation_service_checks(Runner& runner)
	{
		runner.test("active Present scopes migrate without rebinding the device", [] {
			auto resources = CreateImageResources();
			std::mutex mutex;
			std::condition_variable condition;
			bool startupReady{};
			bool attemptPastThread{};
			bool pastThreadDone{};
			uint64_t startupGeneration{};
			DMUI_Result pastThreadResult{ DMUI_RESULT_OK };

			std::thread startup{ [&] {
				{
					RenderExecution::Guard execution{
						RenderExecution::Phase::kBackendInitialization
					};
					(void)execution.NoteBinding(41);
					PresentationServices::BindRenderer(resources.device.Get());
					startupGeneration = PresentationServices::DeviceGeneration();
				}
				{
					std::unique_lock lock{ mutex };
					startupReady = true;
					condition.notify_all();
					condition.wait(lock, [&] { return attemptPastThread; });
				}
				const DMUI_D3D11ImageDescriptor descriptor{
					sizeof(DMUI_D3D11ImageDescriptor),
					resources.view.Get(),
					0,
					0
				};
				DMUI_ImageHandle image{};
				pastThreadResult = PresentationServices::ImportD3D11Image(
					42, &descriptor, &image);
				{
					const std::scoped_lock lock{ mutex };
					pastThreadDone = true;
				}
				condition.notify_all();
			} };

			{
				std::unique_lock lock{ mutex };
				condition.wait(lock, [&] { return startupReady; });
			}
			const auto renderGeneration = PresentationServices::DeviceGeneration();
			require(renderGeneration == startupGeneration,
				"startup publication changed the device generation unexpectedly");

			{
				RenderExecution::Guard execution{
					RenderExecution::Phase::kFrameObservation
				};
				(void)execution.NoteBinding(41);
				{
					const PresentationServices::ClientExecutionGuard observer{
						42, false
					};
					require(PresentationServices::IsActiveClient(42, false),
						"cold observer did not inherit active Present authorization");
					require(!PresentationServices::IsActiveClient(42, true),
						"cold observer acquired drawing authorization");
				}
				{
					const std::scoped_lock lock{ mutex };
					attemptPastThread = true;
				}
				condition.notify_all();

				struct DepthImport
				{
					DXGI_FORMAT texture;
					DXGI_FORMAT view;
				};
				constexpr std::array formats{
					DepthImport{
						DXGI_FORMAT_R24G8_TYPELESS,
						DXGI_FORMAT_R24_UNORM_X8_TYPELESS },
					DepthImport{
						DXGI_FORMAT_R32_TYPELESS,
						DXGI_FORMAT_R32_FLOAT }
				};
				for (const auto& format : formats)
				{
					const D3D11_TEXTURE2D_DESC textureDescription{
						16, 8, 1, 1, format.texture, { 1, 0 },
						D3D11_USAGE_DEFAULT,
						D3D11_BIND_SHADER_RESOURCE,
						0, 0
					};
					ComPtr<ID3D11Texture2D> texture;
					require(SUCCEEDED(resources.device->CreateTexture2D(
								&textureDescription, nullptr, &texture)),
						"cross-thread depth texture creation failed");
					D3D11_SHADER_RESOURCE_VIEW_DESC viewDescription{};
					viewDescription.Format = format.view;
					viewDescription.ViewDimension =
						D3D11_SRV_DIMENSION_TEXTURE2D;
					viewDescription.Texture2D.MipLevels = 1;
					ComPtr<ID3D11ShaderResourceView> view;
					require(SUCCEEDED(resources.device->CreateShaderResourceView(
								texture.Get(), &viewDescription, &view)),
						"cross-thread depth SRV creation failed");
					const DMUI_D3D11ImageDescriptor descriptor{
						sizeof(DMUI_D3D11ImageDescriptor),
						view.Get(),
						0,
						0
					};
					DMUI_ImageHandle image{};
					require(PresentationServices::ImportD3D11Image(
								42, &descriptor, &image) == DMUI_RESULT_OK,
						"authorized migrated observer rejected a depth SRV");
					require(PresentationServices::ReleaseImage(42, image) ==
							DMUI_RESULT_OK,
						"cross-thread depth image release failed");
				}

				std::unique_lock lock{ mutex };
				condition.wait(lock, [&] { return pastThreadDone; });
				require(pastThreadResult == DMUI_RESULT_WRONG_THREAD,
					"past startup thread retained render authorization");
			}
			startup.join();

			const DMUI_D3D11ImageDescriptor descriptor{
				sizeof(DMUI_D3D11ImageDescriptor),
				resources.view.Get(),
				0,
				0
			};
			DMUI_ImageHandle image{};
			require(PresentationServices::ImportD3D11Image(
						42, &descriptor, &image) == DMUI_RESULT_WRONG_THREAD,
				"completed Present scope left its caller authorized");
			require(PresentationServices::DeviceGeneration() == renderGeneration,
				"thread migration invalidated the stable render device");
			PresentationServices::InvalidateDevice();
		});

		runner.test("overlapping Present callback scopes serialize and restore nesting", [] {
			std::mutex mutex;
			std::condition_variable condition;
			bool firstEntered{};
			bool releaseFirst{};
			bool secondAttempted{};
			bool secondEntered{};
			bool releaseSecond{};
			bool firstAuthorized{};
			bool nestedAuthorized{};
			bool outerRestored{};
			bool secondStillAuthorized{};
			bool firstThreadReported{};
			bool secondThreadReported{};
			bool overlapped{};
			int active{};
			int maximumActive{};

			std::thread first{ [&] {
				RenderExecution::Guard execution{
					RenderExecution::Phase::kFrameObservation
				};
				const auto transition = execution.NoteBinding(7);
				firstThreadReported =
					transition.currentThread ==
					static_cast<uint64_t>(::GetCurrentThreadId());
				{
					const PresentationServices::ClientExecutionGuard client{
						70, false
					};
					firstAuthorized =
						PresentationServices::IsActiveClient(70, false);
				}
				std::unique_lock lock{ mutex };
				++active;
				maximumActive = (std::max)(maximumActive, active);
				firstEntered = true;
				condition.notify_all();
				condition.wait(lock, [&] { return releaseFirst; });
				--active;
			} };

			{
				std::unique_lock lock{ mutex };
				condition.wait(lock, [&] { return firstEntered; });
			}

			std::thread second{ [&] {
				{
					const std::scoped_lock lock{ mutex };
					secondAttempted = true;
				}
				condition.notify_all();
				RenderExecution::Guard execution{
					RenderExecution::Phase::kFrameDraw
				};
				const auto transition = execution.NoteBinding(7);
				secondThreadReported =
					transition.currentThread ==
					static_cast<uint64_t>(::GetCurrentThreadId());
				const PresentationServices::ClientExecutionGuard outer{
					71, false
				};
				{
					RenderExecution::Guard nested{
						RenderExecution::Phase::kFrameDraw
					};
					(void)nested.NoteBinding(7);
					const PresentationServices::ClientExecutionGuard inner{
						72, true
					};
					nestedAuthorized =
						RenderExecution::ActivePhase() ==
							RenderExecution::Phase::kFrameDraw &&
						RenderExecution::ActiveBinding() == 7 &&
						PresentationServices::IsActiveClient(72, true);
				}
				outerRestored =
					PresentationServices::IsActiveClient(71, false) &&
					!PresentationServices::IsActiveClient(72, false);
				std::unique_lock lock{ mutex };
				++active;
				maximumActive = (std::max)(maximumActive, active);
				secondEntered = true;
				condition.notify_all();
				condition.wait(lock, [&] { return releaseSecond; });
				secondStillAuthorized =
					PresentationServices::IsActiveClient(71, false);
				--active;
			} };

			{
				std::unique_lock lock{ mutex };
				condition.wait(lock, [&] { return secondAttempted; });
				// Release the test mutex so only the execution gate can block entry.
				overlapped = condition.wait_for(
					lock,
					std::chrono::milliseconds{ 500 },
					[&] { return secondEntered; });
				releaseFirst = true;
			}
			condition.notify_all();
			{
				std::unique_lock lock{ mutex };
				condition.wait(lock, [&] { return secondEntered; });
				releaseSecond = true;
			}
			condition.notify_all();
			first.join();
			second.join();
			require(!overlapped && maximumActive == 1,
				"overlapping Present scopes entered client execution");
			require(firstThreadReported && secondThreadReported,
				"render migration diagnostics did not report OS thread IDs");
			require(firstAuthorized && nestedAuthorized && outerRestored &&
					secondStillAuthorized,
				"nested or handed-off execution authorization was not restored");

			const auto earlyReturn = [] {
				RenderExecution::Guard execution{
					RenderExecution::Phase::kFrameObservation
				};
				(void)execution.NoteBinding(8);
				return RenderExecution::IsActive();
			};
			require(earlyReturn() && !RenderExecution::IsActive(),
				"early return leaked render execution authorization");
			try
			{
				RenderExecution::Guard execution{
					RenderExecution::Phase::kFrameObservation
				};
				(void)execution.NoteBinding(8);
				throw std::runtime_error{ "guard release" };
			}
			catch (const std::runtime_error&)
			{}
			require(!RenderExecution::IsActive(),
				"exception unwinding leaked render execution authorization");
		});

		runner.test("CPU images consume pixels and replace resources transactionally", [] {
			auto resources = CreateImageResources();
			ImGuiFrame frame;
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameDraw
			};
			(void)execution.NoteBinding(1);
			PresentationServices::BindRenderer(resources.device.Get());
			PresentationServices::BeginFrame();

			std::array<uint8_t, 20> padded{
				1, 2, 3, 4, 5, 6, 7, 8,
				91, 92, 93, 94,
				9, 10, 11, 12, 13, 14, 15, 16
			};
			const std::vector<uint8_t> expectedOld{
				1, 2, 3, 4, 5, 6, 7, 8,
				9, 10, 11, 12, 13, 14, 15, 16
			};
			DMUI_ImageDescriptor descriptor{
				sizeof(DMUI_ImageDescriptor),
				2,
				2,
				DMUI_PIXEL_FORMAT_RGBA8_UNORM,
				0,
				12,
				20,
				padded.data()
			};
			DMUI_ImageHandle image{};
			require(PresentationServices::CreateImage(
						21, &descriptor, &image) == DMUI_RESULT_OK,
				"padded CPU image creation failed");
			padded.fill(0);

			DMUI_ImageInfo info{};
			info.structSize = sizeof(info);
			require(PresentationServices::QueryImage(21, image, &info) ==
						DMUI_RESULT_OK &&
					info.contentWidth == 2 &&
					info.contentHeight == 2,
				"CPU image query did not report created dimensions");
			const DMUI_ImageDrawOptions options{
				sizeof(DMUI_ImageDrawOptions),
				{ 32.0f, 32.0f },
				{ 0.0f, 0.0f },
				{ 1.0f, 1.0f },
				{ 1.0f, 1.0f, 1.0f, 1.0f },
				0,
				0
			};
			ID3D11ShaderResourceView* oldView{};
			{
				const PresentationServices::ClientExecutionGuard callback{ 21, true };
				require(PresentationServices::DrawImage(
							21, image, &options) == DMUI_RESULT_OK,
					"created CPU image could not be drawn");
			}
			oldView = PresentationServices::RetainImageViewForTests(21, image);
			require(oldView != nullptr, "created CPU view was not retained");
			oldView->Release();

			auto rejected = descriptor;
			rejected.accessibleByteCount = 19;
			rejected.pixels = expectedOld.data();
			require(PresentationServices::UpdateImage(
						21, image, &rejected) == DMUI_RESULT_INVALID_ARGUMENT,
				"short final-row extent was accepted");
			info = {};
			info.structSize = sizeof(info);
			require(PresentationServices::QueryImage(21, image, &info) ==
						DMUI_RESULT_OK &&
					info.contentWidth == 2 &&
					info.contentHeight == 2,
				"rejected update changed image dimensions");

			std::array<uint8_t, 12> updated{
				21, 22, 23, 24,
				31, 32, 33, 34,
				41, 42, 43, 44
			};
			const auto expectedNew = std::vector<uint8_t>{
				updated.begin(), updated.end()
			};
			descriptor.width = 3;
			descriptor.height = 1;
			descriptor.rowPitch = 12;
			descriptor.accessibleByteCount = 12;
			descriptor.pixels = updated.data();
			require(PresentationServices::UpdateImage(
						21, image, &descriptor) == DMUI_RESULT_OK,
				"valid CPU image update failed");
			updated.fill(0);
			info = {};
			info.structSize = sizeof(info);
			require(PresentationServices::QueryImage(21, image, &info) ==
						DMUI_RESULT_OK &&
					info.contentWidth == 3 &&
					info.contentHeight == 1,
				"successful update did not retain the same handle with new dimensions");

			ID3D11ShaderResourceView* newView{};
			{
				const PresentationServices::ClientExecutionGuard callback{ 21, true };
				require(PresentationServices::DrawImage(
							21, image, &options) == DMUI_RESULT_OK,
					"updated CPU image could not be drawn");
			}
			newView = PresentationServices::RetainImageViewForTests(21, image);
			require(newView != nullptr, "updated CPU view was not retained");
			newView->Release();
			require(oldView != newView,
				"CPU update mutated the queued resource in place");
			require(PresentationServices::ReleaseImage(21, image) ==
					DMUI_RESULT_OK,
				"CPU image release after queued draws failed");

			uint32_t width{};
			uint32_t height{};
			const auto oldPixels = ReadPixels(
				resources.device.Get(),
				resources.context.Get(),
				oldView,
				width,
				height);
			require(width == 2 && height == 2 && oldPixels == expectedOld,
				"queued old draw did not preserve original RGBA and alpha bytes");
			const auto newPixels = ReadPixels(
				resources.device.Get(),
				resources.context.Get(),
				newView,
				width,
				height);
			require(width == 3 && height == 1 && newPixels == expectedNew,
				"later draw did not expose updated RGBA and alpha bytes");
			PresentationServices::CompleteRenderSubmission();
			PresentationServices::InvalidateDevice();
		});

		runner.test("CPU image validation and provenance preserve live resources", [] {
			auto resources = CreateImageResources();
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameObservation
			};
			(void)execution.NoteBinding(1);
			PresentationServices::BindRenderer(resources.device.Get());
			const std::array<uint8_t, 8> pixels{
				10, 20, 30, 40, 50, 60, 70, 80
			};
			DMUI_ImageDescriptor descriptor{
				sizeof(DMUI_ImageDescriptor),
				2,
				1,
				DMUI_PIXEL_FORMAT_RGBA8_UNORM,
				0,
				8,
				8,
				pixels.data()
			};
			DMUI_ImageHandle image{};
			auto invalid = descriptor;
			invalid.structSize = DMUI_IMAGE_DESCRIPTOR_0_1_SIZE - 1u;
			require(PresentationServices::CreateImage(
						22, &invalid, &image) == DMUI_RESULT_STRUCT_TOO_SMALL,
				"small CPU descriptor was accepted");
			invalid = descriptor;
			invalid.pixelFormat = 99;
			require(PresentationServices::CreateImage(
						22, &invalid, &image) == DMUI_RESULT_UNSUPPORTED_RESOURCE,
				"unknown pixel format was accepted");
			invalid = descriptor;
			invalid.reserved = 1;
			require(PresentationServices::CreateImage(
						22, &invalid, &image) == DMUI_RESULT_INVALID_DESCRIPTOR,
				"reserved pixel descriptor bits were accepted");
			invalid = descriptor;
			invalid.pixels = nullptr;
			require(PresentationServices::CreateImage(
						22, &invalid, &image) == DMUI_RESULT_INVALID_ARGUMENT,
				"null CPU pixels were accepted");
			invalid = descriptor;
			invalid.width = 0;
			require(PresentationServices::CreateImage(
						22, &invalid, &image) == DMUI_RESULT_INVALID_ARGUMENT,
				"zero CPU image width was accepted");
			invalid = descriptor;
			invalid.height = 0;
			require(PresentationServices::CreateImage(
						22, &invalid, &image) == DMUI_RESULT_INVALID_ARGUMENT,
				"zero CPU image height was accepted");
			invalid = descriptor;
			invalid.rowPitch = 7;
			require(PresentationServices::CreateImage(
						22, &invalid, &image) == DMUI_RESULT_INVALID_ARGUMENT,
				"short CPU image row pitch was accepted");
			invalid = descriptor;
			invalid.width = (std::numeric_limits<uint32_t>::max)();
			invalid.rowPitch = (std::numeric_limits<uint64_t>::max)();
			invalid.accessibleByteCount =
				(std::numeric_limits<uint64_t>::max)();
			invalid.pixels = reinterpret_cast<const void*>(1);
			require(PresentationServices::CreateImage(
						22, &invalid, &image) == DMUI_RESULT_INVALID_ARGUMENT,
				"maximum-width stride overflow reached pixel memory");
			invalid = descriptor;
			invalid.width = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION + 1u;
			invalid.rowPitch = static_cast<uint64_t>(invalid.width) * 4u;
			invalid.accessibleByteCount = invalid.rowPitch;
			invalid.pixels = reinterpret_cast<const void*>(1);
			require(PresentationServices::CreateImage(
						22, &invalid, &image) == DMUI_RESULT_INVALID_ARGUMENT,
				"oversized CPU dimensions reached pixel memory");

			DMUI_Result workerResult{};
			std::thread worker{ [&] {
				DMUI_ImageHandle workerImage{};
				workerResult = PresentationServices::CreateImage(
					22, &descriptor, &workerImage);
			} };
			worker.join();
			require(workerResult == DMUI_RESULT_WRONG_THREAD,
				"CPU image creation accepted a non-render thread");

			require(PresentationServices::CreateImage(
						22, &descriptor, &image) == DMUI_RESULT_OK,
				"valid tight CPU image creation failed");
			const auto stableSlots = PresentationServices::ImageSlotCount();
			for (uint32_t update = 0; update < 512; ++update)
			{
				require(PresentationServices::UpdateImage(
							22, image, &descriptor) == DMUI_RESULT_OK,
					"repeated transactional CPU update failed");
			}
			require(PresentationServices::ImageSlotCount() == stableSlots,
				"CPU updates consumed image slots");
			workerResult = DMUI_RESULT_OK;
			std::thread updateWorker{ [&] {
				workerResult = PresentationServices::UpdateImage(
					22, image, &descriptor);
			} };
			updateWorker.join();
			require(workerResult == DMUI_RESULT_WRONG_THREAD,
				"CPU image update accepted a non-render thread");
			require(PresentationServices::UpdateImage(
						23, image, &descriptor) == DMUI_RESULT_STALE_HANDLE,
				"CPU image update ignored owner isolation");
			require(PresentationServices::ReleaseImage(22, image) ==
					DMUI_RESULT_OK &&
					PresentationServices::UpdateImage(
						22, image, &descriptor) == DMUI_RESULT_STALE_HANDLE,
				"released CPU image accepted an update");

			const DMUI_D3D11ImageDescriptor importedDescriptor{
				sizeof(DMUI_D3D11ImageDescriptor),
				resources.view.Get(),
				0,
				0
			};
			DMUI_ImageHandle imported{};
			require(PresentationServices::ImportD3D11Image(
						22, &importedDescriptor, &imported) == DMUI_RESULT_OK,
				"import after CPU slot release failed");
			DMUI_ImageInfo staleInfo{};
			staleInfo.structSize = sizeof(staleInfo);
			require(PresentationServices::QueryImage(
						22, image, &staleInfo) == DMUI_RESULT_STALE_HANDLE,
				"CPU handle aliased an imported slot reuse");
			require(PresentationServices::UpdateImage(
						22, imported, &descriptor) ==
					DMUI_RESULT_UNSUPPORTED_RESOURCE,
				"CPU update mutated an imported SRV");
			require(PresentationServices::ReleaseImage(22, imported) ==
					DMUI_RESULT_OK,
				"imported image release failed");
			DMUI_ImageHandle reusedCpu{};
			require(PresentationServices::CreateImage(
						22, &descriptor, &reusedCpu) == DMUI_RESULT_OK &&
					PresentationServices::UpdateImage(
						22, reusedCpu, &descriptor) == DMUI_RESULT_OK,
				"import-to-CPU slot reuse retained imported provenance");
			DMUI_ImageInfo info{};
			info.structSize = sizeof(info);
			PresentationServices::BindRenderer(resources.device.Get());
			require(PresentationServices::QueryImage(
						22, reusedCpu, &info) == DMUI_RESULT_OK &&
					info.status == DMUI_IMAGE_STATUS_READY,
				"same-device renderer binding invalidated CPU images");

			auto replacement = CreateImageResources();
			PresentationServices::SetDevice(replacement.device.Get());
			info = {};
			info.structSize = sizeof(info);
			require(PresentationServices::QueryImage(
						22, reusedCpu, &info) == DMUI_RESULT_OK &&
					info.status == DMUI_IMAGE_STATUS_INVALIDATED &&
					PresentationServices::UpdateImage(
						22, reusedCpu, &descriptor) == DMUI_RESULT_STALE_HANDLE,
				"device replacement did not invalidate CPU images");
			PresentationServices::InvalidateDevice();
			DMUI_ImageHandle unavailable{};
			require(PresentationServices::CreateImage(
						22, &descriptor, &unavailable) ==
					DMUI_RESULT_HOST_NOT_READY,
				"CPU image creation accepted a missing device");
		});

		runner.test("sampleable HDR and depth SRVs retain native queued resources", [] {
			auto resources = CreateImageResources();
			ImGuiFrame frame;
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameDraw
			};
			(void)execution.NoteBinding(1);
			PresentationServices::BindRenderer(resources.device.Get());
			PresentationServices::BeginFrame();
			UINT support{};
			constexpr UINT hdrRequiredSupport =
				D3D11_FORMAT_SUPPORT_TEXTURE2D |
				D3D11_FORMAT_SUPPORT_SHADER_SAMPLE;
			require(SUCCEEDED(resources.device->CheckFormatSupport(
						DXGI_FORMAT_R11G11B10_FLOAT, &support)) &&
					(support & hdrRequiredSupport) == hdrRequiredSupport,
				"WARP packed HDR format lacks ordinary Texture2D sampling");

			struct Dimensions
			{
				UINT width;
				UINT height;
			};
			constexpr std::array dimensions{
				Dimensions{ 3840, 2160 },
				Dimensions{ 2259, 1271 }
			};
			for (const auto& size : dimensions)
			{
				const D3D11_TEXTURE2D_DESC description{
					size.width, size.height, 1, 1,
					DXGI_FORMAT_R11G11B10_FLOAT, { 1, 0 },
					D3D11_USAGE_DEFAULT,
					D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
					0, 0
				};
				ComPtr<ID3D11Texture2D> texture;
				require(SUCCEEDED(resources.device->CreateTexture2D(
							&description, nullptr, &texture)),
					"packed HDR render texture creation failed");
				ComPtr<ID3D11RenderTargetView> target;
				require(SUCCEEDED(resources.device->CreateRenderTargetView(
							texture.Get(), nullptr, &target)),
					"packed HDR render-target view creation failed");
				constexpr float color[]{ 4.0f, 2.0f, 0.5f, 1.0f };
				resources.context->ClearRenderTargetView(target.Get(), color);
				ComPtr<ID3D11ShaderResourceView> view;
				require(SUCCEEDED(resources.device->CreateShaderResourceView(
							texture.Get(), nullptr, &view)),
					"packed HDR shader-resource view creation failed");
				auto* queuedView = view.Get();
				const auto references = ReferenceCount(queuedView);
				const DMUI_D3D11ImageDescriptor descriptor{
					sizeof(DMUI_D3D11ImageDescriptor), view.Get(), 0, 0
				};
				DMUI_ImageHandle image{};
				require(PresentationServices::ImportD3D11Image(
							32, &descriptor, &image) == DMUI_RESULT_OK,
					"sampleable R11G11B10_FLOAT SRV was rejected");
				require(ReferenceCount(queuedView) == references + 1,
					"packed HDR import did not retain the original SRV");
				DMUI_ImageInfo info{};
				info.structSize = sizeof(info);
				require(PresentationServices::QueryImage(32, image, &info) ==
							DMUI_RESULT_OK &&
						info.contentWidth == size.width &&
						info.contentHeight == size.height,
					"packed HDR import changed the source dimensions");
				ComPtr<ID3D11ShaderResourceView> retained;
				retained.Attach(
					PresentationServices::RetainImageViewForTests(32, image));
				require(retained.Get() == queuedView,
					"packed HDR import substituted a converted resource");
				retained.Reset();

				const DMUI_ImageDrawOptions options{
					sizeof(DMUI_ImageDrawOptions),
					{ 64.0f, 36.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f },
					{ 1.0f, 1.0f, 1.0f, 1.0f }, 1, 0
				};
				{
					const PresentationServices::ClientExecutionGuard callback{
						32, true
					};
					require(PresentationServices::DrawImage(
								32, image, &options) == DMUI_RESULT_OK,
						"packed HDR image draw failed");
				}
				require(ReferenceCount(queuedView) == references + 2,
					"packed HDR draw did not retain its submission lease");
				require(PresentationServices::ReleaseImage(32, image) ==
						DMUI_RESULT_OK,
					"packed HDR handle release failed");
				require(ReferenceCount(queuedView) == references + 1,
					"packed HDR handle release discarded a queued draw");
				view.Reset();
				target.Reset();
				texture.Reset();
				D3D11_SHADER_RESOURCE_VIEW_DESC queuedDescription{};
				queuedView->GetDesc(&queuedDescription);
				require(queuedDescription.Format == DXGI_FORMAT_R11G11B10_FLOAT &&
						queuedDescription.ViewDimension ==
							D3D11_SRV_DIMENSION_TEXTURE2D,
					"packed HDR view was lost or converted before submission");
			}
			struct DepthFormat
			{
				DXGI_FORMAT texture;
				DXGI_FORMAT depthView;
				DXGI_FORMAT shaderView;
			};
			constexpr std::array formats{
				DepthFormat{
					DXGI_FORMAT_R16_TYPELESS,
					DXGI_FORMAT_D16_UNORM,
					DXGI_FORMAT_R16_UNORM },
				DepthFormat{
					DXGI_FORMAT_R24G8_TYPELESS,
					DXGI_FORMAT_D24_UNORM_S8_UINT,
					DXGI_FORMAT_R24_UNORM_X8_TYPELESS },
				DepthFormat{
					DXGI_FORMAT_R32_TYPELESS,
					DXGI_FORMAT_D32_FLOAT,
					DXGI_FORMAT_R32_FLOAT },
				DepthFormat{
					DXGI_FORMAT_R32G8X24_TYPELESS,
					DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
					DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS }
			};
			for (const auto& format : formats)
			{
				UINT support{};
				constexpr UINT depthRequiredSupport =
					D3D11_FORMAT_SUPPORT_TEXTURE2D |
					D3D11_FORMAT_SUPPORT_SHADER_SAMPLE;
				require(SUCCEEDED(resources.device->CheckFormatSupport(
							format.shaderView, &support)) &&
						(support & depthRequiredSupport) == depthRequiredSupport,
					"WARP depth view lacks ordinary Texture2D sampling support");
				const D3D11_TEXTURE2D_DESC textureDescription{
					16, 8, 1, 1, format.texture, { 1, 0 },
					D3D11_USAGE_DEFAULT,
					D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL,
					0, 0
				};
				ComPtr<ID3D11Texture2D> texture;
				require(SUCCEEDED(resources.device->CreateTexture2D(
							&textureDescription, nullptr, &texture)),
					"depth texture creation failed");
				D3D11_DEPTH_STENCIL_VIEW_DESC depthDescription{};
				depthDescription.Format = format.depthView;
				depthDescription.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
				ComPtr<ID3D11DepthStencilView> depthView;
				require(SUCCEEDED(resources.device->CreateDepthStencilView(
							texture.Get(), &depthDescription, &depthView)),
					"depth-stencil view creation failed");
				resources.context->ClearDepthStencilView(
					depthView.Get(), D3D11_CLEAR_DEPTH, 0.25f, 0);
				D3D11_SHADER_RESOURCE_VIEW_DESC shaderDescription{};
				shaderDescription.Format = format.shaderView;
				shaderDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				shaderDescription.Texture2D.MipLevels = 1;
				ComPtr<ID3D11ShaderResourceView> view;
				require(SUCCEEDED(resources.device->CreateShaderResourceView(
							texture.Get(), &shaderDescription, &view)),
					"depth shader-resource view creation failed");
				const DMUI_D3D11ImageDescriptor descriptor{
					sizeof(DMUI_D3D11ImageDescriptor), view.Get(), 0, 0
				};
				DMUI_ImageHandle image{};
				require(PresentationServices::ImportD3D11Image(
							31, &descriptor, &image) == DMUI_RESULT_OK,
					"sampleable depth SRV was rejected");
				DMUI_ImageInfo info{};
				info.structSize = sizeof(info);
				require(PresentationServices::QueryImage(31, image, &info) ==
							DMUI_RESULT_OK &&
						info.contentWidth == 16 &&
						info.contentHeight == 8,
					"depth view dimensions changed during import");
				const DMUI_ImageDrawOptions options{
					sizeof(DMUI_ImageDrawOptions),
					{ 32.0f, 16.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f },
					{ 1.0f, 1.0f, 1.0f, 1.0f }, 1, 0
				};
				{
					const PresentationServices::ClientExecutionGuard callback{
						31, true
					};
					require(PresentationServices::DrawImage(
								31, image, &options) == DMUI_RESULT_OK,
						"depth image draw failed");
				}
				auto* queuedView = view.Get();
				ComPtr<ID3D11ShaderResourceView> retained;
				retained.Attach(
					PresentationServices::RetainImageViewForTests(31, image));
				require(retained.Get() == queuedView,
					"depth drawing substituted a converted resource");
				retained.Reset();
				require(PresentationServices::ReleaseImage(31, image) ==
						DMUI_RESULT_OK,
					"queued depth image release failed");
				view.Reset();
				depthView.Reset();
				texture.Reset();
				D3D11_SHADER_RESOURCE_VIEW_DESC queuedDescription{};
				queuedView->GetDesc(&queuedDescription);
				require(queuedDescription.Format == format.shaderView,
					"native depth view was lost before render submission");
			}
			PresentationServices::CompleteRenderSubmission();
			PresentationServices::InvalidateDevice();
		});

		runner.test("integer and stencil-only SRVs are not accepted as sampled images", [] {
			auto resources = CreateImageResources();
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameObservation
			};
			(void)execution.NoteBinding(1);
			PresentationServices::BindRenderer(resources.device.Get());
			struct UnsupportedFormat
			{
				DXGI_FORMAT texture;
				DXGI_FORMAT shaderView;
			};
			constexpr std::array formats{
				UnsupportedFormat{
					DXGI_FORMAT_R10G10B10A2_TYPELESS,
					DXGI_FORMAT_R10G10B10A2_UINT },
				UnsupportedFormat{
					DXGI_FORMAT_R16_TYPELESS,
					DXGI_FORMAT_R16_UINT },
				UnsupportedFormat{
					DXGI_FORMAT_R24G8_TYPELESS,
					DXGI_FORMAT_X24_TYPELESS_G8_UINT },
				UnsupportedFormat{
					DXGI_FORMAT_R32G8X24_TYPELESS,
					DXGI_FORMAT_X32_TYPELESS_G8X24_UINT }
			};
			const auto slots = PresentationServices::ImageSlotCount();
			for (const auto& format : formats)
			{
				const D3D11_TEXTURE2D_DESC description{
					16, 8, 1, 1, format.texture, { 1, 0 },
					D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0
				};
				ComPtr<ID3D11Texture2D> texture;
				require(SUCCEEDED(resources.device->CreateTexture2D(
							&description, nullptr, &texture)),
					"unsupported-format texture creation failed");
				D3D11_SHADER_RESOURCE_VIEW_DESC viewDescription{};
				viewDescription.Format = format.shaderView;
				viewDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				viewDescription.Texture2D.MipLevels = 1;
				ComPtr<ID3D11ShaderResourceView> view;
				require(SUCCEEDED(resources.device->CreateShaderResourceView(
							texture.Get(), &viewDescription, &view)),
					"unsupported-format SRV creation failed");
				const auto references = ReferenceCount(view.Get());
				const DMUI_D3D11ImageDescriptor descriptor{
					sizeof(DMUI_D3D11ImageDescriptor), view.Get(), 0, 0
				};
				DMUI_ImageHandle image{ 1 };
				require(PresentationServices::ImportD3D11Image(
							31, &descriptor, &image) ==
							DMUI_RESULT_UNSUPPORTED_RESOURCE &&
						image == DMUI_INVALID_IMAGE_HANDLE,
					"integer or stencil-only SRV was accepted");
				require(ReferenceCount(view.Get()) == references,
					"rejected SRV acquired a retained reference");
			}
			require(PresentationServices::ImageSlotCount() == slots,
				"rejected SRVs allocated image slots");
			PresentationServices::InvalidateDevice();
		});

		runner.test("image handles retain queued draws and invalidate by device generation", [] {
			auto resources = CreateImageResources();
			ImGuiFrame frame;
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameDraw
			};
			(void)execution.NoteBinding(1);
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
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameDraw
			};
			(void)execution.NoteBinding(1);
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
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameObservation
			};
			(void)execution.NoteBinding(1);
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
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameObservation
			};
			(void)execution.NoteBinding(1);
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
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameObservation
			};
			(void)execution.NoteBinding(1);
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
