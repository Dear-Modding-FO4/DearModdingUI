#include <Platform/menus/FrameMenu.h>

#include <F4SE/API.h>
#include <F4SE/Interfaces.h>
#include <RE/M/MENU_RENDER_CONTEXT.h>
#include <RE/M/Main.h>
#include <RE/U/UIMessageQueue.h>
#include <REX/REX.h>

#include <Platform/menus/HiddenMenu.h>
#include <Platform/rendering/PlatformImGui.h>

#include <string_view>

namespace DearModdingUI::FrameMenu
{
	namespace
	{
		using namespace std::literals;

		inline constexpr auto kMenuName = "DearModdingUI_Frame";

		void RequestOpen() noexcept
		{
			auto* queue = RE::UIMessageQueue::GetSingleton();
			if (!queue)
			{
				REX::ERROR("DearModdingUI: frame menu cannot open because the UI message queue is unavailable"sv);
				return;
			}
			const RE::BSFixedString name{ kMenuName };
			queue->AddMessage(name, RE::UI_MESSAGE_TYPE::kShow);
		}

		// ScreenSpace_RenderMenus asks each menu to draw; this one draws the host.
		class HostFrameMenu final :
			public HiddenMenu::Menu
		{
		public:
			static constexpr std::string_view kName{ kMenuName };

			HostFrameMenu() :
				HiddenMenu::Menu(kName)
			{
				// Below the cursor, so the game cursor draws after the host frame.
				depthPriority = RE::UI_DEPTH_PRIORITY::kConsole;
				// Missing kAllowSaving or kCompanionAppAllowed blocks saving.
				menuFlags.set(
					RE::UI_MENU_FLAGS::kAlwaysOpen,
					// Else UI::RefreshCursor hides the cursor under us.
					RE::UI_MENU_FLAGS::kDontHideCursorWhenTopmost,
					RE::UI_MENU_FLAGS::kAllowSaving,
					RE::UI_MENU_FLAGS::kCompanionAppAllowed,
					RE::UI_MENU_FLAGS::kAdvancesUnderPauseMenu,
					RE::UI_MENU_FLAGS::kRendersUnderPauseMenu);
				// Input comes from the engine queue hooks, never from the menu stack.
				inputEventHandlingEnabled = false;
			}

			bool PassesRenderConditionText(
				RE::MENU_RENDER_CONTEXT a_reason,
				const RE::BSFixedString& a_customRendererName) const override
			{
				if (a_reason != RE::MENU_RENDER_CONTEXT::kRenderScreenspace)
					return RE::IMenu::PassesRenderConditionText(a_reason, a_customRendererName);
				PlatformImGui::DrawUiPass(PlatformImGui::UiPassSite::kFrameMenu);
				return false;
			}

			void OnRemovedFromMenuStack() override
			{
				RE::IMenu::OnRemovedFromMenuStack();
				const auto* main = RE::Main::GetSingleton();
				if (main && main->quitGame)
					return;
				// Loading a save closes it despite kAlwaysOpen.
				REX::INFO("DearModdingUI: the game closed the frame menu; reopening it"sv);
				PlatformImGui::HandleFrameMenuClosed();
				RequestOpen();
			}
		};
	}

	bool Register() noexcept
	{
		auto* tasks = F4SE::GetTaskInterface();
		if (!tasks)
		{
			REX::ERROR("DearModdingUI: frame menu registration failed because the F4SE task interface is unavailable"sv);
			return false;
		}
		if (!HiddenMenu::Register(kMenuName, &HiddenMenu::Create<HostFrameMenu>))
			return false;
		tasks->AddUITask([] { RequestOpen(); });
		return true;
	}
}
