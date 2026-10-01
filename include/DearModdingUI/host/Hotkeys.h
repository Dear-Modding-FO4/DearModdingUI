#pragma once

#include <DearModdingUI/API.h>
#include <Support/KeyCatalog.h>

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

	[[nodiscard]] constexpr bool IsHostBindableKey(uint32_t a_keyCode) noexcept
	{
		if (!KeyCatalog::Find(a_keyCode))
			return false;
		// Escape drives dismissal; standalone modifiers only qualify chords.
		switch (a_keyCode)
		{
		case 0x01:
		case 0x1D:
		case 0x9D:
		case 0x2A:
		case 0x36:
		case 0x38:
		case 0xB8:
		case KeyCatalog::kMouseButtonOffset:
		case KeyCatalog::kMouseButtonOffset + 1:
			return false;
		default:
			return true;
		}
	}

	enum class HotkeySlot : size_t
	{
		kKeyboardMouse,
		kGamepad
	};

	inline constexpr std::array kHotkeySlots{
		HotkeySlot::kKeyboardMouse, HotkeySlot::kGamepad
	};

	[[nodiscard]] constexpr HotkeySlot HotkeySlotForKey(uint32_t a_code) noexcept
	{
		return a_code >= KeyCatalog::kGamepadButtonOffset ?
			HotkeySlot::kGamepad : HotkeySlot::kKeyboardMouse;
	}

	struct HotkeyChord
	{
		std::array<uint32_t, 3> keys{};
		uint32_t modifiers{ 0 };

		constexpr HotkeyChord() noexcept = default;
		constexpr HotkeyChord(uint32_t a_key, uint32_t a_modifiers) noexcept :
			keys{ a_key, 0, 0 }, modifiers(a_modifiers)
		{}
		constexpr HotkeyChord(std::array<uint32_t, 3> a_keys, uint32_t a_modifiers) noexcept :
			keys(a_keys), modifiers(a_modifiers)
		{}
		[[nodiscard]] constexpr bool operator==(const HotkeyChord&) const noexcept = default;
		[[nodiscard]] constexpr auto operator<=>(const HotkeyChord&) const noexcept = default;
		[[nodiscard]] constexpr bool IsNone() const noexcept
		{
			return keys[0] == 0;
		}
		[[nodiscard]] constexpr bool FitsSlot(HotkeySlot a_slot) const noexcept
		{
			return IsNone() || HotkeySlotForKey(keys[0]) == a_slot;
		}
	};

	struct ParsedHotkeyChord
	{
		HotkeyChord chord;
		bool recognized{ false };
	};

	struct HotkeyContextState
	{
		bool hostMenuVisible{};
		bool dialogVisible{};
		bool textEditing{};
		// True only when the platform can affirm that no engine menu mode is active.
		bool gameplaySafe{};
	};

	enum class HotkeyMessageResult : uint32_t
	{
		kPassThrough,
		kConsumed,
		kMenuToggle,
		kConsumedPairDropped
	};

	inline constexpr size_t kHotkeyEventQueueCapacity{ 512 };

	[[nodiscard]] bool ValidHotkeyActionId(std::string_view a_id) noexcept;
	[[nodiscard]] ParsedHotkeyChord ParseHotkeyChord(std::string_view a_value) noexcept;
	[[nodiscard]] std::string SerializeHotkeyChord(HotkeyChord a_chord);
	[[nodiscard]] std::string FormatHotkeyChord(std::string_view a_chord);

	struct HotkeyBindingSnapshot
	{
		std::string overrideChord;
		std::string effectiveChord;
		DMUI_HotkeyBindingState state{ DMUI_HOTKEY_BINDING_UNBOUND_NEVER_SET };
	};

	struct HotkeyActionSnapshot
	{
		std::string id;
		std::string displayName;
		std::string suggestedDefaultChord;
		std::array<HotkeyBindingSnapshot, 2> bindings;
		bool registered{ false };
	};

	class HotkeyRegistry
	{
	public:
		void InitializeOverrides(std::map<std::string, std::string> a_overrides,
			HotkeySlot a_slot = HotkeySlot::kKeyboardMouse) noexcept;
		void SetReservedChord(HotkeyChord a_chord,
			HotkeySlot a_slot = HotkeySlot::kKeyboardMouse) noexcept;
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
		[[nodiscard]] DMUI_Result SetEnabled(
			DMUI_ClientHandle a_client,
			DMUI_HotkeyActionHandle a_action,
			bool a_enabled) noexcept;
		void SetContext(HotkeyContextState a_context) noexcept;
		void ReleaseActiveKeys() noexcept;
		void BeginCapture(HotkeySlot a_slot = HotkeySlot::kKeyboardMouse) noexcept;
		[[nodiscard]] bool CancelCapture() noexcept;
		[[nodiscard]] std::optional<HotkeyChord> TakeCapture() noexcept;
		[[nodiscard]] bool IsCapturing() const noexcept;
		[[nodiscard]] DMUI_Result SetOverride(
			std::string_view a_id,
			std::string_view a_chord,
			HotkeySlot a_slot = HotkeySlot::kKeyboardMouse) noexcept;
		[[nodiscard]] bool RemoveOverride(std::string_view a_id,
			HotkeySlot a_slot = HotkeySlot::kKeyboardMouse) noexcept;
		[[nodiscard]] HotkeyMessageResult HandleKey(
			uint32_t a_keyCode,
			uint32_t a_modifiers,
			bool a_pressed,
			bool a_repeat) noexcept;
		void DispatchQueued() noexcept;
		[[nodiscard]] std::vector<HotkeyActionSnapshot> Snapshot() const noexcept;
		[[nodiscard]] std::map<std::string, std::string> Overrides(
			HotkeySlot a_slot = HotkeySlot::kKeyboardMouse) const noexcept;

	private:
		struct Binding
		{
			HotkeyChord effective;
			DMUI_HotkeyBindingState state{ DMUI_HOTKEY_BINDING_UNBOUND_NEVER_SET };
		};

		struct Action
		{
			DMUI_HotkeyActionHandle handle{ DMUI_INVALID_HOTKEY_ACTION_HANDLE };
			DMUI_ClientHandle client{ DMUI_INVALID_CLIENT_HANDLE };
			std::string id;
			std::string displayName;
			HotkeyChord suggestedDefault;
			std::string suggestedDefaultChord;
			std::array<Binding, 2> bindings;
			DMUI_HotkeyCallback callback{ nullptr };
			void* userData{ nullptr };
			DMUI_HotkeyContextPolicy contextPolicy{ DMUI_HOTKEY_CONTEXT_ALWAYS };
			bool enabled{ true };
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
			bool captured{ false };
			bool toggle{ false };
			HotkeySlot slot{ HotkeySlot::kKeyboardMouse };

			[[nodiscard]] bool IsOwned() const noexcept
			{
				return action != DMUI_INVALID_HOTKEY_ACTION_HANDLE || captured || toggle;
			}
		};

		void RecomputeBindingsLocked() noexcept;
		void ReleaseKeyLocked(ActiveKey& a_active) noexcept;
		[[nodiscard]] Action* FindActionLocked(DMUI_HotkeyActionHandle a_action) noexcept;
		[[nodiscard]] const Action* FindActionLocked(
			DMUI_HotkeyActionHandle a_action) const noexcept;

		mutable std::mutex m_mutex;
		std::vector<Action> m_actions;
		std::array<std::map<std::string, std::string>, 2> m_overrides;
		std::array<Event, kHotkeyEventQueueCapacity> m_events{};
		std::array<ActiveKey, KeyCatalog::kMaximumMacroCode> m_activeKeys{};
		std::array<bool, KeyCatalog::kMaximumMacroCode> m_heldKeys{};
		size_t m_eventHead{ 0 };
		size_t m_eventCount{ 0 };
		size_t m_reservedReleaseCount{ 0 };
		DMUI_HotkeyActionHandle m_nextAction{ 1 };
		std::array<HotkeyChord, 2> m_reservedChords;
		HotkeyContextState m_context;
		bool m_capturing{ false };
		HotkeySlot m_captureSlot{ HotkeySlot::kKeyboardMouse };
		HotkeyChord m_captureKeys;
		bool m_captureValid{ true };
		std::optional<HotkeyChord> m_capture;
	};

	namespace Hotkeys
	{
		void InitializeOverrides(std::map<std::string, std::string> a_overrides,
			HotkeySlot a_slot = HotkeySlot::kKeyboardMouse) noexcept;
		void SetReservedChord(HotkeyChord a_chord,
			HotkeySlot a_slot = HotkeySlot::kKeyboardMouse) noexcept;
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
		[[nodiscard]] DMUI_Result SetEnabled(
			DMUI_ClientHandle a_client,
			DMUI_HotkeyActionHandle a_action,
			bool a_enabled) noexcept;
		void SetContext(HotkeyContextState a_context) noexcept;
		void ReleaseActiveKeys() noexcept;
		void BeginCapture(HotkeySlot a_slot = HotkeySlot::kKeyboardMouse) noexcept;
		[[nodiscard]] bool CancelCapture() noexcept;
		[[nodiscard]] std::optional<HotkeyChord> TakeCapture() noexcept;
		[[nodiscard]] bool IsCapturing() noexcept;
		[[nodiscard]] HotkeyMessageResult HandleKey(
			uint32_t a_keyCode,
			uint32_t a_modifiers,
			bool a_pressed,
			bool a_repeat) noexcept;
		void DispatchQueued() noexcept;
		[[nodiscard]] std::vector<HotkeyActionSnapshot> Snapshot() noexcept;
		[[nodiscard]] std::map<std::string, std::string> Overrides(
			HotkeySlot a_slot = HotkeySlot::kKeyboardMouse) noexcept;
		[[nodiscard]] DMUI_Result SetOverride(
			std::string_view a_id,
			std::string_view a_chord,
			HotkeySlot a_slot = HotkeySlot::kKeyboardMouse) noexcept;
		[[nodiscard]] bool RemoveOverride(std::string_view a_id,
			HotkeySlot a_slot = HotkeySlot::kKeyboardMouse) noexcept;
	}
}
