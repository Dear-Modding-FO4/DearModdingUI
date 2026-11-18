#include <Platform/menus/HiddenMenu.h>

#include <RE/B/BSScaleformManager.h>
#include <REX/REX.h>

namespace DearModdingUI::HiddenMenu
{
	namespace
	{
		using namespace std::literals;

		inline constexpr auto kMoviePath = "Interface/CursorMenu.swf"sv;

		[[nodiscard]] bool HasRegistration(
			RE::UI& a_ui,
			const RE::BSFixedString& a_name,
			RE::UI::Create_t* a_create) noexcept
		{
			const RE::BSAutoReadLock lock{ RE::UI::GetMenuMapRWLock() };
			const auto entry = a_ui.menuMap.find(a_name);
			return entry != a_ui.menuMap.end() &&
				(!a_create || entry->second.create == a_create);
		}
	}

	Menu::Menu(std::string_view a_name) :
		m_name(a_name)
	{
		auto* scaleform = RE::BSScaleformManager::GetSingleton();
		if (scaleform && scaleform->LoadMovieEx(*this, kMoviePath) && uiMovie)
			uiMovie->SetVisible(false);
	}

	void Menu::AdvanceMovie(
		[[maybe_unused]] float a_interval,
		[[maybe_unused]] uint64_t a_currentTime)
	{}

	RE::UI_MESSAGE_RESULTS Menu::ProcessMessage(RE::UIMessage& a_message)
	{
		return a_message.menu == m_name ?
			RE::UI_MESSAGE_RESULTS::kHandled :
			RE::UI_MESSAGE_RESULTS::kPassOn;
	}

	void Menu::PreDisplay()
	{
		if (uiMovie)
			uiMovie->SetVisible(false);
	}

	void LogCreateFailure(std::string_view a_name) noexcept
	{
		REX::ERROR("DearModdingUI: {} could not load {}"sv, a_name, kMoviePath);
	}

	bool Register(const char* a_name, RE::UI::Create_t* a_create) noexcept
	{
		auto* ui = RE::UI::GetSingleton();
		if (!ui)
		{
			REX::ERROR("DearModdingUI: {} registration failed because UI is unavailable"sv, a_name);
			return false;
		}
		const RE::BSFixedString name{ a_name };
		if (HasRegistration(*ui, name, a_create))
			return true;
		if (HasRegistration(*ui, name, nullptr))
		{
			REX::ERROR("DearModdingUI: {} registration failed because another plugin owns the name"sv, a_name);
			return false;
		}
		ui->RegisterMenu(a_name, a_create);
		if (!HasRegistration(*ui, name, a_create))
		{
			REX::ERROR("DearModdingUI: {} registration was not retained by UI"sv, a_name);
			return false;
		}
		REX::INFO("DearModdingUI: registered menu \"{}\""sv, a_name);
		return true;
	}
}
