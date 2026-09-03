#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace DearModdingUI::MCM
{
	using ScaleformArgument =
		std::variant<bool, int64_t, uint64_t, double, std::string>;

	enum class ScaleformInvocationStatus : uint8_t
	{
		kSucceeded,
		kNoMovieLoaded,
		kPluginNotRegistered,
		kFunctionNotRegistered,
		kInvocationFailed
	};

	class ScaleformInvoker
	{
	public:
		ScaleformInvoker() = default;
		virtual ~ScaleformInvoker() = default;

		ScaleformInvoker(const ScaleformInvoker&) = delete;
		ScaleformInvoker(ScaleformInvoker&&) = delete;
		ScaleformInvoker& operator=(const ScaleformInvoker&) = delete;
		ScaleformInvoker& operator=(ScaleformInvoker&&) = delete;

		[[nodiscard]] virtual ScaleformInvocationStatus Invoke(
			std::string_view a_plugin,
			std::string_view a_function,
			const std::vector<ScaleformArgument>& a_arguments) noexcept = 0;
	};
}
