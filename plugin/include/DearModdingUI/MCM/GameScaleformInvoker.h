#pragma once

#include <DearModdingUI/MCM/ScaleformInvoker.h>

namespace DearModdingUI::MCM
{
	class GameScaleformInvoker final : public ScaleformInvoker
	{
	public:
		[[nodiscard]] ScaleformInvocationStatus Invoke(
			std::string_view a_plugin,
			std::string_view a_function,
			const std::vector<ScaleformArgument>& a_arguments) noexcept override;

	private:
		[[nodiscard]] static ScaleformInvocationStatus InvokeUnlogged(
			std::string_view a_plugin,
			std::string_view a_function,
			const std::vector<ScaleformArgument>& a_arguments) noexcept;
	};
}
