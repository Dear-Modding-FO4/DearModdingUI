#include <Platform/menus/HostMenu.h>

#include <DearModdingUI/settings/HostSettings.h>
#include <Platform/menus/HiddenMenu.h>

#include <F4SE/API.h>
#include <F4SE/Interfaces.h>
#include <RE/U/UI.h>
#include <RE/U/UIMessageQueue.h>
#include <Support/OnceFlag.h>
#include <REX/REX.h>

#include <atomic>
#include <mutex>
#include <string_view>

namespace DearModdingUI::HostMenu
{
	namespace
	{
		using namespace std::literals;

		inline constexpr auto kMenuName = "DearModdingUI_Menu";

		void UpdatePauseSetting(RE::IMenu& a_menu) noexcept
		{
			a_menu.UpdateFlag(
				RE::UI_MENU_FLAGS::kPausesGame,
				!HostSettings::Current().fallSoulsMode);
		}

		// Carries the game cursor, input context, and pause while DMUI is open.
		class HostGameMenu final :
			public HiddenMenu::Menu
		{
		public:
			static constexpr std::string_view kName{ kMenuName };

			HostGameMenu() :
				HiddenMenu::Menu(kName)
			{
				// Explicitly return the shared game cursor from custom menu rendering to screen space.
				customRendererName = "FlatScreenModel";
				depthPriority = RE::UI_DEPTH_PRIORITY::kConsole;
				menuFlags.set(
					RE::UI_MENU_FLAGS::kUsesCursor,
					RE::UI_MENU_FLAGS::kAssignCursorToRenderer,
					RE::UI_MENU_FLAGS::kModal,
					RE::UI_MENU_FLAGS::kAdvancesUnderPauseMenu,
					RE::UI_MENU_FLAGS::kRendersUnderPauseMenu);
				UpdatePauseSetting(*this);
				inputContext = RE::UserEvents::INPUT_CONTEXT_ID::kCursor;
			}
		};

		std::atomic<bool> s_registrationAttempted{ false };
		std::atomic<bool> s_registered{ false };
		Support::OnceFlag s_taskFailureLogged;
		Support::OnceFlag s_queueFailureLogged;
		std::mutex s_stateLock;
		State s_state{};

		[[nodiscard]] bool Dispatch(Action a_action) noexcept
		{
			if (a_action == Action::kNone)
				return true;
			auto* tasks = F4SE::GetTaskInterface();
			if (!tasks)
			{
				if (s_taskFailureLogged.First())
					REX::ERROR("DearModdingUI: host menu UI task interface is unavailable"sv);
				return false;
			}

			const auto message = a_action == Action::kShow ?
				RE::UI_MESSAGE_TYPE::kShow :
				RE::UI_MESSAGE_TYPE::kHide;
			tasks->AddUITask([message] {
				const RE::BSFixedString name{ kMenuName };
				if (message == RE::UI_MESSAGE_TYPE::kShow)
				{
					auto* ui = RE::UI::GetSingleton();
					auto menu = ui ? ui->GetMenu(name) : nullptr;
					if (menu && !menu->OnStack())
						UpdatePauseSetting(*menu);
				}
				auto* queue = RE::UIMessageQueue::GetSingleton();
				if (!queue)
				{
					if (s_queueFailureLogged.First())
						REX::ERROR("DearModdingUI: host menu message queue is unavailable"sv);
					return;
				}
				queue->AddMessage(name, message);
			});
			return true;
		}
	}

	bool Register() noexcept
	{
		bool expected{ false };
		if (!s_registrationAttempted.compare_exchange_strong(
				expected, true, std::memory_order_acq_rel))
			return s_registered.load(std::memory_order_acquire);

		const auto registered = HiddenMenu::Register(kMenuName, &HiddenMenu::Create<HostGameMenu>);
		s_registered.store(registered, std::memory_order_release);
		return registered;
	}

	void Handle(Event a_event) noexcept
	{
		if (!s_registered.load(std::memory_order_acquire))
			return;

		const std::lock_guard lock{ s_stateLock };
		const auto previous = s_state;
		const auto action = Transition(s_state, a_event);
		if (!Dispatch(action))
			s_state = previous;
	}
}
