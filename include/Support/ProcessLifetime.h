#pragma once

#include <type_traits>

namespace Addictol::Support
{
	// Permanent hooks outlive static teardown; resources must be retired explicitly.
	template <class T>
	union ProcessLifetime
	{
		T value;

		ProcessLifetime() noexcept(std::is_nothrow_default_constructible_v<T>) : value{} {}
		~ProcessLifetime() noexcept {}

		ProcessLifetime(const ProcessLifetime&) = delete;
		ProcessLifetime& operator=(const ProcessLifetime&) = delete;
	};
}
