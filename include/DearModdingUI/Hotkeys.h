#pragma once

#include <DearModdingUI/API.h>
#include <DearModdingUI/MenuToggleKey.h>

#include <array>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace DearModdingUI
{
	inline constexpr uint32_t kHotkeyModifierShift{ 1u << 0 };
	inline constexpr uint32_t kHotkeyModifierControl{ 1u << 1 };
	inline constexpr uint32_t kHotkeyModifierAlt{ 1u << 2 };

	struct HotkeyChord
	{
		uint32_t virtualKey{ 0 };
		uint32_t modifiers{ 0 };

		[[nodiscard]] constexpr bool operator==(const HotkeyChord&) const noexcept = default;
		[[nodiscard]] constexpr bool IsNone() const noexcept
		{
			return virtualKey == 0;
		}
	};

	struct ParsedHotkeyChord
	{
		HotkeyChord chord;
		bool recognized{ false };
	};

	enum class HotkeyMessageResult : uint32_t
	{
		kPassThrough,
		kConsumed,
		kConsumedPairDropped
	};

	inline constexpr size_t kHotkeyEventQueueCapacity{ 512 };

	[[nodiscard]] bool ValidHotkeyActionId(std::string_view a_id) noexcept;
	[[nodiscard]] ParsedHotkeyChord ParseHotkeyChord(std::string_view a_value) noexcept;
	[[nodiscard]] std::string SerializeHotkeyChord(HotkeyChord a_chord);

	struct HotkeyActionSnapshot
	{
		std::string id;
		std::string displayName;
		std::string suggestedDefaultChord;
		std::string overrideChord;
		std::string effectiveChord;
		DMUI_HotkeyBindingState state{ DMUI_HOTKEY_BINDING_UNBOUND_NEVER_SET };
		bool registered{ false };
	};

	class HotkeyRegistry
	{
	public:
		void BindRenderThread() noexcept;
		void InitializeOverrides(std::map<std::string, std::string> a_overrides) noexcept;
		void SetReservedVirtualKey(uint32_t a_virtualKey) noexcept;
		[[nodiscard]] DMUI_Result Register(
			DMUI_ClientHandle a_client,
			const DMUI_HotkeyActionDescriptor* a_descriptor,
			DMUI_HotkeyActionHandle* a_action) noexcept;
		[[nodiscard]] DMUI_Result Query(
			DMUI_ClientHandle a_client,
			DMUI_HotkeyActionHandle a_action,
			DMUI_HotkeyBindingInfo* a_binding) const noexcept;
		[[nodiscard]] DMUI_Result Unregister(
			DMUI_ClientHandle a_client,
			DMUI_HotkeyActionHandle a_action) noexcept;
		[[nodiscard]] DMUI_Result SetOverride(
			std::string_view a_id,
			std::string_view a_chord) noexcept;
		[[nodiscard]] bool RemoveOverride(std::string_view a_id) noexcept;
		[[nodiscard]] HotkeyMessageResult HandleKey(
			uint32_t a_virtualKey,
			uint32_t a_modifiers,
			bool a_pressed,
			bool a_repeat) noexcept;
		void DispatchQueued() noexcept;
		[[nodiscard]] std::vector<HotkeyActionSnapshot> Snapshot() const noexcept;
		[[nodiscard]] std::map<std::string, std::string> Overrides() const noexcept;

	private:
		struct Action
		{
			DMUI_HotkeyActionHandle handle{ DMUI_INVALID_HOTKEY_ACTION_HANDLE };
			DMUI_ClientHandle client{ DMUI_INVALID_CLIENT_HANDLE };
			std::string id;
			std::string displayName;
			HotkeyChord suggestedDefault;
			std::string suggestedDefaultChord;
			HotkeyChord effective;
			DMUI_HotkeyBindingState state{ DMUI_HOTKEY_BINDING_UNBOUND_NEVER_SET };
			DMUI_HotkeyCallback callback{ nullptr };
			void* userData{ nullptr };
			bool callbackFailed{ false };
			// Erase invalidates references, so we do not erase action slots.
			bool live{ true };
		};

		struct Event
		{
			DMUI_HotkeyActionHandle action{ DMUI_INVALID_HOTKEY_ACTION_HANDLE };
			bool pressed{ false };
		};

		struct ActiveKey
		{
			DMUI_HotkeyActionHandle action{ DMUI_INVALID_HOTKEY_ACTION_HANDLE };
			bool queued{ false };
		};

		void RecomputeBindingsLocked() noexcept;
		[[nodiscard]] Action* FindActionLocked(DMUI_HotkeyActionHandle a_action) noexcept;
		[[nodiscard]] const Action* FindActionLocked(
			DMUI_HotkeyActionHandle a_action) const noexcept;

		mutable std::mutex m_mutex;
		std::vector<Action> m_actions;
		std::map<std::string, std::string> m_overrides;
		std::array<Event, kHotkeyEventQueueCapacity> m_events{};
		std::array<ActiveKey, 256> m_activeKeys{};
		size_t m_eventHead{ 0 };
		size_t m_eventCount{ 0 };
		size_t m_reservedReleaseCount{ 0 };
		DMUI_HotkeyActionHandle m_nextAction{ 1 };
		uint32_t m_reservedVirtualKey{ 0 };
		std::thread::id m_renderThread;
	};

	namespace Hotkeys
	{
		void BindRenderThread() noexcept;
		void InitializeOverrides(std::map<std::string, std::string> a_overrides) noexcept;
		void SetReservedVirtualKey(uint32_t a_virtualKey) noexcept;
		[[nodiscard]] DMUI_Result Register(
			DMUI_ClientHandle a_client,
			const DMUI_HotkeyActionDescriptor* a_descriptor,
			DMUI_HotkeyActionHandle* a_action) noexcept;
		[[nodiscard]] DMUI_Result Query(
			DMUI_ClientHandle a_client,
			DMUI_HotkeyActionHandle a_action,
			DMUI_HotkeyBindingInfo* a_binding) noexcept;
		[[nodiscard]] DMUI_Result Unregister(
			DMUI_ClientHandle a_client,
			DMUI_HotkeyActionHandle a_action) noexcept;
		[[nodiscard]] HotkeyMessageResult HandleKey(
			uint32_t a_virtualKey,
			uint32_t a_modifiers,
			bool a_pressed,
			bool a_repeat) noexcept;
		void DispatchQueued() noexcept;
		[[nodiscard]] std::vector<HotkeyActionSnapshot> Snapshot() noexcept;
		[[nodiscard]] std::map<std::string, std::string> Overrides() noexcept;
		[[nodiscard]] DMUI_Result SetOverride(
			std::string_view a_id,
			std::string_view a_chord) noexcept;
		[[nodiscard]] bool RemoveOverride(std::string_view a_id) noexcept;
	}
}
