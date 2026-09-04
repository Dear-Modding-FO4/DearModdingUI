#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

#include <DearModdingUI/Client.h>

#include <functional>
#include <span>
#include <string_view>
#include <vector>

namespace DearModdingUI::MCM
{
	using PapyrusDispatchCompletion =
		std::function<void(bool, std::optional<dmui::SettingValue>)>;

	struct PapyrusArgument
	{
		dmui::SettingValue value;
		SourceValueKind kind{ SourceValueKind::kNone };
	};

	class PapyrusDispatcher
	{
	public:
		PapyrusDispatcher() = default;
		virtual ~PapyrusDispatcher() = default;

		PapyrusDispatcher(const PapyrusDispatcher&) = delete;
		PapyrusDispatcher(PapyrusDispatcher&&) = delete;
		PapyrusDispatcher& operator=(const PapyrusDispatcher&) = delete;
		PapyrusDispatcher& operator=(PapyrusDispatcher&&) = delete;

		[[nodiscard]] virtual bool DispatchStatic(
			std::string_view a_script,
			std::string_view a_function,
			std::span<const PapyrusArgument> a_arguments,
			const std::optional<dmui::SettingValue>& a_resultTarget,
			PapyrusDispatchCompletion a_completion) = 0;
	};
}
