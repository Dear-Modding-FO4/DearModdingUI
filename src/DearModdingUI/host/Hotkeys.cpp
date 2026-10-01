#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <Support/BoundedString.h>

#include <algorithm>
#include <cstring>
#include <new>
#include <set>

namespace DearModdingUI
{
	namespace
	{
		inline constexpr size_t kActionIdCapacity{ 128 };
		inline constexpr size_t kDisplayNameCapacity{ 256 };
		inline constexpr size_t kChordCapacity{ sizeof(DMUI_HotkeyBindingInfo::chord) - 1 };

		[[nodiscard]] size_t ChordLength(HotkeyChord a_chord) noexcept
		{
			size_t length = 0;
			if (a_chord.modifiers & kHotkeyModifierControl)
				length += 5;
			if (a_chord.modifiers & kHotkeyModifierAlt)
				length += 4;
			if (a_chord.modifiers & kHotkeyModifierShift)
				length += 6;
			for (const auto key : a_chord.keys)
			{
				if (key)
					length += KeyCatalog::Token(key).size() + 1;
			}
			return length ? length - 1 : 4;
		}

		[[nodiscard]] bool ValidDisplayName(std::string_view a_value) noexcept
		{
			if (a_value.empty())
				return false;
			return std::ranges::none_of(a_value, [](char a_character) {
				return static_cast<unsigned char>(a_character) < 0x20u &&
					a_character != '\t';
			});
		}

		[[nodiscard]] bool InvokeHotkeyCpp(
			DMUI_HotkeyCallback a_callback,
			DMUI_HotkeyActionHandle a_action,
			bool a_pressed,
			void* a_userData) noexcept
		{
			try
			{
				a_callback(a_action, a_pressed ? 1u : 0u, a_userData);
				return true;
			}
			catch (...)
			{
				return false;
			}
		}

		[[nodiscard]] bool InvokeHotkey(
			DMUI_HotkeyCallback a_callback,
			DMUI_HotkeyActionHandle a_action,
			bool a_pressed,
			void* a_userData) noexcept
		{
#if defined(_MSC_VER)
			__try
			{
				return InvokeHotkeyCpp(a_callback, a_action, a_pressed, a_userData);
			}
			__except (1)
			{
				return false;
			}
#else
			return InvokeHotkeyCpp(a_callback, a_action, a_pressed, a_userData);
#endif
		}

		[[nodiscard]] HotkeyRegistry& RegistryInstance() noexcept
		{
			static HotkeyRegistry registry;
			return registry;
		}
	}

	bool ValidHotkeyActionId(std::string_view a_id) noexcept
	{
		if (a_id.empty() || a_id.size() > kActionIdCapacity ||
			a_id.front() == '.' || a_id.back() == '.')
			return false;
		bool namespaced = false;
		bool segmentStart = true;
		for (const auto character : a_id)
		{
			if (character == '.')
			{
				if (segmentStart)
					return false;
				namespaced = true;
				segmentStart = true;
				continue;
			}
			const auto alpha =
				(character >= 'a' && character <= 'z') ||
				(character >= 'A' && character <= 'Z');
			const auto digit = character >= '0' && character <= '9';
			if (segmentStart)
			{
				if (!alpha)
					return false;
				segmentStart = false;
			}
			else if (!alpha && !digit && character != '_' && character != '-')
				return false;
		}
		return namespaced && !segmentStart;
	}

	ParsedHotkeyChord ParseHotkeyChord(std::string_view a_value) noexcept
	{
		if (a_value.size() > kChordCapacity)
			return {};
		if (EqualsIgnoringCase(a_value, "none"))
			return { {}, true };

		HotkeyChord chord;
		size_t keyCount = 0;
		size_t start = 0;
		while (start < a_value.size())
		{
			const auto end = a_value.find('+', start);
			const auto token = a_value.substr(
				start,
				end == std::string_view::npos ? a_value.size() - start : end - start);
			if (token.empty())
				return {};
			if (EqualsIgnoringCase(token, "Shift"))
			{
				if (keyCount || (chord.modifiers & kHotkeyModifierShift))
					return {};
				chord.modifiers |= kHotkeyModifierShift;
			}
			else if (EqualsIgnoringCase(token, "Ctrl"))
			{
				if (keyCount || (chord.modifiers & kHotkeyModifierControl))
					return {};
				chord.modifiers |= kHotkeyModifierControl;
			}
			else if (EqualsIgnoringCase(token, "Alt"))
			{
				if (keyCount || (chord.modifiers & kHotkeyModifierAlt))
					return {};
				chord.modifiers |= kHotkeyModifierAlt;
			}
			else
			{
				if (keyCount == chord.keys.size())
					return {};
				const auto code = KeyCatalog::Parse(token);
				if (!code || !IsHostBindableKey(*code))
					return {};
				if (std::ranges::find(chord.keys, *code) != chord.keys.end() ||
					(keyCount && HotkeySlotForKey(*code) != HotkeySlotForKey(chord.keys[0])))
					return {};
				chord.keys[keyCount++] = *code;
			}
			if (end == std::string_view::npos)
				break;
			start = end + 1;
			if (start == a_value.size())
				return {};
		}
		if (!keyCount || (chord.modifiers && chord.FitsSlot(HotkeySlot::kGamepad)))
			return {};
		std::sort(chord.keys.begin(), chord.keys.begin() + keyCount);
		return { chord, ChordLength(chord) <= kChordCapacity };
	}

	std::string SerializeHotkeyChord(HotkeyChord a_chord)
	{
		if (a_chord.IsNone())
			return "none";
		std::string value;
		if (a_chord.modifiers & kHotkeyModifierControl)
			value += "Ctrl+";
		if (a_chord.modifiers & kHotkeyModifierAlt)
			value += "Alt+";
		if (a_chord.modifiers & kHotkeyModifierShift)
			value += "Shift+";
		for (const auto key : a_chord.keys)
		{
			if (!key)
				break;
			value += KeyCatalog::Token(key);
			value += '+';
		}
		value.pop_back();
		return value;
	}

	std::string FormatHotkeyChord(std::string_view a_chord)
	{
		if (a_chord.empty())
			return "Not set";
		auto parsed = ParseHotkeyChord(a_chord);
		if (!parsed.recognized)
			return "Invalid binding";
		if (parsed.chord.IsNone())
			return "Not set";
		std::string value;
		const auto append = [&](std::string_view a_label) {
			if (!value.empty())
				value += " + ";
			value += a_label;
		};
		if (parsed.chord.modifiers & kHotkeyModifierControl)
			append("Ctrl");
		if (parsed.chord.modifiers & kHotkeyModifierAlt)
			append("Alt");
		if (parsed.chord.modifiers & kHotkeyModifierShift)
			append("Shift");
		const auto end = std::find(parsed.chord.keys.begin(), parsed.chord.keys.end(), 0u);
		std::sort(parsed.chord.keys.begin(), end, [](uint32_t a_left, uint32_t a_right) {
			const auto* left = KeyCatalog::Find(a_left);
			const auto* right = KeyCatalog::Find(a_right);
			return (left->displayOrder ? left->displayOrder : left->code) <
				(right->displayOrder ? right->displayOrder : right->code);
		});
		for (auto key = parsed.chord.keys.begin(); key != end; ++key)
		{
			const auto* entry = KeyCatalog::Find(*key);
			append(entry->shortLabel.empty() ? entry->label : entry->shortLabel);
		}
		return value;
	}

	void HotkeyRegistry::InitializeOverrides(
		std::map<std::string, std::string> a_overrides, HotkeySlot a_slot) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		m_overrides[static_cast<size_t>(a_slot)] = std::move(a_overrides);
		RecomputeBindingsLocked();
	}

	void HotkeyRegistry::SetReservedChord(HotkeyChord a_chord, HotkeySlot a_slot) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		m_reservedChords[static_cast<size_t>(a_slot)] = a_chord;
		RecomputeBindingsLocked();
	}

	DMUI_Result HotkeyRegistry::Register(
		DMUI_ClientHandle a_client,
		const DMUI_HotkeyActionDescriptor* a_descriptor,
		DMUI_HotkeyActionHandle* a_action) noexcept
	{
		if (!a_descriptor || !a_action ||
			a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_action = DMUI_INVALID_HOTKEY_ACTION_HANDLE;
		if (a_descriptor->structSize < DMUI_HOTKEY_ACTION_DESCRIPTOR_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (!a_descriptor->callback)
			return DMUI_RESULT_INVALID_DESCRIPTOR;
		const auto contextPolicy =
			a_descriptor->structSize >= DMUI_HOTKEY_ACTION_DESCRIPTOR_CONTEXT_SIZE ?
				a_descriptor->contextPolicy :
				DMUI_HOTKEY_CONTEXT_ALWAYS;
		if (contextPolicy > DMUI_HOTKEY_CONTEXT_GAMEPLAY_UNOBSTRUCTED)
			return DMUI_RESULT_INVALID_DESCRIPTOR;

		try
		{
			Action action;
			action.client = a_client;
			action.callback = a_descriptor->callback;
			action.userData = a_descriptor->userData;
			action.contextPolicy = contextPolicy;
			if (!Internal::CopyBoundedString(
					a_descriptor->id,
					kActionIdCapacity,
					false,
					action.id) ||
				!ValidHotkeyActionId(action.id))
				return DMUI_RESULT_MALFORMED_ACTION_ID;
			if (!Internal::CopyBoundedString(
					a_descriptor->displayName,
					kDisplayNameCapacity,
					false,
					action.displayName) ||
				!ValidDisplayName(action.displayName))
				return DMUI_RESULT_INVALID_DESCRIPTOR;
			if (!Internal::CopyBoundedString(
					a_descriptor->suggestedDefaultChord,
					kChordCapacity,
					false,
					action.suggestedDefaultChord))
				return DMUI_RESULT_UNKNOWN_CHORD;
			const auto parsed = ParseHotkeyChord(action.suggestedDefaultChord);
			if (!parsed.recognized || !parsed.chord.FitsSlot(HotkeySlot::kKeyboardMouse))
				return DMUI_RESULT_UNKNOWN_CHORD;
			action.suggestedDefault = parsed.chord;
			action.suggestedDefaultChord = SerializeHotkeyChord(parsed.chord);

			const std::scoped_lock lock{ m_mutex };
			if (std::ranges::any_of(m_actions, [&](const auto& a_existing) {
					return a_existing.live && a_existing.id == action.id;
				}))
				return DMUI_RESULT_DUPLICATE_ACTION_ID;
			if (m_nextAction == DMUI_INVALID_HOTKEY_ACTION_HANDLE)
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			action.handle = m_nextAction++;
			m_actions.push_back(std::move(action));
			RecomputeBindingsLocked();
			*a_action = m_actions.back().handle;
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	DMUI_Result HotkeyRegistry::SetEnabled(
		DMUI_ClientHandle a_client,
		DMUI_HotkeyActionHandle a_action,
		bool a_enabled) noexcept
	{
		if (a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_action == DMUI_INVALID_HOTKEY_ACTION_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		const std::scoped_lock lock{ m_mutex };
		auto* action = FindActionLocked(a_action);
		if (!action || action->client != a_client)
			return DMUI_RESULT_ACTION_NOT_FOUND;
		action->enabled = a_enabled;
		return DMUI_RESULT_OK;
	}

	void HotkeyRegistry::SetContext(HotkeyContextState a_context) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		m_context = a_context;
		if (!a_context.hostMenuVisible)
		{
			m_capturing = false;
			m_capture.reset();
		}
	}

	void HotkeyRegistry::BeginCapture(HotkeySlot a_slot) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		m_capture.reset();
		m_captureKeys = {};
		m_captureSlot = a_slot;
		m_captureValid = true;
		m_capturing = true;
	}

	bool HotkeyRegistry::CancelCapture() noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		const auto capturing = m_capturing;
		m_capturing = false;
		m_capture.reset();
		return capturing;
	}

	std::optional<HotkeyChord> HotkeyRegistry::TakeCapture() noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		const auto captured = m_capture;
		m_capture.reset();
		return captured;
	}

	bool HotkeyRegistry::IsCapturing() const noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		return m_capturing;
	}

	void HotkeyRegistry::ReleaseActiveKeys() noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		m_capturing = false;
		m_capture.reset();
		m_heldKeys.fill(false);
		m_heldToggleChords = {};
		for (auto& active : m_activeKeys)
			ReleaseKeyLocked(active);
	}

	bool HotkeyRegistry::IsToggleChordHeld() const noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		return std::ranges::any_of(m_heldToggleChords, [](const auto& a_chord) {
			return !a_chord.IsNone();
		});
	}

	void HotkeyRegistry::ReleaseKeyLocked(ActiveKey& a_active) noexcept
	{
		if (a_active.queued)
		{
			const auto tail = (m_eventHead + m_eventCount) % m_events.size();
			m_events[tail] = { a_active.action, false };
			++m_eventCount;
			--m_reservedReleaseCount;
		}
		a_active = {};
	}

	DMUI_Result HotkeyRegistry::Query(
		DMUI_ClientHandle a_client,
		DMUI_HotkeyActionHandle a_action,
		DMUI_HotkeyBindingInfo* a_binding) const noexcept
	{
		if (!a_binding || a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_action == DMUI_INVALID_HOTKEY_ACTION_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_binding->structSize < sizeof(DMUI_HotkeyBindingInfo))
			return DMUI_RESULT_STRUCT_TOO_SMALL;

		const std::scoped_lock lock{ m_mutex };
		const auto* action = FindActionLocked(a_action);
		if (!action || action->client != a_client)
			return DMUI_RESULT_ACTION_NOT_FOUND;
		const auto& binding = action->bindings[0];
		a_binding->state = binding.state;
		const auto chord = binding.state == DMUI_HOTKEY_BINDING_BOUND ?
			SerializeHotkeyChord(binding.effective) :
			std::string{ "none" };
		std::memcpy(a_binding->chord, chord.c_str(), chord.size() + 1);
		return DMUI_RESULT_OK;
	}

	DMUI_Result HotkeyRegistry::Unregister(
		DMUI_ClientHandle a_client,
		DMUI_HotkeyActionHandle a_action) noexcept
	{
		if (a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_action == DMUI_INVALID_HOTKEY_ACTION_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;

		const std::scoped_lock lock{ m_mutex };
		if (!RenderExecution::IsActive())
			return DMUI_RESULT_WRONG_THREAD;
		const auto action = std::ranges::find(m_actions, a_action, &Action::handle);
		if (action == m_actions.end() || !action->live || action->client != a_client)
			return DMUI_RESULT_ACTION_NOT_FOUND;
		action->live = false;
		action->callback = nullptr;
		action->userData = nullptr;
		RecomputeBindingsLocked();
		return DMUI_RESULT_OK;
	}

	DMUI_Result HotkeyRegistry::SetOverride(
		std::string_view a_id,
		std::string_view a_chord, HotkeySlot a_slot) noexcept
	{
		const auto parsed = ParseHotkeyChord(a_chord);
		if (!parsed.recognized || !parsed.chord.FitsSlot(a_slot))
			return DMUI_RESULT_UNKNOWN_CHORD;
		try
		{
			const std::scoped_lock lock{ m_mutex };
			auto& overrides = m_overrides[static_cast<size_t>(a_slot)];
			const auto found = std::ranges::find_if(m_actions, [&](const auto& a_action) {
				return a_action.live && a_action.id == a_id;
			});
			if (found == m_actions.end())
				return DMUI_RESULT_ACTION_NOT_FOUND;
			const auto previous = overrides.find(found->id);
			const auto hadPrevious = previous != overrides.end();
			const auto previousChord = hadPrevious ? previous->second : std::string{};
			overrides[found->id] = SerializeHotkeyChord(parsed.chord);
			RecomputeBindingsLocked();
			const auto state = found->bindings[static_cast<size_t>(a_slot)].state;
			if (state == DMUI_HOTKEY_BINDING_BOUND ||
				state == DMUI_HOTKEY_BINDING_UNBOUND_USER)
				return DMUI_RESULT_OK;
			if (hadPrevious)
				overrides[found->id] = previousChord;
			else
				overrides.erase(found->id);
			RecomputeBindingsLocked();
			return DMUI_RESULT_DUPLICATE_ACTION_ID;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	bool HotkeyRegistry::RemoveOverride(std::string_view a_id, HotkeySlot a_slot) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		const auto erased = m_overrides[static_cast<size_t>(a_slot)].erase(std::string{ a_id }) != 0;
		if (erased)
			RecomputeBindingsLocked();
		return erased;
	}

	HotkeyMessageResult HotkeyRegistry::HandleKey(
		uint32_t a_keyCode,
		uint32_t a_modifiers,
		bool a_pressed,
		bool a_repeat) noexcept
	{
		if (a_keyCode >= m_activeKeys.size())
			return HotkeyMessageResult::kPassThrough;
		const std::scoped_lock lock{ m_mutex };
		const auto wasHeld = m_heldKeys[a_keyCode];
		m_heldKeys[a_keyCode] = a_pressed;
		auto& active = m_activeKeys[a_keyCode];
		if (!a_pressed)
		{
			auto& toggleChord = m_heldToggleChords[static_cast<size_t>(HotkeySlotForKey(a_keyCode))];
			if (std::ranges::none_of(toggleChord.keys, [&](auto a_key) {
					return a_key && m_heldKeys[a_key];
				}))
				toggleChord = {};
			if (m_capturing && active.captured &&
				std::ranges::find(m_captureKeys.keys, a_keyCode) != m_captureKeys.keys.end())
			{
				if (m_captureValid && ChordLength(m_captureKeys) <= kChordCapacity)
					m_capture = m_captureKeys;
				m_capturing = false;
			}
			if (!active.IsOwned())
				return HotkeyMessageResult::kPassThrough;
			ReleaseKeyLocked(active);
			return HotkeyMessageResult::kConsumed;
		}
		if (active.IsOwned())
			return HotkeyMessageResult::kConsumed;
		if (a_repeat || wasHeld || !IsHostBindableKey(a_keyCode))
			return HotkeyMessageResult::kPassThrough;
		const auto slot = HotkeySlotForKey(a_keyCode);
		const auto index = static_cast<size_t>(slot);
		if (slot == HotkeySlot::kGamepad)
			a_modifiers = 0;
		if (m_capturing)
		{
			if (slot != m_captureSlot)
				return HotkeyMessageResult::kPassThrough;
			const auto empty = std::ranges::find(m_captureKeys.keys, 0u);
			if (empty == m_captureKeys.keys.end())
				m_captureValid = false;
			else
			{
				*empty = a_keyCode;
				std::sort(m_captureKeys.keys.begin(), empty + 1);
			}
			m_captureKeys.modifiers = a_modifiers;
			active.captured = true;
			return HotkeyMessageResult::kConsumed;
		}
		const auto matches = [&](HotkeyChord a_chord) {
			return !a_chord.IsNone() && a_chord.modifiers == a_modifiers &&
				std::ranges::find(a_chord.keys, a_keyCode) != a_chord.keys.end() &&
				std::ranges::all_of(a_chord.keys, [&](auto a_key) {
					return !a_key || m_heldKeys[a_key];
				});
		};
		const auto hasActivation = [&](DMUI_HotkeyActionHandle a_action, bool a_toggle) {
			return std::ranges::any_of(m_activeKeys, [&](const auto& a_active) {
				return a_active.slot == slot &&
					(a_toggle ? a_active.toggle : a_active.action == a_action);
			});
		};
		if (matches(m_reservedChords[index]) &&
			!hasActivation(DMUI_INVALID_HOTKEY_ACTION_HANDLE, true))
		{
			active.toggle = true;
			active.slot = slot;
			// Retain the whole chord after its triggering key releases or the binding changes.
			m_heldToggleChords[index] = m_reservedChords[index];
			return HotkeyMessageResult::kMenuToggle;
		}
		const auto found = std::ranges::find_if(m_actions, [&](const auto& a_action) {
			const auto contextAllowed =
				a_action.contextPolicy == DMUI_HOTKEY_CONTEXT_ALWAYS ||
				(a_action.contextPolicy == DMUI_HOTKEY_CONTEXT_HOST_INPUT_INACTIVE &&
					!m_context.hostMenuVisible &&
					!m_context.dialogVisible &&
					!m_context.textEditing) ||
				(a_action.contextPolicy == DMUI_HOTKEY_CONTEXT_GAMEPLAY_UNOBSTRUCTED &&
					!m_context.hostMenuVisible &&
					!m_context.dialogVisible &&
					!m_context.textEditing &&
					m_context.gameplaySafe);
			return a_action.live &&
				a_action.enabled &&
				contextAllowed &&
				!a_action.callbackFailed &&
				a_action.bindings[index].state == DMUI_HOTKEY_BINDING_BOUND &&
				matches(a_action.bindings[index].effective) &&
				!hasActivation(a_action.handle, false);
		});
		if (found == m_actions.end())
			return HotkeyMessageResult::kPassThrough;
		if (m_eventCount + m_reservedReleaseCount + 2 > m_events.size())
		{
			active = { found->handle, false, false, false, slot };
			return HotkeyMessageResult::kConsumedPairDropped;
		}
		const auto tail = (m_eventHead + m_eventCount) % m_events.size();
		m_events[tail] = { found->handle, true };
		++m_eventCount;
		++m_reservedReleaseCount;
		active = { found->handle, true, false, false, slot };
		return HotkeyMessageResult::kConsumed;
	}

	void HotkeyRegistry::DispatchQueued() noexcept
	{
		for (;;)
		{
			Event event;
			DMUI_HotkeyCallback callback{ nullptr };
			void* userData{ nullptr };
			{
				const std::scoped_lock lock{ m_mutex };
				if (!m_eventCount)
					return;
				event = m_events[m_eventHead];
				m_eventHead = (m_eventHead + 1) % m_events.size();
				--m_eventCount;
				auto* action = FindActionLocked(event.action);
				if (!action || action->callbackFailed)
					continue;
				callback = action->callback;
				userData = action->userData;
			}
			if (InvokeHotkey(callback, event.action, event.pressed, userData))
				continue;
			const std::scoped_lock lock{ m_mutex };
			if (auto* action = FindActionLocked(event.action))
				action->callbackFailed = true;
		}
	}

	std::vector<HotkeyActionSnapshot> HotkeyRegistry::Snapshot() const noexcept
	{
		try
		{
			const std::scoped_lock lock{ m_mutex };
			std::map<std::string, HotkeyActionSnapshot> snapshots;
			for (const auto& action : m_actions)
			{
				if (!action.live)
					continue;
				auto& snapshot = snapshots[action.id];
				snapshot.id = action.id;
				snapshot.displayName = action.displayName;
				snapshot.suggestedDefaultChord = action.suggestedDefaultChord;
				snapshot.registered = true;
				for (const auto slot : kHotkeySlots)
				{
					const auto index = static_cast<size_t>(slot);
					const auto& binding = action.bindings[index];
					snapshot.bindings[index].effectiveChord =
						binding.state == DMUI_HOTKEY_BINDING_BOUND ?
							SerializeHotkeyChord(binding.effective) : "none";
					snapshot.bindings[index].state = binding.state;
				}
			}
			for (const auto slot : kHotkeySlots)
			{
				const auto index = static_cast<size_t>(slot);
				for (const auto& [id, chord] : m_overrides[index])
				{
					auto& snapshot = snapshots[id];
					snapshot.id = id;
					if (!snapshot.registered)
					{
						snapshot.displayName = id;
						snapshot.bindings[index].effectiveChord = chord;
					}
					snapshot.bindings[index].overrideChord = chord;
				}
			}
			std::vector<HotkeyActionSnapshot> result;
			result.reserve(snapshots.size());
			for (auto& [id, snapshot] : snapshots)
				result.push_back(std::move(snapshot));
			return result;
		}
		catch (...)
		{
			return {};
		}
	}

	std::map<std::string, std::string> HotkeyRegistry::Overrides(HotkeySlot a_slot) const noexcept
	{
		try
		{
			const std::scoped_lock lock{ m_mutex };
			return m_overrides[static_cast<size_t>(a_slot)];
		}
		catch (...)
		{
			return {};
		}
	}

	void HotkeyRegistry::RecomputeBindingsLocked() noexcept
	{
		for (auto& action : m_actions)
			action.bindings = {};
		std::vector<Action*> ordered;
		ordered.reserve(m_actions.size());
		for (auto& action : m_actions)
		{
			if (action.live)
				ordered.push_back(&action);
		}
		std::ranges::sort(ordered, {}, [](const auto* a_action) {
			return a_action->id;
		});
		for (const auto slot : kHotkeySlots)
		{
			const auto index = static_cast<size_t>(slot);
			const auto& overrides = m_overrides[index];
			std::set<HotkeyChord> occupied{ m_reservedChords[index] };
			// User choices take priority over client suggestions.
			for (const bool userOverride : { true, false })
			{
				for (auto* action : ordered)
				{
					const auto override = overrides.find(action->id);
					if ((override != overrides.end()) != userOverride)
						continue;
					auto& binding = action->bindings[index];
					const auto parsed = userOverride ? ParseHotkeyChord(override->second) :
						ParsedHotkeyChord{
							slot == HotkeySlot::kKeyboardMouse ? action->suggestedDefault : HotkeyChord{},
							true };
					if (!parsed.recognized || !parsed.chord.FitsSlot(slot))
						binding.state = DMUI_HOTKEY_BINDING_UNBOUND_INVALID_OVERRIDE;
					else if (parsed.chord.IsNone())
						binding.state = userOverride ? DMUI_HOTKEY_BINDING_UNBOUND_USER :
							DMUI_HOTKEY_BINDING_UNBOUND_NEVER_SET;
					else if (!occupied.insert(parsed.chord).second)
						binding.state = userOverride ? DMUI_HOTKEY_BINDING_UNBOUND_OVERRIDE_CONFLICT :
							DMUI_HOTKEY_BINDING_UNBOUND_DEFAULT_CONFLICT;
					else
					{
						binding.effective = parsed.chord;
						binding.state = DMUI_HOTKEY_BINDING_BOUND;
					}
				}
			}
		}
	}

	HotkeyRegistry::Action* HotkeyRegistry::FindActionLocked(
		DMUI_HotkeyActionHandle a_action) noexcept
	{
		const auto found = std::ranges::find(m_actions, a_action, &Action::handle);
		return found != m_actions.end() && found->live ? &*found : nullptr;
	}

	const HotkeyRegistry::Action* HotkeyRegistry::FindActionLocked(
		DMUI_HotkeyActionHandle a_action) const noexcept
	{
		const auto found = std::ranges::find(m_actions, a_action, &Action::handle);
		return found != m_actions.end() && found->live ? &*found : nullptr;
	}

	namespace Hotkeys
	{
		void InitializeOverrides(std::map<std::string, std::string> a_overrides, HotkeySlot a_slot) noexcept
		{
			RegistryInstance().InitializeOverrides(std::move(a_overrides), a_slot);
		}

		void SetReservedChord(HotkeyChord a_chord, HotkeySlot a_slot) noexcept
		{
			RegistryInstance().SetReservedChord(a_chord, a_slot);
		}

		DMUI_Result Register(
			DMUI_ClientHandle a_client,
			const DMUI_HotkeyActionDescriptor* a_descriptor,
			DMUI_HotkeyActionHandle* a_action) noexcept
		{
			return RegistryInstance().Register(a_client, a_descriptor, a_action);
		}

		DMUI_Result Query(
			DMUI_ClientHandle a_client,
			DMUI_HotkeyActionHandle a_action,
			DMUI_HotkeyBindingInfo* a_binding) noexcept
		{
			return RegistryInstance().Query(a_client, a_action, a_binding);
		}

		DMUI_Result Unregister(
			DMUI_ClientHandle a_client,
			DMUI_HotkeyActionHandle a_action) noexcept
		{
			return RegistryInstance().Unregister(a_client, a_action);
		}

		DMUI_Result SetEnabled(
			DMUI_ClientHandle a_client,
			DMUI_HotkeyActionHandle a_action,
			bool a_enabled) noexcept
		{
			return RegistryInstance().SetEnabled(a_client, a_action, a_enabled);
		}

		void SetContext(HotkeyContextState a_context) noexcept
		{
			RegistryInstance().SetContext(a_context);
		}

		void ReleaseActiveKeys() noexcept
		{
			RegistryInstance().ReleaseActiveKeys();
		}

		bool IsToggleChordHeld() noexcept
		{
			return RegistryInstance().IsToggleChordHeld();
		}

		void BeginCapture(HotkeySlot a_slot) noexcept
		{
			RegistryInstance().BeginCapture(a_slot);
		}

		bool CancelCapture() noexcept
		{
			return RegistryInstance().CancelCapture();
		}

		std::optional<HotkeyChord> TakeCapture() noexcept
		{
			return RegistryInstance().TakeCapture();
		}

		bool IsCapturing() noexcept
		{
			return RegistryInstance().IsCapturing();
		}

		HotkeyMessageResult HandleKey(
			uint32_t a_keyCode,
			uint32_t a_modifiers,
			bool a_pressed,
			bool a_repeat) noexcept
		{
			return RegistryInstance().HandleKey(
				a_keyCode, a_modifiers, a_pressed, a_repeat);
		}

		void DispatchQueued() noexcept
		{
			RegistryInstance().DispatchQueued();
		}

		std::vector<HotkeyActionSnapshot> Snapshot() noexcept
		{
			return RegistryInstance().Snapshot();
		}

		std::map<std::string, std::string> Overrides(HotkeySlot a_slot) noexcept
		{
			return RegistryInstance().Overrides(a_slot);
		}

		DMUI_Result SetOverride(
			std::string_view a_id,
			std::string_view a_chord, HotkeySlot a_slot) noexcept
		{
			return RegistryInstance().SetOverride(a_id, a_chord, a_slot);
		}

		bool RemoveOverride(std::string_view a_id, HotkeySlot a_slot) noexcept
		{
			return RegistryInstance().RemoveOverride(a_id, a_slot);
		}
	}
}
