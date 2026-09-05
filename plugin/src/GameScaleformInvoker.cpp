#include <DearModdingUI/MCM/GameScaleformInvoker.h>

#include <RE/H/HUDMenu.h>
#include <RE/I/IMenu.h>
#include <RE/P/PauseMenu.h>
#include <RE/U/UI.h>

#include <array>
#include <limits>
#include <type_traits>

namespace DearModdingUI::MCM
{
	using namespace std::literals;

	ScaleformInvocationStatus GameScaleformInvoker::InvokeUnlogged(
		std::string_view a_plugin,
		std::string_view a_function,
		const std::vector<ScaleformArgument>& a_arguments) noexcept
	{
		try
		{
			const std::string pluginName{ a_plugin };
			const std::string functionName{ a_function };
			auto* ui = RE::UI::GetSingleton();
			if (!ui)
				return ScaleformInvocationStatus::kNoMovieLoaded;
			auto suitableMovieLoaded = false;
			auto pluginRegistered = false;
			auto functionRegistered = false;
			const std::array menuNames{
				RE::HUDMenu::MENU_NAME,
				RE::PauseMenu::MENU_NAME
			};
			for (const auto menuName : menuNames)
			{
				auto menu = ui->GetMenu(RE::BSFixedString{ menuName });
				if (!menu || !menu->uiMovie)
					continue;
				Scaleform::GFx::Value plugins;
				if (!menu->uiMovie->GetVariable(
						&plugins,
						"root.f4se.plugins") ||
					!plugins.IsObject())
					continue;
				suitableMovieLoaded = true;
				Scaleform::GFx::Value plugin;
				if (!plugins.GetMember(pluginName, &plugin) || !plugin.IsObject())
					continue;
				pluginRegistered = true;
				if (!plugin.HasMember(functionName))
					continue;
				functionRegistered = true;
				std::vector<Scaleform::GFx::Value> arguments;
				arguments.reserve(a_arguments.size());
				for (const auto& argument : a_arguments)
				{
					std::visit(
						[&arguments](const auto& a_value) {
							using T = std::remove_cvref_t<decltype(a_value)>;
							if constexpr (std::same_as<T, int64_t>)
							{
								if (a_value >=
										(std::numeric_limits<int32_t>::min)() &&
									a_value <=
										(std::numeric_limits<int32_t>::max)())
									arguments.emplace_back(
										static_cast<int32_t>(a_value));
								else
									arguments.emplace_back(
										static_cast<double>(a_value));
							}
							else if constexpr (std::same_as<T, uint64_t>)
							{
								if (a_value <=
									(std::numeric_limits<uint32_t>::max)())
									arguments.emplace_back(
										static_cast<uint32_t>(a_value));
								else
									arguments.emplace_back(
										static_cast<double>(a_value));
							}
							else if constexpr (std::same_as<T, std::string>)
								arguments.emplace_back(a_value.c_str());
							else
								arguments.emplace_back(a_value);
						},
						argument);
				}
				Scaleform::GFx::Value result;
				if (plugin.Invoke(
						functionName.c_str(),
						&result,
						arguments.empty() ? nullptr : arguments.data(),
						arguments.size()))
					return ScaleformInvocationStatus::kSucceeded;
			}
			if (!suitableMovieLoaded)
				return ScaleformInvocationStatus::kNoMovieLoaded;
			if (!pluginRegistered)
				return ScaleformInvocationStatus::kPluginNotRegistered;
			if (!functionRegistered)
				return ScaleformInvocationStatus::kFunctionNotRegistered;
			return ScaleformInvocationStatus::kInvocationFailed;
		}
		catch (...)
		{
			return ScaleformInvocationStatus::kInvocationFailed;
		}
	}

	ScaleformInvocationStatus GameScaleformInvoker::Invoke(
		std::string_view a_plugin,
		std::string_view a_function,
		const std::vector<ScaleformArgument>& a_arguments) noexcept
	{
		const auto status = InvokeUnlogged(a_plugin, a_function, a_arguments);
		if (status == ScaleformInvocationStatus::kSucceeded)
		{
			REX::INFO(
				"[dmui.mcm.scaleform] DearModdingUI-MCM: invoked {}.{}"sv,
				a_plugin,
				a_function);
		}
		else
		{
			REX::WARN(
				"[dmui.mcm.scaleform] DearModdingUI-MCM: {}.{} was not invoked: {}"sv,
				a_plugin,
				a_function,
				DescribeScaleformInvocation(status));
		}
		return status;
	}
}
