#include "PresentationDemo.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <DearModdingUI/Client.h>

#include <array>
#include <optional>
#include <utility>

namespace DearModdingUIPreview
{
	using Microsoft::WRL::ComPtr;

	bool ParsePresentationDemo(
		std::string_view a_name,
		PresentationDemoKind& a_kind) noexcept
	{
		if (a_name == "overlay")
			a_kind = PresentationDemoKind::kOverlay;
		else if (a_name == "notification")
			a_kind = PresentationDemoKind::kNotification;
		else if (a_name == "image")
			a_kind = PresentationDemoKind::kImage;
		else if (a_name == "plot")
			a_kind = PresentationDemoKind::kPlot;
		else if (a_name == "dialog")
			a_kind = PresentationDemoKind::kDialog;
		else
			return false;
		return true;
	}

	struct PresentationDemo::Impl
	{
		explicit Impl(PresentationDemoKind a_kind) :
			kind(a_kind),
			client(
				"dearmodding.presentation.preview",
				"Presentation Services",
				{ 1, 0 },
				"presentation",
				{},
				{
					.requiredServices =
						DMUI_HOST_SERVICE_IMAGE_RESOURCES |
						DMUI_HOST_SERVICE_MANAGED_OVERLAYS |
						DMUI_HOST_SERVICE_NOTIFICATIONS |
						DMUI_HOST_SERVICE_ANNOTATED_PLOTS |
						DMUI_HOST_SERVICE_DIALOGS
				})
		{}

		[[nodiscard]] bool Register(std::string& a_error)
		{
			if (!client.Connect())
			{
				a_error = DMUI_ResultToString(client.LastResult());
				return false;
			}
			const auto pageKind = kind == PresentationDemoKind::kOverlay ?
				DMUI_PAGE_KIND_OVERLAY :
				DMUI_PAGE_KIND_SETTINGS;
			page = client.AddPage(
				{
					.id = "showcase",
					.displayName = kind == PresentationDemoKind::kOverlay ?
						"Performance overlay" :
						"Presentation showcase",
					.category = "Services",
					.summary = "Synthetic forwarding presentation state.",
					.kind = pageKind
				},
				[this] { Draw(); });
			if (!page)
			{
				a_error = DMUI_ResultToString(client.LastResult());
				return false;
			}
			return true;
		}

		[[nodiscard]] bool Activate(
			ID3D11Device* a_device,
			std::string& a_error)
		{
			if (!page)
			{
				a_error = "presentation page was not registered";
				return false;
			}
			if (kind == PresentationDemoKind::kOverlay)
			{
				const DMUI_ManagedOverlayOptions options{
					sizeof(DMUI_ManagedOverlayOptions),
					DMUI_OVERLAY_ANCHOR_TOP_RIGHT,
					{ 24.0f, 24.0f },
					{ 440.0f, 230.0f },
					{ 440.0f, 230.0f },
					0.82f,
					1.0f,
					1,
					1,
					1,
					0
				};
				if (!client.ConfigureOverlay(*page, options) ||
					!client.RequestFrame(*page))
				{
					a_error = DMUI_ResultToString(client.LastResult());
					return false;
				}
			}
			else if (kind == PresentationDemoKind::kNotification)
			{
				if (!client.PostNotification(
						DMUI_STATUS_SEVERITY_WARNING,
						"Shader cache rebuilt; one preset needs review.",
						30000))
				{
					a_error = DMUI_ResultToString(client.LastResult());
					return false;
				}
			}
			else if (kind == PresentationDemoKind::kImage)
			{
				if (!CreateImage(a_device))
				{
					a_error = "could not create the synthetic image";
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] bool CreateImage(ID3D11Device* a_device)
		{
			if (!a_device)
				return false;
			constexpr std::array<uint32_t, 16> pixels{
				0xFF25213Fu, 0xFFDB4E79u, 0xFF25213Fu, 0xFFDB4E79u,
				0xFFDB4E79u, 0xFF39C6B0u, 0xFFDB4E79u, 0xFF39C6B0u,
				0xFF25213Fu, 0xFFDB4E79u, 0xFF25213Fu, 0xFFDB4E79u,
				0xFFDB4E79u, 0xFF39C6B0u, 0xFFDB4E79u, 0xFF39C6B0u
			};
			const D3D11_TEXTURE2D_DESC description{
				4,
				4,
				1,
				1,
				DXGI_FORMAT_R8G8B8A8_UNORM,
				{ 1, 0 },
				D3D11_USAGE_IMMUTABLE,
				D3D11_BIND_SHADER_RESOURCE,
				0,
				0
			};
			const D3D11_SUBRESOURCE_DATA data{
				pixels.data(),
				4u * sizeof(uint32_t),
				0
			};
			return SUCCEEDED(a_device->CreateTexture2D(
					   &description, &data, &texture)) &&
				SUCCEEDED(a_device->CreateShaderResourceView(
					texture.Get(), nullptr, &view));
		}

		void Draw()
		{
			ImGui::TextUnformatted("Forwarding-only presentation services");
			ImGui::Separator();
			switch (kind)
			{
			case PresentationDemoKind::kOverlay:
				ImGui::TextUnformatted("Frame pacing");
				DrawPlot("##overlay.plot", { 400.0f, 92.0f });
				break;
			case PresentationDemoKind::kImage:
				ImGui::TextWrapped(
					"The synthetic SRV is retained by an owner-scoped image "
					"handle and drawn without CPU readback.");
				if (!image && view)
					image = client.ImportD3D11Image(view.Get(), 4, 4);
				if (image)
				{
					const DMUI_ImageDrawOptions options{
						sizeof(DMUI_ImageDrawOptions),
						{ 640.0f, 360.0f },
						{ 0.0f, 0.0f },
						{ 1.0f, 1.0f },
						{ 1.0f, 1.0f, 1.0f, 1.0f },
						1,
						0
					};
					(void)client.DrawImage(image->Handle(), options);
				}
				break;
			case PresentationDemoKind::kPlot:
				ImGui::TextWrapped(
					"Reference lines are clipped to the host-owned plot interior.");
				DrawPlot("Frame time", { 760.0f, 260.0f });
				break;
			case PresentationDemoKind::kDialog:
				ImGui::TextWrapped(
					"The dialog keeps entered text while a rejected submission "
					"is corrected.");
				if (!dialog)
				{
					const DMUI_DialogDescriptor descriptor{
						sizeof(DMUI_DialogDescriptor),
						DMUI_DIALOG_KIND_TEXT_ENTRY,
						"Save preset",
						"Choose a unique preset name.",
						"Save",
						"Cancel",
						"Preset name",
						"Ultra Commonwealth",
						65
					};
					dialog = client.RequestDialog(descriptor);
				}
				break;
			default:
				break;
			}
		}

		void DrawPlot(const char* a_id, DMUI_Vec2 a_size)
		{
			static constexpr std::array samples{
				8.4f, 8.1f, 8.7f, 9.2f, 8.8f, 16.3f, 9.0f, 8.5f,
				8.2f, 8.0f, 8.6f, 9.1f, 8.7f, 8.4f, 8.3f, 8.1f
			};
			static constexpr std::array lines{
				DMUI_PlotReferenceLine{
					1000.0f / 120.0f,
					{ 0.25f, 0.78f, 0.65f, 0.9f } },
				DMUI_PlotReferenceLine{
					1000.0f / 60.0f,
					{ 0.95f, 0.72f, 0.22f, 0.9f } },
				DMUI_PlotReferenceLine{
					1000.0f / 30.0f,
					{ 0.90f, 0.28f, 0.25f, 0.9f } }
			};
			const DMUI_AnnotatedPlotDescriptor descriptor{
				sizeof(DMUI_AnnotatedPlotDescriptor),
				samples.data(),
				static_cast<uint32_t>(samples.size()),
				0,
				0.0f,
				36.0f,
				a_size,
				"8.6 ms average",
				lines.data(),
				static_cast<uint32_t>(lines.size())
			};
			(void)client.DrawAnnotatedPlot(a_id, descriptor);
		}

		PresentationDemoKind kind;
		dmui::Client client;
		std::optional<DMUI_PageHandle> page;
		ComPtr<ID3D11Texture2D> texture;
		ComPtr<ID3D11ShaderResourceView> view;
		std::optional<dmui::ImageResource> image;
		std::optional<DMUI_DialogHandle> dialog;
	};

	PresentationDemo::PresentationDemo(PresentationDemoKind a_kind) :
		m_impl(std::make_unique<Impl>(a_kind))
	{}

	PresentationDemo::~PresentationDemo() = default;

	bool PresentationDemo::Register(std::string& a_error) noexcept
	{
		try
		{
			return m_impl->Register(a_error);
		}
		catch (...)
		{
			a_error = "presentation registration threw";
			return false;
		}
	}

	bool PresentationDemo::Activate(
		ID3D11Device* a_device,
		std::string& a_error) noexcept
	{
		try
		{
			return m_impl->Activate(a_device, a_error);
		}
		catch (...)
		{
			a_error = "presentation activation threw";
			return false;
		}
	}

	bool PresentationDemo::UsesMenu() const noexcept
	{
		return m_impl->kind != PresentationDemoKind::kOverlay &&
			m_impl->kind != PresentationDemoKind::kNotification;
	}

	uint64_t PresentationDemo::Page() const noexcept
	{
		return m_impl->page.value_or(DMUI_INVALID_PAGE_HANDLE);
	}
}
