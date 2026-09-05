#pragma once

#include <DearModdingUI/MCM/PapyrusDispatcher.h>

namespace DearModdingUI::MCM
{
	class GamePapyrusDispatcher final : public PapyrusDispatcher
	{
	public:
		[[nodiscard]] bool DispatchStatic(
			std::string_view a_script,
			std::string_view a_function,
			std::span<const PapyrusArgument> a_arguments,
			const std::optional<dmui::SettingValue>& a_resultTarget,
			PapyrusDispatchCompletion a_completion) override;
	};
}
