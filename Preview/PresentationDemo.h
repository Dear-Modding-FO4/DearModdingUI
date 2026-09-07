#pragma once

#include <memory>
#include <string>
#include <string_view>

struct ID3D11Device;

namespace DearModdingUIPreview
{
	enum class PresentationDemoKind
	{
		kOverlay,
		kNotification,
		kImage,
		kPlot,
		kDialog
	};

	[[nodiscard]] bool ParsePresentationDemo(
		std::string_view a_name,
		PresentationDemoKind& a_kind) noexcept;

	class PresentationDemo final
	{
	public:
		explicit PresentationDemo(PresentationDemoKind a_kind);
		~PresentationDemo();

		PresentationDemo(const PresentationDemo&) = delete;
		PresentationDemo(PresentationDemo&&) = delete;
		PresentationDemo& operator=(const PresentationDemo&) = delete;
		PresentationDemo& operator=(PresentationDemo&&) = delete;

		[[nodiscard]] bool Register(std::string& a_error) noexcept;
		[[nodiscard]] bool Activate(
			ID3D11Device* a_device,
			std::string& a_error) noexcept;
		[[nodiscard]] bool UsesMenu() const noexcept;
		[[nodiscard]] uint64_t Page() const noexcept;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
