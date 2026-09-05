#include <Platform/GameInput.h>
#include <Support/Detours.h>

#include <RE/B/BSInputEventReceiver.h>
#include <RE/H/hkRefPtr.h>
#include <RE/M/MenuControls.h>
#include <RE/P/PlayerCamera.h>
#include <RE/P/PlayerControls.h>
#include <REX/REX.h>

#include <atomic>
#include <string_view>

namespace Addictol::GameInput
{
	using namespace std::literals;

	namespace
	{
		using TPerformInputProcessing = void (*)(
			RE::BSInputEventReceiver*,
			const RE::InputEvent*);

		struct ReceiverHook
		{
			std::atomic<TPerformInputProcessing> original{ nullptr };
			std::atomic<bool> missingOriginalLogged{ false };
		};

		static ReceiverHook s_menuControlsHook{};
		static ReceiverHook s_playerControlsHook{};
		static ReceiverHook s_playerCameraHook{};
		static std::atomic<bool> s_installAttempted{ false };
		static std::atomic<bool> s_installed{ false };
		static std::atomic<bool> s_blocked{ false };

		static void Forward(
			ReceiverHook& a_hook,
			std::string_view a_name,
			RE::BSInputEventReceiver* a_receiver,
			const RE::InputEvent* a_queueHead) noexcept
		{
			const auto original = a_hook.original.load(std::memory_order_acquire);
			if (!original)
			{
				if (!a_hook.missingOriginalLogged.exchange(true, std::memory_order_acq_rel))
					REX::ERROR("Game input: {} hook has no original target"sv, a_name);
				return;
			}

			const auto decision = DecideInputQueue(
				s_blocked.load(std::memory_order_acquire));
			original(
				a_receiver,
				decision == InputQueueDecision::kDiscard ? nullptr : a_queueHead);
		}

		static void HKMenuControls(
			RE::BSInputEventReceiver* a_receiver,
			const RE::InputEvent* a_queueHead) noexcept
		{
			Forward(s_menuControlsHook, "MenuControls"sv, a_receiver, a_queueHead);
		}

		static void HKPlayerControls(
			RE::BSInputEventReceiver* a_receiver,
			const RE::InputEvent* a_queueHead) noexcept
		{
			Forward(s_playerControlsHook, "PlayerControls"sv, a_receiver, a_queueHead);
		}

		static void HKPlayerCamera(
			RE::BSInputEventReceiver* a_receiver,
			const RE::InputEvent* a_queueHead) noexcept
		{
			Forward(s_playerCameraHook, "PlayerCamera"sv, a_receiver, a_queueHead);
		}

		template <class T>
		[[nodiscard]] bool InstallReceiver(
			std::string_view a_name,
			T* a_instance,
			uintptr_t a_expectedOffset,
			TPerformInputProcessing a_hook,
			ReceiverHook& a_record) noexcept
		{
			if (!a_instance)
			{
				REX::ERROR("Game input: {} singleton is unavailable; hook skipped"sv, a_name);
				return false;
			}

			auto* const receiver =
				static_cast<RE::BSInputEventReceiver*>(a_instance);
			const auto objectAddress = reinterpret_cast<uintptr_t>(a_instance);
			const auto receiverAddress = reinterpret_cast<uintptr_t>(receiver);
			if (!MatchesReceiverOffset(
					objectAddress,
					receiverAddress,
					a_expectedOffset))
			{
				REX::ERROR(
					"Game input: {} receiver offset did not match 0x{:X}; hook skipped"sv,
					a_name,
					a_expectedOffset);
				return false;
			}

			auto** const vtable = *reinterpret_cast<void***>(receiver);
			if (!vtable)
			{
				REX::ERROR("Game input: {} receiver vtable is unavailable; hook skipped"sv, a_name);
				return false;
			}

			const auto current = reinterpret_cast<TPerformInputProcessing>(
				vtable[kPerformInputProcessingSlot]);
			if (!current || current == a_hook)
			{
				REX::ERROR("Game input: {} receiver vtable cannot be hooked safely"sv, a_name);
				return false;
			}

			a_record.original.store(current, std::memory_order_release);
			const auto previous = reinterpret_cast<TPerformInputProcessing>(
				Support::DetourVTable(
					reinterpret_cast<uintptr_t>(vtable),
					reinterpret_cast<uintptr_t>(a_hook),
					kPerformInputProcessingSlot));
			if (!previous)
			{
				a_record.original.store(nullptr, std::memory_order_release);
				REX::ERROR("Game input: {} receiver vtable patch failed"sv, a_name);
				return false;
			}
			if (previous != a_hook)
				a_record.original.store(previous, std::memory_order_release);

			REX::INFO(
				"Game input: {} PerformInputProcessing hooked at receiver offset 0x{:X}"sv,
				a_name,
				a_expectedOffset);
			return true;
		}
	}

	bool InstallHooks() noexcept
	{
		bool expected{ false };
		if (!s_installAttempted.compare_exchange_strong(
				expected, true, std::memory_order_acq_rel))
			return s_installed.load(std::memory_order_acquire);

		const auto menuControls = InstallReceiver(
			"MenuControls"sv,
			RE::MenuControls::GetSingleton(),
			0,
			&HKMenuControls,
			s_menuControlsHook);
		const auto playerControls = InstallReceiver(
			"PlayerControls"sv,
			RE::PlayerControls::GetSingleton(),
			0,
			&HKPlayerControls,
			s_playerControlsHook);
		const auto playerCamera = InstallReceiver(
			"PlayerCamera"sv,
			RE::PlayerCamera::GetSingleton(),
			kPlayerCameraReceiverOffset,
			&HKPlayerCamera,
			s_playerCameraHook);
		const auto installed = menuControls && playerControls && playerCamera;
		s_installed.store(installed, std::memory_order_release);
		if (installed)
			REX::INFO("Game input: all PerformInputProcessing hooks installed"sv);
		else
			REX::WARN("Game input: one or more PerformInputProcessing hooks were skipped"sv);
		return installed;
	}

	void SetBlocked(bool a_blocked) noexcept
	{
		s_blocked.store(a_blocked, std::memory_order_release);
	}
}
