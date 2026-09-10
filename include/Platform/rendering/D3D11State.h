#pragma once

#include <d3d11.h>
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
			a_context->OMGetRenderTargets(
				static_cast<UINT>(m_targets.size()),
				m_targets.data(),
				m_depthStencil.GetAddressOf());
		}

		~RenderTargetState() noexcept
		{
			for (auto* target : m_targets)
				if (target)
					target->Release();
		}

		RenderTargetState(const RenderTargetState&) = delete;
		RenderTargetState& operator=(const RenderTargetState&) = delete;

		void Restore(ID3D11DeviceContext* a_context) const noexcept
		{
			a_context->OMSetRenderTargets(
				static_cast<UINT>(m_targets.size()),
				m_targets.data(),
				m_depthStencil.Get());
		}

	private:
		std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT>
			m_targets{};
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthStencil;
	};
}
