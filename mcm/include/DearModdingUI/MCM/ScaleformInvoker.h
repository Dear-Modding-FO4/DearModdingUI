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

	[[nodiscard]] constexpr std::string_view DescribeScaleformInvocation(
		ScaleformInvocationStatus a_status) noexcept
	{
		switch (a_status)
		{
		case ScaleformInvocationStatus::kSucceeded:
			return "succeeded";
		case ScaleformInvocationStatus::kNoMovieLoaded:
			return "no loaded UI movie exposes F4SE plugins";
		case ScaleformInvocationStatus::kPluginNotRegistered:
			return "the plugin is not registered in a loaded UI movie";
		case ScaleformInvocationStatus::kFunctionNotRegistered:
			return "the plugin registered no such function";
		case ScaleformInvocationStatus::kInvocationFailed:
			return "the movie rejected the call";
		}
		return "the call failed for an unrecognized reason";
	}

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
