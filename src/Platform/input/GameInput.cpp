#include <Platform/input/GameInput.h>
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
		s_blocked.store(a_blocked, std::memory_order_release);
	}
}
