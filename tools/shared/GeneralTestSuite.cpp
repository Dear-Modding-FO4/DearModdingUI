#include "GeneralTestSuiteInternal.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <ranges>
#include <string>
#include <utility>

namespace DmuiTests
{
	namespace dmui = ::dmui;
	using namespace std::literals;
	using Detail::InitializationStatus;

	namespace
	{
		class State final
		{
		public:
			explicit State(Environment& a_environment) :
				m_context(a_environment),
				m_settings(m_context),
				m_resources(m_context),
				m_overlay(m_context, m_resources),
				m_dialogs(m_context),
				m_hotkeys(m_context)
			{}

			[[nodiscard]] bool Initialize() noexcept
			{
				auto& client = m_context.Client();
				const auto connected = client.Connect();
				m_initializationResult = client.LastResult();
				m_context.Info(
					"dmui-test-client: preflight connect={} result={} "
					"host-present={} unavailable-reason={} required-services=0x{:X} "
					"ui-abi={} minimum-ui-revision={} minimum-ui-size={}"sv,
					connected,
					DMUI_ResultToString(m_initializationResult),
					client.HostPresent(),
					static_cast<uint32_t>(client.UnavailableReason()),
					static_cast<uint64_t>(Detail::kRequiredServices),
					DMUI_UI_ABI_CURRENT,
					DMUI_UI_REVISION_CURRENT,
					DMUI_UI_API_REQUIRED_SIZE);
				if (!connected)
				{
					m_initializationStatus = InitializationStatus::kUnavailable;
					m_initializationStage = "host preflight";
					LogUnavailableOnce();
					return false;
				}

				if (const auto services = client.QueryServices())
				{
					m_services = *services;
					m_servicesResult = client.LastResult();
					m_context.Info(
						"dmui-test-client: service preflight result={} "
						"ui-abi={} ui-revision={} ui-size={} "
						"supported=0x{:X} required=0x{:X}"sv,
						DMUI_ResultToString(m_servicesResult),
						m_services.uiABI,
						m_services.uiRevision,
						m_services.uiTableSize,
						static_cast<uint64_t>(m_services.supported),
						static_cast<uint64_t>(Detail::kRequiredServices));
				}
				else
				{
					m_servicesResult = client.LastResult();
					m_context.Error(
						"dmui-test-client: service preflight result={}"sv,
						DMUI_ResultToString(m_servicesResult));
					return RegistrationFailure("service preflight");
				}

				if (!client.AddFrameObserver([this] { ObserveFrame(); }))
					return RegistrationFailure("frame observer");
				m_context.Info(
					"dmui-test-client: registration frame-observer result={}"sv,
					DMUI_ResultToString(client.LastResult()));

				const auto exercises = DmuiTestFixtures::RegisterExercises(
					client,
					[this](DmuiTestFixtures::ExerciseKind a_kind) {
						DrawExercise(a_kind);
					});
				if (!exercises)
					return RegistrationFailure("exercise pages");
				m_exercisePages = exercises->pages;
				m_context.Info(
					"dmui-test-client: registration exercise-pages "
					"result={} count={}"sv,
					DMUI_ResultToString(client.LastResult()),
					m_exercisePages.size());

				if (!m_overlay.Register(DmuiTestFixtures::kCategoryId))
					return RegistrationFailure("overlay page");
				if (!m_overlay.Configure())
					return RegistrationFailure("overlay configuration");

				if (!client.AddPageActivityObserver(
						[this](const dmui::PageActivity& a_activity) {
							OnPageActivity(a_activity);
						}))
					return RegistrationFailure("page activity observer");
				m_context.Info(
					"dmui-test-client: registration page-activity-observer "
					"result={}"sv,
					DMUI_ResultToString(client.LastResult()));

				std::string failedHotkey;
				DMUI_Result hotkeyResult{ DMUI_RESULT_OK };
				if (!m_hotkeys.Register(
						[this] { m_overlay.Toggle(); },
						[this] { m_dialogs.ScheduleDelayedNotification(); },
						failedHotkey,
						hotkeyResult))
				{
					MarkInitializationIncomplete(failedHotkey, hotkeyResult);
					return false;
				}

				std::string syntheticError;
				if (!DmuiTestFixtures::RegisterSyntheticClients(
						m_syntheticClients,
						m_syntheticSettings,
						syntheticError))
				{
					m_context.Error(
						"dmui-test-client: synthetic fixtures failed: {}"sv,
						syntheticError);
					MarkInitializationIncomplete(
						"synthetic fixture clients",
						DMUI_RESULT_CALLBACK_FAILED);
					return false;
				}

				m_initializationStatus = InitializationStatus::kComplete;
				m_initializationStage = "complete";
				m_initializationResult = DMUI_RESULT_OK;
				m_context.Info(
					"dmui-test-client: initialization complete; "
					"registered stable-UI client (API {}.{})"sv,
					DMUI_VERSION_MAJOR(DMUI_API_VERSION_CURRENT),
					DMUI_VERSION_MINOR(DMUI_API_VERSION_CURRENT));
				return true;
			}

			void Stop() noexcept
			{
				m_dialogs.Stop();
			}

			[[nodiscard]] bool ActivatePresentationScenario(
				PresentationScenario a_scenario,
				std::string& a_error) noexcept
			{
				a_error.clear();
				if (m_initializationStatus != InitializationStatus::kComplete)
				{
					a_error =
						"The shared test suite is not completely registered.";
					return false;
				}
				if (m_presentationState.Active())
				{
					a_error = "A presentation scenario is already active.";
					return false;
				}
				if (PresentationPage(a_scenario) == DMUI_INVALID_PAGE_HANDLE)
				{
					a_error =
						"The presentation scenario does not have a registered page.";
					return false;
				}

				bool activated{ true };
				switch (a_scenario)
				{
				case PresentationScenario::kOverlay:
					m_resources.SeedPlot();
					m_overlay.ConfigurePresentation();
					activated =
						m_overlay.Configure() &&
						m_overlay.SetEnabled(true);
					break;
				case PresentationScenario::kNotification:
					activated = m_dialogs.PostPageNotification(
						DMUI_STATUS_SEVERITY_WARNING,
						"Shader cache rebuilt; one preset needs review.",
						30000);
					break;
				case PresentationScenario::kImage:
					m_resources.RequestPresentationImageUpdate();
					break;
				case PresentationScenario::kPlot:
					m_resources.SeedPlot();
					break;
				case PresentationScenario::kDialog:
					m_dialogs.RequestPresentationDialog();
					break;
				}
				if (!activated)
				{
					a_error = "Could not activate presentation scenario (result ";
					a_error += DMUI_ResultToString(m_context.Client().LastResult());
					a_error += ").";
					return false;
				}
				if (!m_presentationState.Activate(a_scenario))
				{
					a_error = "A presentation scenario is already active.";
					return false;
				}
				return true;
			}

			[[nodiscard]] DMUI_PageHandle PresentationPage(
				PresentationScenario a_scenario) const noexcept
			{
				if (a_scenario == PresentationScenario::kOverlay)
					return m_overlay.Page();
				DmuiTestFixtures::ExerciseKind exercise{};
				switch (a_scenario)
				{
				case PresentationScenario::kNotification:
				case PresentationScenario::kDialog:
					exercise =
						DmuiTestFixtures::ExerciseKind::kNotificationsAndDialogs;
					break;
				case PresentationScenario::kImage:
					exercise = DmuiTestFixtures::ExerciseKind::kImages;
					break;
				case PresentationScenario::kPlot:
					exercise = DmuiTestFixtures::ExerciseKind::kPlot;
					break;
				default:
					return DMUI_INVALID_PAGE_HANDLE;
				}
				const auto found = std::ranges::find(
					DmuiTestFixtures::kExercisePages,
					exercise,
					&DmuiTestFixtures::ExercisePage::kind);
				if (found == DmuiTestFixtures::kExercisePages.end())
					return DMUI_INVALID_PAGE_HANDLE;
				return m_exercisePages[static_cast<size_t>(
					std::distance(
						DmuiTestFixtures::kExercisePages.begin(),
						found))];
			}

			[[nodiscard]] bool ValidatePresentationCapture(
				std::string& a_error) const
			{
				const auto active = m_presentationState.Active();
				if (!active)
					return true;
				bool complete{};
				std::string_view expected;
				switch (*active)
				{
				case PresentationScenario::kOverlay:
					expected = "a drawn managed overlay and annotated plot";
					complete =
						m_overlay.DrawCount() > 0 &&
						m_resources.PlotCaptureComplete() &&
						m_overlay.Result() == DMUI_RESULT_OK;
					break;
				case PresentationScenario::kNotification:
					expected = "a successfully posted notification";
					complete = m_dialogs.NotificationCaptureComplete();
					break;
				case PresentationScenario::kImage:
					expected = "drawn CPU-created/updated and imported images";
					complete = m_resources.ImageCaptureComplete();
					break;
				case PresentationScenario::kPlot:
					expected = "a drawn annotated plot";
					complete = m_resources.PlotCaptureComplete();
					break;
				case PresentationScenario::kDialog:
					expected = "a successfully requested text-entry dialog";
					complete = m_dialogs.DialogCaptureComplete();
					break;
				}
				if (!complete)
				{
					a_error =
						"Presentation capture requires " +
						std::string{ expected } +
						"; inspect the fixture results or allow more frames.";
				}
				return complete;
			}

		private:
			void MarkInitializationIncomplete(
				std::string_view a_scope,
				DMUI_Result a_result) noexcept
			{
				if (m_initializationStatus != InitializationStatus::kIncomplete)
				{
					m_initializationStage = a_scope;
					m_initializationResult = a_result;
				}
				m_initializationStatus = InitializationStatus::kIncomplete;
			}

			[[nodiscard]] bool RegistrationFailure(
				std::string_view a_scope) noexcept
			{
				const auto result = m_context.Client().LastResult();
				MarkInitializationIncomplete(a_scope, result);
				m_context.Error(
					"dmui-test-client: initialization incomplete; "
					"registration={} result={} partial-registration={}"sv,
					a_scope,
					DMUI_ResultToString(result),
					m_exercisePages.front() != DMUI_INVALID_PAGE_HANDLE);
				return false;
			}

			void LogUnavailableOnce() noexcept
			{
				if (m_unavailableLogged.exchange(
						true,
						std::memory_order_acq_rel))
					return;
				auto& client = m_context.Client();
				if (!client.HostPresent())
				{
					m_context.Warning(
						"dmui-test-client: DearModdingUI host absent; "
						"registered nothing"sv);
				}
				else
				{
					m_context.Warning(
						"dmui-test-client: host incompatible ({}); "
						"registered nothing"sv,
						DMUI_ResultToString(client.LastResult()));
				}
			}

			void ObserveFrame() noexcept
			{
				++m_frameCount;
				auto& client = m_context.Client();
				if (const auto state = client.QueryState())
				{
					m_hostState = *state;
					m_dialogs.SetPostingAllowed(
						state->state == DMUI_HOST_STATE_READY &&
						client.UnavailableReason() == DMUI_UNAVAILABLE_NONE);
				}
				else
				{
					m_stateResult = client.LastResult();
					m_dialogs.SetPostingAllowed(false);
				}
				if (const auto visible = client.IsMenuVisible())
				{
					if (!*visible)
						++m_hiddenMenuObservations;
				}

				m_resources.ObserveFrame();
				m_dialogs.Observe(m_frameCount);
				m_overlay.SetFrameObservations(
					m_frameCount,
					m_hiddenMenuObservations,
					m_resources.ElapsedSeconds());
				m_overlay.Observe();
				m_hotkeys.Observe();
			}

			void OnPageActivity(const dmui::PageActivity& a_activity) noexcept
			{
				const auto isExercise = [this](DMUI_PageHandle a_page) {
					return a_page != DMUI_INVALID_PAGE_HANDLE &&
						std::ranges::find(m_exercisePages, a_page) !=
							m_exercisePages.end();
				};
				if (!isExercise(a_activity.previousPage) ||
					a_activity.previousPage == a_activity.activePage)
					return;
				LogSnapshot(
					a_activity.activePage == DMUI_INVALID_PAGE_HANDLE ?
						"exercise-menu-deactivated" :
						"exercise-page-left");
			}

			void DrawExercise(
				DmuiTestFixtures::ExerciseKind a_kind) noexcept
			{
				DmuiTestFixtures::DrawExerciseIntro(
					m_context.Client(),
					a_kind,
					OutcomeFor(a_kind),
					OutcomeObservation(a_kind));
				switch (a_kind)
				{
				case DmuiTestFixtures::ExerciseKind::kResults:
					DrawResults();
					break;
				case DmuiTestFixtures::ExerciseKind::kImages:
					m_resources.DrawImages();
					break;
				case DmuiTestFixtures::ExerciseKind::kOverlay:
					m_overlay.DrawControls();
					break;
				case DmuiTestFixtures::ExerciseKind::kInteractions:
					m_settings.Draw();
					m_hotkeys.Draw();
					break;
				case DmuiTestFixtures::ExerciseKind::kNotificationsAndDialogs:
					m_dialogs.Draw();
					break;
				case DmuiTestFixtures::ExerciseKind::kPlot:
					m_overlay.DrawOverlay();
					break;
				}
			}

			void DrawResults() noexcept
			{
				++m_settingsDraws;
				dmui::ui::TextWrapped(
					"This page uses only the official stable DMUI UI API. "
					"Expected: host ready, all service bits present, and "
					"UI ABI %u revision %u.",
					DMUI_UI_ABI_CURRENT,
					DMUI_UI_REVISION_CURRENT);
				dmui::ui::Text(
					"Host: %s | API %u.%u | UI ABI %u rev %u | services 0x%llX",
					Detail::HostStateName(m_hostState.state),
					DMUI_VERSION_MAJOR(DMUI_API_VERSION_CURRENT),
					DMUI_VERSION_MINOR(DMUI_API_VERSION_CURRENT),
					m_services.uiABI,
					m_services.uiRevision,
					m_services.supported);
				dmui::ui::Text(
					"Observer %llu | hidden-menu samples %llu | settings draws %llu",
					m_frameCount,
					m_hiddenMenuObservations,
					m_settingsDraws);
				dmui::ui::Text(
					"Initialization: %s | stage=%s | result=%s",
					Detail::InitializationStatusName(m_initializationStatus),
					m_initializationStage.c_str(),
					DMUI_ResultToString(m_initializationResult));
				dmui::ui::Text(
					"Last results: state=%s image=%s overlay=%s plot=%s "
					"notification=%s dialog=%s hotkey=%s services=%s",
					DMUI_ResultToString(m_stateResult),
					DMUI_ResultToString(m_resources.ImageResult()),
					DMUI_ResultToString(m_overlay.Result()),
					DMUI_ResultToString(m_resources.PlotResult()),
					DMUI_ResultToString(m_dialogs.NotificationResult()),
					DMUI_ResultToString(m_dialogs.DialogResult()),
					DMUI_ResultToString(m_hotkeys.Result()),
					DMUI_ResultToString(m_servicesResult));
				if (dmui::ui::Button("Log current results"))
					LogSnapshot("manual-button");
				(void)m_context.Client().DrawSectionHeader("Expected outcomes");
				(void)m_context.Client().DrawBulletText(
					"Unchanged clicks do not increment simulated saves; "
					"completed edits and effective resets do.");
				(void)m_context.Client().DrawBulletText(
					"Overlay and delayed-toast defaults yield during menus, "
					"console, text entry, and unsafe gameplay UI.");
				(void)m_context.Client().DrawBulletText(
					"Free overlay arrangement is interactive only while the "
					"host menu owns input.");
				(void)m_context.Client().DrawBulletText(
					"Rejected names preserve text. Confirm operations count "
					"only after COMPLETED; cancellation never counts.");
			}

			[[nodiscard]] DmuiTestFixtures::Outcome OutcomeFor(
				DmuiTestFixtures::ExerciseKind a_kind) const noexcept
			{
				using DmuiTestFixtures::ExerciseKind;
				using DmuiTestFixtures::Outcome;
				switch (a_kind)
				{
				case ExerciseKind::kResults:
					return m_initializationStatus == InitializationStatus::kComplete ?
						Outcome::kObserved :
						m_initializationStatus == InitializationStatus::kIncomplete ?
							Outcome::kFailed :
							Outcome::kUnexercised;
				case ExerciseKind::kImages:
					if (m_resources.ImageFailed())
						return Outcome::kFailed;
					return m_resources.InteractionEventCount() > 0 ?
						Outcome::kObserved :
						Outcome::kUnexercised;
				case ExerciseKind::kOverlay:
					if (m_overlay.Failed())
						return Outcome::kFailed;
					return m_overlay.ObservedEventCount() > 0 ?
						Outcome::kObserved :
						Outcome::kUnexercised;
				case ExerciseKind::kInteractions:
					if (m_hotkeys.Result() != DMUI_RESULT_OK)
						return Outcome::kFailed;
					return m_settings.SimulatedSaves() > 0 ||
							m_hotkeys.EdgeCount() > 0 ?
						Outcome::kObserved :
						Outcome::kUnexercised;
				case ExerciseKind::kNotificationsAndDialogs:
					if (m_dialogs.NotificationFailed() ||
						m_dialogs.DialogFailed())
						return Outcome::kFailed;
					return m_dialogs.ObservedEventCount() > 0 ?
						Outcome::kObserved :
						Outcome::kUnexercised;
				case ExerciseKind::kPlot:
					if (m_resources.PlotDraws() > 0 &&
						m_resources.PlotResult() != DMUI_RESULT_OK)
						return Outcome::kFailed;
					return m_resources.PlotDraws() > 0 ?
						Outcome::kObserved :
						Outcome::kUnexercised;
				default:
					return Outcome::kUnexercised;
				}
			}

			[[nodiscard]] std::string_view OutcomeObservation(
				DmuiTestFixtures::ExerciseKind a_kind) const noexcept
			{
				if (a_kind == DmuiTestFixtures::ExerciseKind::kInteractions &&
					!m_context.HostEnvironment().SupportsGameInputContexts())
					return "edits are active; F4SE game-input contexts are unavailable in preview";
				return OutcomeFor(a_kind) == DmuiTestFixtures::Outcome::kUnexercised ?
					"no explicit action or observation recorded" :
					"see the live counters and last-result fields below";
			}

			void LogSnapshot(std::string_view a_trigger) noexcept
			{
				const auto settings = m_settings.CurrentSnapshot();
				const auto images = m_resources.CurrentSnapshot();
				const auto overlay = m_overlay.CurrentSnapshot();
				const auto dialogs = m_dialogs.CurrentSnapshot();
				const auto hotkeyEdges = m_hotkeys.EdgeCount();
				++m_snapshotsLogged;
				m_context.Info(
					"dmui-test-client: snapshot trigger={} sequence={} "
					"initialization={} stage={} result={} overall=not-evaluated "
					"outcomes[hotkeys={},overlay={},image={},notifications={},"
					"edits={},dialogs={}] counts[hotkey-edges={},frame-demand={}/{},"
					"image={}/{}/{}/{}/{}/{},notifications={}/{}/{}/{},edits={}/{}/{},"
					"dialogs={}/{}/{}/{}/{}]"sv,
					a_trigger,
					m_snapshotsLogged,
					Detail::InitializationStatusName(m_initializationStatus),
					m_initializationStage,
					DMUI_ResultToString(m_initializationResult),
					hotkeyEdges ? "observed" : "unexercised",
					m_overlay.EventCount() ? "observed" : "unexercised",
					m_resources.ImageEventCount() ? "observed" : "unexercised",
					m_dialogs.NotificationEventCount() ? "observed" : "unexercised",
					m_settings.EventCount() ? "observed" : "unexercised",
					m_dialogs.EventCount() ? "observed" : "unexercised",
					hotkeyEdges,
					overlay.requests,
					overlay.releases,
					images.imports,
					images.releases,
					images.cycles,
					images.failures,
					images.cpuCreates,
					images.cpuUpdates,
					dialogs.pageNotifications,
					dialogs.notificationSchedules,
					dialogs.delayedNotifications,
					dialogs.suppressedNotifications,
					settings.textEvents,
					settings.sliderEvents,
					settings.multilineEvents,
					dialogs.dialogRequests,
					dialogs.dialogSubmissions,
					dialogs.confirmOperations,
					dialogs.textResolutions,
					dialogs.dialogCancellations);
				for (const auto& probe : m_hotkeys.Probes())
				{
					const auto exercised =
						probe.presses.load() || probe.releases.load();
					m_context.Info(
						"dmui-test-client: snapshot-hotkey sequence={} "
						"id={} outcome={} enabled={} effective={} state={} "
						"result={} down={} up={}"sv,
						m_snapshotsLogged,
						probe.id,
						exercised ? "observed" : "unexercised",
						probe.enabled,
						probe.binding.chord[0] ?
							probe.binding.chord :
							"none",
						Detail::BindingStateName(probe.binding.state),
						DMUI_ResultToString(probe.lastResult),
						probe.presses.load(),
						probe.releases.load());
				}
			}

			Detail::DiagnosticContext m_context;
			Detail::SettingsExercise m_settings;
			Detail::PresentationResources m_resources;
			Detail::OverlayExercise m_overlay;
			Detail::NotificationDialogExercise m_dialogs;
			Detail::HotkeyExercise m_hotkeys;
			std::atomic_bool m_unavailableLogged{};
			InitializationStatus m_initializationStatus{
				InitializationStatus::kPending
			};
			std::string m_initializationStage{ "pending" };
			DMUI_Result m_initializationResult{ DMUI_RESULT_OK };
			dmui::HostServices m_services{};
			DMUI_Result m_servicesResult{ DMUI_RESULT_OK };
			DMUI_HostStateInfo m_hostState{};
			DMUI_Result m_stateResult{ DMUI_RESULT_OK };
			std::array<
				DMUI_PageHandle,
				DmuiTestFixtures::kExercisePages.size()> m_exercisePages{};
			uint64_t m_snapshotsLogged{};
			uint64_t m_frameCount{};
			uint64_t m_hiddenMenuObservations{};
			uint64_t m_settingsDraws{};
			PresentationScenarioState m_presentationState;
			DmuiTestFixtures::SyntheticSettingsState m_syntheticSettings;
			std::vector<std::unique_ptr<dmui::Client>> m_syntheticClients;
		};
	}

	struct GeneralTestSuite::Impl
	{
		explicit Impl(Environment& a_environment) :
			state(a_environment)
		{}

		State state;
	};

	GeneralTestSuite::GeneralTestSuite(Environment& a_environment) :
		m_impl(std::make_unique<Impl>(a_environment))
	{}

	GeneralTestSuite::~GeneralTestSuite()
	{
		Stop();
	}

	bool GeneralTestSuite::Initialize() noexcept
	{
		return m_impl->state.Initialize();
	}

	bool GeneralTestSuite::ActivatePresentationScenario(
		PresentationScenario a_scenario,
		std::string& a_error) noexcept
	{
		return m_impl->state.ActivatePresentationScenario(a_scenario, a_error);
	}

	uint64_t GeneralTestSuite::PresentationPage(
		PresentationScenario a_scenario) const noexcept
	{
		return m_impl->state.PresentationPage(a_scenario);
	}

	bool GeneralTestSuite::ValidatePresentationCapture(
		std::string& a_error) const
	{
		return m_impl->state.ValidatePresentationCapture(a_error);
	}

	void GeneralTestSuite::Stop() noexcept
	{
		m_impl->state.Stop();
	}
}
