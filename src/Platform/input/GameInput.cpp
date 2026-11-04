#include <Platform/input/GameInput.h>
#include <DearModdingUI/host/Hotkeys.h>
#include <Support/Detours.h>
#include <Support/OnceFlag.h>
#include <Platform/input/EngineFocus.h>
#include <Platform/input/EngineKeys.h>
#include <Platform/rendering/PlatformImGui.h>
#include <F4SE/InputMap.h>

#include <RE/B/BSInputDevice.h>
#include <RE/B/BSInputDeviceManager.h>
#include <RE/B/BSInputEventReceiver.h>
#include <RE/B/BSInputEventUser.h>
#include <RE/B/ButtonEvent.h>
#include <RE/C/CharacterEvent.h>
#include <RE/D/DeviceConnectEvent.h>
#include <RE/M/MouseMoveEvent.h>
#include <RE/T/ThumbstickEvent.h>
#include <RE/H/hkRefPtr.h>
#include <RE/I/InputEvent.h>
#include <RE/M/MenuControls.h>
#include <RE/P/PlayerCamera.h>
#include <RE/P/PlayerControls.h>
#include <REX/REX.h>

#include <Windows.h>

#include <array>
#include <atomic>
#include <mutex>
#include <string_view>
#include <type_traits>
#include <utility>

#undef ERROR

namespace DearModdingUI::GameInput
{
	using namespace std::literals;

	namespace
	{
		using TPerformInputProcessing = void (*)(
			RE::BSInputEventReceiver*,
			const RE::InputEvent*);

		using TClearInputState = void (*)(RE::BSInputDevice*);

		template <class Target>
		struct VTableHook
		{
			std::atomic<Target> original{ nullptr };
			Support::OnceFlag missingOriginalLogged;
		};

		using ReceiverHook = VTableHook<TPerformInputProcessing>;

		static ReceiverHook s_menuControlsHook{};
		static ReceiverHook s_playerControlsHook{};
		static ReceiverHook s_playerCameraHook{};
		static VTableHook<TClearInputState> s_keyboardClearHook{};
		static std::atomic<bool> s_installAttempted{ false };
		static std::atomic<bool> s_installed{ false };
		static std::atomic<bool> s_suppressed{ false };
		static Support::OnceFlag s_runtimeFailureReported;

		constexpr std::array kHooks{
			InputReceiver::kMenuControls,
			InputReceiver::kPlayerControls,
			InputReceiver::kPlayerCamera,
			InputReceiver::kKeyboardDevice
		};
		static_assert(kHooks.size() == kInputHookCount);

		constexpr Support::Phrase kWaitingForGameData{
			"$DMUI_Input_WaitingForGameData",
			"Waiting for the game-data-ready prerequisite."
		};

		class InputHealthReporter final : public DearModdingUI::HealthReporter
		{
		public:
			explicit InputHealthReporter(std::string_view a_label) noexcept :
				m_label(a_label)
			{}

			void Report(
				DearModdingUI::HealthEvent a_event,
				const DearModdingUI::HealthSnapshot& a_snapshot) noexcept override
			{
				using DearModdingUI::HealthEvent;
				using DearModdingUI::HealthState;
				if (a_snapshot.state == HealthState::kFailed)
				{
					REX::ERROR("[{}] {} Failed: {}"sv,
						a_snapshot.identity,
						m_label,
						a_snapshot.reason.English());
				}
				else if (a_snapshot.state == HealthState::kReady)
				{
					REX::INFO("[{}] {} {}: {}"sv,
						a_snapshot.identity,
						m_label,
						a_event == HealthEvent::kRecovery ?
							"recovered" :
							"Ready",
						a_snapshot.reason.English());
				}
			}

		private:
			std::string_view m_label;
		};

		InputHealthReporter s_inputHealthReporter{ "Game-input interception" };
		DearModdingUI::SubsystemHealth s_inputHealth{
			"dmui.input.game-interception",
			{ "$DMUI_Health_SubsystemGameInput", "Game input" },
			s_inputHealthReporter,
			DearModdingUI::HostSubsystemHealthRegistry()
		};

		InputHealthReporter s_tapHealthReporter{ "Window thread tap" };
		DearModdingUI::SubsystemHealth s_tapHealth{
			"dmui.input.thread-tap",
			{ "$DMUI_Health_SubsystemThreadTap", "Text input" },
			s_tapHealthReporter,
			DearModdingUI::HostSubsystemHealthRegistry()
		};

		// Mouse moves drive the game cursor and device events its input mode.
		[[nodiscard]] bool IsSuppressible(const RE::InputEvent& a_event) noexcept
		{
			return a_event.As<RE::ButtonEvent>() ||
				a_event.As<RE::CharacterEvent>() ||
				a_event.As<RE::ThumbstickEvent>();
		}

		void PublishRuntimeFailure(InputReceiver a_receiver) noexcept
		{
			if (!s_runtimeFailureReported.First())
				return;
			try
			{
				std::array<InputReceiverHookOutcome, kInputHookCount> outcomes{};
				for (size_t index = 0; index < kHooks.size(); ++index)
				{
					outcomes[index] = { kHooks[index],
						kHooks[index] == a_receiver ?
							InputHookFailure::kOriginalTargetLost :
							InputHookFailure::kNone };
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

		// Receivers poll overlapping queues, so events are tracked by time code.
		class EventObserver
		{
		public:
			// Observes each event once, in whichever receiver polls it first.
			// Every engine receiver and menu skips kStop events.
			template <class Observe>
			void ObserveNew(
				const RE::InputEvent* a_queueHead,
				bool a_suppress,
				Observe&& a_observe) noexcept
			{
				const std::scoped_lock lock{ m_mutex };
				// Poll sorts by code, so a modifier can trail the key it affects.
				FoldModifiers(a_queueHead);
				for (auto* event = a_queueHead; event; event = event->next)
				{
					if (!IsNew(*event, m_seen, m_observedThrough))
						continue;
					m_seen = true;
					m_observedThrough = event->timeCode;
					if (a_observe(*event, m_modifiers.Mask()) ||
						(a_suppress && IsSuppressible(*event)))
						const_cast<RE::InputEvent*>(event)->handled =
							RE::InputEvent::HANDLED_RESULT::kStop;
				}
			}

			[[nodiscard]] uint32_t Modifiers() noexcept
			{
				const std::scoped_lock lock{ m_mutex };
				return m_modifiers.Mask();
			}

			void ResetModifiers() noexcept
			{
				const std::scoped_lock lock{ m_mutex };
				m_modifiers.Reset();
			}

		private:
			[[nodiscard]] static bool IsNew(
				const RE::InputEvent& a_event, bool a_seen, uint32_t a_observedThrough) noexcept
			{
				return !a_seen || static_cast<int32_t>(a_event.timeCode - a_observedThrough) > 0;
			}

			void FoldModifiers(const RE::InputEvent* a_queueHead) noexcept
			{
				std::array<EngineKeys::KeyEdge, 16> batch;
				size_t count{ 0 };
				const auto flush = [&]() noexcept {
					m_modifiers.FoldBatch({ batch.data(), count });
					count = 0;
				};
				auto seen = m_seen;
				auto observedThrough = m_observedThrough;
				for (auto* event = a_queueHead; event; event = event->next)
				{
					if (!IsNew(*event, seen, observedThrough))
						continue;
					seen = true;
					observedThrough = event->timeCode;
					const auto* button = event->As<RE::ButtonEvent>();
					if (!button || button->device != RE::INPUT_DEVICE::kKeyboard)
						continue;
					batch[count++] = { static_cast<uint32_t>(button->idCode),
						EngineKeys::ClassifyEdge(button->value, button->heldDownSecs) };
					if (count == batch.size())
						flush();
				}
				flush();
			}

			std::mutex m_mutex;
			EngineKeys::ModifierShadow m_modifiers;
			uint32_t m_observedThrough{ 0 };
			bool m_seen{ false };
		};

		static EventObserver s_observer{};

		// True when DMUI acted on this event's edge, so the game must not see it.
		[[nodiscard]] bool ObserveEvent(
			const RE::InputEvent& a_event,
			uint32_t a_modifiers) noexcept
		{
			if (const auto* connection = a_event.As<RE::DeviceConnectEvent>();
				connection && connection->device == RE::INPUT_DEVICE::kGamepad && !connection->connected)
				PlatformImGui::ReleaseGamepad();
			if (const auto* stick = a_event.As<RE::ThumbstickEvent>();
				stick && stick->device == RE::INPUT_DEVICE::kGamepad)
				PlatformImGui::ObserveStick(
					stick->idCode == RE::ThumbstickEvent::kLeft,
					stick->xValue, stick->yValue);
			if (const auto* move = a_event.As<RE::MouseMoveEvent>();
				move && (move->mouseInputX || move->mouseInputY))
				PlatformImGui::ObserveMouseMove();
			const auto* button = a_event.As<RE::ButtonEvent>();
			if (!button)
				return false;
			PlatformImGui::ButtonEdge edge{
				.edge = EngineKeys::ClassifyEdge(button->value, button->heldDownSecs),
				.id = static_cast<uint32_t>(button->idCode),
				.value = button->value,
				.modifiers = a_modifiers
			};
			switch (button->device.get())
			{
			case RE::INPUT_DEVICE::kKeyboard:
				edge.device = PlatformImGui::InputDevice::kKeyboard;
				break;
			case RE::INPUT_DEVICE::kMouse:
				edge.device = PlatformImGui::InputDevice::kMouse;
				edge.id = PlatformImGui::MouseKeyCode(button->idCode);
				break;
			case RE::INPUT_DEVICE::kGamepad:
				edge.device = PlatformImGui::InputDevice::kGamepad;
				edge.id = F4SE::InputMap::GamepadMaskToKeycode(button->idCode);
				break;
			default:
				return false;
			}
			return edge.id && edge.id < F4SE::InputMap::kMaxMacros && PlatformImGui::ObserveButton(edge);
		}

		// ClearInputState sends no release edges for held keys.
		void ReleaseHeldInput() noexcept
		{
			s_observer.ResetModifiers();
			const auto released = PlatformImGui::ResetInput();
			REX::INFO(
				"DearModdingUI: engine cleared keyboard input state (polling {}, {} held keys released)"sv,
				EngineFocus::Polling(),
				released);
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
				if (a_hook.missingOriginalLogged.First())
					PublishRuntimeFailure(a_receiverKind);
				return;
			}

			s_observer.ObserveNew(
				a_queueHead,
				SuppressesGameInput(
					s_suppressed.load(std::memory_order_acquire),
					DearModdingUI::Hotkeys::IsToggleChordHeld()),
				[](const RE::InputEvent& a_event, uint32_t a_modifiers) noexcept {
					return ObserveEvent(a_event, a_modifiers);
				});
			original(a_receiver, a_queueHead);
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

		template <class Target>
		[[nodiscard]] InputHookFailure PatchVTableSlot(
			void** a_vtable,
			uint32_t a_slot,
			std::type_identity_t<Target> a_hook,
			VTableHook<Target>& a_record) noexcept
		{
			const auto current = reinterpret_cast<Target>(a_vtable[a_slot]);
			if (!current)
				return InputHookFailure::kTargetUnavailable;
			if (current == a_hook)
				return InputHookFailure::kAlreadyHooked;

			a_record.original.store(current, std::memory_order_release);
			const auto previous = reinterpret_cast<Target>(
				Support::DetourVTable(
					reinterpret_cast<uintptr_t>(a_vtable),
					reinterpret_cast<uintptr_t>(a_hook),
					a_slot));
			if (!previous)
			{
				a_record.original.store(nullptr, std::memory_order_release);
				return InputHookFailure::kPatchFailed;
			}
			if (previous != a_hook)
				a_record.original.store(previous, std::memory_order_release);

			return InputHookFailure::kNone;
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

			return PatchVTableSlot(
				vtable, kPerformInputProcessingSlot, a_hook, a_record);
		}

		static void HKClearInputState(RE::BSInputDevice* a_device) noexcept
		{
			if (const auto original = s_keyboardClearHook.original.load(std::memory_order_acquire))
				original(a_device);
			else if (s_keyboardClearHook.missingOriginalLogged.First())
				PublishRuntimeFailure(InputReceiver::kKeyboardDevice);
			ReleaseHeldInput();
		}

		[[nodiscard]] InputHookFailure InstallKeyboardDevice() noexcept
		{
			const auto* manager = RE::BSInputDeviceManager::GetSingleton();
			auto* device = manager ?
				manager->devices[std::to_underlying(RE::BSInputDeviceManager::BS_DEVICES::kKeyboard)] :
				nullptr;
			if (!device)
				return InputHookFailure::kSingletonUnavailable;
			auto** const vtable = *reinterpret_cast<void***>(device);
			if (!vtable)
				return InputHookFailure::kVtableUnavailable;
			return PatchVTableSlot(
				vtable, kClearInputStateSlot, &HKClearInputState, s_keyboardClearHook);
		}
	}

	void InitializeHealth() noexcept
	{
		(void)s_inputHealth.Observe(
			DearModdingUI::HealthState::kWaiting,
			Support::Message{ kWaitingForGameData });
		(void)s_tapHealth.Observe(
			DearModdingUI::HealthState::kWaiting,
			Support::Message{ kWaitingForGameData });
	}

	bool InstallHooks() noexcept
	{
		bool expected{ false };
		if (!s_installAttempted.compare_exchange_strong(
				expected, true, std::memory_order_acq_rel))
			return s_installed.load(std::memory_order_acquire);

		try
		{
			(void)s_inputHealth.Observe(
				DearModdingUI::HealthState::kProgressing,
				AllReceiversMessage({
					"$DMUI_Input_Installing",
					"Installing {}, {}, {}, and {} interception."
				}));
		}
		catch (...)
		{
			// Health is advisory; installation still proceeds.
		}
		const std::array<InputReceiverHookOutcome, kInputHookCount> outcomes{
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
					s_playerCameraHook) },
			InputReceiverHookOutcome{
				InputReceiver::kKeyboardDevice,
				InstallKeyboardDevice() }
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

	bool HooksInstalled() noexcept
	{
		return s_installed.load(std::memory_order_acquire);
	}

	uint32_t HeldModifiers() noexcept
	{
		return s_observer.Modifiers();
	}

	void PublishThreadTapHealth(uint32_t a_thread, bool a_installed) noexcept
	{
		try
		{
			(void)s_tapHealth.Observe(
				a_installed ? DearModdingUI::HealthState::kReady : DearModdingUI::HealthState::kFailed,
				a_installed ?
					Support::Message{ Support::Phrase{
						"$DMUI_Input_TapReady",
						"Message tap installed on UI thread {}." }, a_thread } :
					Support::Message{ Support::Phrase{
						"$DMUI_Input_TapFailed",
						"The window thread message tap is unavailable; typed text and cursor ownership will not work." } });
		}
		catch (...)
		{
			// Health is advisory; the log already names the failure.
		}
	}

	void SuppressGameInput(bool a_suppressed) noexcept
	{
		if (s_suppressed.exchange(a_suppressed, std::memory_order_acq_rel) || !a_suppressed)
			return;
		// Mirror the engine's menu-mode release, which non-pausing menus skip.
		if (auto* controls = RE::PlayerControls::GetSingleton())
		{
			controls->data.setupHeldStatesForRelease = true;
			controls->data.checkHeldStates = true;
		}
	}
}
