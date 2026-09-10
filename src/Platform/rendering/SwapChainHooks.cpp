#include "SwapChainHooks.h"

#include <Support/Detours.h>

#include <REX/REX.h>

#include <array>
#include <atomic>
#include <memory>

namespace Addictol::platformImguiDetail
{
	using namespace std::literals;
	using namespace ImguiPlatform;

	namespace
	{
		struct HookRecord
		{
			std::atomic<bool> claimed{ false };
			std::atomic<void**> vtable{ nullptr };
			std::atomic<PresentHook> present{ nullptr };
			std::atomic<ResizeBuffersHook> resizeBuffers{ nullptr };
			std::atomic<bool> presentInstalled{ false };
			std::atomic<bool> resizeBuffersInstalled{ false };
		};

		struct DispatchRecord
		{
			std::atomic<bool> claimed{ false };
			std::atomic<IDXGISwapChain*> swapChain{ nullptr };
			std::atomic<HookRecord*> hook{ nullptr };
		};

		constexpr size_t kHookCapacity = 8;
		constexpr size_t kDispatchCapacity = 16;
		std::array<HookRecord, kHookCapacity> s_hooks{};
		std::array<DispatchRecord, kDispatchCapacity> s_dispatches{};

		[[nodiscard]] HookRecord* FindHook(void** a_vtable) noexcept
		{
			for (auto& record : s_hooks)
				if (record.vtable.load(std::memory_order_acquire) == a_vtable)
					return std::addressof(record);
			return nullptr;
		}

		[[nodiscard]] HookRecord* FindAssociatedHook(
			IDXGISwapChain* a_swapChain) noexcept
		{
			const auto swapChain = reinterpret_cast<uintptr_t>(a_swapChain);
			for (auto& dispatch : s_dispatches)
			{
				const auto associated =
					dispatch.swapChain.load(std::memory_order_acquire);
				if (MatchHookDispatch(
						swapChain,
						0,
						reinterpret_cast<uintptr_t>(associated),
						0) == HookDispatchMatch::kSwapChain)
					return dispatch.hook.load(std::memory_order_acquire);
			}
			return nullptr;
		}

		[[nodiscard]] HookRecord* FindDispatch(
			IDXGISwapChain* a_swapChain) noexcept
		{
			if (auto* associated = FindAssociatedHook(a_swapChain))
				return associated;
			return FindHook(*reinterpret_cast<void***>(a_swapChain));
		}

		[[nodiscard]] bool Associate(
			IDXGISwapChain* a_swapChain,
			HookRecord& a_hook) noexcept
		{
			for (auto& dispatch : s_dispatches)
			{
				if (dispatch.swapChain.load(std::memory_order_acquire) == a_swapChain)
				{
					dispatch.hook.store(std::addressof(a_hook), std::memory_order_release);
					return true;
				}
			}
			for (auto& dispatch : s_dispatches)
			{
				bool expected{ false };
				if (!dispatch.claimed.compare_exchange_strong(
						expected, true, std::memory_order_acq_rel))
					continue;
				dispatch.hook.store(std::addressof(a_hook), std::memory_order_relaxed);
				dispatch.swapChain.store(a_swapChain, std::memory_order_release);
				return true;
			}
			REX::ERROR("Platform Imgui: swapchain dispatch capacity exhausted"sv);
			return false;
		}

		[[nodiscard]] bool PatchPresent(
			HookRecord& a_record,
			void** a_vtable,
			PresentHook a_hook) noexcept
		{
			if (a_record.presentInstalled.load(std::memory_order_acquire))
				return true;
			const auto current =
				reinterpret_cast<PresentHook>(a_vtable[kPresentSlot]);
			if (!current)
				return false;
			if (current == a_hook)
			{
				if (a_record.present.load(std::memory_order_acquire) == a_hook)
					return false;
				a_record.presentInstalled.store(true, std::memory_order_release);
				return true;
			}

			a_record.present.store(current, std::memory_order_release);
			const auto previous = Support::DetourVTable(
				reinterpret_cast<uintptr_t>(a_vtable),
				reinterpret_cast<uintptr_t>(a_hook),
				kPresentSlot);
			if (!previous)
				return false;
			a_record.present.store(
				reinterpret_cast<PresentHook>(previous),
				std::memory_order_release);
			a_record.presentInstalled.store(true, std::memory_order_release);
			return true;
		}

		[[nodiscard]] bool PatchResizeBuffers(
			HookRecord& a_record,
			void** a_vtable,
			ResizeBuffersHook a_hook) noexcept
		{
			if (a_record.resizeBuffersInstalled.load(std::memory_order_acquire))
				return true;
			const auto current =
				reinterpret_cast<ResizeBuffersHook>(a_vtable[kResizeBuffersSlot]);
			if (!current)
				return false;
			if (current == a_hook)
			{
				if (a_record.resizeBuffers.load(std::memory_order_acquire) == a_hook)
					return false;
				a_record.resizeBuffersInstalled.store(true, std::memory_order_release);
				return true;
			}

			a_record.resizeBuffers.store(current, std::memory_order_release);
			const auto previous = Support::DetourVTable(
				reinterpret_cast<uintptr_t>(a_vtable),
				reinterpret_cast<uintptr_t>(a_hook),
				kResizeBuffersSlot);
			if (!previous)
				return false;
			a_record.resizeBuffers.store(
				reinterpret_cast<ResizeBuffersHook>(previous),
				std::memory_order_release);
			a_record.resizeBuffersInstalled.store(true, std::memory_order_release);
			return true;
		}
	}

	PresentHook SwapChainHooks::PreviousPresent(
		IDXGISwapChain* a_swapChain,
		PresentHook a_presentHook) noexcept
	{
		auto* record = FindDispatch(a_swapChain);
		const auto previous =
			record ? record->present.load(std::memory_order_acquire) : nullptr;
		return previous != a_presentHook ? previous : nullptr;
	}

	ResizeBuffersHook SwapChainHooks::PreviousResizeBuffers(
		IDXGISwapChain* a_swapChain,
		ResizeBuffersHook a_resizeBuffersHook) noexcept
	{
		auto* record = FindDispatch(a_swapChain);
		const auto previous =
			record ? record->resizeBuffers.load(std::memory_order_acquire) : nullptr;
		return previous != a_resizeBuffersHook ? previous : nullptr;
	}

	bool SwapChainHooks::Install(
		IDXGISwapChain* a_swapChain,
		AttachmentLifecycle a_lifecycle,
		PresentHook a_presentHook,
		ResizeBuffersHook a_resizeBuffersHook) noexcept
	{
		auto** vtable = *reinterpret_cast<void***>(a_swapChain);
		if (auto* associated = FindAssociatedHook(a_swapChain))
		{
			auto** capturedVtable =
				associated->vtable.load(std::memory_order_acquire);
			if (ReusesHookAssociation(
					a_lifecycle,
					reinterpret_cast<uintptr_t>(vtable),
					reinterpret_cast<uintptr_t>(capturedVtable)))
			{
				const auto resizeReady =
					PatchResizeBuffers(*associated, capturedVtable, a_resizeBuffersHook);
				const auto presentReady =
					PatchPresent(*associated, capturedVtable, a_presentHook);
				return resizeReady && presentReady;
			}
		}

		auto* record = FindHook(vtable);
		if (!record)
		{
			for (auto& candidate : s_hooks)
			{
				bool expected{ false };
				if (!candidate.claimed.compare_exchange_strong(
						expected, true, std::memory_order_acq_rel))
					continue;
				candidate.present.store(
					reinterpret_cast<PresentHook>(vtable[kPresentSlot]),
					std::memory_order_release);
				candidate.resizeBuffers.store(
					reinterpret_cast<ResizeBuffersHook>(vtable[kResizeBuffersSlot]),
					std::memory_order_release);
				candidate.vtable.store(vtable, std::memory_order_release);
				record = std::addressof(candidate);
				break;
			}
		}
		if (!record)
		{
			REX::ERROR("Platform Imgui: swapchain vtable hook capacity exhausted"sv);
			return false;
		}
		if (!Associate(a_swapChain, *record))
			return false;

		const auto resizeReady =
			PatchResizeBuffers(*record, vtable, a_resizeBuffersHook);
		const auto presentReady = PatchPresent(*record, vtable, a_presentHook);
		if (!resizeReady || !presentReady)
		{
			REX::ERROR(
				"Platform Imgui: swapchain vtable patch failed (Present {}, ResizeBuffers {})"sv,
				presentReady,
				resizeReady);
			return false;
		}
		return true;
	}
}
