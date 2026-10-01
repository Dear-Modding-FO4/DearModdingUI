#include "../support/D3DTestResources.h"
#include "../support/PresentationTestSupport.h"
#include <imgui/imgui.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/presentation/images/ImageFileQueue.h>
#include <Support/Runtime.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <thread>

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI;
		namespace images = PresentationServices;
		constexpr DMUI_ClientHandle kOwner{ 9001 };

		std::string Fixture(const char* a_name)
		{
			return (std::filesystem::current_path() / "tests/fixtures/images" / a_name).string();
		}

		struct ImageEnvironment
		{
			support::D3DTestResources resources{ support::CreateImageResources() };
			RenderExecution::Guard execution{ RenderExecution::Phase::kFrameDraw };
			ImageEnvironment()
			{
				(void)execution.NoteBinding(1);
				images::SetDevice(resources.device.Get());
			}
			~ImageEnvironment()
			{
				images::CancelClientImages(kOwner);
				images::SetDevice(nullptr);
			}
		};

		DMUI_ImageInfo Info(DMUI_ImageHandle a_handle)
		{
			DMUI_ImageInfo info{};
			require(images::QueryImage(kOwner, a_handle, &info) == DMUI_RESULT_OK, "query failed");
			return info;
		}

		DMUI_ImageHandle Load(const std::string& a_path)
		{
			DMUI_ImageHandle handle{};
			require(images::LoadImageFile(kOwner, a_path.c_str(), &handle) == DMUI_RESULT_OK,
				"load was not accepted");
			require(Info(handle).status == DMUI_IMAGE_STATUS_LOADING,
				"load published outside the frame boundary");
			return handle;
		}

		DMUI_ImageInfo Complete(DMUI_ImageHandle a_handle)
		{
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
			for (;;)
			{
				images::BeginFrame();
				const auto info = Info(a_handle);
				if (info.status != DMUI_IMAGE_STATUS_LOADING)
					return info;
				require(std::chrono::steady_clock::now() < deadline, "file image completion timed out");
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}

	void run_presentation_image_file_checks(Runner& runner)
	{
		runner.test("draw-list images skip loading and device loss but reject foreign and released handles", [] {
			ImageEnvironment env;
			support::presentation::ImGuiFrame frame;
			const auto image = Load(Fixture("Alpha.png"));
			const auto& api = UI::API();
			const auto draw = [&](DMUI_ClientHandle owner) {
				const RenderExecution::ClientGuard callback{ owner, true };
				return api.drawListAddImage(owner, DMUI_DRAW_TARGET_WINDOW, image,
					{ 30, 30 }, { 90, 90 }, { 0, 0 }, { 1, 1 }, ~0u);
			};
			auto* list = ImGui::GetWindowDrawList();
			const auto before = list->VtxBuffer.Size;
			require(draw(kOwner) == DMUI_RESULT_OK && list->VtxBuffer.Size == before,
				"loading image failed or submitted geometry");
			require(draw(kOwner + 1) == DMUI_RESULT_STALE_HANDLE, "foreign image was accepted");
			require(Complete(image).status == DMUI_IMAGE_STATUS_READY, "image did not load");
			require(draw(kOwner) == DMUI_RESULT_OK && list->VtxBuffer.Size > before,
				"ready draw-list image did not submit");
			const auto ready = list->VtxBuffer.Size;
			images::InvalidateDevice();
			require(draw(kOwner) == DMUI_RESULT_OK && list->VtxBuffer.Size == ready,
				"device-lost image was not skipped");
			require(images::ReleaseImage(kOwner, image) == DMUI_RESULT_OK &&
				draw(kOwner) == DMUI_RESULT_STALE_HANDLE, "released image was accepted");
			images::CompleteRenderSubmission();
		});

		runner.test("File PNG uploads straight RGBA and DDS preserves mips without sRGB sampling", [] {
			ImageEnvironment env;
			const auto png = Load(Fixture("Alpha.png"));
			require(Complete(png).status == DMUI_IMAGE_STATUS_READY, "PNG failed to load");
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
			view.Attach(images::RetainImageViewForTests(kOwner, png));
			uint32_t width{}, height{};
			const auto pixels = support::ReadPixels(env.resources.device.Get(), env.resources.context.Get(),
				view.Get(), width, height);
			require(width == 2 && height == 2 && pixels == std::vector<uint8_t>{
				180, 90, 30, 128, 40, 120, 200, 64, 70, 80, 90, 0, 15, 25, 35, 255 },
				"WIC changed stored color values or premultiplied alpha");
			const auto dds = Load(Fixture("Tiles.dds"));
			const auto info = Complete(dds);
			require(info.status == DMUI_IMAGE_STATUS_READY && info.contentWidth == 8 && info.contentHeight == 8,
				"BC1 DDS failed to load");
			view.Attach(images::RetainImageViewForTests(kOwner, dds));
			D3D11_SHADER_RESOURCE_VIEW_DESC srv{};
			view->GetDesc(&srv);
			require(srv.Format == DXGI_FORMAT_BC1_UNORM && srv.Texture2D.MipLevels == 4,
				"DDS mip chain or stored-value sampling was lost");
		});

		runner.test("File failures stay queryable and unsafe virtual paths are denied", [] {
			ImageEnvironment env;
			const std::array cases{
				std::pair{ Fixture("Missing.png"), DMUI_RESULT_FILE_NOT_FOUND },
				std::pair{ Fixture("Unsupported.bin"), DMUI_RESULT_UNSUPPORTED_RESOURCE },
				std::pair{ Fixture("Cube.dds"), DMUI_RESULT_UNSUPPORTED_RESOURCE },
				std::pair{ std::string{ "textures/../../Outside.png" }, DMUI_RESULT_ACCESS_DENIED },
				std::pair{ std::string{ "\\\\server\\share\\image.png" }, DMUI_RESULT_ACCESS_DENIED },
				std::pair{ std::string{ "\\\\?\\C:\\image.png" }, DMUI_RESULT_ACCESS_DENIED }
			};
			for (const auto& [path, failure] : cases)
			{
				const auto image = Load(path);
				const auto info = Complete(image);
				require(info.status == DMUI_IMAGE_STATUS_FAILED && info.failure == failure,
					"wrong failure for " + path + ": " + DMUI_ResultToString(info.failure));
				require(Info(image).failure == failure, "failed handle did not retain its failure");
				require(images::ReleaseImage(kOwner, image) == DMUI_RESULT_OK, "failed release rejected");
			}
		});

		runner.test("Released loading slots and retired owners cannot receive late file completions", [] {
			ImageEnvironment env;
			const auto old = Load(Fixture("Tiles.dds"));
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
			while (!images::HasFrameDemand())
			{
				require(std::chrono::steady_clock::now() < deadline, "decoded completion did not arrive");
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			require(Info(old).status == DMUI_IMAGE_STATUS_LOADING, "worker published before BeginFrame");
			require(images::ReleaseImage(kOwner, old) == DMUI_RESULT_OK, "loading release failed");
			const auto replacement = Load(Fixture("Alpha.png"));
			require(static_cast<uint32_t>(old) == static_cast<uint32_t>(replacement) && old != replacement,
				"test did not exercise slot reuse");
			require(Complete(replacement).status == DMUI_IMAGE_STATUS_READY &&
				Info(replacement).contentWidth == 2, "late DDS completion overwrote replacement PNG");
			DMUI_ImageInfo stale{};
			require(images::QueryImage(kOwner, old, &stale) == DMUI_RESULT_STALE_HANDLE,
				"old generation remained addressable");
			const auto retired = Load(Fixture("Tiles.dds"));
			images::CancelClientImages(kOwner);
			const auto fresh = Load(Fixture("Alpha.png"));
			require(Complete(fresh).status == DMUI_IMAGE_STATUS_READY, "retirement poisoned future work");
			require(images::QueryImage(kOwner, retired, &stale) != DMUI_RESULT_OK ||
				stale.status == DMUI_IMAGE_STATUS_RELEASED, "retired work published");
		});

		runner.test("Device changes reload from the file instead of retaining encoded or decoded data", [] {
			ImageEnvironment env;
			const auto folder = std::filesystem::path{ Addictol::Support::GetRuntimeDirectory() } / "Data/ImageFileTests";
			std::filesystem::create_directories(folder);
			const auto file = folder / "Reload.png";
			std::filesystem::copy_file(Fixture("Alpha.png"), file, std::filesystem::copy_options::overwrite_existing);
			const auto image = Load("ImageFileTests/./Reload.png");
			const auto before = Complete(image);
			require(before.status == DMUI_IMAGE_STATUS_READY && before.contentWidth == 2, "relative Data path failed");
			std::filesystem::copy_file(Fixture("Tiles.png"), file, std::filesystem::copy_options::overwrite_existing);
			auto device = support::CreateImageResources();
			images::SetDevice(device.device.Get());
			const auto loading = Info(image);
			require(loading.status == DMUI_IMAGE_STATUS_LOADING &&
				loading.deviceGeneration != before.deviceGeneration, "device change did not restart loading");
			const auto after = Complete(image);
			require(after.status == DMUI_IMAGE_STATUS_READY && after.contentWidth == 8,
				"reload reused old bytes instead of reading the changed source");
			std::filesystem::remove(file);
			images::SetDevice(env.resources.device.Get());
			require(Complete(image).failure == DMUI_RESULT_FILE_NOT_FOUND, "reload failure was not queryable");
		});

		runner.test("File work stays bounded until completion publication", [] {
			ImageEnvironment env;
			using images::ImageFiles::FileQueue;
			std::array<DMUI_ImageHandle, FileQueue::kClientCapacity> handles{};
			for (auto& handle : handles)
				handle = Load(Fixture("Tiles.png"));
			DMUI_ImageHandle rejected{};
			require(images::LoadImageFile(kOwner, Fixture("Tiles.png").c_str(), &rejected) == DMUI_RESULT_BUSY &&
				rejected == DMUI_INVALID_IMAGE_HANDLE, "client in-flight cap did not reject work");
			for (const auto handle : handles)
				require(Complete(handle).status == DMUI_IMAGE_STATUS_READY, "bounded queue lost accepted work");
			require(Complete(Load(Fixture("Tiles.png"))).status == DMUI_IMAGE_STATUS_READY,
				"published work did not return capacity");

			FileQueue queue;
			for (size_t index = 0; index < FileQueue::kCapacity; ++index)
				require(queue.Submit({ index / FileQueue::kClientCapacity + 1, index + 1, 1, 1 },
					Fixture("Tiles.png")) == DMUI_RESULT_OK, "global queue rejected within capacity");
			require(queue.Submit({ 99, 99, 1, 1 }, Fixture("Tiles.png")) == DMUI_RESULT_BUSY,
				"global queue cap did not reject another client's work");
		});
	}
}
