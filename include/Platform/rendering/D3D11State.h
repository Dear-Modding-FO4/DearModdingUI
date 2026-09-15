#pragma once

#include <d3d11_1.h>
#include <wrl/client.h>

#include <array>

namespace DearModdingUI::Rendering
{
	template <class Shader>
	struct ShaderState
	{
		Microsoft::WRL::ComPtr<Shader> shader;
		std::array<ID3D11ClassInstance*, 256> instances{};
		UINT count{ static_cast<UINT>(instances.size()) };

		ShaderState() = default;
		ShaderState(const ShaderState&) = delete;
		ShaderState& operator=(const ShaderState&) = delete;

		~ShaderState() noexcept
		{
			for (UINT index = 0; index < count; ++index)
				if (instances[index])
					instances[index]->Release();
		}
	};

	class RenderTargetState
	{
	public:
		explicit RenderTargetState(ID3D11DeviceContext* a_context) noexcept
		{
			Microsoft::WRL::ComPtr<ID3D11Device> device;
			a_context->GetDevice(device.GetAddressOf());
			m_unorderedCount = device->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_1 ?
				D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;
			a_context->OMGetRenderTargetsAndUnorderedAccessViews(
				static_cast<UINT>(m_targets.size()),
				m_targets.data(),
				m_depthStencil.GetAddressOf(),
				0, m_unorderedCount, m_unordered.data());
			while (m_targetCount && !m_targets[m_targetCount - 1])
				--m_targetCount;
		}

		~RenderTargetState() noexcept
		{
			for (auto* target : m_targets)
				if (target)
					target->Release();
			for (auto* view : m_unordered)
				if (view)
					view->Release();
		}

		RenderTargetState(const RenderTargetState&) = delete;
		RenderTargetState& operator=(const RenderTargetState&) = delete;

		void Restore(ID3D11DeviceContext* a_context) const noexcept
		{
			std::array<UINT, D3D11_1_UAV_SLOT_COUNT> counts;
			counts.fill(D3D11_KEEP_UNORDERED_ACCESS_VIEWS);
			// RTV slots overlap pixel UAV slots, including null trailing RTVs.
			a_context->OMSetRenderTargetsAndUnorderedAccessViews(
				m_targetCount,
				m_targets.data(),
				m_depthStencil.Get(),
				m_targetCount, m_unorderedCount - m_targetCount,
				m_unordered.data() + m_targetCount, counts.data());
		}

	private:
		std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT>
			m_targets{};
		UINT m_targetCount{ static_cast<UINT>(m_targets.size()) };
		std::array<ID3D11UnorderedAccessView*, D3D11_1_UAV_SLOT_COUNT> m_unordered{};
		UINT m_unorderedCount{};
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthStencil;
	};
}
