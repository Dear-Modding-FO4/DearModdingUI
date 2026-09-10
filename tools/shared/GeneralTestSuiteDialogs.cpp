#include "GeneralTestSuiteInternal.h"

#include <algorithm>

namespace DmuiTests::Detail
{
	using namespace std::chrono_literals;

	NotificationDialogExercise::NotificationDialogExercise(
		DiagnosticContext& a_context) noexcept :
		m_context(a_context)
	{}

	NotificationDialogExercise::~NotificationDialogExercise()
	{
		Stop();
	}

	void NotificationDialogExercise::Stop() noexcept
	{
		std::call_once(m_workerStopOnce, [this] {
			m_workerPostingAllowed.store(false, std::memory_order_release);
			std::jthread worker;
			{
				std::scoped_lock lock{ m_workerMutex };
				if (m_notificationWorker.joinable())
				{
					m_notificationWorker.request_stop();
					worker = std::move(m_notificationWorker);
				}
			}
			if (worker.joinable())
				worker.join();
			m_workerBusy.store(false, std::memory_order_release);
		});
	}

	void NotificationDialogExercise::SetPostingAllowed(bool a_allowed) noexcept
	{
		m_workerPostingAllowed.store(a_allowed, std::memory_order_release);
	}

	bool NotificationDialogExercise::PostPageNotification(
		DMUI_StatusSeverity a_severity,
		const char* a_message,
		uint32_t a_durationMilliseconds) noexcept
	{
		auto& client = m_context.Client();
		const auto posted = client.PostNotification(
			a_severity,
			a_message,
			a_durationMilliseconds);
		if (posted)
			++m_pageNotifications;
		m_notificationResult.store(
			client.LastResult(),
			std::memory_order_release);
		m_context.Info(
			"dmui-test-client: notification page-post={} result={} count={}"sv,
			posted,
			DMUI_ResultToString(m_notificationResult.load()),
			m_pageNotifications);
		return posted;
	}

	void NotificationDialogExercise::ScheduleDelayedNotification() noexcept
	{
		std::scoped_lock lock{ m_workerMutex };
		auto& client = m_context.Client();
		if (!m_workerPostingAllowed.load(std::memory_order_acquire) ||
			client.UnavailableReason() != DMUI_UNAVAILABLE_NONE)
		{
			++m_workerSuppressed;
			m_context.Info(
				"dmui-test-client: delayed notification "
				"schedule=suppressed unavailable-reason={} count={}"sv,
				static_cast<uint32_t>(client.UnavailableReason()),
				m_workerSuppressed.load());
			return;
		}

		auto expected = false;
		if (!m_workerBusy.compare_exchange_strong(
				expected,
				true,
				std::memory_order_acq_rel))
		{
			++m_workerBusyRejections;
			m_context.Info(
				"dmui-test-client: delayed notification "
				"schedule=busy-rejected count={}"sv,
				m_workerBusyRejections);
			return;
		}

		m_context.Info(
			"dmui-test-client: delayed notification schedule=accepted"sv);
		++m_notificationSchedules;
		m_notificationWorker = std::jthread([this](std::stop_token a_stop) {
			std::this_thread::sleep_for(650ms);
			{
				std::scoped_lock workerLock{ m_workerMutex };
				auto& workerClient = m_context.Client();
				if (a_stop.stop_requested() ||
					!m_workerPostingAllowed.load(std::memory_order_acquire) ||
					workerClient.UnavailableReason() != DMUI_UNAVAILABLE_NONE)
				{
					++m_workerSuppressed;
					m_context.Info(
						"dmui-test-client: delayed notification "
						"post=suppressed unavailable-reason={} count={}"sv,
						static_cast<uint32_t>(
							workerClient.UnavailableReason()),
						m_workerSuppressed.load());
					m_workerBusy.store(false, std::memory_order_release);
					return;
				}
				const auto posted = workerClient.PostNotification(
					DMUI_STATUS_SEVERITY_INFO,
					"DMUI Tests: delayed notification posted "
					"from a bounded worker thread.",
					4000);
				m_notificationResult.store(
					workerClient.LastResult(),
					std::memory_order_release);
				if (posted)
					++m_delayedNotifications;
				m_context.Info(
					"dmui-test-client: delayed notification "
					"post={} result={} count={}"sv,
					posted,
					DMUI_ResultToString(m_notificationResult.load()),
					m_delayedNotifications.load());
			}
			m_workerBusy.store(false, std::memory_order_release);
		});
	}

	void NotificationDialogExercise::Draw() noexcept
	{
		if (m_presentationDialogPending)
		{
			m_presentationDialogPending = false;
			(void)RequestTextDialog("Ultra Commonwealth");
		}
		auto& client = m_context.Client();
		(void)client.DrawSectionHeader("Notifications and dialogs");
		if (dmui::ui::Button("Post page notification"))
			(void)PostPageNotification(
				DMUI_STATUS_SEVERITY_SUCCESS,
				"DMUI Tests: page notification posted successfully.",
				3500);
		dmui::ui::SameLine();
		if (dmui::ui::Button("Schedule delayed any-thread notification"))
			ScheduleDelayedNotification();

		if (dmui::ui::Button("Request harmless confirm"))
			RequestConfirmDialog();
		dmui::ui::SameLine();
		if (dmui::ui::Button("Request validated text entry"))
			(void)RequestTextDialog();
		dmui::ui::SameLine();
		auto rejectWithoutMessage = m_rejectWithoutMessage;
		if (dmui::ui::Checkbox(
				"Reject with nullptr error",
				&rejectWithoutMessage))
			m_rejectWithoutMessage = rejectWithoutMessage;

		dmui::ui::Text(
			"dialog=%s id=%llu submitted=%llu accepted confirms=%llu "
			"text accepts=%llu rejects=%llu cancels=%llu duplicates ignored=%llu",
			DialogEventName(m_lastDialogEvent),
			m_activeSubmission,
			m_dialogSubmissions,
			m_confirmOperations,
			m_textAccepts,
			m_textRejects,
			m_dialogCancellations,
			m_duplicateSubmissions);
		dmui::ui::Text(
			"notifications page=%llu delayed=%llu busy-rejected=%llu "
			"suppressed=%llu",
			m_pageNotifications,
			m_delayedNotifications.load(),
			m_workerBusyRejections,
			m_workerSuppressed.load());
		dmui::ui::TextDisabled(
			"Text rejects empty, \"reject\", or an in-memory duplicate "
			"(initial duplicate: alpha). Rejection preserves text.");
	}

	void NotificationDialogExercise::RequestConfirmDialog() noexcept
	{
		++m_dialogRequestAttempts;
		if (m_dialog != DMUI_INVALID_DIALOG_HANDLE)
		{
			m_context.Info(
				"dmui-test-client: dialog confirm request ignored; "
				"another dialog is pending"sv);
			return;
		}
		const DMUI_DialogDescriptor descriptor{
			sizeof(DMUI_DialogDescriptor),
			DMUI_DIALOG_KIND_CONFIRM,
			"DMUI Tests confirmation",
			"Accepting performs one harmless in-memory operation after "
			"a short simulated in-flight delay.",
			"Accept",
			"Cancel",
			nullptr,
			nullptr,
			1
		};
		auto& client = m_context.Client();
		bool requested{};
		if (const auto handle = client.RequestDialog(descriptor))
		{
			requested = true;
			m_dialog = *handle;
			m_dialogProbe = DialogProbe::kConfirm;
			m_lastDialogEvent = DMUI_DIALOG_EVENT_PENDING;
			m_activeSubmission = 0;
			m_resolutionSent = false;
			++m_dialogRequests;
		}
		m_dialogResult = client.LastResult();
		if (requested)
		{
			m_context.Info(
				"dmui-test-client: dialog kind=confirm event=pending "
				"result={} requests={}"sv,
				DMUI_ResultToString(m_dialogResult),
				m_dialogRequests);
		}
		else
		{
			m_context.Error(
				"dmui-test-client: dialog kind=confirm "
				"request-failed result={}"sv,
				DMUI_ResultToString(m_dialogResult));
		}
	}

	bool NotificationDialogExercise::RequestTextDialog(
		const char* a_initialValue) noexcept
	{
		++m_dialogRequestAttempts;
		if (m_dialog != DMUI_INVALID_DIALOG_HANDLE)
		{
			m_context.Info(
				"dmui-test-client: dialog text request ignored; "
				"another dialog is pending"sv);
			return false;
		}
		const DMUI_DialogDescriptor descriptor{
			sizeof(DMUI_DialogDescriptor),
			DMUI_DIALOG_KIND_TEXT_ENTRY,
			"DMUI Tests name",
			"Enter a unique in-memory name.",
			"Validate",
			"Cancel",
			"Try alpha, reject, or an empty value.",
			a_initialValue,
			96
		};
		auto& client = m_context.Client();
		bool requested{};
		if (const auto handle = client.RequestDialog(descriptor))
		{
			requested = true;
			m_dialog = *handle;
			m_dialogProbe = DialogProbe::kText;
			m_lastDialogEvent = DMUI_DIALOG_EVENT_PENDING;
			m_activeSubmission = 0;
			m_resolutionSent = false;
			++m_dialogRequests;
		}
		m_dialogResult = client.LastResult();
		if (requested)
		{
			m_context.Info(
				"dmui-test-client: dialog kind=text event=pending "
				"result={} requests={}"sv,
				DMUI_ResultToString(m_dialogResult),
				m_dialogRequests);
		}
		else
		{
			m_context.Error(
				"dmui-test-client: dialog kind=text request-failed result={}"sv,
				DMUI_ResultToString(m_dialogResult));
		}
		return requested;
	}

	void NotificationDialogExercise::Observe(uint64_t a_frameCount) noexcept
	{
		PollDialog(a_frameCount);
	}

	void NotificationDialogExercise::PollDialog(
		uint64_t a_frameCount) noexcept
	{
		if (m_dialog == DMUI_INVALID_DIALOG_HANDLE)
			return;
		std::string text;
		auto& client = m_context.Client();
		const auto event = client.PollDialogEvent(m_dialog, text);
		m_dialogResult = client.LastResult();
		if (!event)
			return;
		m_lastDialogEvent = event->kind;
		switch (event->kind)
		{
		case DMUI_DIALOG_EVENT_SUBMITTED:
			if (event->submissionId != m_activeSubmission)
			{
				if (event->submissionId <= m_highestSubmission)
				{
					if (event->submissionId != m_lastDuplicateSubmission)
					{
						m_lastDuplicateSubmission = event->submissionId;
						++m_duplicateSubmissions;
						m_context.Info(
							"dmui-test-client: dialog event=duplicate "
							"submission={} duplicates={}"sv,
							event->submissionId,
							m_duplicateSubmissions);
					}
					return;
				}
				m_highestSubmission = event->submissionId;
				m_activeSubmission = event->submissionId;
				m_submittedText = std::move(text);
				m_resolveAtFrame = a_frameCount + kDialogDelayFrames;
				m_resolutionSent = false;
				++m_dialogSubmissions;
				m_context.Info(
					"dmui-test-client: dialog event=submitted "
					"submission={} submissions={}"sv,
					m_activeSubmission,
					m_dialogSubmissions);
			}
			else if (m_resolutionSent)
			{
				if (event->submissionId != m_lastDuplicateSubmission)
				{
					m_lastDuplicateSubmission = event->submissionId;
					++m_duplicateSubmissions;
					m_context.Info(
						"dmui-test-client: dialog event=duplicate "
						"submission={} duplicates={}"sv,
						event->submissionId,
						m_duplicateSubmissions);
				}
				return;
			}
			if (a_frameCount >= m_resolveAtFrame && !m_resolutionSent)
				ResolveSubmission();
			break;
		case DMUI_DIALOG_EVENT_CANCELLED:
			++m_dialogCancellations;
			m_context.Info(
				"dmui-test-client: dialog event=cancelled cancellations={}"sv,
				m_dialogCancellations);
			ClearDialog();
			break;
		case DMUI_DIALOG_EVENT_COMPLETED:
			if (m_dialogProbe == DialogProbe::kConfirm)
				++m_confirmOperations;
			else if (m_dialogProbe == DialogProbe::kText)
			{
				if (m_acceptedNames.size() < kMaximumAcceptedNames)
					m_acceptedNames.push_back(m_submittedText);
				++m_textAccepts;
			}
			m_context.Info(
				"dmui-test-client: dialog event=completed "
				"confirm-operations={} text-accepts={} text-rejects={} "
				"cancellations={}"sv,
				m_confirmOperations,
				m_textAccepts,
				m_textRejects,
				m_dialogCancellations);
			ClearDialog();
			break;
		default:
			break;
		}
	}

	void NotificationDialogExercise::ResolveSubmission() noexcept
	{
		bool accepted{ true };
		const char* error{};
		const auto submission = m_activeSubmission;
		if (m_dialogProbe == DialogProbe::kText)
		{
			if (m_submittedText.empty())
			{
				accepted = false;
				error = "A non-empty name is required.";
			}
			else if (m_submittedText == "reject" ||
				std::ranges::find(m_acceptedNames, m_submittedText) !=
					m_acceptedNames.end())
			{
				accepted = false;
				error = "That sentinel or in-memory name is already used.";
			}
			else if (m_acceptedNames.size() >= kMaximumAcceptedNames)
			{
				accepted = false;
				error = "The bounded in-memory name list is full.";
			}
		}
		if (!accepted && m_rejectWithoutMessage)
			error = nullptr;

		auto& client = m_context.Client();
		const auto resolved = client.ResolveDialogSubmission(
			m_dialog,
			m_activeSubmission,
			accepted,
			error);
		if (resolved)
		{
			m_resolutionSent = true;
			if (!accepted)
			{
				++m_textRejects;
				m_activeSubmission = 0;
			}
		}
		m_dialogResult = client.LastResult();
		m_context.Info(
			"dmui-test-client: dialog event=resolution "
			"submission={} accepted={} result={} rejects={}"sv,
			submission,
			accepted,
			DMUI_ResultToString(m_dialogResult),
			m_textRejects);
	}

	void NotificationDialogExercise::ClearDialog() noexcept
	{
		m_dialog = DMUI_INVALID_DIALOG_HANDLE;
		m_dialogProbe = DialogProbe::kNone;
		m_activeSubmission = 0;
		m_submittedText.clear();
		m_resolutionSent = false;
	}

	void NotificationDialogExercise::RequestPresentationDialog() noexcept
	{
		m_presentationDialogPending = true;
	}

	uint64_t NotificationDialogExercise::EventCount() const noexcept
	{
		return
			m_dialogRequestAttempts + m_dialogSubmissions +
			m_dialogCancellations + m_confirmOperations +
			m_textAccepts + m_textRejects;
	}

	uint64_t NotificationDialogExercise::NotificationEventCount() const noexcept
	{
		return
			m_pageNotifications + m_notificationSchedules +
			m_delayedNotifications.load() + m_workerBusyRejections +
			m_workerSuppressed.load();
	}

	bool NotificationDialogExercise::NotificationCaptureComplete() const noexcept
	{
		return m_pageNotifications > 0 &&
			m_notificationResult.load() == DMUI_RESULT_OK;
	}

	bool NotificationDialogExercise::DialogCaptureComplete() const noexcept
	{
		return m_dialogRequests > 0 &&
			m_dialog != DMUI_INVALID_DIALOG_HANDLE &&
			m_dialogResult == DMUI_RESULT_OK;
	}

	bool NotificationDialogExercise::NotificationFailed() const noexcept
	{
		return m_notificationSchedules > 0 &&
			m_notificationResult.load() != DMUI_RESULT_OK;
	}

	bool NotificationDialogExercise::DialogFailed() const noexcept
	{
		return m_dialogRequestAttempts > 0 &&
			m_dialogResult != DMUI_RESULT_OK;
	}

	uint64_t NotificationDialogExercise::ObservedEventCount() const noexcept
	{
		return
			m_pageNotifications +
			m_delayedNotifications.load() +
			m_dialogSubmissions +
			m_dialogCancellations;
	}

	DMUI_Result NotificationDialogExercise::NotificationResult() const noexcept
	{
		return m_notificationResult.load();
	}

	DMUI_Result NotificationDialogExercise::DialogResult() const noexcept
	{
		return m_dialogResult;
	}

	NotificationDialogExercise::Snapshot
		NotificationDialogExercise::CurrentSnapshot() const noexcept
	{
		return {
			m_pageNotifications,
			m_notificationSchedules,
			m_delayedNotifications.load(),
			m_workerSuppressed.load() + m_workerBusyRejections,
			m_dialogRequests,
			m_dialogSubmissions,
			m_confirmOperations,
			m_textAccepts + m_textRejects,
			m_dialogCancellations
		};
	}
}
