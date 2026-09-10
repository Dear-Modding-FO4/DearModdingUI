#include "../support/D3DTestResources.h"
#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace vmm_tests
{
	using namespace DearModdingUI;
	using Microsoft::WRL::ComPtr;
	using support::CreateImageResources;

	void run_presentation_render_execution_checks(Runner& runner)
	{
		runner.test("active Present scopes migrate without rebinding the device", [] {
			auto resources = CreateImageResources();
			std::mutex mutex;
			std::condition_variable condition;
			bool startupReady{};
			bool attemptPastThread{};
			bool pastThreadDone{};
			uint64_t startupGeneration{};
			DMUI_Result pastThreadResult{ DMUI_RESULT_OK };

			std::thread startup{ [&] {
				{
					RenderExecution::Guard execution{
						RenderExecution::Phase::kBackendInitialization
					};
					(void)execution.NoteBinding(41);
					PresentationServices::SetDevice(resources.device.Get());
					startupGeneration = PresentationServices::DeviceGeneration();
				}
				{
					std::unique_lock lock{ mutex };
					startupReady = true;
					condition.notify_all();
					condition.wait(lock, [&] { return attemptPastThread; });
				}
				const DMUI_D3D11ImageDescriptor descriptor{
					sizeof(DMUI_D3D11ImageDescriptor),
					resources.view.Get(),
					0,
					0
				};
				DMUI_ImageHandle image{};
				pastThreadResult = PresentationServices::ImportD3D11Image(
					42, &descriptor, &image);
				{
					const std::scoped_lock lock{ mutex };
					pastThreadDone = true;
				}
				condition.notify_all();
			} };

			{
				std::unique_lock lock{ mutex };
				condition.wait(lock, [&] { return startupReady; });
			}
			const auto renderGeneration = PresentationServices::DeviceGeneration();
			require(renderGeneration == startupGeneration,
				"startup publication changed the device generation unexpectedly");

			{
				RenderExecution::Guard execution{
					RenderExecution::Phase::kFrameObservation
				};
				(void)execution.NoteBinding(41);
				{
					const RenderExecution::ClientGuard observer{
						42, false
					};
					require(RenderExecution::IsActiveClient(42, false),
						"cold observer did not inherit active Present authorization");
					require(!RenderExecution::IsActiveClient(42, true),
						"cold observer acquired drawing authorization");
				}
				{
					const std::scoped_lock lock{ mutex };
					attemptPastThread = true;
				}
				condition.notify_all();

				struct DepthImport
				{
					DXGI_FORMAT texture;
					DXGI_FORMAT view;
				};
				constexpr std::array formats{
					DepthImport{
						DXGI_FORMAT_R24G8_TYPELESS,
						DXGI_FORMAT_R24_UNORM_X8_TYPELESS },
					DepthImport{
						DXGI_FORMAT_R32_TYPELESS,
						DXGI_FORMAT_R32_FLOAT }
				};
				for (const auto& format : formats)
				{
					const D3D11_TEXTURE2D_DESC textureDescription{
						16, 8, 1, 1, format.texture, { 1, 0 },
						D3D11_USAGE_DEFAULT,
						D3D11_BIND_SHADER_RESOURCE,
						0, 0
					};
					ComPtr<ID3D11Texture2D> texture;
					require(SUCCEEDED(resources.device->CreateTexture2D(
								&textureDescription, nullptr, &texture)),
						"cross-thread depth texture creation failed");
					D3D11_SHADER_RESOURCE_VIEW_DESC viewDescription{};
					viewDescription.Format = format.view;
					viewDescription.ViewDimension =
						D3D11_SRV_DIMENSION_TEXTURE2D;
					viewDescription.Texture2D.MipLevels = 1;
					ComPtr<ID3D11ShaderResourceView> view;
					require(SUCCEEDED(resources.device->CreateShaderResourceView(
								texture.Get(), &viewDescription, &view)),
						"cross-thread depth SRV creation failed");
					const DMUI_D3D11ImageDescriptor descriptor{
						sizeof(DMUI_D3D11ImageDescriptor),
						view.Get(),
						0,
						0
					};
					DMUI_ImageHandle image{};
					require(PresentationServices::ImportD3D11Image(
								42, &descriptor, &image) == DMUI_RESULT_OK,
						"authorized migrated observer rejected a depth SRV");
					require(PresentationServices::ReleaseImage(42, image) ==
							DMUI_RESULT_OK,
						"cross-thread depth image release failed");
				}

				std::unique_lock lock{ mutex };
				condition.wait(lock, [&] { return pastThreadDone; });
				require(pastThreadResult == DMUI_RESULT_WRONG_THREAD,
					"past startup thread retained render authorization");
			}
			startup.join();

			const DMUI_D3D11ImageDescriptor descriptor{
				sizeof(DMUI_D3D11ImageDescriptor),
				resources.view.Get(),
				0,
				0
			};
			DMUI_ImageHandle image{};
			require(PresentationServices::ImportD3D11Image(
						42, &descriptor, &image) == DMUI_RESULT_WRONG_THREAD,
				"completed Present scope left its caller authorized");
			require(PresentationServices::DeviceGeneration() == renderGeneration,
				"thread migration invalidated the stable render device");
			PresentationServices::InvalidateDevice();
		});

		runner.test("overlapping Present callback scopes serialize and restore nesting", [] {
			std::mutex mutex;
			std::condition_variable condition;
			bool firstEntered{};
			bool releaseFirst{};
			bool secondAttempted{};
			bool secondEntered{};
			bool releaseSecond{};
			bool firstAuthorized{};
			bool nestedAuthorized{};
			bool outerRestored{};
			bool secondStillAuthorized{};
			bool firstThreadReported{};
			bool secondThreadReported{};
			bool overlapped{};
			int active{};
			int maximumActive{};

			std::thread first{ [&] {
				RenderExecution::Guard execution{
					RenderExecution::Phase::kFrameObservation
				};
				const auto transition = execution.NoteBinding(7);
				firstThreadReported =
					transition.currentThread ==
					static_cast<uint64_t>(::GetCurrentThreadId());
				{
					const RenderExecution::ClientGuard client{
						70, false
					};
					firstAuthorized =
						RenderExecution::IsActiveClient(70, false);
				}
				std::unique_lock lock{ mutex };
				++active;
				maximumActive = (std::max)(maximumActive, active);
				firstEntered = true;
				condition.notify_all();
				condition.wait(lock, [&] { return releaseFirst; });
				--active;
			} };

			{
				std::unique_lock lock{ mutex };
				condition.wait(lock, [&] { return firstEntered; });
			}

			std::thread second{ [&] {
				{
					const std::scoped_lock lock{ mutex };
					secondAttempted = true;
				}
				condition.notify_all();
				RenderExecution::Guard execution{
					RenderExecution::Phase::kFrameDraw
				};
				const auto transition = execution.NoteBinding(7);
				secondThreadReported =
					transition.currentThread ==
					static_cast<uint64_t>(::GetCurrentThreadId());
				const RenderExecution::ClientGuard outer{
					71, false
				};
				{
					RenderExecution::Guard nested{
						RenderExecution::Phase::kFrameDraw
					};
					(void)nested.NoteBinding(7);
					const RenderExecution::ClientGuard inner{
						72, true
					};
					nestedAuthorized =
						RenderExecution::ActivePhase() ==
							RenderExecution::Phase::kFrameDraw &&
						RenderExecution::ActiveBinding() == 7 &&
						RenderExecution::IsActiveClient(72, true);
				}
				outerRestored =
					RenderExecution::IsActiveClient(71, false) &&
					!RenderExecution::IsActiveClient(72, false);
				std::unique_lock lock{ mutex };
				++active;
				maximumActive = (std::max)(maximumActive, active);
				secondEntered = true;
				condition.notify_all();
				condition.wait(lock, [&] { return releaseSecond; });
				secondStillAuthorized =
					RenderExecution::IsActiveClient(71, false);
				--active;
			} };

			{
				std::unique_lock lock{ mutex };
				condition.wait(lock, [&] { return secondAttempted; });
				// Release the test mutex so only the execution gate can block entry.
				overlapped = condition.wait_for(
					lock,
					std::chrono::milliseconds{ 500 },
					[&] { return secondEntered; });
				releaseFirst = true;
			}
			condition.notify_all();
			{
				std::unique_lock lock{ mutex };
				condition.wait(lock, [&] { return secondEntered; });
				releaseSecond = true;
			}
			condition.notify_all();
			first.join();
			second.join();
			require(!overlapped && maximumActive == 1,
				"overlapping Present scopes entered client execution");
			require(firstThreadReported && secondThreadReported,
				"render migration diagnostics did not report OS thread IDs");
			require(firstAuthorized && nestedAuthorized && outerRestored &&
					secondStillAuthorized,
				"nested or handed-off execution authorization was not restored");

			const auto earlyReturn = [] {
				RenderExecution::Guard execution{
					RenderExecution::Phase::kFrameObservation
				};
				(void)execution.NoteBinding(8);
				return RenderExecution::IsActive();
			};
			require(earlyReturn() && !RenderExecution::IsActive(),
				"early return leaked render execution authorization");
			try
			{
				RenderExecution::Guard execution{
					RenderExecution::Phase::kFrameObservation
				};
				(void)execution.NoteBinding(8);
				throw std::runtime_error{ "guard release" };
			}
			catch (const std::runtime_error&)
			{}
			require(!RenderExecution::IsActive(),
				"exception unwinding leaked render execution authorization");
		});

	}
}
