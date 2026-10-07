#include <Platform/input/GameInput.h>
#include <DearModdingUI/host/Hotkeys.h>
#include <Support/Detours.h>
#include <Platform/rendering/PlatformImGui.h>
#include <F4SE/InputMap.h>

#include <RE/B/BSInputEventReceiver.h>
#include <RE/B/BSInputEventUser.h>
#include <RE/B/ButtonEvent.h>
#include <RE/D/DeviceConnectEvent.h>
#include <RE/M/MouseMoveEvent.h>
#include <RE/T/ThumbstickEvent.h>
#include <RE/H/hkRefPtr.h>
#include <RE/I/InputEvent.h>
#include <RE/M/MenuControls.h>
#include <RE/P/PlayerCamera.h>
#include <RE/P/PlayerControls.h>
#include <REX/REX.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
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
		static std::atomic<bool> s_runtimeFailureReported{ false };

		class InputHealthReporter final : public DearModdingUI::HealthReporter
		{
		public:
			void Report(
				DearModdingUI::HealthEvent a_event,
				const DearModdingUI::HealthSnapshot& a_snapshot) noexcept override
			{
				using DearModdingUI::HealthEvent;
				using DearModdingUI::HealthState;
				if (a_snapshot.state == HealthState::kFailed)
				{
					REX::ERROR("[{}] Game-input interception Failed: {}"sv,
						a_snapshot.identity,
						a_snapshot.reason);
				}
				else if (a_snapshot.state == HealthState::kReady)
				{
					REX::INFO("[{}] Game-input interception {}: {}"sv,
						a_snapshot.identity,
						a_event == HealthEvent::kRecovery ?
							"recovered" :
							"Ready",
						a_snapshot.reason);
				}
			}
		};

		InputHealthReporter s_inputHealthReporter;
		DearModdingUI::SubsystemHealth s_inputHealth{
			"dmui.input.game-interception",
			s_inputHealthReporter,
			DearModdingUI::HostSubsystemHealthRegistry()
		};

		void ObserveMenuInput(
			RE::MenuControls& a_controls,
			const RE::InputEvent* a_queueHead)
		{
			for (auto* event = a_queueHead; event; event = event->next)
			{
				const auto* connection = event->As<RE::DeviceConnectEvent>();
				for (auto* handler : a_controls.handlers)
				{
					if (*event->handled == RE::InputEvent::HANDLED_RESULT::kStop)
						break;
					if (!handler->inputEventHandlingEnabled)
						continue;

					// Device-switch observers use the predicate without accepting game actions.
					if (handler->ShouldHandleEvent(event) && connection)
						handler->OnDeviceConnectEvent(connection);
				}
			}
		}

		void PublishRuntimeFailure(InputReceiver a_receiver) noexcept
		{
			if (s_runtimeFailureReported.exchange(true, std::memory_order_acq_rel))
				return;
			try
			{
				std::array outcomes{
					InputReceiverHookOutcome{
						InputReceiver::kMenuControls,
						InputHookFailure::kNone },
					InputReceiverHookOutcome{
						InputReceiver::kPlayerControls,
						InputHookFailure::kNone },
					InputReceiverHookOutcome{
						InputReceiver::kPlayerCamera,
						InputHookFailure::kNone }
				};
				for (auto& outcome : outcomes)
				{
					if (outcome.receiver == a_receiver)
						outcome.failure = InputHookFailure::kOriginalTargetLost;
				}
				const auto observation = ClassifyInputHookHealth(outcomes);
				(void)s_inputHealth.Observe(
					observation.state,
					observation.reason);
			}
			catch (...)
			{
				REX::ERROR(
					"Game input: a hooked receiver lost its original target"sv);
			}
		}

		enum class KeyEdge : uint8_t
		{
			kNone,
			kPress,
			kRelease
		};

		// Edges the window procedure consumed before the engine polls raw input.
		class KeyboardEdgeLedger
		{
		public:
			void Note(uint32_t a_code, bool a_pressed, bool a_consumed) noexcept
			{
				if (a_code >= m_pending.size())
					return;
				// A new edge supersedes one the engine never reported.
				m_pending[a_code].store(
					a_consumed ? (a_pressed ? KeyEdge::kPress : KeyEdge::kRelease) : KeyEdge::kNone,
					std::memory_order_release);
			}

			[[nodiscard]] bool Take(uint32_t a_code, KeyEdge a_edge) noexcept
			{
				if (a_code >= m_pending.size() || a_edge == KeyEdge::kNone)
					return false;
				auto expected = a_edge;
				return m_pending[a_code].compare_exchange_strong(
					expected, KeyEdge::kNone, std::memory_order_acq_rel);
			}

		private:
			std::array<std::atomic<KeyEdge>, 256> m_pending{};
		};

		// Receivers poll overlapping queues, so events are tracked by time code.
		class EventObserver
		{
		public:
			// Observes each event once, in whichever receiver polls it first.
			template <class Observe>
			void ObserveNew(const RE::InputEvent* a_queueHead, Observe&& a_observe) noexcept
			{
				const std::scoped_lock lock{ m_mutex };
				for (auto* event = a_queueHead; event; event = event->next)
				{
					if (m_seen && static_cast<int32_t>(event->timeCode - m_observedThrough) <= 0)
						continue;
					m_seen = true;
					m_observedThrough = event->timeCode;
					if (a_observe(*event))
						m_hidden[m_next++ % m_hidden.size()] = event->timeCode;
				}
			}

			[[nodiscard]] bool HidesAny(const RE::InputEvent* a_queueHead) noexcept
			{
				const std::scoped_lock lock{ m_mutex };
				for (auto* event = a_queueHead; event; event = event->next)
				{
					if (std::ranges::find(m_hidden, event->timeCode) != m_hidden.end())
						return true;
				}
				return false;
			}

		private:
			std::mutex m_mutex;
			std::array<uint32_t, 16> m_hidden{ [] {
				std::array<uint32_t, 16> unset{};
				unset.fill(UINT32_MAX);
				return unset;
			}() };
			size_t m_next{ 0 };
			uint32_t m_observedThrough{ 0 };
			bool m_seen{ false };
		};

		static KeyboardEdgeLedger s_ledger{};
		static EventObserver s_observer{};

		// True when DMUI acted on this event's edge, so the game must not see it.
		[[nodiscard]] bool ObserveEvent(const RE::InputEvent& a_event) noexcept
		{
			if (const auto* connection = a_event.As<RE::DeviceConnectEvent>();
				connection && connection->device == RE::INPUT_DEVICE::kGamepad && !connection->connected)
				PlatformImgui::ReleaseGamepad();
			if (const auto* stick = a_event.As<RE::ThumbstickEvent>();
				stick && stick->device == RE::INPUT_DEVICE::kGamepad)
				PlatformImgui::ObserveStick(
					stick->idCode == RE::ThumbstickEvent::kLeft,
					stick->xValue, stick->yValue);
			if (const auto* move = a_event.As<RE::MouseMoveEvent>();
				move && (move->mouseInputX || move->mouseInputY))
				PlatformImgui::ObserveMouseMove();
			const auto* button = a_event.As<RE::ButtonEvent>();
			if (!button)
				return false;
			if (button->device == RE::INPUT_DEVICE::kKeyboard)
				return s_ledger.Take(button->idCode, button->QJustPressed() ? KeyEdge::kPress :
					button->value == 0.0f ? KeyEdge::kRelease : KeyEdge::kNone);
			uint32_t code = 0;
			bool pulse = false;
			if (button->device == RE::INPUT_DEVICE::kMouse)
			{
				code = ImguiPlatform::MouseKeyCode(button->idCode);
				pulse = button->idCode == static_cast<uint32_t>(RE::BS_BUTTON_CODE::kWheelUp) ||
					button->idCode == static_cast<uint32_t>(RE::BS_BUTTON_CODE::kWheelDown);
			}
			else if (button->device == RE::INPUT_DEVICE::kGamepad)
				code = F4SE::InputMap::GamepadMaskToKeycode(button->idCode);
			// Only edges hide; a held owned button still reaches the game.
			return code && code < F4SE::InputMap::kMaxMacros &&
				PlatformImgui::ObserveButton(
					code, button->QPressed(), !pulse && button->QHeldDown(), pulse, button->value) &&
				(pulse || !button->QHeldDown());
		}

		static void Forward(
			ReceiverHook& a_hook,
			InputReceiver a_receiverKind,
			RE::BSInputEventReceiver* a_receiver,
			const RE::InputEvent* a_queueHead) noexcept
		{
			const auto original = a_hook.original.load(std::memory_order_acquire);
			if (!original)
			{
				if (!a_hook.missingOriginalLogged.exchange(true, std::memory_order_acq_rel))
					PublishRuntimeFailure(a_receiverKind);
				return;
			}

			s_observer.ObserveNew(a_queueHead, ObserveEvent);
			const auto decision = DecideInputQueue(
				s_blocked.load(std::memory_order_acquire),
				kMenuInputSuppression,
				s_observer.HidesAny(a_queueHead) || DearModdingUI::Hotkeys::IsToggleChordHeld());
			if (decision == InputQueueDecision::kDiscard &&
				a_receiverKind == InputReceiver::kMenuControls)
			{
				ObserveMenuInput(
					*static_cast<RE::MenuControls*>(a_receiver), a_queueHead);
			}
			original(
				a_receiver,
				decision == InputQueueDecision::kDiscard ? nullptr : a_queueHead);
		}

		static void HKMenuControls(
			RE::BSInputEventReceiver* a_receiver,
			const RE::InputEvent* a_queueHead) noexcept
		{
			Forward(
				s_menuControlsHook,
				InputReceiver::kMenuControls,
				a_receiver,
				a_queueHead);
		}

		static void HKPlayerControls(
			RE::BSInputEventReceiver* a_receiver,
			const RE::InputEvent* a_queueHead) noexcept
		{
			Forward(
				s_playerControlsHook,
				InputReceiver::kPlayerControls,
				a_receiver,
				a_queueHead);
		}

		static void HKPlayerCamera(
			RE::BSInputEventReceiver* a_receiver,
			const RE::InputEvent* a_queueHead) noexcept
		{
			Forward(
				s_playerCameraHook,
				InputReceiver::kPlayerCamera,
				a_receiver,
				a_queueHead);
		}

		template <class T>
		[[nodiscard]] InputHookFailure InstallReceiver(
			T* a_instance,
			uintptr_t a_expectedOffset,
			TPerformInputProcessing a_hook,
			ReceiverHook& a_record) noexcept
		{
			if (!a_instance)
				return InputHookFailure::kSingletonUnavailable;

			auto* const receiver =
				static_cast<RE::BSInputEventReceiver*>(a_instance);
			const auto objectAddress = reinterpret_cast<uintptr_t>(a_instance);
			const auto receiverAddress = reinterpret_cast<uintptr_t>(receiver);
			if (!MatchesReceiverOffset(
					objectAddress,
					receiverAddress,
					a_expectedOffset))
				return InputHookFailure::kReceiverOffsetMismatch;

			auto** const vtable = *reinterpret_cast<void***>(receiver);
			if (!vtable)
				return InputHookFailure::kVtableUnavailable;

			const auto current = reinterpret_cast<TPerformInputProcessing>(
				vtable[kPerformInputProcessingSlot]);
			if (!current)
				return InputHookFailure::kTargetUnavailable;
			if (current == a_hook)
				return InputHookFailure::kAlreadyHooked;

			a_record.original.store(current, std::memory_order_release);
			const auto previous = reinterpret_cast<TPerformInputProcessing>(
				Support::DetourVTable(
					reinterpret_cast<uintptr_t>(vtable),
					reinterpret_cast<uintptr_t>(a_hook),
					kPerformInputProcessingSlot));
			if (!previous)
			{
				a_record.original.store(nullptr, std::memory_order_release);
				return InputHookFailure::kPatchFailed;
			}
			if (previous != a_hook)
				a_record.original.store(previous, std::memory_order_release);

			return InputHookFailure::kNone;
		}
	}

	void InitializeHealth() noexcept
	{
		(void)s_inputHealth.Observe(
			DearModdingUI::HealthState::kWaiting,
			"Waiting for the game-data-ready prerequisite.");
	}

	bool InstallHooks() noexcept
	{
		bool expected{ false };
		if (!s_installAttempted.compare_exchange_strong(
				expected, true, std::memory_order_acq_rel))
			return s_installed.load(std::memory_order_acquire);

		(void)s_inputHealth.Observe(
			DearModdingUI::HealthState::kProgressing,
			"Installing MenuControls, PlayerControls, and PlayerCamera interception.");
		const std::array outcomes{
			InputReceiverHookOutcome{
				InputReceiver::kMenuControls,
				InstallReceiver(
					RE::MenuControls::GetSingleton(),
					0,
					&HKMenuControls,
					s_menuControlsHook) },
			InputReceiverHookOutcome{
				InputReceiver::kPlayerControls,
				InstallReceiver(
					RE::PlayerControls::GetSingleton(),
					0,
					&HKPlayerControls,
					s_playerControlsHook) },
			InputReceiverHookOutcome{
				InputReceiver::kPlayerCamera,
				InstallReceiver(
					RE::PlayerCamera::GetSingleton(),
					kPlayerCameraReceiverOffset,
					&HKPlayerCamera,
					s_playerCameraHook) }
		};
		try
		{
			const auto observation = ClassifyInputHookHealth(outcomes);
			const auto installed =
				observation.state == DearModdingUI::HealthState::kReady;
			s_installed.store(installed, std::memory_order_release);
			(void)s_inputHealth.Observe(
				observation.state,
				observation.reason);
			return installed;
		}
		catch (...)
		{
			s_installed.store(false, std::memory_order_release);
			REX::ERROR(
				"Game input: hook outcomes could not be retained; treating installation as failed"sv);
			return false;
		}
	}

	void SetBlocked(bool a_blocked) noexcept
	{
		if (s_blocked.exchange(a_blocked, std::memory_order_acq_rel) || !a_blocked)
			return;
		// Mirror the engine's menu-mode release, which non-pausing menus skip.
		if (auto* controls = RE::PlayerControls::GetSingleton())
		{
			controls->data.setupHeldStatesForRelease = true;
			controls->data.checkHeldStates = true;
		}
	}

	void NoteKeyboardEdge(uint32_t a_engineCode, bool a_pressed, bool a_consumed) noexcept
	{
		s_ledger.Note(a_engineCode, a_pressed, a_consumed);
	}
}
