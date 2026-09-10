#pragma once

#include "GeneralTestSuite.h"
#include "TestHotkeyDescriptors.h"

#include <GeneralTestFixtures.h>

#include <DearModdingUI/Client.h>

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#undef ERROR

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace DmuiTests::Detail
{
	namespace dmui = ::dmui;
	using Microsoft::WRL::ComPtr;
	using namespace std::literals;

	inline constexpr DMUI_HostServices kRequiredServices{
		DMUI_HOST_SERVICE_FRAME_CONTROL |
		DMUI_HOST_SERVICE_EDIT_LIFECYCLE |
		DMUI_HOST_SERVICE_CONTEXTUAL_HOTKEYS |
		DMUI_HOST_SERVICE_IMAGE_RESOURCES |
		DMUI_HOST_SERVICE_MANAGED_OVERLAYS |
		DMUI_HOST_SERVICE_NOTIFICATIONS |
		DMUI_HOST_SERVICE_ANNOTATED_PLOTS |
		DMUI_HOST_SERVICE_DIALOGS |
		DMUI_HOST_SERVICE_PIXEL_IMAGES
	};

	enum class InitializationStatus
	{
		kPending,
		kComplete,
		kIncomplete,
		kUnavailable
	};

	[[nodiscard]] const char* HostStateName(DMUI_HostState a_state) noexcept;
	[[nodiscard]] const char* BindingStateName(
		DMUI_HotkeyBindingState a_state) noexcept;
	[[nodiscard]] const char* DialogEventName(
		DMUI_DialogEventKind a_kind) noexcept;
	[[nodiscard]] const char* InitializationStatusName(
		InitializationStatus a_status) noexcept;
	[[nodiscard]] const char* AnchorName(DMUI_OverlayAnchor a_anchor) noexcept;

	class DiagnosticContext final
	{
	public:
		explicit DiagnosticContext(Environment& a_environment);

		[[nodiscard]] dmui::Client& Client() noexcept;
		[[nodiscard]] const dmui::Client& Client() const noexcept;
		[[nodiscard]] Environment& HostEnvironment() noexcept;
		[[nodiscard]] const Environment& HostEnvironment() const noexcept;

		template <class... Args>
		void Log(
			LogLevel a_level,
			std::string_view a_format,
			Args&&... a_args) noexcept
		{
			try
			{
				m_environment.Log(
					a_level,
					std::vformat(
						a_format,
						std::make_format_args(a_args...)));
			}
			catch (const std::format_error&)
			{
				m_environment.Log(
					LogLevel::kError,
					"dmui-test-client: invalid internal log format follows");
				m_environment.Log(LogLevel::kError, a_format);
			}
		}

		template <class... Args>
		void Info(std::string_view a_format, Args&&... a_args) noexcept
		{
			Log(
				LogLevel::kInfo,
				a_format,
				std::forward<Args>(a_args)...);
		}

		template <class... Args>
		void Warning(std::string_view a_format, Args&&... a_args) noexcept
		{
			Log(
				LogLevel::kWarning,
				a_format,
				std::forward<Args>(a_args)...);
		}

		template <class... Args>
		void Error(std::string_view a_format, Args&&... a_args) noexcept
		{
			Log(
				LogLevel::kError,
				a_format,
				std::forward<Args>(a_args)...);
		}

	private:
		Environment& m_environment;
		dmui::Client m_client;
	};

	struct EditCounters
	{
		uint64_t changed{};
		uint64_t completed{};
		uint64_t resets{};
		uint64_t saves{};
		bool dirty{};
	};

	class SettingsExercise final
	{
	public:
		struct Snapshot
		{
			uint64_t textEvents;
			uint64_t sliderEvents;
			uint64_t multilineEvents;
		};

		explicit SettingsExercise(DiagnosticContext& a_context) noexcept;
		void Draw() noexcept;
		[[nodiscard]] uint64_t EventCount() const noexcept;
		[[nodiscard]] uint64_t SimulatedSaves() const noexcept;
		[[nodiscard]] Snapshot CurrentSnapshot() const noexcept;

	private:
		void RecordEdit(
			EditCounters& a_counters,
			std::string_view a_id,
			bool a_changed,
			bool a_completed) noexcept;
		template <class DrawCallback, class ResetCallback>
		[[nodiscard]] bool DrawEditableRow(
			const char* a_id,
			const char* a_label,
			const char* a_description,
			bool a_isDefault,
			DrawCallback&& a_draw,
			EditCounters& a_counters,
			ResetCallback&& a_reset);
		template <size_t N>
		static void CopyText(
			std::array<char, N>& a_destination,
			std::string_view a_text) noexcept;

		DiagnosticContext& m_context;
		std::array<char, 96> m_shortText{
			's', 'm', 'o', 'k', 'e', '\0'
		};
		std::array<char, 256> m_multiline{
			'l', 'i', 'n', 'e', ' ', 'o', 'n', 'e', '\n',
			'l', 'i', 'n', 'e', ' ', 't', 'w', 'o', '\0'
		};
		float m_sliderValue{ 50.0f };
		EditCounters m_textEdits;
		EditCounters m_sliderEdits;
		EditCounters m_multilineEdits;
		uint64_t m_simulatedSaves{};
	};

	struct HotkeyProbe
	{
		const char* id{};
		const char* name{};
		const char* suggested{};
		DMUI_HotkeyContextPolicy policy{};
		DMUI_HotkeyActionHandle handle{ DMUI_INVALID_HOTKEY_ACTION_HANDLE };
		bool enabled{ true };
		std::atomic<uint64_t> presses{};
		std::atomic<uint64_t> releases{};
		DMUI_HotkeyBindingInfo binding{};
		DMUI_Result lastResult{ DMUI_RESULT_OK };
	};

	class HotkeyExercise final
	{
	public:
		explicit HotkeyExercise(DiagnosticContext& a_context) noexcept;
		[[nodiscard]] bool Register(
			std::function<void()> a_toggleOverlay,
			std::function<void()> a_scheduleNotification,
			std::string& a_failedId,
			DMUI_Result& a_result) noexcept;
		void Observe() noexcept;
		void Draw() noexcept;
		[[nodiscard]] uint64_t EdgeCount() const noexcept;
		[[nodiscard]] DMUI_Result Result() const noexcept;
		[[nodiscard]] const std::array<
			HotkeyProbe,
			kHotkeyDescriptors.size()>& Probes() const noexcept;

	private:
		void OnHotkey(size_t a_index, bool a_pressed) noexcept;

		DiagnosticContext& m_context;
		std::array<HotkeyProbe, kHotkeyDescriptors.size()> m_probes{};
		std::function<void()> m_toggleOverlay;
		std::function<void()> m_scheduleNotification;
		DMUI_Result m_result{ DMUI_RESULT_OK };
	};

	class PresentationResources final
	{
	public:
		struct Snapshot
		{
			uint64_t imports;
			uint64_t releases;
			uint64_t cycles;
			uint64_t failures;
			uint64_t cpuCreates;
			uint64_t cpuUpdates;
		};

		explicit PresentationResources(DiagnosticContext& a_context) noexcept;
		~PresentationResources();

		void ObserveFrame() noexcept;
		void DrawImages() noexcept;
		void DrawPlot(const char* a_id) noexcept;
		void SeedPlot() noexcept;
		void DrawOverlayImages() noexcept;
		[[nodiscard]] uint64_t ImageEventCount() const noexcept;
		[[nodiscard]] uint64_t InteractionEventCount() const noexcept;
		[[nodiscard]] bool ImageFailed() const noexcept;
		[[nodiscard]] bool ImageCaptureComplete() const noexcept;
		[[nodiscard]] bool PlotCaptureComplete() const noexcept;
		[[nodiscard]] uint64_t PlotDraws() const noexcept;
		[[nodiscard]] DMUI_Result ImageResult() const noexcept;
		[[nodiscard]] DMUI_Result CpuImageResult() const noexcept;
		[[nodiscard]] DMUI_Result PlotResult() const noexcept;
		[[nodiscard]] double ElapsedSeconds() const noexcept;
		[[nodiscard]] Snapshot CurrentSnapshot() const noexcept;
		void RequestPresentationImageUpdate() noexcept;

	private:
		void RefreshImportedImage() noexcept;
		void ReleaseImportedImage(std::string_view a_reason) noexcept;
		void LogImageFailure(
			std::string_view a_scope,
			DMUI_Result a_result) noexcept;
		void CreateImportedImage() noexcept;
		void QueueImportedImage(bool a_large) noexcept;
		void FillCpuPixels(
			std::array<uint8_t, 80u * 64u * 4u>& a_pixels,
			uint32_t a_width,
			uint32_t a_height,
			uint32_t a_step) noexcept;
		[[nodiscard]] DMUI_ImageDescriptor CpuImageDescriptor(
			const std::array<uint8_t, 80u * 64u * 4u>& a_pixels,
			uint32_t a_width,
			uint32_t a_height) noexcept;
		void RefreshCpuImage() noexcept;
		void UpdateCpuImage() noexcept;
		void QueueCpuImage() noexcept;
		void ReleaseAfterQueuedDraw() noexcept;
		void RequestImageCycle() noexcept;

		static constexpr uint32_t kImageExtent{ 64 };
		static constexpr size_t kSampleCount{ 120 };
		DiagnosticContext& m_context;
		std::chrono::steady_clock::time_point m_startTime{};
		std::chrono::steady_clock::time_point m_previousFrame{};
		double m_elapsedSeconds{};
		std::array<float, kSampleCount> m_samples{};
		size_t m_sampleOffset{};
		uint64_t m_plotDraws{};
		DMUI_Result m_plotResult{ DMUI_RESULT_OK };
		bool m_plotSeeded{};
		ComPtr<ID3D11Device> m_imageDevice;
		ComPtr<ID3D11Texture2D> m_imageTexture;
		ComPtr<ID3D11ShaderResourceView> m_imageView;
		std::optional<dmui::ImageResource> m_image;
		std::atomic_bool m_recreateImage{};
		bool m_deviceWaitingLogged{};
		uint64_t m_imageImportCount{};
		uint64_t m_imageDrawCount{};
		uint64_t m_imageReleaseCount{};
		uint64_t m_imageCycleRequests{};
		uint64_t m_imageFailureCount{};
		uint64_t m_deviceChangeCount{};
		bool m_imageFailureActive{};
		std::string_view m_lastImageFailureScope;
		DMUI_Result m_lastImageFailureResult{ DMUI_RESULT_OK };
		uint64_t m_imageGeneration{};
		DMUI_ImageStatus m_imageStatus{ DMUI_IMAGE_STATUS_RELEASED };
		DMUI_Result m_imageResult{ DMUI_RESULT_OK };
		std::optional<dmui::ImageResource> m_cpuImage;
		uint32_t m_cpuImageStep{};
		uint32_t m_cpuImageWidth{};
		uint32_t m_cpuImageHeight{};
		uint64_t m_cpuImageCreateCount{};
		uint64_t m_cpuImageUpdateCount{};
		uint64_t m_cpuImageDrawCount{};
		uint64_t m_cpuImageReleaseCount{};
		DMUI_Result m_cpuImageResult{ DMUI_RESULT_OK };
		bool m_presentationImageUpdatePending{};
	};

	class OverlayExercise final
	{
	public:
		struct Snapshot
		{
			uint64_t requests;
			uint64_t releases;
		};

		OverlayExercise(
			DiagnosticContext& a_context,
			PresentationResources& a_resources) noexcept;
		[[nodiscard]] bool Register(std::string_view a_categoryId) noexcept;
		[[nodiscard]] bool Configure() noexcept;
		[[nodiscard]] bool SetEnabled(bool a_enabled) noexcept;
		void Toggle() noexcept;
		void Observe() noexcept;
		void DrawControls() noexcept;
		void DrawOverlay() noexcept;
		void ConfigurePresentation() noexcept;
		[[nodiscard]] DMUI_PageHandle Page() const noexcept;
		[[nodiscard]] uint64_t EventCount() const noexcept;
		[[nodiscard]] uint64_t ObservedEventCount() const noexcept;
		[[nodiscard]] bool Failed() const noexcept;
		[[nodiscard]] uint64_t DrawCount() const noexcept;
		[[nodiscard]] DMUI_Result Result() const noexcept;
		[[nodiscard]] Snapshot CurrentSnapshot() const noexcept;
		void SetFrameObservations(
			uint64_t a_frameCount,
			uint64_t a_hiddenMenuObservations,
			double a_elapsedSeconds) noexcept;

	private:
		template <class DrawCallback>
		[[nodiscard]] bool DrawSimpleRow(
			const char* a_id,
			const char* a_label,
			const char* a_description,
			DrawCallback&& a_draw);
		[[nodiscard]] bool DrawFloatRow(
			const char* a_id,
			const char* a_label,
			float& a_value,
			float a_minimum,
			float a_maximum) noexcept;

		DiagnosticContext& m_context;
		PresentationResources& m_resources;
		DMUI_PageHandle m_page{ DMUI_INVALID_PAGE_HANDLE };
		DMUI_ManagedOverlayOptions m_options{};
		DMUI_ManagedOverlayPlacement m_placement{};
		bool m_enabled{};
		uint64_t m_demandAttempts{};
		uint64_t m_frameRequests{};
		uint64_t m_frameReleases{};
		uint64_t m_arrangementCompletions{};
		uint64_t m_draws{};
		uint64_t m_frameCount{};
		uint64_t m_hiddenMenuObservations{};
		double m_elapsedSeconds{};
		DMUI_Result m_result{ DMUI_RESULT_OK };
	};

	enum class DialogProbe
	{
		kNone,
		kConfirm,
		kText
	};

	class NotificationDialogExercise final
	{
	public:
		struct Snapshot
		{
			uint64_t pageNotifications;
			uint64_t notificationSchedules;
			uint64_t delayedNotifications;
			uint64_t suppressedNotifications;
			uint64_t dialogRequests;
			uint64_t dialogSubmissions;
			uint64_t confirmOperations;
			uint64_t textResolutions;
			uint64_t dialogCancellations;
		};

		explicit NotificationDialogExercise(DiagnosticContext& a_context) noexcept;
		~NotificationDialogExercise();

		void Stop() noexcept;
		void SetPostingAllowed(bool a_allowed) noexcept;
		[[nodiscard]] bool PostPageNotification(
			DMUI_StatusSeverity a_severity,
			const char* a_message,
			uint32_t a_durationMilliseconds) noexcept;
		void ScheduleDelayedNotification() noexcept;
		void Draw() noexcept;
		void Observe(uint64_t a_frameCount) noexcept;
		void RequestPresentationDialog() noexcept;
		[[nodiscard]] uint64_t EventCount() const noexcept;
		[[nodiscard]] uint64_t NotificationEventCount() const noexcept;
		[[nodiscard]] bool NotificationCaptureComplete() const noexcept;
		[[nodiscard]] bool DialogCaptureComplete() const noexcept;
		[[nodiscard]] bool NotificationFailed() const noexcept;
		[[nodiscard]] bool DialogFailed() const noexcept;
		[[nodiscard]] uint64_t ObservedEventCount() const noexcept;
		[[nodiscard]] DMUI_Result NotificationResult() const noexcept;
		[[nodiscard]] DMUI_Result DialogResult() const noexcept;
		[[nodiscard]] Snapshot CurrentSnapshot() const noexcept;

	private:
		void RequestConfirmDialog() noexcept;
		[[nodiscard]] bool RequestTextDialog(
			const char* a_initialValue = "") noexcept;
		void PollDialog(uint64_t a_frameCount) noexcept;
		void ResolveSubmission() noexcept;
		void ClearDialog() noexcept;

		static constexpr uint64_t kDialogDelayFrames{ 18 };
		static constexpr size_t kMaximumAcceptedNames{ 32 };
		DiagnosticContext& m_context;
		std::mutex m_workerMutex;
		std::once_flag m_workerStopOnce;
		std::jthread m_notificationWorker;
		std::atomic_bool m_workerBusy{};
		std::atomic_bool m_workerPostingAllowed{};
		std::atomic<uint64_t> m_delayedNotifications{};
		std::atomic<DMUI_Result> m_notificationResult{ DMUI_RESULT_OK };
		uint64_t m_pageNotifications{};
		uint64_t m_notificationSchedules{};
		uint64_t m_workerBusyRejections{};
		std::atomic<uint64_t> m_workerSuppressed{};
		DMUI_DialogHandle m_dialog{ DMUI_INVALID_DIALOG_HANDLE };
		DialogProbe m_dialogProbe{ DialogProbe::kNone };
		DMUI_DialogEventKind m_lastDialogEvent{ DMUI_DIALOG_EVENT_PENDING };
		uint64_t m_activeSubmission{};
		uint64_t m_highestSubmission{};
		uint64_t m_resolveAtFrame{};
		uint64_t m_lastDuplicateSubmission{};
		bool m_resolutionSent{};
		bool m_rejectWithoutMessage{};
		std::string m_submittedText;
		std::vector<std::string> m_acceptedNames{ "alpha" };
		uint64_t m_dialogSubmissions{};
		uint64_t m_dialogRequests{};
		uint64_t m_dialogRequestAttempts{};
		uint64_t m_duplicateSubmissions{};
		uint64_t m_confirmOperations{};
		uint64_t m_textAccepts{};
		uint64_t m_textRejects{};
		uint64_t m_dialogCancellations{};
		DMUI_Result m_dialogResult{ DMUI_RESULT_OK };
		bool m_presentationDialogPending{};
	};
}
