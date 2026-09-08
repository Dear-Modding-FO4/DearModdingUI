#pragma once

#include <memory>
#include <string>

struct ID3D11Device;

namespace DearModdingUIPreview
{
	class FakeData final
	{
	public:
		FakeData();
		~FakeData();

		FakeData(const FakeData&) = delete;
		FakeData(FakeData&&) = delete;
		FakeData& operator=(const FakeData&) = delete;
		FakeData& operator=(FakeData&&) = delete;

		[[nodiscard]] bool Register(
			ID3D11Device* a_device,
			std::string& a_error,
			bool a_includeNavigationComparisonFixtures = false) noexcept;
		void Stop() noexcept;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
