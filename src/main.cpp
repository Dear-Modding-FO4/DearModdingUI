#include <DearModdingUI/CarrierMenu.h>
#include <DearModdingUI/CursorLoader.h>
#include <DearModdingUI/Host.h>
#include <DearModdingUI/HostSettings.h>
#include <DearModdingUI/Shell.h>
#include <DearModdingUI/Theme.h>
#include <Platform/PlatformImgui.h>

#include <F4SE/F4SE.h>
#include <REX/REX.h>

#include <mutex>

namespace Addictol
{
	using namespace std::literals;

	namespace
	{
		void SetupHost(void* a_window) noexcept
		{
			DearModdingUI::Theme::Initialize(a_window);
			DearModdingUI::CursorLoader::Initialize(a_window);
			REX::INFO("DearModdingUI: visuals configured"sv);
		}

		void DrawHost() noexcept
		{
			DearModdingUI::DrawDemandedOverlays();
			if (DearModdingUI::IsMenuVisible())
				DearModdingUI::DrawShell();
		}

		[[nodiscard]] bool RegisterHostSinks() noexcept
		{
			if (PlatformImgui::RegisterSetupSink("DearModdingUI"sv, &SetupHost) &&
				PlatformImgui::RegisterDrawSink("DearModdingUI"sv, &DrawHost))
				return true;

			REX::ERROR("DearModdingUI: the ImGui platform refused the host sinks"sv);
			DearModdingUI::DeferBackendUnavailable(
				DMUI_UNAVAILABLE_BACKEND_FAILED);
			return false;
		}

		void MessageListener(F4SE::MessagingInterface::Message* a_message) noexcept
		{
			if (!a_message)
				return;

			switch (a_message->type)
			{
			case F4SE::MessagingInterface::kPreLoadGame:
			case F4SE::MessagingInterface::kNewGame:
			case F4SE::MessagingInterface::kGameLoaded:
			case F4SE::MessagingInterface::kGameDataReady:
				PlatformImgui::HandleGameTransition();
				break;
			default:
				break;
			}

			if (a_message->type != F4SE::MessagingInterface::kGameLoaded)
				return;

			if (!DearModdingUI::CarrierMenu::Register())
				REX::ERROR("DearModdingUI: carrier menu registration failed"sv);
			if (!PlatformImgui::InitializeWindow())
				REX::ERROR("DearModdingUI: render-window initialization failed"sv);
		}

		[[nodiscard]] bool InitializePlugin(
			const F4SE::LoadInterface* a_f4se) noexcept
		{
			static std::once_flag once;
			static bool initialized{ false };
			std::call_once(once, [&]() noexcept {
				F4SE::Init(a_f4se);
				REX::INFO("DearModdingUI initializing"sv);

				DearModdingUI::HostSettings::Initialize();
				DearModdingUI::Initialize();

				auto* messaging = F4SE::GetMessagingInterface();
				if (!messaging || !messaging->RegisterListener(MessageListener))
				{
					REX::ERROR("DearModdingUI: F4SE message listener registration failed"sv);
					return;
				}

				const auto hooksInstalled = PlatformImgui::InstallHooks();
				const auto sinksRegistered = RegisterHostSinks();
				if (!hooksInstalled)
					REX::ERROR("DearModdingUI: D3D11 hooks could not be installed"sv);

				initialized = hooksInstalled && sinksRegistered;
				if (initialized)
					REX::INFO("DearModdingUI initialized"sv);
			});
			return initialized;
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
	return Addictol::InitializePlugin(a_f4se);
}
