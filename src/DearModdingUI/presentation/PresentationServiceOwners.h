#pragma once

#include <cstdint>

struct ID3D11Device;

namespace DearModdingUI::PresentationServices
{
	namespace ImageResources
	{
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
