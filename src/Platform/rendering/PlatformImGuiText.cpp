// CommonLib declarations must precede the Windows SDK macros.
#include <REX/REX.h>

#include "PlatformImGuiInternal.h"

#include <Platform/input/CursorLoader.h>
#include <Platform/input/EngineFocus.h>
#include <Platform/input/EngineKeys.h>
#include <Platform/input/GameInput.h>
#include <Platform/input/ThreadMessageTap.h>
#include <DearModdingUI/host/Hotkeys.h>

#include <imgui/imgui.h>

#include <array>
#include <atomic>
#include <mutex>

namespace DearModdingUI::PlatformImGuiDetail
{
	using namespace std::literals;

	namespace
	{
		struct TextEvent
		{
			char16_t unit{ 0 };
			bool languageChanged{ false };
		};

		constexpr size_t kTextCapacity = 64;

		// The tap fills this on the window thread; the frame drains it.
		struct TextQueue
		{
			std::mutex mutex;
			std::array<TextEvent, kTextCapacity> events{};
			size_t count{ 0 };
		};

		TextQueue s_text;
		std::atomic<bool> s_wantsText{ false };

		void Enqueue(TextEvent a_event) noexcept
		{
			const std::scoped_lock lock{ s_text.mutex };
			if (s_text.count < s_text.events.size())
				s_text.events[s_text.count++] = a_event;
		}

		bool ClaimsKeyDown(uint32_t a_virtualKey) noexcept
		{
			if (!EngineFocus::Active())
				return false;
			RefreshHotkeyContext();
			const auto modifiers = GameInput::HeldModifiers();
			for (const auto code : EngineKeys::ToHotkeyCode(a_virtualKey))
			{
				if (Hotkeys::WouldActOn(code, modifiers))
					return true;
			}
			return false;
		}

		// A hotkey's own character never reaches the menu or the game.
		bool RouteText(char16_t a_unit, bool a_claimed) noexcept
		{
			if (a_claimed)
				return true;
			if (!Context().drawingEnabled.load(std::memory_order_acquire))
				return false;
			Enqueue({ a_unit, false });
			return s_wantsText.load(std::memory_order_acquire);
		}

		void NoteInputLanguage() noexcept
		{
			Enqueue({ 0, true });
		}

		void ApplySetCursor(void* a_window, uint32_t a_hitTest) noexcept
		{
			const ContextLock lock;
			(void)DearModdingUI::CursorLoader::ApplySetCursor(a_window, a_hitTest);
		}
	}

	void DrainTextInputLocked() noexcept
	{
		std::array<TextEvent, kTextCapacity> events;
		size_t count;
		{
			const std::scoped_lock lock{ s_text.mutex };
			events = s_text.events;
			count = std::exchange(s_text.count, size_t{ 0 });
		}
		if (!count || !ImGui::GetCurrentContext())
			return;
		auto& io = ImGui::GetIO();
		for (size_t index = 0; index < count; ++index)
		{
			if (events[index].languageChanged)
				RefreshInputLanguageLocked(Context().activeWindow.load(std::memory_order_acquire), io);
			else
				io.AddInputCharacterUTF16(events[index].unit);
		}
	}

	void ClearTextInput() noexcept
	{
		const std::scoped_lock lock{ s_text.mutex };
		s_text.count = 0;
	}

	void PublishTextCapture(bool a_wantsText) noexcept
	{
		s_wantsText.store(a_wantsText, std::memory_order_release);
	}
}

namespace DearModdingUI::PlatformImGui
{
	bool InstallThreadTap() noexcept
	{
		using namespace PlatformImGuiDetail;
		if (ThreadMessageTap::Installed())
			return true;
		if (!ThreadMessageTap::Install(
				{ &ClaimsKeyDown, &RouteText, &NoteInputLanguage, &ApplySetCursor }))
		{
			REX::ERROR(
				"DearModdingUI: message tap unavailable on thread {}"sv,
				GetCurrentThreadId());
			return false;
		}
		REX::INFO("DearModdingUI: message tap installed on thread {}"sv, ThreadMessageTap::Thread());
		return true;
	}

	bool VerifyThreadTap() noexcept
	{
		using namespace std::literals;
		const auto tapped = ThreadMessageTap::Thread();
		const auto ui = EngineFocus::UiThread();
		const auto valid = tapped != 0 && tapped == ui;
		if (!valid)
		{
			REX::ERROR(
				"DearModdingUI: message tap is on thread {} but the game window is on thread {}; "
				"typed text and cursor ownership will not work"sv,
				tapped,
				ui);
		}
		GameInput::PublishThreadTapHealth(ui, valid);
		return valid;
	}
}
