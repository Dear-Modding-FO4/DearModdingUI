#include <Platform/menus/HostMenu.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/ControllerNavigation.h>
#include <DearModdingUI/host/ModalCoordinator.h>
#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/host/MenuToggleChord.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/host/Shell.h>
#include <DearModdingUI/presentation/Theme.h>
#include <Platform/input/EngineFocus.h>
#include <Platform/input/GameInput.h>
#include <Platform/menus/StartMenuEntry.h>
#include <Platform/input/ToggleOutcome.h>
#include <Platform/rendering/PlatformImGui.h>

#include <F4SE/F4SE.h>
#include <RE/B/BSInputDeviceManager.h>
#include <RE/M/Main.h>
#include <REX/REX.h>

#include <Windows.h>

#include <atomic>
#include <mutex>

#undef ERROR

namespace DearModdingUI::Plugin
{
	using namespace std::literals;

	namespace
	{
		ToggleOutcome::ChangeFilter s_toggleOutcomes;

		EngineFocus::Snapshot ReadEngineFocus() noexcept
		{
			const auto* main = RE::Main::GetSingleton();
			const auto* devices = RE::BSInputDeviceManager::GetSingleton();
			if (!main || !devices)
				return {};
			// The window thread pumps messages; threadID is the fallback.
			const auto window = reinterpret_cast<HWND>(main->hwnd);
			const auto windowThread = window ? GetWindowThreadProcessId(window, nullptr) : 0;
			return { devices->pollingEnabled, windowThread ? windowThread : main->threadID };
		}

		void SetupHost(void* a_window) noexcept
		{
			DearModdingUI::PresentationServices::RegisterOverlaySettings();
			DearModdingUI::Theme::Initialize(a_window);
			REX::INFO("DearModdingUI: visuals configured"sv);
		}

		void DrawHost() noexcept
		{
			DearModdingUI::ModalCoordinator::BeginFrame();
			DearModdingUI::DrawDemandedOverlays();
			if (DearModdingUI::IsMenuVisible())
			{
				DearModdingUI::DrawShell();
			}
			DearModdingUI::PresentationServices::DrawDialog(DearModdingUI::IsMenuVisible());
			DearModdingUI::PresentationServices::DrawNotifications(
				DearModdingUI::IsMenuVisible() &&
				!DearModdingUI::ControllerNavigation::IsNavigating());
			if (DearModdingUI::CurrentInputMode() != DearModdingUI::HostInputMode::kGameplay)
				DearModdingUI::ApplyMenuEscapeDismissal();
			DearModdingUI::ModalCoordinator::FinishFrame();
		}

		void ToggleHost() noexcept
		{
			const auto readiness = PlatformImGui::CurrentReadiness();
			const auto open = DearModdingUI::DecideMenuToggle(
				DearModdingUI::IsMenuVisible(),
				readiness == PlatformImGui::Readiness::kReady);
			const auto result = DearModdingUI::SetMenuVisible(open);
			PlatformImGui::SetDrawingEnabled(
				result == DMUI_RESULT_OK && open);

			const auto outcome = ToggleOutcome::Classify({
				.inputHooks = GameInput::HooksInstalled(),
				.engineActive = EngineFocus::Active(),
				.renderer = readiness,
				.hostAccepted = result == DMUI_RESULT_OK });
			if (!s_toggleOutcomes.Changed(outcome))
				return;
			if (outcome == ToggleOutcome::Outcome::kToggled)
				REX::INFO("DearModdingUI: menu toggle {}"sv, open ? "opened"sv : "closed"sv);
			else
				REX::WARN(
					"DearModdingUI: menu toggle dropped: {} (host result {})"sv,
					ToggleOutcome::Name(outcome),
					static_cast<int>(result));
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
				PlatformImGui::HandleGameTransition();
				break;
			case F4SE::MessagingInterface::kPostPostLoad:
				// The title menu shows its rows while game data still loads.
				(void)DearModdingUI::StartMenuEntry::Register(&ToggleHost);
				// Game data loads on a worker that exits; this is the main thread.
				(void)PlatformImGui::InstallThreadTap();
				break;
			case F4SE::MessagingInterface::kGameDataReady:
				PlatformImGui::HandleGameTransition();
				if (a_message->data)
				{
					(void)GameInput::InstallHooks();
					if (!DearModdingUI::StartMenuEntry::Register(&ToggleHost))
						REX::ERROR("DearModdingUI: menu entry registration failed"sv);
					(void)PlatformImGui::VerifyThreadTap();
					PlatformImGui::LogEngineFocus("game data ready");
				}
				break;
			default:
				break;
			}

			if (a_message->type != F4SE::MessagingInterface::kGameLoaded)
				return;

			if (!DearModdingUI::HostMenu::Register())
				REX::ERROR("DearModdingUI: host menu registration failed"sv);
			if (!PlatformImGui::InitializeWindow())
				REX::ERROR("DearModdingUI: render-window initialization failed"sv);
		}

		[[nodiscard]] bool InitializePlugin(
			const F4SE::LoadInterface* a_f4se) noexcept
		{
			static std::once_flag once;
			static bool initialized{ false };
			std::call_once(once, [&]() noexcept {
				F4SE::Init(a_f4se);
				EngineFocus::SetSource(&ReadEngineFocus);
				REX::INFO("DearModdingUI initializing"sv);

				DearModdingUI::HostSettings::Initialize();
				DearModdingUI::Initialize();
				GameInput::InitializeHealth();

				auto* messaging = F4SE::GetMessagingInterface();
				if (!messaging || !messaging->RegisterListener(MessageListener))
				{
					REX::ERROR("DearModdingUI: F4SE message listener registration failed"sv);
					return;
				}

				const auto hooksInstalled = PlatformImGui::InstallHooks({
					&SetupHost,
					&DrawHost,
					&ToggleHost
				});
				if (!hooksInstalled)
					REX::ERROR("DearModdingUI: renderer reconciliation could not be installed"sv);

				initialized = hooksInstalled;
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
	return DearModdingUI::Plugin::InitializePlugin(a_f4se);
}
