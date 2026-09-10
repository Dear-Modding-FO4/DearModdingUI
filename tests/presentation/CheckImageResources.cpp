#include "../support/D3DTestResources.h"
#include "../support/PresentationTestSupport.h"
#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <imgui/imgui.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <thread>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;
	using Microsoft::WRL::ComPtr;
	using support::CreateImageResources;
	using support::ReadPixels;
	using support::ReferenceCount;
	using support::presentation::ImGuiFrame;

	void run_presentation_image_resource_checks(Runner& runner)
	{
		runner.test("CPU images consume pixels and replace resources transactionally", [] {
			auto resources = CreateImageResources();
			ImGuiFrame frame;
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameDraw
			};
			(void)execution.NoteBinding(1);
			PresentationServices::SetDevice(resources.device.Get());
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
				const RenderExecution::ClientGuard callback{ 21, true };
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
				const RenderExecution::ClientGuard callback{ 21, true };
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
			PresentationServices::SetDevice(resources.device.Get());
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
			PresentationServices::SetDevice(resources.device.Get());
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
			PresentationServices::SetDevice(resources.device.Get());
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
					const RenderExecution::ClientGuard callback{
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
					const RenderExecution::ClientGuard callback{
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
			PresentationServices::SetDevice(resources.device.Get());
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
				const RenderExecution::ClientGuard callback{ 7, true };
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

	}
}
