#include <F4SE/F4SE.h>
#include <RE/B/BSGraphics.h>
#include <REX/REX.h>

#include <GeneralTestSuite.h>
#include <d3d11.h>

#undef ERROR

#include <atomic>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string_view>

namespace DmuiTests
{
	using Microsoft::WRL::ComPtr;
	using namespace std::literals;

	namespace
	{
		class RendererDataLock final
		{
		public:
			explicit RendererDataLock(
				RE::BSGraphics::RendererData& a_data) noexcept :
				m_lock(std::addressof(a_data.rendererLock.criticalSection))
			{
				REX::W32::EnterCriticalSection(m_lock);
			}

			~RendererDataLock()
			{
				REX::W32::LeaveCriticalSection(m_lock);
			}

		private:
			REX::W32::CRITICAL_SECTION* m_lock;
		};

		class GameEnvironment final : public Environment
		{
		public:
			[[nodiscard]] ComPtr<ID3D11Device>
				AcquireRendererDevice() noexcept override
			{
				auto* rendererData = RE::BSGraphics::GetRendererData();
				if (!rendererData)
					return {};

				const RendererDataLock lock{ *rendererData };
				if (RE::BSGraphics::GetRendererData() != rendererData ||
					!rendererData->initialized ||
					!rendererData->device)
					return {};

				auto* device =
					reinterpret_cast<ID3D11Device*>(rendererData->device);
				device->AddRef();
				ComPtr<ID3D11Device> result;
				result.Attach(device);
				return result;
			}

			[[nodiscard]] bool SupportsGameInputContexts() const noexcept override
			{
				return true;
			}

			void Log(LogLevel a_level, std::string_view a_message) noexcept override
			{
				switch (a_level)
				{
				case LogLevel::kWarning:
					REX::WARN("{}", a_message);
					break;
				case LogLevel::kError:
					REX::ERROR("{}", a_message);
					break;
				default:
					REX::INFO("{}", a_message);
					break;
				}
			}
		};

		struct Application
		{
			GameEnvironment environment;
			GeneralTestSuite suite{ environment };
		};

		[[nodiscard]] Application& GetApplication()
		{
			static auto* application = new Application;
			static const auto teardownRegistered = std::atexit([]() noexcept {
				application->suite.Stop();
			});
			(void)teardownRegistered;
			return *application;
		}

		std::once_flag s_initializationOnce;
		std::atomic_bool s_initialized{};

		void MessageListener(
			F4SE::MessagingInterface::Message* a_message) noexcept
		{
			if (!a_message ||
				a_message->type != F4SE::MessagingInterface::kPostPostLoad)
				return;

			std::call_once(s_initializationOnce, []() noexcept {
				const auto initialized = GetApplication().suite.Initialize();
				s_initialized.store(initialized, std::memory_order_release);
				if (!initialized)
					REX::ERROR(
						"dmui-test-client: initialization failed at kPostPostLoad"sv);
			});
		}

		[[nodiscard]] bool Load(
			const F4SE::LoadInterface* a_f4se) noexcept
		{
			if (!a_f4se)
				return false;

			F4SE::Init(a_f4se);
			const auto* messaging = F4SE::GetMessagingInterface();
			if (!messaging)
			{
				REX::ERROR(
					"dmui-test-client: F4SE messaging interface unavailable"sv);
				return false;
			}
			if (!messaging->RegisterListener(MessageListener))
			{
				REX::ERROR(
					"dmui-test-client: F4SE message listener registration failed"sv);
				return false;
			}
			return true;
		}
	}
}

F4SE_PLUGIN_QUERY(
	const F4SE::QueryInterface* a_f4se,
	F4SE::PluginInfo* a_info)
{
	if (!a_f4se || !a_info ||
		a_f4se->RuntimeVersion() < REL::Version(F4SE::RUNTIME_1_10_163))
		return false;

	if (const auto* data = F4SE::PluginVersionData::GetSingleton())
	{
		a_info->infoVersion = F4SE::PluginInfo::kVersion;
		a_info->name = data->GetPluginName().data();
		a_info->version = data->GetPluginVersion().pack();
	}
	return true;
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	return DmuiTests::Load(a_f4se);
}
