#include "MCMTestEndpoint.h"

#include <F4SE/F4SE.h>
#include <REX/REX.h>
#include <Scaleform/G/GFx_FunctionHandler.h>
#include <Scaleform/G/GFx_Movie.h>
#include <Scaleform/G/GFx_Value.h>
#include <Scaleform/P/Ptr.h>

#include <atomic>
#include <cmath>
#include <exception>
#include <string_view>

namespace DmuiTests
{
	namespace
	{
		std::atomic_uint64_t s_invocations{};

		class RecordCallHandler final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				if (!a_params.retVal || !a_params.args || a_params.argCount != 2 ||
					!a_params.args[0].IsString())
				{
					REX::ERROR("DMUITests.RecordCall requires a probe name and a number");
					return;
				}
				*a_params.retVal = false;
				const std::string_view source{ a_params.args[0].GetString() };
				if (source != "button" && source != "global-float")
				{
					REX::ERROR("DMUITests.RecordCall received an unknown probe name");
					return;
				}
				const auto& argument = a_params.args[1];
				double value{};
				if (argument.IsNumber())
					value = argument.GetNumber();
				else if (argument.IsInt())
					value = argument.GetInt();
				else if (argument.IsUInt())
					value = argument.GetUInt();
				else
				{
					REX::ERROR("DMUITests.RecordCall received a nonnumeric value");
					return;
				}
				if (!std::isfinite(value))
				{
					REX::ERROR("DMUITests.RecordCall received a nonfinite value");
					return;
				}
				const auto count =
					s_invocations.fetch_add(1, std::memory_order_relaxed) + 1;
				REX::INFO(
					"DMUITests.RecordCall: source={} value={} invocation={}",
					source, value, count);
				*a_params.retVal = static_cast<double>(count);
			}
		};

		bool RegisterMovie(
			Scaleform::GFx::Movie* a_movie,
			Scaleform::GFx::Value* a_plugin)
		{
			if (!a_movie || !a_plugin || !a_plugin->IsObject())
			{
				REX::ERROR("DMUITests Scaleform registration received no movie or plugin object");
				return false;
			}
			try
			{
				const auto handler = Scaleform::make_shared<RecordCallHandler>();
				Scaleform::GFx::Value function;
				a_movie->CreateFunction(&function, handler.get());
				if (a_plugin->SetMember("RecordCall", function))
					return true;
				REX::ERROR("DMUITests could not publish its Scaleform probe");
			}
			catch (const std::exception& a_error)
			{
				REX::ERROR("DMUITests Scaleform registration failed: {}", a_error.what());
			}
			return false;
		}
	}

	bool RegisterMCMTestEndpoint()
	{
		const auto* scaleform = F4SE::GetScaleformInterface();
		if (scaleform && scaleform->Register("DMUITests", RegisterMovie))
			return true;
		REX::ERROR("dmui-test-client: MCM Scaleform probe registration failed");
		return false;
	}
}
