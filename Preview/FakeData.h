#pragma once

#include <memory>
#include <string>

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

		[[nodiscard]] bool Register(std::string& a_error) noexcept;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
