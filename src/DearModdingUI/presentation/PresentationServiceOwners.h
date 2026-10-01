#pragma once

#include <DearModdingUI/API.h>
#include <cstdint>

struct ID3D11Device;
struct ID3D11ShaderResourceView;

namespace DearModdingUI::PresentationServices
{
	namespace ImageResources
	{
		struct AcquiredImage
		{
			ID3D11ShaderResourceView* view{};
			uint32_t width{};
			uint32_t height{};
		};

		[[nodiscard]] DMUI_Result Acquire(
			DMUI_ClientHandle a_client, DMUI_ImageHandle a_image, AcquiredImage& a_acquired) noexcept;
		void SetDevice(ID3D11Device* a_device) noexcept;
		void ReleaseFrameLeases() noexcept;
		[[nodiscard]] bool HasDevice() noexcept;
		[[nodiscard]] uint64_t DeviceGeneration() noexcept;
	}

	namespace Notifications
	{
		void Clear() noexcept;
		[[nodiscard]] bool HasFrameDemand() noexcept;
	}

	namespace Dialogs
	{
		[[nodiscard]] bool HasFrameDemand() noexcept;
	}
}
