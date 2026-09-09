#pragma once

namespace REX
{
	template <class... Args>
	void INFO(Args&&...) noexcept
	{}

	template <class... Args>
	void WARN(Args&&...) noexcept
	{}

	template <class... Args>
	void ERROR(Args&&...) noexcept
	{}
}
