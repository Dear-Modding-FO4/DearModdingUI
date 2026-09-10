#include <DearModdingUI/presentation/BlurPipelineState.h>
#include <Platform/rendering/D3D11State.h>

#include "../Harness.h"
#include "../support/D3DTestResources.h"

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>

namespace vmm_tests
{
	namespace
	{
		using Microsoft::WRL::ComPtr;
		using DearModdingUI::BackgroundBlur::BlurPipelineState;
		using DearModdingUI::Rendering::RenderTargetState;

		struct ShaderEntry
		{
			const wchar_t* file;
			const char* entry;
			const char* target;
		};

		[[nodiscard]] std::filesystem::path ShaderPath(
			const wchar_t* a_file)
		{
			return std::filesystem::current_path() /
				L"data/F4SE/Plugins/DearModdingUI/Shaders" /
				a_file;
		}

		[[nodiscard]] ComPtr<ID3DBlob> CompileShader(
			const ShaderEntry& a_shader)
		{
			ComPtr<ID3DBlob> bytecode;
			ComPtr<ID3DBlob> diagnostics;
			const auto path = ShaderPath(a_shader.file);
			const auto result = D3DCompileFromFile(
				path.c_str(),
				nullptr,
				D3D_COMPILE_STANDARD_FILE_INCLUDE,
				a_shader.entry,
				a_shader.target,
				D3DCOMPILE_ENABLE_STRICTNESS,
				0,
				&bytecode,
				&diagnostics);
			if (FAILED(result))
			{
				std::string message{
					"shader compilation failed for "
				};
				message += a_shader.entry;
				if (diagnostics && diagnostics->GetBufferPointer())
				{
					message += ": ";
					message.append(
						static_cast<const char*>(
							diagnostics->GetBufferPointer()),
						diagnostics->GetBufferSize());
				}
				throw Failure(std::move(message));
			}
			require(bytecode && bytecode->GetBufferSize() != 0,
				"shader compilation returned empty bytecode");
			return bytecode;
		}

		[[nodiscard]] ComPtr<ID3DBlob> CompileShaderSource(
			const char* a_source,
			const char* a_entry,
			const char* a_target)
		{
			ComPtr<ID3DBlob> bytecode;
			ComPtr<ID3DBlob> diagnostics;
			const auto result = D3DCompile(
				a_source,
				std::strlen(a_source),
				"BlurPipelineStateTest",
				nullptr,
				nullptr,
				a_entry,
				a_target,
				D3DCOMPILE_ENABLE_STRICTNESS,
				0,
				&bytecode,
				&diagnostics);
			if (FAILED(result))
			{
				std::string message{ "test shader compilation failed" };
				if (diagnostics && diagnostics->GetBufferPointer())
				{
					message += ": ";
					message.append(
						static_cast<const char*>(
							diagnostics->GetBufferPointer()),
						diagnostics->GetBufferSize());
				}
				throw Failure(std::move(message));
			}
			return bytecode;
		}

		struct OutputResources
		{
			std::array<ComPtr<ID3D11Texture2D>, 2> colorTextures;
			std::array<ComPtr<ID3D11RenderTargetView>, 2> targets;
			ComPtr<ID3D11Texture2D> depthTexture;
			ComPtr<ID3D11DepthStencilView> depth;
		};

		[[nodiscard]] OutputResources CreateOutputResources(
			ID3D11Device* a_device)
		{
			OutputResources resources;
			const D3D11_TEXTURE2D_DESC colorDescription{
				64,
				32,
				1,
				1,
				DXGI_FORMAT_R8G8B8A8_UNORM,
				{ 1, 0 },
				D3D11_USAGE_DEFAULT,
				D3D11_BIND_RENDER_TARGET,
				0,
				0
			};
			for (size_t index = 0; index < resources.targets.size(); ++index)
			{
				require(SUCCEEDED(a_device->CreateTexture2D(
							&colorDescription,
							nullptr,
							&resources.colorTextures[index])),
					"render-target texture creation failed");
				require(SUCCEEDED(a_device->CreateRenderTargetView(
							resources.colorTextures[index].Get(),
							nullptr,
							&resources.targets[index])),
					"render-target view creation failed");
			}

			auto depthDescription = colorDescription;
			depthDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			depthDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			require(SUCCEEDED(a_device->CreateTexture2D(
						&depthDescription,
						nullptr,
						&resources.depthTexture)),
				"depth-stencil texture creation failed");
			require(SUCCEEDED(a_device->CreateDepthStencilView(
						resources.depthTexture.Get(),
						nullptr,
						&resources.depth)),
				"depth-stencil view creation failed");
			return resources;
		}

		void RequireOutputTargets(
			ID3D11DeviceContext* a_context,
			const OutputResources& a_expected)
		{
			std::array<
				ID3D11RenderTargetView*,
				D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> targets{};
			ComPtr<ID3D11DepthStencilView> depth;
			a_context->OMGetRenderTargets(
				static_cast<UINT>(targets.size()),
				targets.data(),
				&depth);
			const auto release = [&] {
				for (auto* target : targets)
					if (target)
						target->Release();
			};
			const auto matches =
				targets[0] == a_expected.targets[0].Get() &&
				targets[1] == a_expected.targets[1].Get() &&
				depth.Get() == a_expected.depth.Get();
			auto trailingNull = true;
			for (size_t index = 2; index < targets.size(); ++index)
				trailingNull = trailingNull && targets[index] == nullptr;
			release();
			require(matches && trailingNull,
				"captured output targets or depth stencil were not restored");
		}

		template <class Interface, size_t Size>
		void ReleaseInterfaces(std::array<Interface*, Size>& a_interfaces)
		{
			for (auto*& value : a_interfaces)
			{
				if (value)
					value->Release();
				value = nullptr;
			}
		}
	}

	void run_presentation_blur_pipeline_checks(Runner& a_runner)
	{
		a_runner.test("background blur shader entries compile from shipped files", [] {
			constexpr std::array entries{
				ShaderEntry{
					L"BackgroundBlurDownsample.hlsl",
					"VS_Main",
					"vs_5_0" },
				ShaderEntry{
					L"BackgroundBlurDownsample.hlsl",
					"PS_Main",
					"ps_5_0" },
				ShaderEntry{
					L"BackgroundBlurGaussian.hlsl",
					"PS_Horizontal",
					"ps_5_0" },
				ShaderEntry{
					L"BackgroundBlurGaussian.hlsl",
					"PS_Vertical",
					"ps_5_0" },
				ShaderEntry{
					L"BackgroundBlurComposite.hlsl",
					"PS_Main",
					"ps_5_0" }
			};
			for (const auto& entry : entries)
				(void)CompileShader(entry);
		});

		a_runner.test("render target state restores all outputs and depth stencil", [] {
			auto device = support::CreateImageResources();
			const auto outputs = CreateOutputResources(device.device.Get());
			const std::array<ID3D11RenderTargetView*, 2> targets{
				outputs.targets[0].Get(),
				outputs.targets[1].Get()
			};
			device.context->OMSetRenderTargets(
				static_cast<UINT>(targets.size()),
				targets.data(),
				outputs.depth.Get());
			{
				const RenderTargetState state{ device.context.Get() };
				device.context->OMSetRenderTargets(0, nullptr, nullptr);
				state.Restore(device.context.Get());
			}
			RequireOutputTargets(device.context.Get(), outputs);
			device.context->OMSetRenderTargets(0, nullptr, nullptr);
		});

		a_runner.test("blur pipeline state restores every touched native binding", [] {
			auto device = support::CreateImageResources();
			const auto outputs = CreateOutputResources(device.device.Get());
			const std::array<ID3D11RenderTargetView*, 2> targets{
				outputs.targets[0].Get(),
				outputs.targets[1].Get()
			};

			D3D11_BLEND_DESC blendDescription{};
			blendDescription.RenderTarget[0].BlendEnable = TRUE;
			blendDescription.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			blendDescription.RenderTarget[0].DestBlend =
				D3D11_BLEND_INV_SRC_ALPHA;
			blendDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDescription.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blendDescription.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			blendDescription.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDescription.RenderTarget[0].RenderTargetWriteMask =
				D3D11_COLOR_WRITE_ENABLE_ALL;
			ComPtr<ID3D11BlendState> blend;
			require(SUCCEEDED(device.device->CreateBlendState(
						&blendDescription,
						&blend)),
				"blend state creation failed");
			const std::array<float, 4> blendFactor{
				0.25f, 0.5f, 0.75f, 1.0f
			};
			constexpr UINT sampleMask = UINT32_C(0xA55AA55A);

			D3D11_DEPTH_STENCIL_DESC depthDescription{};
			depthDescription.DepthEnable = TRUE;
			depthDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			depthDescription.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
			ComPtr<ID3D11DepthStencilState> depthState;
			require(SUCCEEDED(device.device->CreateDepthStencilState(
						&depthDescription,
						&depthState)),
				"depth state creation failed");
			constexpr UINT stencilReference = 23;

			D3D11_RASTERIZER_DESC rasterizerDescription{};
			rasterizerDescription.FillMode = D3D11_FILL_SOLID;
			rasterizerDescription.CullMode = D3D11_CULL_BACK;
			rasterizerDescription.FrontCounterClockwise = TRUE;
			rasterizerDescription.DepthClipEnable = TRUE;
			rasterizerDescription.ScissorEnable = TRUE;
			ComPtr<ID3D11RasterizerState> rasterizer;
			require(SUCCEEDED(device.device->CreateRasterizerState(
						&rasterizerDescription,
						&rasterizer)),
				"rasterizer state creation failed");

			const std::array viewports{
				D3D11_VIEWPORT{ 3.0f, 5.0f, 40.0f, 20.0f, 0.1f, 0.8f },
				D3D11_VIEWPORT{ 47.0f, 7.0f, 12.0f, 18.0f, 0.2f, 0.9f }
			};
			const std::array scissors{
				D3D11_RECT{ 3, 5, 43, 25 },
				D3D11_RECT{ 47, 7, 59, 25 }
			};

			const auto vertexBytecode = CompileShader({
				L"BackgroundBlurDownsample.hlsl",
				"VS_Main",
				"vs_5_0"
			});
			const auto pixelBytecode = CompileShader({
				L"BackgroundBlurGaussian.hlsl",
				"PS_Horizontal",
				"ps_5_0"
			});
			ComPtr<ID3D11VertexShader> vertexShader;
			ComPtr<ID3D11PixelShader> pixelShader;
			require(SUCCEEDED(device.device->CreateVertexShader(
						vertexBytecode->GetBufferPointer(),
						vertexBytecode->GetBufferSize(),
						nullptr,
						&vertexShader)) &&
					SUCCEEDED(device.device->CreatePixelShader(
						pixelBytecode->GetBufferPointer(),
						pixelBytecode->GetBufferSize(),
						nullptr,
						&pixelShader)),
				"test shader creation failed");
			const auto geometryBytecode = CompileShaderSource(
				"struct Vertex { float4 position : SV_POSITION; };"
				"[maxvertexcount(1)]"
				"void GS_Main(point Vertex input[1], "
				"inout PointStream<Vertex> output) {"
				"output.Append(input[0]);"
				"}",
				"GS_Main",
				"gs_5_0");
			ComPtr<ID3D11GeometryShader> geometryShader;
			require(SUCCEEDED(device.device->CreateGeometryShader(
						geometryBytecode->GetBufferPointer(),
						geometryBytecode->GetBufferSize(),
						nullptr,
						&geometryShader)),
				"test geometry shader creation failed");

			D3D11_SAMPLER_DESC samplerDescription{};
			samplerDescription.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			samplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			samplerDescription.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			samplerDescription.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			samplerDescription.MaxLOD = D3D11_FLOAT32_MAX;
			ComPtr<ID3D11SamplerState> sampler;
			require(SUCCEEDED(device.device->CreateSamplerState(
						&samplerDescription,
						&sampler)),
				"sampler state creation failed");

			const D3D11_BUFFER_DESC bufferDescription{
				32,
				D3D11_USAGE_DEFAULT,
				D3D11_BIND_CONSTANT_BUFFER,
				0,
				0,
				0
			};
			std::array<ComPtr<ID3D11Buffer>, 2> constantBuffers;
			for (auto& buffer : constantBuffers)
			{
				require(SUCCEEDED(device.device->CreateBuffer(
							&bufferDescription,
							nullptr,
							&buffer)),
					"constant buffer creation failed");
			}
			const std::array<ID3D11Buffer*, 2> constants{
				constantBuffers[0].Get(),
				constantBuffers[1].Get()
			};
			auto* samplerBinding = sampler.Get();
			auto* resourceBinding = device.view.Get();

			device.context->OMSetRenderTargets(
				static_cast<UINT>(targets.size()),
				targets.data(),
				outputs.depth.Get());
			device.context->OMSetBlendState(
				blend.Get(),
				blendFactor.data(),
				sampleMask);
			device.context->OMSetDepthStencilState(
				depthState.Get(),
				stencilReference);
			device.context->RSSetState(rasterizer.Get());
			device.context->RSSetViewports(
				static_cast<UINT>(viewports.size()),
				viewports.data());
			device.context->RSSetScissorRects(
				static_cast<UINT>(scissors.size()),
				scissors.data());
			device.context->IASetPrimitiveTopology(
				D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			device.context->VSSetShader(vertexShader.Get(), nullptr, 0);
			device.context->PSSetShader(pixelShader.Get(), nullptr, 0);
			device.context->GSSetShader(geometryShader.Get(), nullptr, 0);
			device.context->PSSetSamplers(0, 1, &samplerBinding);
			device.context->PSSetConstantBuffers(
				0,
				static_cast<UINT>(constants.size()),
				constants.data());
			device.context->PSSetShaderResources(0, 1, &resourceBinding);

			{
				const BlurPipelineState state{ device.context.Get() };
				device.context->OMSetRenderTargets(0, nullptr, nullptr);
				device.context->OMSetBlendState(nullptr, nullptr, ~UINT{});
				device.context->OMSetDepthStencilState(nullptr, 0);
				device.context->RSSetState(nullptr);
				const D3D11_VIEWPORT replacementViewport{
					0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
				};
				const D3D11_RECT replacementScissor{ 0, 0, 1, 1 };
				device.context->RSSetViewports(1, &replacementViewport);
				device.context->RSSetScissorRects(1, &replacementScissor);
				device.context->IASetPrimitiveTopology(
					D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
				device.context->VSSetShader(nullptr, nullptr, 0);
				device.context->PSSetShader(nullptr, nullptr, 0);
				device.context->GSSetShader(nullptr, nullptr, 0);
				ID3D11SamplerState* nullSampler{};
				const std::array<ID3D11Buffer*, 2> nullConstants{};
				ID3D11ShaderResourceView* nullResource{};
				device.context->PSSetSamplers(0, 1, &nullSampler);
				device.context->PSSetConstantBuffers(
					0,
					static_cast<UINT>(nullConstants.size()),
					nullConstants.data());
				device.context->PSSetShaderResources(0, 1, &nullResource);
			}

			RequireOutputTargets(device.context.Get(), outputs);

			ComPtr<ID3D11BlendState> actualBlend;
			std::array<float, 4> actualBlendFactor{};
			UINT actualSampleMask{};
			device.context->OMGetBlendState(
				&actualBlend,
				actualBlendFactor.data(),
				&actualSampleMask);
			require(
				actualBlend.Get() == blend.Get() &&
					actualBlendFactor == blendFactor &&
					actualSampleMask == sampleMask,
				"blur pipeline did not restore blend state");

			ComPtr<ID3D11DepthStencilState> actualDepth;
			UINT actualStencilReference{};
			device.context->OMGetDepthStencilState(
				&actualDepth,
				&actualStencilReference);
			require(
				actualDepth.Get() == depthState.Get() &&
					actualStencilReference == stencilReference,
				"blur pipeline did not restore depth state");

			ComPtr<ID3D11RasterizerState> actualRasterizer;
			device.context->RSGetState(&actualRasterizer);
			require(actualRasterizer.Get() == rasterizer.Get(),
				"blur pipeline did not restore rasterizer state");

			std::array<D3D11_VIEWPORT, 2> actualViewports{};
			auto viewportCount = static_cast<UINT>(actualViewports.size());
			device.context->RSGetViewports(
				&viewportCount,
				actualViewports.data());
			std::array<D3D11_RECT, 2> actualScissors{};
			auto scissorCount = static_cast<UINT>(actualScissors.size());
			device.context->RSGetScissorRects(
				&scissorCount,
				actualScissors.data());
			require(
				viewportCount == viewports.size() &&
					scissorCount == scissors.size() &&
					std::memcmp(
						actualViewports.data(),
						viewports.data(),
						sizeof(viewports)) == 0 &&
					std::memcmp(
						actualScissors.data(),
						scissors.data(),
						sizeof(scissors)) == 0,
				"blur pipeline did not restore viewport or scissor arrays");

			D3D11_PRIMITIVE_TOPOLOGY actualTopology{};
			device.context->IAGetPrimitiveTopology(&actualTopology);
			require(
				actualTopology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
				"blur pipeline did not restore primitive topology");

			ComPtr<ID3D11VertexShader> actualVertex;
			ComPtr<ID3D11PixelShader> actualPixel;
			ComPtr<ID3D11GeometryShader> actualGeometry;
			device.context->VSGetShader(&actualVertex, nullptr, nullptr);
			device.context->PSGetShader(&actualPixel, nullptr, nullptr);
			device.context->GSGetShader(&actualGeometry, nullptr, nullptr);
			require(
				actualVertex.Get() == vertexShader.Get() &&
					actualPixel.Get() == pixelShader.Get() &&
					actualGeometry.Get() == geometryShader.Get(),
				"blur pipeline did not restore touched shader stages");

			ComPtr<ID3D11SamplerState> actualSampler;
			device.context->PSGetSamplers(0, 1, &actualSampler);
			std::array<ID3D11Buffer*, 2> actualConstants{};
			device.context->PSGetConstantBuffers(
				0,
				static_cast<UINT>(actualConstants.size()),
				actualConstants.data());
			ComPtr<ID3D11ShaderResourceView> actualResource;
			device.context->PSGetShaderResources(0, 1, &actualResource);
			const auto constantBuffersMatch =
				actualConstants[0] == constantBuffers[0].Get() &&
				actualConstants[1] == constantBuffers[1].Get();
			ReleaseInterfaces(actualConstants);
			require(
				actualSampler.Get() == sampler.Get() &&
					constantBuffersMatch &&
					actualResource.Get() == device.view.Get(),
				"blur pipeline did not restore pixel shader resources");

			device.context->ClearState();
		});
	}
}
