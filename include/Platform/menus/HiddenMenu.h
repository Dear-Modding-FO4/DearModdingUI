#pragma once

#include <RE/I/IMenu.h>
#include <RE/U/UI.h>

#include <cstdint>
#include <string_view>

namespace DearModdingUI::HiddenMenu
{
	// A game menu that carries a hidden movie; subclasses add only their flags.
	class Menu :
		public RE::IMenu
	{
	public:
		explicit Menu(std::string_view a_name);

		void AdvanceMovie(float a_interval, uint64_t a_currentTime) override;
		RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& a_message) override;
		void PreDisplay() override;

		[[nodiscard]] bool Valid() const noexcept
		{
			return uiMovie != nullptr;
		}

	private:
		std::string_view m_name;
	};

	void LogCreateFailure(std::string_view a_name) noexcept;

	template <class T>
	[[nodiscard]] RE::IMenu* Create([[maybe_unused]] const RE::UIMessage& a_message)
	{
		auto* menu = new T();
		if (menu->Valid())
			return menu;
		LogCreateFailure(T::kName);
		delete menu;
		return nullptr;
	}

	// False when the UI is missing or another plugin already owns the name.
	[[nodiscard]] bool Register(const char* a_name, RE::UI::Create_t* a_create) noexcept;
}
