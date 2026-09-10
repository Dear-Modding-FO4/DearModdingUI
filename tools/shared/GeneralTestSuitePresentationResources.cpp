#include "GeneralTestSuiteInternal.h"

#include <algorithm>
#include <limits>

namespace DmuiTests::Detail
{
	PresentationResources::PresentationResources(
		DiagnosticContext& a_context) noexcept :
		m_context(a_context)
	{
		m_samples.fill(0.0f);
	}

	PresentationResources::~PresentationResources() = default;

	void PresentationResources::ObserveFrame() noexcept
	{
		const auto now = std::chrono::steady_clock::now();
		if (m_startTime == std::chrono::steady_clock::time_point{})
		{
			m_startTime = now;
			m_previousFrame = now;
		}
		const auto frameSeconds =
			std::chrono::duration<float>(now - m_previousFrame).count();
		m_previousFrame = now;
		if (!m_plotSeeded)
		{
			m_elapsedSeconds =
				std::chrono::duration<double>(now - m_startTime).count();
			m_samples[m_sampleOffset] =
				(std::min)(frameSeconds * 1000.0f, 50.0f);
			m_sampleOffset = (m_sampleOffset + 1) % m_samples.size();
		}

		RefreshImportedImage();
		RefreshCpuImage();
		if (m_presentationImageUpdatePending && m_cpuImage)
		{
			UpdateCpuImage();
			m_presentationImageUpdatePending = false;
		}
	}

	void PresentationResources::RefreshImportedImage() noexcept
	{
		auto currentDevice =
			m_context.HostEnvironment().AcquireRendererDevice();
		if (!currentDevice)
		{
			if (!m_deviceWaitingLogged)
			{
				m_deviceWaitingLogged = true;
				m_context.Info(
					"dmui-test-client: waiting for the renderer D3D11 device"sv);
			}
			return;
		}
		m_deviceWaitingLogged = false;

		bool stale{};
		auto& client = m_context.Client();
		if (m_image)
		{
			const auto info = client.QueryImage(m_image->Handle());
			m_imageResult = client.LastResult();
			if (!info)
			{
				LogImageFailure("query", m_imageResult);
				stale = true;
			}
			else
			{
				if (m_imageStatus != info->status ||
					m_imageGeneration != info->deviceGeneration)
				{
					m_context.Info(
						"dmui-test-client: image status transition "
						"status={}->{} generation={}->{}"sv,
						static_cast<uint32_t>(m_imageStatus),
						static_cast<uint32_t>(info->status),
						m_imageGeneration,
						info->deviceGeneration);
				}
				m_imageStatus = info->status;
				m_imageGeneration = info->deviceGeneration;
				stale = info->status != DMUI_IMAGE_STATUS_READY;
			}
		}

		const auto deviceChanged =
			m_imageDevice && m_imageDevice.Get() != currentDevice.Get();
		if (deviceChanged)
		{
			++m_deviceChangeCount;
			m_context.Info(
				"dmui-test-client: renderer device transition count={}"sv,
				m_deviceChangeCount);
		}
		const auto recreate =
			m_recreateImage.exchange(false, std::memory_order_acq_rel);
		if (deviceChanged || stale || recreate)
		{
			ReleaseImportedImage(
				deviceChanged ? "device-transition" :
					stale ? "stale-or-invalidated" :
						"cycle");
		}
		if (m_image)
			return;
		m_imageDevice = std::move(currentDevice);
		CreateImportedImage();
	}

	void PresentationResources::ReleaseImportedImage(
		std::string_view a_reason) noexcept
	{
		const bool owned =
			m_image.has_value() ||
			static_cast<bool>(m_imageView) ||
			static_cast<bool>(m_imageTexture) ||
			static_cast<bool>(m_imageDevice);
		m_image.reset();
		m_imageView.Reset();
		m_imageTexture.Reset();
		m_imageDevice.Reset();
		if (owned)
		{
			m_imageStatus = DMUI_IMAGE_STATUS_RELEASED;
			++m_imageReleaseCount;
			m_context.Info(
				"dmui-test-client: image release reason={} status={} count={}"sv,
				a_reason,
				static_cast<uint32_t>(m_imageStatus),
				m_imageReleaseCount);
		}
	}

	void PresentationResources::LogImageFailure(
		std::string_view a_scope,
		DMUI_Result a_result) noexcept
	{
		if (m_imageFailureActive &&
			m_lastImageFailureScope == a_scope &&
			m_lastImageFailureResult == a_result)
			return;
		m_imageFailureActive = true;
		m_lastImageFailureScope = a_scope;
		m_lastImageFailureResult = a_result;
		++m_imageFailureCount;
		m_context.Error(
			"dmui-test-client: image failure scope={} result={} count={}"sv,
			a_scope,
			DMUI_ResultToString(a_result),
			m_imageFailureCount);
	}

	void PresentationResources::CreateImportedImage() noexcept
	{
		std::array<uint32_t, kImageExtent * kImageExtent> pixels{};
		for (uint32_t y = 0; y < kImageExtent; ++y)
		{
			for (uint32_t x = 0; x < kImageExtent; ++x)
			{
				const auto checker = ((x / 8) + (y / 8)) % 2;
				const auto red = static_cast<uint8_t>(
					48u + (x * 207u) / (kImageExtent - 1));
				const auto green = static_cast<uint8_t>(
					48u + (y * 207u) / (kImageExtent - 1));
				const auto blue = static_cast<uint8_t>(
					checker ? 230u : 45u);
				pixels[y * kImageExtent + x] =
					UINT32_C(0xFF000000) |
					(static_cast<uint32_t>(blue) << 16u) |
					(static_cast<uint32_t>(green) << 8u) |
					red;
			}
		}

		const D3D11_TEXTURE2D_DESC description{
			kImageExtent,
			kImageExtent,
			1,
			1,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			{ 1, 0 },
			D3D11_USAGE_IMMUTABLE,
			D3D11_BIND_SHADER_RESOURCE,
			0,
			0
		};
		const D3D11_SUBRESOURCE_DATA initial{
			pixels.data(),
			kImageExtent * sizeof(uint32_t),
			0
		};
		if (FAILED(m_imageDevice->CreateTexture2D(
				&description,
				&initial,
				m_imageTexture.ReleaseAndGetAddressOf())) ||
			FAILED(m_imageDevice->CreateShaderResourceView(
				m_imageTexture.Get(),
				nullptr,
				m_imageView.ReleaseAndGetAddressOf())))
		{
			m_imageResult = DMUI_RESULT_RESOURCE_EXHAUSTED;
			LogImageFailure("D3D11 resource creation", m_imageResult);
			m_imageView.Reset();
			m_imageTexture.Reset();
			return;
		}

		auto& client = m_context.Client();
		auto image = client.ImportD3D11Image(
			m_imageView.Get(),
			kImageExtent,
			kImageExtent);
		m_imageResult = client.LastResult();
		if (!image)
		{
			LogImageFailure("host image import", m_imageResult);
			m_imageView.Reset();
			m_imageTexture.Reset();
			return;
		}
		m_image = std::move(*image);
		++m_imageImportCount;
		m_imageFailureActive = false;
		m_context.Info(
			"dmui-test-client: image import result={} count={}"sv,
			DMUI_ResultToString(m_imageResult),
			m_imageImportCount);
	}

	void PresentationResources::QueueImportedImage(bool a_large) noexcept
	{
		if (!m_image)
		{
			dmui::ui::TextDisabled("Image waiting for renderer/import.");
			return;
		}
		const auto extent = a_large ? 112.0f : 64.0f;
		const DMUI_ImageDrawOptions options{
			sizeof(DMUI_ImageDrawOptions),
			{ extent, extent },
			{ 0.0f, 0.0f },
			{ 1.0f, 1.0f },
			{ 1.0f, 1.0f, 1.0f, 1.0f },
			1,
			0
		};
		auto& client = m_context.Client();
		if (client.DrawImage(m_image->Handle(), options))
			++m_imageDrawCount;
		m_imageResult = client.LastResult();
	}

	void PresentationResources::FillCpuPixels(
		std::array<uint8_t, 80u * 64u * 4u>& a_pixels,
		uint32_t a_width,
		uint32_t a_height,
		uint32_t a_step) noexcept
	{
		for (uint32_t y = 0; y < a_height; ++y)
		{
			for (uint32_t x = 0; x < a_width; ++x)
			{
				const auto offset = (y * a_width + x) * 4u;
				a_pixels[offset] = static_cast<uint8_t>(
					32u + (x * 191u) / (a_width - 1u));
				a_pixels[offset + 1u] = static_cast<uint8_t>(
					32u + (y * 191u) / (a_height - 1u));
				a_pixels[offset + 2u] = a_step % 2u == 0 ? 64u : 224u;
				a_pixels[offset + 3u] = static_cast<uint8_t>(
					96u + ((x + y) % 2u) * 159u);
			}
		}
	}

	DMUI_ImageDescriptor PresentationResources::CpuImageDescriptor(
		const std::array<uint8_t, 80u * 64u * 4u>& a_pixels,
		uint32_t a_width,
		uint32_t a_height) noexcept
	{
		return {
			sizeof(DMUI_ImageDescriptor),
			a_width,
			a_height,
			DMUI_PIXEL_FORMAT_RGBA8_UNORM,
			0,
			static_cast<uint64_t>(a_width) * 4u,
			static_cast<uint64_t>(a_width) * a_height * 4u,
			a_pixels.data()
		};
	}

	void PresentationResources::RefreshCpuImage() noexcept
	{
		auto& client = m_context.Client();
		if (m_cpuImage)
		{
			const auto info = client.QueryImage(m_cpuImage->Handle());
			m_cpuImageResult = client.LastResult();
			if (!info || info->status != DMUI_IMAGE_STATUS_READY)
			{
				m_cpuImage.reset();
				++m_cpuImageReleaseCount;
				m_context.Info(
					"dmui-test-client: CPU image recycle result={} count={}"sv,
					DMUI_ResultToString(m_cpuImageResult),
					m_cpuImageReleaseCount);
			}
			else
			{
				m_cpuImageWidth = info->contentWidth;
				m_cpuImageHeight = info->contentHeight;
			}
		}
		if (m_cpuImage)
			return;

		std::array<uint8_t, 80u * 64u * 4u> pixels{};
		constexpr uint32_t width{ 48 };
		constexpr uint32_t height{ 48 };
		FillCpuPixels(pixels, width, height, m_cpuImageStep);
		auto image = client.CreateImage(
			CpuImageDescriptor(pixels, width, height));
		m_cpuImageResult = client.LastResult();
		if (!image)
		{
			LogImageFailure("CPU image create", m_cpuImageResult);
			return;
		}
		m_cpuImage = std::move(*image);
		m_cpuImageWidth = width;
		m_cpuImageHeight = height;
		++m_cpuImageCreateCount;
		m_imageFailureActive = false;
		m_context.Info(
			"dmui-test-client: CPU image create result={} "
			"dimensions={}x{} count={}"sv,
			DMUI_ResultToString(m_cpuImageResult),
			width,
			height,
			m_cpuImageCreateCount);
	}

	void PresentationResources::UpdateCpuImage() noexcept
	{
		if (!m_cpuImage)
			return;
		++m_cpuImageStep;
		const auto width = m_cpuImageStep % 2u == 0 ? 48u : 72u;
		const auto height = m_cpuImageStep % 2u == 0 ? 48u : 40u;
		std::array<uint8_t, 80u * 64u * 4u> pixels{};
		FillCpuPixels(pixels, width, height, m_cpuImageStep);
		auto& client = m_context.Client();
		const auto updated = client.UpdateImage(
			m_cpuImage->Handle(),
			CpuImageDescriptor(pixels, width, height));
		m_cpuImageResult = client.LastResult();
		if (!updated)
		{
			LogImageFailure("CPU image update", m_cpuImageResult);
			return;
		}
		m_cpuImageWidth = width;
		m_cpuImageHeight = height;
		++m_cpuImageUpdateCount;
		m_imageFailureActive = false;
		m_context.Info(
			"dmui-test-client: CPU image update result={} "
			"dimensions={}x{} count={}"sv,
			DMUI_ResultToString(m_cpuImageResult),
			width,
			height,
			m_cpuImageUpdateCount);
	}

	void PresentationResources::QueueCpuImage() noexcept
	{
		if (!m_cpuImage)
			return;
		const DMUI_ImageDrawOptions options{
			sizeof(DMUI_ImageDrawOptions),
			{ 112.0f, 80.0f },
			{ 0.0f, 0.0f },
			{ 1.0f, 1.0f },
			{ 1.0f, 1.0f, 1.0f, 1.0f },
			1,
			0
		};
		auto& client = m_context.Client();
		if (client.DrawImage(m_cpuImage->Handle(), options))
			++m_cpuImageDrawCount;
		m_cpuImageResult = client.LastResult();
	}

	void PresentationResources::ReleaseAfterQueuedDraw() noexcept
	{
		if (!m_image)
		{
			m_context.Info(
				"dmui-test-client: release-after-queued-draw "
				"ignored; image=unexercised"sv);
			return;
		}
		m_imageResult = m_image->Release();
		m_context.Info(
			"dmui-test-client: release-after-queued-draw result={}"sv,
			DMUI_ResultToString(m_imageResult));
		ReleaseImportedImage("release-after-queued-draw");
		m_recreateImage.store(true, std::memory_order_release);
	}

	void PresentationResources::RequestImageCycle() noexcept
	{
		++m_imageCycleRequests;
		const auto owned =
			m_image.has_value() || m_imageView || m_imageTexture || m_imageDevice;
		m_context.Info(
			"dmui-test-client: image cycle requested count={} owned={}"sv,
			m_imageCycleRequests,
			owned);
		m_recreateImage.store(true, std::memory_order_release);
	}

	void PresentationResources::DrawImages() noexcept
	{
		auto& client = m_context.Client();
		(void)client.DrawSectionHeader("Shared image resources");
		dmui::ui::TextUnformatted("Host-owned CPU-pixel image");
		QueueCpuImage();
		dmui::ui::Text(
			"creates=%llu updates=%llu draws=%llu dimensions=%ux%u result=%s",
			m_cpuImageCreateCount,
			m_cpuImageUpdateCount,
			m_cpuImageDrawCount,
			m_cpuImageWidth,
			m_cpuImageHeight,
			DMUI_ResultToString(m_cpuImageResult));
		if (dmui::ui::Button("Update CPU image"))
			UpdateCpuImage();
		dmui::ui::TextDisabled(
			"Expected: the same handle changes dimensions and pixels.");
		dmui::ui::TextUnformatted("Existing imported D3D11 SRV");
		QueueImportedImage(true);
		dmui::ui::Text(
			"imports=%llu draws=%llu releases=%llu status=%u generation=%llu",
			m_imageImportCount,
			m_imageDrawCount,
			m_imageReleaseCount,
			m_imageStatus,
			m_imageGeneration);
		if (dmui::ui::Button("Cycle / recreate image"))
			RequestImageCycle();
		dmui::ui::SameLine();
		if (dmui::ui::Button("Release after queued draw"))
			ReleaseAfterQueuedDraw();
		dmui::ui::TextDisabled(
			"Expected: the already queued draw survives release; "
			"the observer imports one replacement.");
	}

	void PresentationResources::DrawPlot(const char* a_id) noexcept
	{
		const DMUI_PlotReferenceLine references[]{
			{ 16.67f, { 0.25f, 0.85f, 0.35f, 0.90f } },
			{ 33.33f, { 0.95f, 0.55f, 0.20f, 0.90f } }
		};
		const DMUI_AnnotatedPlotDescriptor plot{
			sizeof(DMUI_AnnotatedPlotDescriptor),
			m_samples.data(),
			static_cast<uint32_t>(m_samples.size()),
			static_cast<uint32_t>(m_sampleOffset),
			0.0f,
			50.0f,
			{ 310.0f, 82.0f },
			"FRAME TIME (MS) - LABEL MUST REMAIN VISIBLE / CLIPPED LINES",
			references,
			static_cast<uint32_t>(std::size(references))
		};
		auto& client = m_context.Client();
		if (client.DrawAnnotatedPlot(a_id, plot))
			++m_plotDraws;
		m_plotResult = client.LastResult();
	}

	void PresentationResources::SeedPlot() noexcept
	{
		static constexpr std::array pattern{
			8.4f, 8.1f, 8.7f, 9.2f, 8.8f, 16.3f, 9.0f, 8.5f,
			8.2f, 8.0f, 8.6f, 9.1f, 8.7f, 8.4f, 8.3f, 8.1f
		};
		for (size_t index = 0; index < m_samples.size(); ++index)
			m_samples[index] = pattern[index % pattern.size()];
		m_sampleOffset = 0;
		m_elapsedSeconds = 12.5;
		m_plotSeeded = true;
	}

	void PresentationResources::DrawOverlayImages() noexcept
	{
		QueueImportedImage(false);
		QueueCpuImage();
	}

	uint64_t PresentationResources::ImageEventCount() const noexcept
	{
		return
			m_imageImportCount + m_imageReleaseCount +
			m_imageCycleRequests + m_imageFailureCount +
			m_deviceChangeCount + m_cpuImageCreateCount +
			m_cpuImageUpdateCount;
	}

	uint64_t PresentationResources::InteractionEventCount() const noexcept
	{
		return m_cpuImageUpdateCount + m_imageCycleRequests;
	}

	bool PresentationResources::ImageFailed() const noexcept
	{
		return m_imageFailureCount > 0 ||
			(m_cpuImageCreateCount > 0 &&
			 m_cpuImageResult != DMUI_RESULT_OK);
	}

	bool PresentationResources::ImageCaptureComplete() const noexcept
	{
		return
			m_imageFailureCount == 0 &&
			m_imageImportCount > 0 &&
			m_imageDrawCount > 0 &&
			m_cpuImageCreateCount > 0 &&
			m_cpuImageUpdateCount > 0 &&
			m_cpuImageDrawCount > 0 &&
			m_imageResult == DMUI_RESULT_OK &&
			m_cpuImageResult == DMUI_RESULT_OK;
	}

	bool PresentationResources::PlotCaptureComplete() const noexcept
	{
		return m_plotDraws > 0 && m_plotResult == DMUI_RESULT_OK;
	}

	uint64_t PresentationResources::PlotDraws() const noexcept
	{
		return m_plotDraws;
	}

	DMUI_Result PresentationResources::ImageResult() const noexcept
	{
		return m_imageResult;
	}

	DMUI_Result PresentationResources::CpuImageResult() const noexcept
	{
		return m_cpuImageResult;
	}

	DMUI_Result PresentationResources::PlotResult() const noexcept
	{
		return m_plotResult;
	}

	double PresentationResources::ElapsedSeconds() const noexcept
	{
		return m_elapsedSeconds;
	}

	PresentationResources::Snapshot
		PresentationResources::CurrentSnapshot() const noexcept
	{
		return {
			m_imageImportCount,
			m_imageReleaseCount,
			m_imageCycleRequests,
			m_imageFailureCount,
			m_cpuImageCreateCount,
			m_cpuImageUpdateCount
		};
	}

	void PresentationResources::RequestPresentationImageUpdate() noexcept
	{
		m_presentationImageUpdatePending = true;
	}
}
