#include <DearModdingUI/Client.h>

#include "HotkeyDescriptors.h"

#if defined(IMGUI_VERSION) || defined(IMGUI_VERSION_NUM)
#error "dmui-forwarding-smoke must use forwarding declarations, never real Dear ImGui headers"
#endif

#include <F4SE/F4SE.h>
#include <RE/B/BSGraphics.h>
#include <REX/REX.h>

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#undef ERROR

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace DmuiForwardingSmoke
{
	using namespace std::chrono_literals;
	using namespace std::literals;
	using Microsoft::WRL::ComPtr;

	namespace
	{
		constexpr DMUI_HostServices kRequiredServices{
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
		constexpr uint32_t kImageExtent{ 64 };
		constexpr size_t kSampleCount{ 120 };
		constexpr uint64_t kDialogDelayFrames{ 18 };

		enum class DialogProbe
		{
			kNone,
			kConfirm,
			kText
		};

		enum class InitializationStatus
		{
			kPending,
			kComplete,
			kIncomplete,
			kUnavailable
		};

		struct EditCounters
		{
			uint64_t changed{};
			uint64_t completed{};
			uint64_t resets{};
			uint64_t saves{};
			bool dirty{};
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

		[[nodiscard]] const char* HostStateName(DMUI_HostState a_state) noexcept
		{
			switch (a_state)
			{
			case DMUI_HOST_STATE_NOT_INITIALIZED:
				return "not initialized";
			case DMUI_HOST_STATE_WAITING_FOR_PRESENT:
				return "waiting for Present";
			case DMUI_HOST_STATE_INITIALIZING:
				return "initializing";
			case DMUI_HOST_STATE_READY:
				return "ready";
			case DMUI_HOST_STATE_UNAVAILABLE:
				return "unavailable";
			default:
				return "unknown";
			}
		}

		[[nodiscard]] const char* BindingStateName(
			DMUI_HotkeyBindingState a_state) noexcept
		{
			switch (a_state)
			{
			case DMUI_HOTKEY_BINDING_BOUND:
				return "bound";
			case DMUI_HOTKEY_BINDING_UNBOUND_USER:
				return "unbound by user";
			case DMUI_HOTKEY_BINDING_UNBOUND_DEFAULT_CONFLICT:
				return "default conflict";
			case DMUI_HOTKEY_BINDING_UNBOUND_NEVER_SET:
				return "none";
			case DMUI_HOTKEY_BINDING_UNBOUND_OVERRIDE_CONFLICT:
				return "override conflict";
			case DMUI_HOTKEY_BINDING_UNBOUND_INVALID_OVERRIDE:
				return "invalid override";
			default:
				return "unknown";
			}
		}

		[[nodiscard]] const char* DialogEventName(
			DMUI_DialogEventKind a_kind) noexcept
		{
			switch (a_kind)
			{
			case DMUI_DIALOG_EVENT_PENDING:
				return "PENDING";
			case DMUI_DIALOG_EVENT_SUBMITTED:
				return "SUBMITTED";
			case DMUI_DIALOG_EVENT_CANCELLED:
				return "CANCELLED";
			case DMUI_DIALOG_EVENT_COMPLETED:
				return "COMPLETED";
			default:
				return "NONE";
			}
		}

		[[nodiscard]] const char* InitializationStatusName(
			InitializationStatus a_status) noexcept
		{
			switch (a_status)
			{
			case InitializationStatus::kPending:
				return "pending";
			case InitializationStatus::kComplete:
				return "complete";
			case InitializationStatus::kIncomplete:
				return "incomplete";
			case InitializationStatus::kUnavailable:
				return "unavailable";
			default:
				return "unknown";
			}
		}

		[[nodiscard]] const char* AnchorName(DMUI_OverlayAnchor a_anchor) noexcept
		{
			switch (a_anchor)
			{
			case DMUI_OVERLAY_ANCHOR_TOP_LEFT:
				return "Top left";
			case DMUI_OVERLAY_ANCHOR_TOP_RIGHT:
				return "Top right";
			case DMUI_OVERLAY_ANCHOR_BOTTOM_LEFT:
				return "Bottom left";
			case DMUI_OVERLAY_ANCHOR_BOTTOM_RIGHT:
				return "Bottom right";
			case DMUI_OVERLAY_ANCHOR_FREE:
				return "Free";
			default:
				return "Unknown";
			}
		}

		class RendererDataLock
		{
		public:
			explicit RendererDataLock(RE::BSGraphics::RendererData& a_data) noexcept :
				lock_(std::addressof(a_data.rendererLock.criticalSection))
			{
				REX::W32::EnterCriticalSection(lock_);
			}

			~RendererDataLock() noexcept
			{
				REX::W32::LeaveCriticalSection(lock_);
			}

			RendererDataLock(const RendererDataLock&) = delete;
			RendererDataLock& operator=(const RendererDataLock&) = delete;

		private:
			REX::W32::CRITICAL_SECTION* lock_;
		};

		class State
		{
		public:
			State() :
				client_(
					"dearmodding.forwarding-smoke",
					"Forwarding Smoke Test",
					dmui::Version{ 0, 1 },
					dmui::kForwardingClient,
					"test-tube",
					{},
					{
						.requiredServices = kRequiredServices,
						.minimumForwardingVersion =
							DMUI_FORWARDING_VERSION_CURRENT
					})
			{
				samples_.fill(0.0f);
				overlayOptions_.structSize = sizeof(overlayOptions_);
				overlayOptions_.anchor = DMUI_OVERLAY_ANCHOR_TOP_RIGHT;
				overlayOptions_.offset = { 28.0f, 28.0f };
				overlayOptions_.minimumSize = { 360.0f, 220.0f };
				overlayOptions_.maximumSize = { 640.0f, 520.0f };
				overlayOptions_.opacity = 0.88f;
				overlayOptions_.contentScale = 1.0f;
				overlayOptions_.backgroundVisible = 1;
				overlayOptions_.borderVisible = 1;
				overlayOptions_.allowArrangement = 1;

				for (size_t index = 0; index < kHotkeyDescriptors.size(); ++index)
				{
					const auto& descriptor = kHotkeyDescriptors[index];
					SetHotkey(
						index,
						descriptor.id,
						descriptor.name,
						descriptor.suggested,
						descriptor.policy);
				}
			}

			[[nodiscard]] bool Initialize() noexcept
			{
				const auto connected = client_.Connect();
				initializationResult_ = client_.LastResult();
				REX::INFO(
					"dmui-forwarding-smoke: preflight connect={} result={} "
					"host-present={} unavailable-reason={} required-services=0x{:X} "
					"minimum-forwarding={}.{}"sv,
					connected,
					DMUI_ResultToString(initializationResult_),
					client_.HostPresent(),
					static_cast<uint32_t>(client_.UnavailableReason()),
					static_cast<uint64_t>(kRequiredServices),
					DMUI_VERSION_MAJOR(DMUI_FORWARDING_VERSION_CURRENT),
					DMUI_VERSION_MINOR(DMUI_FORWARDING_VERSION_CURRENT));
				if (!connected)
				{
					initializationStatus_ = InitializationStatus::kUnavailable;
					initializationStage_ = "host preflight";
					LogUnavailableOnce();
					return true;
				}

				if (const auto services = client_.QueryServices())
				{
					services_ = *services;
					servicesResult_ = client_.LastResult();
					REX::INFO(
						"dmui-forwarding-smoke: service preflight result={} "
						"forwarding={}.{} supported=0x{:X} required=0x{:X}"sv,
						DMUI_ResultToString(servicesResult_),
						DMUI_VERSION_MAJOR(services_.forwardingVersion),
						DMUI_VERSION_MINOR(services_.forwardingVersion),
						static_cast<uint64_t>(services_.supported),
						static_cast<uint64_t>(kRequiredServices));
				}
				else
				{
					servicesResult_ = client_.LastResult();
					REX::ERROR(
						"dmui-forwarding-smoke: service preflight result={}"sv,
						DMUI_ResultToString(servicesResult_));
					return RegistrationFailure("service preflight");
				}

				// Register this before pages and before any frame demand. Image
				// import is attempted only by this observer.
				if (!client_.AddFrameObserver([this] { ObserveFrame(); }))
					return RegistrationFailure("frame observer");
				REX::INFO(
					"dmui-forwarding-smoke: registration frame-observer result={}"sv,
					DMUI_ResultToString(client_.LastResult()));

				const auto settings = client_.AddPage(
						{
							.id = "forwarding-smoke",
							.displayName = "Forwarding Smoke Test (Development Only)",
							.category = "Development",
							.summary =
								"Manual verification of layout-independent forwarding services.",
							.sortKey = 9900
						},
						[this] { DrawSettings(); });
				if (!settings)
					return RegistrationFailure("settings page");
				settingsPage_ = *settings;
				REX::INFO(
					"dmui-forwarding-smoke: registration settings-page "
					"result={} handle={}"sv,
					DMUI_ResultToString(client_.LastResult()),
					settingsPage_);

				const auto overlay = client_.AddPage(
					{
						.id = "forwarding-smoke-overlay",
						.displayName = "Forwarding Smoke Overlay",
						.category = "Development",
						.summary = "Managed non-interactive smoke-test overlay.",
						.sortKey = 9901,
						.kind = DMUI_PAGE_KIND_OVERLAY
					},
					[this] { DrawOverlay(); });
				if (!overlay)
					return RegistrationFailure("overlay page");
				overlayPage_ = *overlay;
				REX::INFO(
					"dmui-forwarding-smoke: registration overlay-page "
					"result={} handle={}"sv,
					DMUI_ResultToString(client_.LastResult()),
					overlayPage_);
				ApplyOverlayConfiguration();

				if (!client_.AddPageActivityObserver(
						[this](const dmui::PageActivity& a_activity) {
							OnPageActivity(a_activity);
						}))
					return RegistrationFailure("page activity observer");
				REX::INFO(
					"dmui-forwarding-smoke: registration page-activity-observer "
					"result={}"sv,
					DMUI_ResultToString(client_.LastResult()));

				bool allHotkeysRegistered{ true };
				for (size_t index = 0; index < hotkeys_.size(); ++index)
				{
					auto& probe = hotkeys_[index];
					const auto handle = client_.AddHotkeyAction(
						probe.id,
						probe.name,
						probe.suggested,
						[this, index](bool a_pressed) {
							OnHotkey(index, a_pressed);
						},
						probe.policy);
					if (!handle)
					{
						const auto result = client_.LastResult();
						REX::ERROR(
							"dmui-forwarding-smoke: hotkey registration id={} "
							"result={} suggested={} policy={} effective=unavailable"sv,
							probe.id,
							DMUI_ResultToString(result),
							probe.suggested,
							static_cast<uint32_t>(probe.policy));
						MarkInitializationIncomplete(probe.id, result);
						allHotkeysRegistered = false;
						continue;
					}
					probe.handle = *handle;

					const auto binding =
						client_.QueryHotkeyBinding(probe.handle);
					probe.lastResult = client_.LastResult();
					if (!binding)
					{
						REX::ERROR(
							"dmui-forwarding-smoke: hotkey registration id={} "
							"result=OK suggested={} policy={} binding-result={} "
							"effective=unavailable"sv,
							probe.id,
							probe.suggested,
							static_cast<uint32_t>(probe.policy),
							DMUI_ResultToString(probe.lastResult));
						MarkInitializationIncomplete(
							"initial hotkey binding query",
							probe.lastResult);
						allHotkeysRegistered = false;
						continue;
					}
					probe.binding = *binding;
					REX::INFO(
						"dmui-forwarding-smoke: hotkey registration id={} "
						"result=OK suggested={} policy={} effective={} state={}"sv,
						probe.id,
						probe.suggested,
						static_cast<uint32_t>(probe.policy),
						probe.binding.chord[0] ? probe.binding.chord : "none",
						BindingStateName(probe.binding.state));
				}
				if (!allHotkeysRegistered)
					return false;

				initializationStatus_ = InitializationStatus::kComplete;
				initializationStage_ = "complete";
				initializationResult_ = DMUI_RESULT_OK;
				REX::INFO(
					"dmui-forwarding-smoke: initialization complete; "
					"registered forwarding 0.1 client"sv);
				return true;
			}

			void StopWorker() noexcept
			{
				workerPostingAllowed_.store(false, std::memory_order_release);
				std::jthread worker;
				{
					std::scoped_lock lock{ workerMutex_ };
					if (notificationWorker_.joinable())
					{
						notificationWorker_.request_stop();
						worker = std::move(notificationWorker_);
					}
				}
				if (worker.joinable())
					worker.join();
				workerBusy_.store(false, std::memory_order_release);
			}

		private:
			void MarkInitializationIncomplete(
				std::string_view a_scope,
				DMUI_Result a_result) noexcept
			{
				if (initializationStatus_ != InitializationStatus::kIncomplete)
				{
					initializationStage_ = a_scope;
					initializationResult_ = a_result;
				}
				initializationStatus_ = InitializationStatus::kIncomplete;
			}

			[[nodiscard]] bool RegistrationFailure(
				std::string_view a_scope) noexcept
			{
				const auto result = client_.LastResult();
				MarkInitializationIncomplete(a_scope, result);
				REX::ERROR(
					"dmui-forwarding-smoke: initialization incomplete; "
					"registration={} result={} partial-registration={}"sv,
					a_scope,
					DMUI_ResultToString(result),
					settingsPage_ != DMUI_INVALID_PAGE_HANDLE);
				return false;
			}

			void LogUnavailableOnce() noexcept
			{
				if (unavailableLogged_.exchange(true, std::memory_order_acq_rel))
					return;
				if (!client_.HostPresent())
				{
					REX::WARN(
						"dmui-forwarding-smoke: DearModdingUI host absent; "
						"registered nothing"sv);
				}
				else
				{
					REX::WARN(
						"dmui-forwarding-smoke: host incompatible ({}); "
						"registered nothing"sv,
						DMUI_ResultToString(client_.LastResult()));
				}
			}

			void LogSnapshot(std::string_view a_trigger) noexcept
			{
				uint64_t hotkeyEdges{};
				for (const auto& probe : hotkeys_)
					hotkeyEdges += probe.presses.load() + probe.releases.load();

				const auto editEvents =
					textEdits_.completed + textEdits_.resets +
					sliderEdits_.completed + sliderEdits_.resets +
					multilineEdits_.completed + multilineEdits_.resets;
				const auto dialogEvents =
					dialogRequestAttempts_ + dialogSubmissions_ +
					dialogCancellations_ + confirmOperations_ +
					textAccepts_ + textRejects_;
				const auto notificationEvents =
					pageNotifications_ + notificationSchedules_ +
					delayedNotifications_.load() +
					workerBusyRejections_ + workerSuppressed_.load();
				const auto overlayEvents = overlayDemandAttempts_;
				const auto imageEvents =
					imageImportCount_ + imageReleaseCount_ +
					imageCycleRequests_ + imageFailureCount_ +
					deviceChangeCount_ + cpuImageCreateCount_ +
					cpuImageUpdateCount_;

				++snapshotsLogged_;
				REX::INFO(
					"dmui-forwarding-smoke: snapshot trigger={} sequence={} "
					"initialization={} stage={} result={} overall=not-evaluated "
					"outcomes[hotkeys={},overlay={},image={},notifications={},"
					"edits={},dialogs={}] counts[hotkey-edges={},frame-demand={}/{},"
					"image={}/{}/{}/{}/{}/{},notifications={}/{}/{}/{},edits={}/{}/{},"
					"dialogs={}/{}/{}/{}/{}]"sv,
					a_trigger,
					snapshotsLogged_,
					InitializationStatusName(initializationStatus_),
					initializationStage_,
					DMUI_ResultToString(initializationResult_),
					hotkeyEdges ? "observed" : "unexercised",
					overlayEvents ? "observed" : "unexercised",
					imageEvents ? "observed" : "unexercised",
					notificationEvents ? "observed" : "unexercised",
					editEvents ? "observed" : "unexercised",
					dialogEvents ? "observed" : "unexercised",
					hotkeyEdges,
					frameRequests_,
					frameReleases_,
					imageImportCount_,
					imageReleaseCount_,
					imageCycleRequests_,
					imageFailureCount_,
					cpuImageCreateCount_,
					cpuImageUpdateCount_,
					pageNotifications_,
					notificationSchedules_,
					delayedNotifications_.load(),
					workerSuppressed_.load() + workerBusyRejections_,
					textEdits_.completed + textEdits_.resets,
					sliderEdits_.completed + sliderEdits_.resets,
					multilineEdits_.completed + multilineEdits_.resets,
					dialogRequests_,
					dialogSubmissions_,
					confirmOperations_,
					textAccepts_ + textRejects_,
					dialogCancellations_);
				for (const auto& probe : hotkeys_)
				{
					const auto exercised =
						probe.presses.load() || probe.releases.load();
					REX::INFO(
						"dmui-forwarding-smoke: snapshot-hotkey sequence={} "
						"id={} outcome={} enabled={} effective={} state={} "
						"result={} down={} up={}"sv,
						snapshotsLogged_,
						probe.id,
						exercised ? "observed" : "unexercised",
						probe.enabled,
						probe.binding.chord[0] ?
							probe.binding.chord :
							"none",
						BindingStateName(probe.binding.state),
						DMUI_ResultToString(probe.lastResult),
						probe.presses.load(),
						probe.releases.load());
				}
			}

			void OnPageActivity(const dmui::PageActivity& a_activity) noexcept
			{
				if (settingsPage_ == DMUI_INVALID_PAGE_HANDLE)
					return;

				const auto active =
					a_activity.activePage == settingsPage_;
				if (active)
				{
					settingsPageActive_ = true;
					return;
				}
				if (settingsPageActive_ &&
					a_activity.previousPage == settingsPage_)
				{
					settingsPageActive_ = false;
					LogSnapshot("settings-page-deactivated");
				}
			}

			void SetHotkey(
				size_t a_index,
				const char* a_id,
				const char* a_name,
				const char* a_suggested,
				DMUI_HotkeyContextPolicy a_policy) noexcept
			{
				hotkeys_[a_index].id = a_id;
				hotkeys_[a_index].name = a_name;
				hotkeys_[a_index].suggested = a_suggested;
				hotkeys_[a_index].policy = a_policy;
			}

			void ObserveFrame() noexcept
			{
				++frameCount_;
				const auto now = std::chrono::steady_clock::now();
				if (startTime_ == std::chrono::steady_clock::time_point{})
				{
					startTime_ = now;
					previousFrame_ = now;
				}
				const auto frameSeconds =
					std::chrono::duration<float>(now - previousFrame_).count();
				previousFrame_ = now;
				elapsedSeconds_ =
					std::chrono::duration<double>(now - startTime_).count();
				samples_[sampleOffset_] =
					(std::min)(frameSeconds * 1000.0f, 50.0f);
				sampleOffset_ = (sampleOffset_ + 1) % samples_.size();

				if (const auto state = client_.QueryState())
				{
					hostState_ = *state;
					workerPostingAllowed_.store(
						state->state == DMUI_HOST_STATE_READY &&
							client_.UnavailableReason() == DMUI_UNAVAILABLE_NONE,
						std::memory_order_release);
				}
				else
				{
					stateResult_ = client_.LastResult();
					workerPostingAllowed_.store(
						false,
						std::memory_order_release);
				}

				if (const auto visible = client_.IsMenuVisible())
				{
					menuVisible_ = *visible;
					if (!*visible)
						++hiddenMenuObservations_;
				}

				RefreshImage();
				RefreshCpuImage();
				PollDialog();
				QueryOverlay();
				QueryHotkeys();
			}

			[[nodiscard]] ComPtr<ID3D11Device> AcquireRendererDevice() noexcept
			{
				auto* rendererData = RE::BSGraphics::GetRendererData();
				if (!rendererData)
					return {};

				// Match PlatformImgui::CaptureRendererSnapshot: lock the
				// published RendererData, revalidate its identity, and retain the
				// COM object before the protected scope can end.
				const RendererDataLock rendererLock{ *rendererData };
				if (RE::BSGraphics::GetRendererData() != rendererData ||
					!rendererData->initialized ||
					!rendererData->device)
					return {};

				auto* device =
					reinterpret_cast<ID3D11Device*>(rendererData->device);
				device->AddRef();
				ComPtr<ID3D11Device> result;
				result.Attach(device);
				return result;
			}

			void RefreshImage() noexcept
			{
				auto currentDevice = AcquireRendererDevice();
				if (!currentDevice)
				{
					if (!deviceWaitingLogged_)
					{
						deviceWaitingLogged_ = true;
						REX::INFO(
							"dmui-forwarding-smoke: waiting for the game D3D11 device"sv);
					}
					return;
				}

				bool stale{};
				if (image_)
				{
					const auto info = client_.QueryImage(image_->Handle());
					imageResult_ = client_.LastResult();
					if (!info)
					{
						LogImageFailure("query", imageResult_);
						stale = true;
					}
					else
					{
						if (imageStatus_ != info->status ||
							imageGeneration_ != info->deviceGeneration)
						{
							REX::INFO(
								"dmui-forwarding-smoke: image status transition "
								"status={}->{} generation={}->{}"sv,
								static_cast<uint32_t>(imageStatus_),
								static_cast<uint32_t>(info->status),
								imageGeneration_,
								info->deviceGeneration);
						}
						imageStatus_ = info->status;
						imageGeneration_ = info->deviceGeneration;
						if (info->status != DMUI_IMAGE_STATUS_READY)
							stale = true;
					}
				}

				const auto deviceChanged =
					imageDevice_ && imageDevice_.Get() != currentDevice.Get();
				if (deviceChanged)
				{
					++deviceChangeCount_;
					REX::INFO(
						"dmui-forwarding-smoke: renderer device transition "
						"count={}"sv,
						deviceChangeCount_);
				}
				const auto recreate =
					recreateImage_.exchange(false, std::memory_order_acq_rel);
				if (deviceChanged || stale || recreate)
				{
					ReleaseOwnedImageResources(
						deviceChanged ? "device-transition" :
							stale ? "stale-or-invalidated" :
								"cycle");
				}

				if (image_)
					return;

				imageDevice_ = std::move(currentDevice);
				CreateAndImportImage();
			}

			void ReleaseOwnedImageResources(std::string_view a_reason) noexcept
			{
				const bool owned =
					image_.has_value() ||
					static_cast<bool>(imageView_) ||
					static_cast<bool>(imageTexture_) ||
					static_cast<bool>(imageDevice_);
				image_.reset();
				imageView_.Reset();
				imageTexture_.Reset();
				imageDevice_.Reset();
				if (owned)
				{
					imageStatus_ = DMUI_IMAGE_STATUS_RELEASED;
					++imageReleaseCount_;
					REX::INFO(
						"dmui-forwarding-smoke: image release reason={} "
						"status={} count={}"sv,
						a_reason,
						static_cast<uint32_t>(imageStatus_),
						imageReleaseCount_);
				}
			}

			void LogImageFailure(
				std::string_view a_scope,
				DMUI_Result a_result) noexcept
			{
				if (imageFailureActive_ &&
					lastImageFailureScope_ == a_scope &&
					lastImageFailureResult_ == a_result)
					return;

				imageFailureActive_ = true;
				lastImageFailureScope_ = a_scope;
				lastImageFailureResult_ = a_result;
				++imageFailureCount_;
				REX::ERROR(
					"dmui-forwarding-smoke: image failure scope={} result={} "
					"count={}"sv,
					a_scope,
					DMUI_ResultToString(a_result),
					imageFailureCount_);
			}

			void CreateAndImportImage() noexcept
			{
				std::array<uint32_t, kImageExtent * kImageExtent> pixels{};
				for (uint32_t y = 0; y < kImageExtent; ++y)
				{
					for (uint32_t x = 0; x < kImageExtent; ++x)
					{
						const auto checker = ((x / 8) + (y / 8)) % 2;
						const auto red = static_cast<uint8_t>(
							48u + (x * 207u) / (kImageExtent - 1));
						const auto green = static_cast<uint8_t>(
							48u + (y * 207u) / (kImageExtent - 1));
						const auto blue = static_cast<uint8_t>(
							checker ? 230u : 45u);
						pixels[y * kImageExtent + x] =
							UINT32_C(0xFF000000) |
							(static_cast<uint32_t>(blue) << 16u) |
							(static_cast<uint32_t>(green) << 8u) |
							red;
					}
				}

				const D3D11_TEXTURE2D_DESC description{
					kImageExtent,
					kImageExtent,
					1,
					1,
					DXGI_FORMAT_R8G8B8A8_UNORM,
					{ 1, 0 },
					D3D11_USAGE_IMMUTABLE,
					D3D11_BIND_SHADER_RESOURCE,
					0,
					0
				};
				const D3D11_SUBRESOURCE_DATA initial{
					pixels.data(),
					kImageExtent * sizeof(uint32_t),
					0
				};
				if (FAILED(imageDevice_->CreateTexture2D(
						&description,
						&initial,
						imageTexture_.ReleaseAndGetAddressOf())) ||
					FAILED(imageDevice_->CreateShaderResourceView(
						imageTexture_.Get(),
						nullptr,
						imageView_.ReleaseAndGetAddressOf())))
				{
					imageResult_ = DMUI_RESULT_RESOURCE_EXHAUSTED;
					LogImageFailure("D3D11 resource creation", imageResult_);
					imageView_.Reset();
					imageTexture_.Reset();
					return;
				}

				auto image = client_.ImportD3D11Image(
					imageView_.Get(),
					kImageExtent,
					kImageExtent);
				imageResult_ = client_.LastResult();
				if (!image)
				{
					LogImageFailure("forwarded image import", imageResult_);
					imageView_.Reset();
					imageTexture_.Reset();
					return;
				}

				image_ = std::move(*image);
				++imageImportCount_;
				imageFailureActive_ = false;
				REX::INFO(
					"dmui-forwarding-smoke: image import result={} count={}"sv,
					DMUI_ResultToString(imageResult_),
					imageImportCount_);
			}

			void QueueImage(bool a_large) noexcept
			{
				if (!image_)
					{
						ImGui::TextDisabled("Image waiting for renderer/import.");
						return;
					}
				const auto extent = a_large ? 112.0f : 64.0f;
				const DMUI_ImageDrawOptions options{
					sizeof(DMUI_ImageDrawOptions),
					{ extent, extent },
					{ 0.0f, 0.0f },
					{ 1.0f, 1.0f },
					{ 1.0f, 1.0f, 1.0f, 1.0f },
					1,
					0
				};
				if (client_.DrawImage(image_->Handle(), options))
					++imageDrawCount_;
				imageResult_ = client_.LastResult();
			}

			void FillCpuPixels(
				std::array<uint8_t, 80u * 64u * 4u>& a_pixels,
				uint32_t a_width,
				uint32_t a_height,
				uint32_t a_step) noexcept
			{
				for (uint32_t y = 0; y < a_height; ++y)
				{
					for (uint32_t x = 0; x < a_width; ++x)
					{
						const auto offset = (y * a_width + x) * 4u;
						a_pixels[offset] = static_cast<uint8_t>(
							32u + (x * 191u) / (a_width - 1u));
						a_pixels[offset + 1u] = static_cast<uint8_t>(
							32u + (y * 191u) / (a_height - 1u));
						a_pixels[offset + 2u] =
							a_step % 2u == 0 ? 64u : 224u;
						a_pixels[offset + 3u] = static_cast<uint8_t>(
							96u + ((x + y) % 2u) * 159u);
					}
				}
			}

			[[nodiscard]] DMUI_ImageDescriptor CpuImageDescriptor(
				const std::array<uint8_t, 80u * 64u * 4u>& a_pixels,
				uint32_t a_width,
				uint32_t a_height) noexcept
			{
				return {
					sizeof(DMUI_ImageDescriptor),
					a_width,
					a_height,
					DMUI_PIXEL_FORMAT_RGBA8_UNORM,
					0,
					static_cast<uint64_t>(a_width) * 4u,
					static_cast<uint64_t>(a_width) * a_height * 4u,
					a_pixels.data()
				};
			}

			void RefreshCpuImage() noexcept
			{
				if (cpuImage_)
				{
					const auto info = client_.QueryImage(cpuImage_->Handle());
					cpuImageResult_ = client_.LastResult();
					if (!info || info->status != DMUI_IMAGE_STATUS_READY)
					{
						cpuImage_.reset();
						++cpuImageReleaseCount_;
						REX::INFO(
							"dmui-forwarding-smoke: CPU image recycle result={} "
							"count={}"sv,
							DMUI_ResultToString(cpuImageResult_),
							cpuImageReleaseCount_);
					}
					else
					{
						cpuImageWidth_ = info->contentWidth;
						cpuImageHeight_ = info->contentHeight;
					}
				}
				if (cpuImage_)
					return;

				std::array<uint8_t, 80u * 64u * 4u> pixels{};
				constexpr uint32_t width{ 48 };
				constexpr uint32_t height{ 48 };
				FillCpuPixels(pixels, width, height, cpuImageStep_);
				auto image = client_.CreateImage(
					CpuImageDescriptor(pixels, width, height));
				cpuImageResult_ = client_.LastResult();
				if (!image)
				{
					LogImageFailure("CPU image create", cpuImageResult_);
					return;
				}
				cpuImage_ = std::move(*image);
				cpuImageWidth_ = width;
				cpuImageHeight_ = height;
				++cpuImageCreateCount_;
				imageFailureActive_ = false;
				REX::INFO(
					"dmui-forwarding-smoke: CPU image create result={} "
					"dimensions={}x{} count={}"sv,
					DMUI_ResultToString(cpuImageResult_),
					width,
					height,
					cpuImageCreateCount_);
			}

			void UpdateCpuImage() noexcept
			{
				if (!cpuImage_)
					return;
				++cpuImageStep_;
				const auto width = cpuImageStep_ % 2u == 0 ? 48u : 72u;
				const auto height = cpuImageStep_ % 2u == 0 ? 48u : 40u;
				std::array<uint8_t, 80u * 64u * 4u> pixels{};
				FillCpuPixels(pixels, width, height, cpuImageStep_);
				const auto updated = client_.UpdateImage(
					cpuImage_->Handle(),
					CpuImageDescriptor(pixels, width, height));
				cpuImageResult_ = client_.LastResult();
				if (!updated)
				{
					LogImageFailure("CPU image update", cpuImageResult_);
					return;
				}
				cpuImageWidth_ = width;
				cpuImageHeight_ = height;
				++cpuImageUpdateCount_;
				imageFailureActive_ = false;
				REX::INFO(
					"dmui-forwarding-smoke: CPU image update result={} "
					"dimensions={}x{} count={}"sv,
					DMUI_ResultToString(cpuImageResult_),
					width,
					height,
					cpuImageUpdateCount_);
			}

			void QueueCpuImage() noexcept
			{
				if (!cpuImage_)
					return;
				const DMUI_ImageDrawOptions options{
					sizeof(DMUI_ImageDrawOptions),
					{ 112.0f, 80.0f },
					{ 0.0f, 0.0f },
					{ 1.0f, 1.0f },
					{ 1.0f, 1.0f, 1.0f, 1.0f },
					1,
					0
				};
				if (client_.DrawImage(cpuImage_->Handle(), options))
					++cpuImageDrawCount_;
				cpuImageResult_ = client_.LastResult();
			}

			void ReleaseAfterQueuedDraw() noexcept
			{
				if (!image_)
				{
					REX::INFO(
						"dmui-forwarding-smoke: release-after-queued-draw "
						"ignored; image=unexercised"sv);
					return;
				}
				imageResult_ = image_->Release();
				REX::INFO(
					"dmui-forwarding-smoke: release-after-queued-draw result={}"sv,
					DMUI_ResultToString(imageResult_));
				ReleaseOwnedImageResources("release-after-queued-draw");
				recreateImage_.store(true, std::memory_order_release);
			}

			void RequestImageCycle() noexcept
			{
				++imageCycleRequests_;
				const auto owned =
					image_.has_value() || imageView_ || imageTexture_ || imageDevice_;
				REX::INFO(
					"dmui-forwarding-smoke: image cycle requested count={} "
					"owned={}"sv,
					imageCycleRequests_,
					owned);
				recreateImage_.store(true, std::memory_order_release);
			}

			void ApplyOverlayConfiguration() noexcept
			{
				if (overlayPage_ == DMUI_INVALID_PAGE_HANDLE)
					return;
				(void)client_.ConfigureOverlay(overlayPage_, overlayOptions_);
				overlayResult_ = client_.LastResult();
			}

			void SetOverlayEnabled(bool a_enabled) noexcept
			{
				if (a_enabled == overlayEnabled_ ||
					overlayPage_ == DMUI_INVALID_PAGE_HANDLE)
					return;
				const auto succeeded = a_enabled ?
					client_.RequestFrame(overlayPage_) :
					client_.ReleaseFrame(overlayPage_);
				++overlayDemandAttempts_;
				overlayResult_ = client_.LastResult();
				if (succeeded)
				{
					overlayEnabled_ = a_enabled;
					if (a_enabled)
						++frameRequests_;
					else
						++frameReleases_;
				}
				REX::INFO(
					"dmui-forwarding-smoke: overlay demand enabled={} result={} "
					"requests={} releases={}"sv,
					a_enabled,
					DMUI_ResultToString(overlayResult_),
					frameRequests_,
					frameReleases_);
			}

			void QueryOverlay() noexcept
			{
				if (overlayPage_ == DMUI_INVALID_PAGE_HANDLE)
					return;
				if (const auto placement = client_.QueryOverlay(overlayPage_))
				{
					overlayPlacement_ = *placement;
					if (placement->arrangementCompleted)
						++arrangementCompletions_;
				}
				else
					overlayResult_ = client_.LastResult();
			}

			void DrawOverlay() noexcept
			{
				++overlayDraws_;
				ImGui::TextUnformatted("FORWARDING SMOKE / MANAGED OVERLAY");
				ImGui::Separator();
				ImGui::Text(
					"observer=%llu  overlay=%llu  hidden-menu=%llu",
					frameCount_,
					overlayDraws_,
					hiddenMenuObservations_);
				ImGui::Text("timer=%.2f s", elapsedSeconds_);
				QueueImage(false);
				QueueCpuImage();

				const DMUI_PlotReferenceLine references[]{
					{ 16.67f, { 0.25f, 0.85f, 0.35f, 0.90f } },
					{ 33.33f, { 0.95f, 0.55f, 0.20f, 0.90f } }
				};
				const DMUI_AnnotatedPlotDescriptor plot{
					sizeof(DMUI_AnnotatedPlotDescriptor),
					samples_.data(),
					static_cast<uint32_t>(samples_.size()),
					static_cast<uint32_t>(sampleOffset_),
					0.0f,
					50.0f,
					{ 310.0f, 82.0f },
					"FRAME TIME (MS) - LABEL MUST REMAIN VISIBLE / CLIPPED LINES",
					references,
					static_cast<uint32_t>(std::size(references))
				};
				if (client_.DrawAnnotatedPlot("overlay-frame-times", plot))
					++plotDraws_;
				plotResult_ = client_.LastResult();
			}

			void RecordEdit(
				EditCounters& a_counters,
				std::string_view a_id,
				bool a_changed,
				bool a_completed) noexcept
			{
				if (a_changed)
				{
					++a_counters.changed;
					a_counters.dirty = true;
				}
				if (a_completed)
				{
					++a_counters.completed;
					if (a_counters.dirty)
					{
						++a_counters.saves;
						++simulatedSaves_;
						a_counters.dirty = false;
					}
					REX::INFO(
						"dmui-forwarding-smoke: edit id={} event=completed "
						"changed={} completed={} resets={} saves={} total-saves={}"sv,
						a_id,
						a_changed,
						a_counters.completed,
						a_counters.resets,
						a_counters.saves,
						simulatedSaves_);
				}
			}

			template <class Draw>
			[[nodiscard]] bool DrawEditableRow(
				const char* a_id,
				const char* a_label,
				const char* a_description,
				bool a_isDefault,
				Draw&& a_draw,
				EditCounters& a_counters,
				auto&& a_reset)
			{
				const auto visible =
					client_.BeginSettingsRow(a_id, a_label, a_description);
				if (!visible)
					return false;
				if (!*visible)
					return true;

				const auto changed = a_draw();
				const auto completed = ImGui::IsItemDeactivatedAfterEdit();
				RecordEdit(a_counters, a_id, changed, completed);
				const auto reset = client_.EndSettingsRow(true, !a_isDefault);
				if (!reset)
					return false;
				if (reset && *reset && !a_isDefault)
				{
					a_reset();
					++a_counters.resets;
					RecordEdit(a_counters, a_id, true, true);
					REX::INFO(
						"dmui-forwarding-smoke: edit id={} event=reset "
						"completed={} resets={} saves={} total-saves={}"sv,
						a_id,
						a_counters.completed,
						a_counters.resets,
						a_counters.saves,
						simulatedSaves_);
				}
				return true;
			}

			void DrawSettings() noexcept
			{
				++settingsDraws_;
				(void)client_.DrawSectionHeader(
					"Development-only forwarding smoke harness");
				ImGui::TextWrapped(
					"This page uses only the official forwarding API. "
					"Expected: host ready, all service bits present, and "
					"forwarding version %u.%u.",
					DMUI_VERSION_MAJOR(DMUI_FORWARDING_VERSION_CURRENT),
					DMUI_VERSION_MINOR(DMUI_FORWARDING_VERSION_CURRENT));
				ImGui::Text(
					"Host: %s | API %u.%u | forwarding %u.%u | services 0x%llX",
					HostStateName(hostState_.state),
					DMUI_VERSION_MAJOR(DMUI_API_VERSION_CURRENT),
					DMUI_VERSION_MINOR(DMUI_API_VERSION_CURRENT),
					DMUI_VERSION_MAJOR(services_.forwardingVersion),
					DMUI_VERSION_MINOR(services_.forwardingVersion),
					services_.supported);
				ImGui::Text(
					"Observer %llu | hidden-menu samples %llu | settings draws %llu",
					frameCount_,
					hiddenMenuObservations_,
					settingsDraws_);
				ImGui::Text(
					"Initialization: %s | stage=%s | result=%s",
					InitializationStatusName(initializationStatus_),
					initializationStage_.data(),
					DMUI_ResultToString(initializationResult_));
				ImGui::Text(
					"Last results: state=%s image=%s overlay=%s plot=%s "
					"notification=%s dialog=%s hotkey=%s services=%s",
					DMUI_ResultToString(stateResult_),
					DMUI_ResultToString(imageResult_),
					DMUI_ResultToString(overlayResult_),
					DMUI_ResultToString(plotResult_),
					DMUI_ResultToString(notificationResult_.load()),
					DMUI_ResultToString(dialogResult_),
					DMUI_ResultToString(hotkeyResult_),
					DMUI_ResultToString(servicesResult_));
				if (ImGui::Button("Log current results"))
					LogSnapshot("manual-button");

				DrawNativeEdits();
				DrawOverlayControls();
				DrawImageControls();
				DrawNotificationAndDialogs();
				DrawHotkeyControls();

				(void)client_.DrawSectionHeader("Expected outcomes");
				(void)client_.DrawBulletText(
					"Unchanged clicks do not increment simulated saves; "
					"completed edits and effective resets do.");
				(void)client_.DrawBulletText(
					"Overlay and delayed-toast defaults yield during menus, "
					"console, text entry, and unsafe gameplay UI.");
				(void)client_.DrawBulletText(
					"Free overlay arrangement is interactive only while the "
					"host menu owns input.");
				(void)client_.DrawBulletText(
					"Rejected names preserve text. Confirm operations count "
					"only after COMPLETED; cancellation never counts.");
			}

			void DrawNativeEdits() noexcept
			{
				(void)client_.DrawSectionHeader(
					"Native forwarding edits and simulated persistence");
				const auto table = client_.BeginSettingsTable("native-edits");
				if (!table)
					return;
				if (!*table)
					return;

				if (!DrawEditableRow(
						"short-text",
						"Native text",
						"Live forwarded InputText; save is simulated on completion.",
						std::strcmp(shortText_.data(), "smoke") == 0,
						[this] {
							return ImGui::InputText(
								"##Value",
								shortText_.data(),
								shortText_.size());
						},
						textEdits_,
						[this] { CopyText(shortText_, "smoke"); }))
				{
					(void)client_.EndSettingsTable();
					return;
				}

				if (!DrawEditableRow(
						"slider",
						"Native slider",
						"Forwarded scalar slider, range 0..100.",
						sliderValue_ == 50.0f,
						[this] {
							const float minimum{};
							const float maximum{ 100.0f };
							return ImGui::SliderScalar(
								"##Value",
								ImGuiDataType_Float,
								&sliderValue_,
								&minimum,
								&maximum,
								"%.1f",
								ImGuiSliderFlags_AlwaysClamp);
						},
						sliderEdits_,
						[this] { sliderValue_ = 50.0f; }))
				{
					(void)client_.EndSettingsTable();
					return;
				}

				if (!DrawEditableRow(
						"multiline",
						"Native multiline",
						"Three-line forwarded editor; no disk writes are performed.",
						std::strcmp(
							multiline_.data(),
							"line one\nline two") == 0,
						[this] {
							return ImGui::InputTextMultiline(
								"##Value",
								multiline_.data(),
								multiline_.size(),
								{
									0.0f,
									ImGui::GetTextLineHeightWithSpacing() * 3.0f
								});
						},
						multilineEdits_,
						[this] {
							CopyText(multiline_, "line one\nline two");
						}))
				{
					(void)client_.EndSettingsTable();
					return;
				}

				const auto counters = client_.BeginSettingsRow(
					"edit-counters",
					"Lifecycle counters",
					"changed / completed / simulated saves",
					dmui::RowPresentation::Layout::kFullSpan);
				if (!counters)
				{
					(void)client_.EndSettingsTable();
					return;
				}
				if (*counters)
				{
					ImGui::Text(
						"changed/completed/reset/saved: text %llu/%llu/%llu/%llu | "
						"slider %llu/%llu/%llu/%llu | multiline "
						"%llu/%llu/%llu/%llu | total saves %llu",
						textEdits_.changed,
						textEdits_.completed,
						textEdits_.resets,
						textEdits_.saves,
						sliderEdits_.changed,
						sliderEdits_.completed,
						sliderEdits_.resets,
						sliderEdits_.saves,
						multilineEdits_.changed,
						multilineEdits_.completed,
						multilineEdits_.resets,
						multilineEdits_.saves,
						simulatedSaves_);
					(void)client_.EndSettingsRow(false, false);
				}
				(void)client_.EndSettingsTable();
			}

			void DrawOverlayControls() noexcept
			{
				(void)client_.DrawSectionHeader("Managed overlay");
				const auto table = client_.BeginSettingsTable("overlay-controls");
				if (!table)
					return;
				if (!*table)
					return;

				if (!DrawSimpleRow(
						"overlay-enabled",
						"Enabled",
						"Balances RequestFrame and ReleaseFrame.",
						[this] {
							auto enabled = overlayEnabled_;
							if (ImGui::Checkbox("##Value", &enabled))
								SetOverlayEnabled(enabled);
						}))
				{
					(void)client_.EndSettingsTable();
					return;
				}
				if (!DrawSimpleRow(
						"overlay-anchor",
						"Anchor",
						"Free position can move only while the menu owns input.",
						[this] {
							if (ImGui::BeginCombo(
									"##Value",
									AnchorName(overlayOptions_.anchor)))
							{
								for (uint32_t anchor =
										DMUI_OVERLAY_ANCHOR_TOP_LEFT;
									 anchor <= DMUI_OVERLAY_ANCHOR_FREE;
									 ++anchor)
								{
									const auto selected =
										overlayOptions_.anchor == anchor;
									if (ImGui::Selectable(
											AnchorName(anchor),
											selected))
									{
										overlayOptions_.anchor = anchor;
										ApplyOverlayConfiguration();
									}
									if (selected)
										ImGui::SetItemDefaultFocus();
								}
								ImGui::EndCombo();
							}
						}))
				{
					(void)client_.EndSettingsTable();
					return;
				}
				if (!DrawFloatRow(
						"overlay-x",
						"Free/anchor X offset",
						overlayOptions_.offset.x,
						0.0f,
						1200.0f) ||
					!DrawFloatRow(
						"overlay-y",
						"Free/anchor Y offset",
						overlayOptions_.offset.y,
						0.0f,
						800.0f) ||
					!DrawFloatRow(
						"overlay-scale",
						"Content scale",
						overlayOptions_.contentScale,
						0.5f,
						3.0f) ||
					!DrawFloatRow(
						"overlay-opacity",
						"Opacity",
						overlayOptions_.opacity,
						0.05f,
						1.0f))
				{
					(void)client_.EndSettingsTable();
					return;
				}
				if (!DrawSimpleRow(
						"overlay-arrangement",
						"Allow arrangement",
						"Host permits free movement only while its menu owns input.",
						[this] {
							auto enabled =
								overlayOptions_.allowArrangement != 0;
							if (ImGui::Checkbox("##Value", &enabled))
							{
								overlayOptions_.allowArrangement =
									enabled ? 1u : 0u;
								ApplyOverlayConfiguration();
							}
						}))
				{
					(void)client_.EndSettingsTable();
					return;
				}
				const auto placement = client_.BeginSettingsRow(
					"overlay-placement",
					"Observed placement",
					"Latest host-owned placement and completion edge.",
					dmui::RowPresentation::Layout::kFullSpan);
				if (!placement)
				{
					(void)client_.EndSettingsTable();
					return;
				}
				if (*placement)
				{
					ImGui::Text(
						"visible=%u pos=(%.1f, %.1f) size=(%.1f, %.1f) "
						"generation=%llu completed=%llu requests/releases=%llu/%llu",
						overlayPlacement_.visible,
						overlayPlacement_.position.x,
						overlayPlacement_.position.y,
						overlayPlacement_.size.x,
						overlayPlacement_.size.y,
						overlayPlacement_.changeGeneration,
						arrangementCompletions_,
						frameRequests_,
						frameReleases_);
					(void)client_.EndSettingsRow(false, false);
				}
				(void)client_.EndSettingsTable();
			}

			[[nodiscard]] bool DrawFloatRow(
				const char* a_id,
				const char* a_label,
				float& a_value,
				float a_minimum,
				float a_maximum) noexcept
			{
				return DrawSimpleRow(
					a_id,
					a_label,
					"Changes are applied through ConfigureOverlay.",
					[this, &a_value, a_minimum, a_maximum] {
						if (ImGui::SliderScalar(
								"##Value",
								ImGuiDataType_Float,
								&a_value,
								&a_minimum,
								&a_maximum,
								"%.2f",
								ImGuiSliderFlags_AlwaysClamp))
							ApplyOverlayConfiguration();
					});
			}

			template <class Draw>
			[[nodiscard]] bool DrawSimpleRow(
				const char* a_id,
				const char* a_label,
				const char* a_description,
				Draw&& a_draw)
			{
				const auto visible =
					client_.BeginSettingsRow(a_id, a_label, a_description);
				if (!visible)
					return false;
				if (!*visible)
					return true;

				a_draw();
				return client_.EndSettingsRow(false, false).has_value();
			}

			void DrawImageControls() noexcept
			{
				(void)client_.DrawSectionHeader("Shared image resources");
				ImGui::TextUnformatted("Host-owned CPU-pixel image");
				QueueCpuImage();
				ImGui::Text(
					"creates=%llu updates=%llu draws=%llu dimensions=%ux%u result=%s",
					cpuImageCreateCount_,
					cpuImageUpdateCount_,
					cpuImageDrawCount_,
					cpuImageWidth_,
					cpuImageHeight_,
					DMUI_ResultToString(cpuImageResult_));
				if (ImGui::Button("Update CPU image"))
					UpdateCpuImage();
				ImGui::TextDisabled(
					"Expected: the same handle changes dimensions and pixels.");

				ImGui::TextUnformatted("Existing imported D3D11 SRV");
				QueueImage(true);
				ImGui::Text(
					"imports=%llu draws=%llu releases=%llu status=%u generation=%llu",
					imageImportCount_,
					imageDrawCount_,
					imageReleaseCount_,
					imageStatus_,
					imageGeneration_);
				if (ImGui::Button("Cycle / recreate image"))
					RequestImageCycle();
				ImGui::SameLine();
				if (ImGui::Button("Release after queued draw"))
					ReleaseAfterQueuedDraw();
				ImGui::TextDisabled(
					"Expected: the already queued draw survives release; "
					"the observer imports one replacement.");
			}

			void DrawNotificationAndDialogs() noexcept
			{
				(void)client_.DrawSectionHeader("Notifications and dialogs");
				if (ImGui::Button("Post page notification"))
				{
					const auto posted = client_.PostNotification(
						DMUI_STATUS_SEVERITY_SUCCESS,
						"Forwarding smoke: page notification copied successfully.",
						3500);
					if (posted)
						++pageNotifications_;
					notificationResult_.store(
						client_.LastResult(),
						std::memory_order_release);
					REX::INFO(
						"dmui-forwarding-smoke: notification page-post={} "
						"result={} count={}"sv,
						posted,
						DMUI_ResultToString(notificationResult_.load()),
						pageNotifications_);
				}
				ImGui::SameLine();
				if (ImGui::Button("Schedule delayed any-thread notification"))
					ScheduleDelayedNotification();

				if (ImGui::Button("Request harmless confirm"))
					RequestConfirmDialog();
				ImGui::SameLine();
				if (ImGui::Button("Request validated text entry"))
					RequestTextDialog();
				ImGui::SameLine();
				auto rejectWithoutMessage = rejectWithoutMessage_;
				if (ImGui::Checkbox(
						"Reject with nullptr error",
						&rejectWithoutMessage))
					rejectWithoutMessage_ = rejectWithoutMessage;

				ImGui::Text(
					"dialog=%s id=%llu submitted=%llu accepted confirms=%llu "
					"text accepts=%llu rejects=%llu cancels=%llu duplicates ignored=%llu",
					DialogEventName(lastDialogEvent_),
					activeSubmission_,
					dialogSubmissions_,
					confirmOperations_,
					textAccepts_,
					textRejects_,
					dialogCancellations_,
					duplicateSubmissions_);
				ImGui::Text(
					"notifications page=%llu delayed=%llu busy-rejected=%llu "
					"suppressed=%llu",
					pageNotifications_,
					delayedNotifications_.load(),
					workerBusyRejections_,
					workerSuppressed_.load());
				ImGui::TextDisabled(
					"Text rejects empty, \"reject\", or an in-memory duplicate "
					"(initial duplicate: alpha). Rejection preserves text.");
			}

			void RequestConfirmDialog() noexcept
			{
				++dialogRequestAttempts_;
				if (dialog_ != DMUI_INVALID_DIALOG_HANDLE)
				{
					REX::INFO(
						"dmui-forwarding-smoke: dialog confirm request ignored; "
						"another dialog is pending"sv);
					return;
				}
				const DMUI_DialogDescriptor descriptor{
					sizeof(DMUI_DialogDescriptor),
					DMUI_DIALOG_KIND_CONFIRM,
					"Forwarding smoke confirmation",
					"Accepting performs one harmless in-memory operation after "
					"a short simulated in-flight delay.",
					"Accept",
					"Cancel",
					nullptr,
					nullptr,
					1
				};
				bool requested{};
				if (const auto handle = client_.RequestDialog(descriptor))
				{
					requested = true;
					dialog_ = *handle;
					dialogProbe_ = DialogProbe::kConfirm;
					lastDialogEvent_ = DMUI_DIALOG_EVENT_PENDING;
					activeSubmission_ = 0;
					resolutionSent_ = false;
					++dialogRequests_;
				}
				dialogResult_ = client_.LastResult();
				if (requested)
				{
					REX::INFO(
						"dmui-forwarding-smoke: dialog kind=confirm event=pending "
						"result={} requests={}"sv,
						DMUI_ResultToString(dialogResult_),
						dialogRequests_);
				}
				else
				{
					REX::ERROR(
						"dmui-forwarding-smoke: dialog kind=confirm "
						"request-failed result={}"sv,
						DMUI_ResultToString(dialogResult_));
				}
			}

			void RequestTextDialog() noexcept
			{
				++dialogRequestAttempts_;
				if (dialog_ != DMUI_INVALID_DIALOG_HANDLE)
				{
					REX::INFO(
						"dmui-forwarding-smoke: dialog text request ignored; "
						"another dialog is pending"sv);
					return;
				}
				const DMUI_DialogDescriptor descriptor{
					sizeof(DMUI_DialogDescriptor),
					DMUI_DIALOG_KIND_TEXT_ENTRY,
					"Forwarding smoke name",
					"Enter a unique in-memory name.",
					"Validate",
					"Cancel",
					"Try alpha, reject, or an empty value.",
					"",
					96
				};
				bool requested{};
				if (const auto handle = client_.RequestDialog(descriptor))
				{
					requested = true;
					dialog_ = *handle;
					dialogProbe_ = DialogProbe::kText;
					lastDialogEvent_ = DMUI_DIALOG_EVENT_PENDING;
					activeSubmission_ = 0;
					resolutionSent_ = false;
					++dialogRequests_;
				}
				dialogResult_ = client_.LastResult();
				if (requested)
				{
					REX::INFO(
						"dmui-forwarding-smoke: dialog kind=text event=pending "
						"result={} requests={}"sv,
						DMUI_ResultToString(dialogResult_),
						dialogRequests_);
				}
				else
				{
					REX::ERROR(
						"dmui-forwarding-smoke: dialog kind=text "
						"request-failed result={}"sv,
						DMUI_ResultToString(dialogResult_));
				}
			}

			void PollDialog() noexcept
			{
				if (dialog_ == DMUI_INVALID_DIALOG_HANDLE)
					return;
				std::string text;
				const auto event = client_.PollDialogEvent(dialog_, text);
				dialogResult_ = client_.LastResult();
				if (!event)
					return;
				lastDialogEvent_ = event->kind;
				switch (event->kind)
				{
				case DMUI_DIALOG_EVENT_SUBMITTED:
					if (event->submissionId != activeSubmission_)
					{
						if (event->submissionId <= highestSubmission_)
						{
							if (event->submissionId != lastDuplicateSubmission_)
							{
								lastDuplicateSubmission_ = event->submissionId;
								++duplicateSubmissions_;
								REX::INFO(
									"dmui-forwarding-smoke: dialog event=duplicate "
									"submission={} duplicates={}"sv,
									event->submissionId,
									duplicateSubmissions_);
							}
							return;
						}
						highestSubmission_ = event->submissionId;
						activeSubmission_ = event->submissionId;
						submittedText_ = std::move(text);
						resolveAtFrame_ = frameCount_ + kDialogDelayFrames;
						resolutionSent_ = false;
						++dialogSubmissions_;
						REX::INFO(
							"dmui-forwarding-smoke: dialog event=submitted "
							"submission={} submissions={}"sv,
							activeSubmission_,
							dialogSubmissions_);
					}
					else if (resolutionSent_)
					{
						if (event->submissionId != lastDuplicateSubmission_)
						{
							lastDuplicateSubmission_ = event->submissionId;
							++duplicateSubmissions_;
							REX::INFO(
								"dmui-forwarding-smoke: dialog event=duplicate "
								"submission={} duplicates={}"sv,
								event->submissionId,
								duplicateSubmissions_);
						}
						return;
					}
					if (frameCount_ >= resolveAtFrame_ && !resolutionSent_)
						ResolveSubmission();
					break;
				case DMUI_DIALOG_EVENT_CANCELLED:
					++dialogCancellations_;
					REX::INFO(
						"dmui-forwarding-smoke: dialog event=cancelled "
						"cancellations={}"sv,
						dialogCancellations_);
					ClearDialog();
					break;
				case DMUI_DIALOG_EVENT_COMPLETED:
					if (dialogProbe_ == DialogProbe::kConfirm)
						++confirmOperations_;
					else if (dialogProbe_ == DialogProbe::kText)
					{
						if (acceptedNames_.size() < kMaximumAcceptedNames)
							acceptedNames_.push_back(submittedText_);
						++textAccepts_;
					}
					REX::INFO(
						"dmui-forwarding-smoke: dialog event=completed "
						"confirm-operations={} text-accepts={} text-rejects={} "
						"cancellations={}"sv,
						confirmOperations_,
						textAccepts_,
						textRejects_,
						dialogCancellations_);
					ClearDialog();
					break;
				default:
					break;
				}
			}

			void ResolveSubmission() noexcept
			{
				bool accepted{ true };
				const char* error{ nullptr };
				const auto submission = activeSubmission_;
				if (dialogProbe_ == DialogProbe::kText)
				{
					if (submittedText_.empty())
					{
						accepted = false;
						error = "A non-empty name is required.";
					}
					else if (submittedText_ == "reject" ||
						std::ranges::find(acceptedNames_, submittedText_) !=
							acceptedNames_.end())
					{
						accepted = false;
						error = "That sentinel or in-memory name is already used.";
					}
					else if (acceptedNames_.size() >= kMaximumAcceptedNames)
					{
						accepted = false;
						error = "The bounded in-memory name list is full.";
					}
				}
				if (!accepted && rejectWithoutMessage_)
					error = nullptr;

				const auto resolved = client_.ResolveDialogSubmission(
						dialog_,
						activeSubmission_,
						accepted,
						error);
				if (resolved)
				{
					resolutionSent_ = true;
					if (!accepted)
					{
						++textRejects_;
						activeSubmission_ = 0;
					}
				}
				dialogResult_ = client_.LastResult();
				REX::INFO(
					"dmui-forwarding-smoke: dialog event=resolution "
					"submission={} accepted={} result={} rejects={}"sv,
					submission,
					accepted,
					DMUI_ResultToString(dialogResult_),
					textRejects_);
			}

			void ClearDialog() noexcept
			{
				dialog_ = DMUI_INVALID_DIALOG_HANDLE;
				dialogProbe_ = DialogProbe::kNone;
				activeSubmission_ = 0;
				submittedText_.clear();
				resolutionSent_ = false;
			}

			void ScheduleDelayedNotification() noexcept
			{
				std::scoped_lock lock{ workerMutex_ };
				if (!workerPostingAllowed_.load(std::memory_order_acquire) ||
					client_.UnavailableReason() != DMUI_UNAVAILABLE_NONE)
				{
					++workerSuppressed_;
					REX::INFO(
						"dmui-forwarding-smoke: delayed notification "
						"schedule=suppressed unavailable-reason={} count={}"sv,
						static_cast<uint32_t>(client_.UnavailableReason()),
						workerSuppressed_.load());
					return;
				}

				auto expected = false;
				if (!workerBusy_.compare_exchange_strong(
						expected,
						true,
						std::memory_order_acq_rel))
				{
					++workerBusyRejections_;
					REX::INFO(
						"dmui-forwarding-smoke: delayed notification "
						"schedule=busy-rejected count={}"sv,
						workerBusyRejections_);
					return;
				}

				REX::INFO(
					"dmui-forwarding-smoke: delayed notification schedule=accepted"sv);
				++notificationSchedules_;
				notificationWorker_ = std::jthread([this](std::stop_token a_stop) {
					std::this_thread::sleep_for(650ms);
					{
						std::scoped_lock workerLock{ workerMutex_ };
						if (a_stop.stop_requested() ||
							!workerPostingAllowed_.load(
								std::memory_order_acquire) ||
							client_.UnavailableReason() !=
								DMUI_UNAVAILABLE_NONE)
						{
							++workerSuppressed_;
							REX::INFO(
								"dmui-forwarding-smoke: delayed notification "
								"post=suppressed unavailable-reason={} count={}"sv,
								static_cast<uint32_t>(
									client_.UnavailableReason()),
								workerSuppressed_.load());
							workerBusy_.store(
								false,
								std::memory_order_release);
							return;
						}
						const auto posted = client_.PostNotification(
							DMUI_STATUS_SEVERITY_INFO,
							"Forwarding smoke: delayed notification posted "
							"from a bounded worker thread.",
							4000);
						notificationResult_.store(
							client_.LastResult(),
							std::memory_order_release);
						if (posted)
							++delayedNotifications_;
						REX::INFO(
							"dmui-forwarding-smoke: delayed notification "
							"post={} result={} count={}"sv,
							posted,
							DMUI_ResultToString(notificationResult_.load()),
							delayedNotifications_.load());
					}
					workerBusy_.store(false, std::memory_order_release);
				});
			}

			void OnHotkey(size_t a_index, bool a_pressed) noexcept
			{
				auto& probe = hotkeys_[a_index];
				if (a_pressed)
					++probe.presses;
				else
					++probe.releases;
				REX::INFO(
					"dmui-forwarding-smoke: hotkey id={} event={} down={} up={}"sv,
					probe.id,
					a_pressed ? "press" : "release",
					probe.presses.load(),
					probe.releases.load());
				if (!a_pressed)
					return;
				if (a_index == 0)
					SetOverlayEnabled(!overlayEnabled_);
				else if (a_index == 1)
					ScheduleDelayedNotification();
			}

			void QueryHotkeys() noexcept
			{
				for (auto& probe : hotkeys_)
				{
					if (probe.handle == DMUI_INVALID_HOTKEY_ACTION_HANDLE)
						continue;
					if (const auto binding =
							client_.QueryHotkeyBinding(probe.handle))
					{
						const auto changed =
							binding->state != probe.binding.state ||
							std::strcmp(
								binding->chord,
								probe.binding.chord) != 0;
						probe.binding = *binding;
						probe.lastResult = client_.LastResult();
						if (changed)
						{
							REX::INFO(
								"dmui-forwarding-smoke: hotkey binding "
								"id={} effective={} state={} result={}"sv,
								probe.id,
								probe.binding.chord[0] ?
									probe.binding.chord :
									"none",
								BindingStateName(probe.binding.state),
								DMUI_ResultToString(probe.lastResult));
						}
					}
					else
						probe.lastResult = client_.LastResult();
				}
			}

			void DrawHotkeyControls() noexcept
			{
				(void)client_.DrawSectionHeader("Official contextual hotkeys");
				const auto table = client_.BeginSettingsTable("hotkeys");
				if (!table)
					return;
				if (!*table)
					return;

				for (size_t index = 0; index < hotkeys_.size(); ++index)
				{
					auto& probe = hotkeys_[index];
					if (!DrawSimpleRow(
							probe.id,
							probe.name,
							"Enablement uses the official host manager.",
							[this, &probe] {
								auto enabled = probe.enabled;
								if (ImGui::Checkbox("##Enabled", &enabled))
								{
									const auto changed =
										client_.SetHotkeyActionEnabled(
											probe.handle,
											enabled);
									hotkeyResult_ = client_.LastResult();
									probe.lastResult = hotkeyResult_;
									if (changed)
										probe.enabled = enabled;
									REX::INFO(
										"dmui-forwarding-smoke: hotkey enable "
										"id={} requested={} applied={} result={}"sv,
										probe.id,
										enabled,
										changed,
										DMUI_ResultToString(hotkeyResult_));
								}
								ImGui::SameLine();
								ImGui::Text(
									"%s | %s | down/up %llu/%llu",
									probe.binding.chord[0] ?
										probe.binding.chord :
										"none",
									BindingStateName(probe.binding.state),
									probe.presses.load(),
									probe.releases.load());
							}))
					{
						(void)client_.EndSettingsTable();
						return;
					}
				}
				(void)client_.EndSettingsTable();
				ImGui::TextDisabled(
					"Defaults: Ctrl+Shift+F10/F11 gameplay-unobstructed. "
					"HOST_INPUT_INACTIVE, optional ALWAYS, letter A, and "
					"digit 7 probes default to NONE; bind them in the host manager.");
			}

			template <size_t N>
			static void CopyText(
				std::array<char, N>& a_destination,
				std::string_view a_text) noexcept
			{
				a_destination.fill('\0');
				const auto length = (std::min)(a_text.size(), N - 1);
				std::memcpy(a_destination.data(), a_text.data(), length);
			}

			dmui::Client client_;
			std::atomic_bool unavailableLogged_{};
			InitializationStatus initializationStatus_{
				InitializationStatus::kPending
			};
			std::string_view initializationStage_{ "pending" };
			DMUI_Result initializationResult_{ DMUI_RESULT_OK };
			dmui::HostServices services_{};
			DMUI_Result servicesResult_{ DMUI_RESULT_OK };
			DMUI_HostStateInfo hostState_{};
			DMUI_Result stateResult_{ DMUI_RESULT_OK };

			DMUI_PageHandle settingsPage_{ DMUI_INVALID_PAGE_HANDLE };
			DMUI_PageHandle overlayPage_{ DMUI_INVALID_PAGE_HANDLE };
			bool settingsPageActive_{};
			uint64_t snapshotsLogged_{};
			DMUI_ManagedOverlayOptions overlayOptions_{};
			DMUI_ManagedOverlayPlacement overlayPlacement_{};
			bool overlayEnabled_{};
			bool menuVisible_{};
			uint64_t overlayDemandAttempts_{};
			uint64_t frameRequests_{};
			uint64_t frameReleases_{};
			uint64_t arrangementCompletions_{};

			std::chrono::steady_clock::time_point startTime_{};
			std::chrono::steady_clock::time_point previousFrame_{};
			double elapsedSeconds_{};
			uint64_t frameCount_{};
			uint64_t hiddenMenuObservations_{};
			uint64_t settingsDraws_{};
			uint64_t overlayDraws_{};
			std::array<float, kSampleCount> samples_{};
			size_t sampleOffset_{};
			uint64_t plotDraws_{};
			DMUI_Result plotResult_{ DMUI_RESULT_OK };

			ComPtr<ID3D11Device> imageDevice_;
			ComPtr<ID3D11Texture2D> imageTexture_;
			ComPtr<ID3D11ShaderResourceView> imageView_;
			std::optional<dmui::ImageResource> image_;
			std::atomic_bool recreateImage_{};
			bool deviceWaitingLogged_{};
			uint64_t imageImportCount_{};
			uint64_t imageDrawCount_{};
			uint64_t imageReleaseCount_{};
			uint64_t imageCycleRequests_{};
			uint64_t imageFailureCount_{};
			uint64_t deviceChangeCount_{};
			bool imageFailureActive_{};
			std::string_view lastImageFailureScope_;
			DMUI_Result lastImageFailureResult_{ DMUI_RESULT_OK };
			uint64_t imageGeneration_{};
			DMUI_ImageStatus imageStatus_{ DMUI_IMAGE_STATUS_RELEASED };
			DMUI_Result imageResult_{ DMUI_RESULT_OK };
			std::optional<dmui::ImageResource> cpuImage_;
			uint32_t cpuImageStep_{};
			uint32_t cpuImageWidth_{};
			uint32_t cpuImageHeight_{};
			uint64_t cpuImageCreateCount_{};
			uint64_t cpuImageUpdateCount_{};
			uint64_t cpuImageDrawCount_{};
			uint64_t cpuImageReleaseCount_{};
			DMUI_Result cpuImageResult_{ DMUI_RESULT_OK };

			std::array<char, 96> shortText_{
				's', 'm', 'o', 'k', 'e', '\0'
			};
			std::array<char, 256> multiline_{
				'l', 'i', 'n', 'e', ' ', 'o', 'n', 'e', '\n',
				'l', 'i', 'n', 'e', ' ', 't', 'w', 'o', '\0'
			};
			float sliderValue_{ 50.0f };
			EditCounters textEdits_;
			EditCounters sliderEdits_;
			EditCounters multilineEdits_;
			uint64_t simulatedSaves_{};

			std::array<HotkeyProbe, kHotkeyDescriptors.size()> hotkeys_{};
			DMUI_Result hotkeyResult_{ DMUI_RESULT_OK };

			std::mutex workerMutex_;
			std::jthread notificationWorker_;
			std::atomic_bool workerBusy_{};
			std::atomic_bool workerPostingAllowed_{};
			std::atomic<uint64_t> delayedNotifications_{};
			std::atomic<DMUI_Result> notificationResult_{ DMUI_RESULT_OK };
			uint64_t pageNotifications_{};
			uint64_t notificationSchedules_{};
			uint64_t workerBusyRejections_{};
			std::atomic<uint64_t> workerSuppressed_{};

			DMUI_DialogHandle dialog_{ DMUI_INVALID_DIALOG_HANDLE };
			DialogProbe dialogProbe_{ DialogProbe::kNone };
			DMUI_DialogEventKind lastDialogEvent_{ DMUI_DIALOG_EVENT_PENDING };
			uint64_t activeSubmission_{};
			uint64_t highestSubmission_{};
			uint64_t resolveAtFrame_{};
			uint64_t lastDuplicateSubmission_{};
			bool resolutionSent_{};
			bool rejectWithoutMessage_{};
			std::string submittedText_;
			static constexpr size_t kMaximumAcceptedNames{ 32 };
			std::vector<std::string> acceptedNames_{ "alpha" };
			uint64_t dialogSubmissions_{};
			uint64_t dialogRequests_{};
			uint64_t dialogRequestAttempts_{};
			uint64_t duplicateSubmissions_{};
			uint64_t confirmOperations_{};
			uint64_t textAccepts_{};
			uint64_t textRejects_{};
			uint64_t dialogCancellations_{};
			DMUI_Result dialogResult_{ DMUI_RESULT_OK };
			DMUI_Result overlayResult_{ DMUI_RESULT_OK };
		};

		[[nodiscard]] State& GetState()
		{
			// F4SE plugins and registered callbacks are process-lived. Deliberately
			// retain the state so callbacks, COM leases, and worker storage cannot
			// be destructed before the host at executable shutdown.
			static auto* state = new State;
			static const auto teardownRegistered = std::atexit([]() noexcept {
				state->StopWorker();
			});
			(void)teardownRegistered;
			return *state;
		}

		enum class InitializationOutcome : uint8_t
		{
			kPending,
			kCompleted,
			kFailed
		};

		std::once_flag s_initializationOnce;
		std::atomic<InitializationOutcome> s_initializationOutcome{
			InitializationOutcome::kPending
		};

		void MessageListener(
			F4SE::MessagingInterface::Message* a_message) noexcept
		{
			if (!a_message ||
				a_message->type != F4SE::MessagingInterface::kPostPostLoad)
				return;

			std::call_once(s_initializationOnce, []() noexcept {
				const auto initialized = GetState().Initialize();
				s_initializationOutcome.store(
					initialized ?
						InitializationOutcome::kCompleted :
						InitializationOutcome::kFailed,
					std::memory_order_release);
				if (!initialized)
				{
					REX::ERROR(
						"dmui-forwarding-smoke: initialization failed at "
						"kPostPostLoad"sv);
				}
			});
		}

		[[nodiscard]] bool Load(
			const F4SE::LoadInterface* a_f4se) noexcept
		{
			if (!a_f4se)
				return false;

			F4SE::Init(a_f4se);
			const auto* messaging = F4SE::GetMessagingInterface();
			if (!messaging)
			{
				REX::ERROR(
					"dmui-forwarding-smoke: F4SE messaging interface unavailable"sv);
				return false;
			}
			if (!messaging->RegisterListener(MessageListener))
			{
				REX::ERROR(
					"dmui-forwarding-smoke: F4SE message listener registration failed"sv);
				return false;
			}
			return true;
		}
	}
}

F4SE_PLUGIN_QUERY(
	const F4SE::QueryInterface* a_f4se,
	F4SE::PluginInfo* a_info)
{
	if (!a_f4se || !a_info ||
		a_f4se->RuntimeVersion() < REL::Version(F4SE::RUNTIME_1_10_163))
		return false;

	if (const auto* data = F4SE::PluginVersionData::GetSingleton())
	{
		a_info->infoVersion = F4SE::PluginInfo::kVersion;
		a_info->name = data->GetPluginName().data();
		a_info->version = data->GetPluginVersion().pack();
	}
	return true;
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	return DmuiForwardingSmoke::Load(a_f4se);
}
