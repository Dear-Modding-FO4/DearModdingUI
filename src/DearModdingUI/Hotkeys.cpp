#include <DearModdingUI/Hotkeys.h>

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
		inline constexpr size_t kChordCapacity{ 31 };

		[[nodiscard]] bool ReadString(
			const char* a_value,
			size_t a_capacity,
			std::string& a_out)
		{
			if (!a_value)
				return false;
			size_t length = 0;
			while (length <= a_capacity && a_value[length])
				++length;
			if (!length || length > a_capacity)
				return false;
			a_out.assign(a_value, length);
			return true;
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

	void HotkeyRegistry::BindRenderThread() noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		if (m_renderThread == std::thread::id{})
			m_renderThread = std::this_thread::get_id();
	}

	ParsedHotkeyChord ParseHotkeyChord(std::string_view a_value) noexcept
	{
		if (a_value.size() > kChordCapacity)
			return {};
		if (EqualsIgnoringCase(a_value, "none"))
			return { {}, true };

		HotkeyChord chord;
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
				if (chord.virtualKey || (chord.modifiers & kHotkeyModifierShift))
					return {};
				chord.modifiers |= kHotkeyModifierShift;
			}
			else if (EqualsIgnoringCase(token, "Ctrl"))
			{
				if (chord.virtualKey || (chord.modifiers & kHotkeyModifierControl))
					return {};
				chord.modifiers |= kHotkeyModifierControl;
			}
			else if (EqualsIgnoringCase(token, "Alt"))
			{
				if (chord.virtualKey || (chord.modifiers & kHotkeyModifierAlt))
					return {};
				chord.modifiers |= kHotkeyModifierAlt;
			}
			else
			{
				if (chord.virtualKey)
					return {};
				const auto parsed = ParseMenuToggleKey(token);
				if (!parsed.recognized)
					return {};
				chord.virtualKey = parsed.virtualKey;
			}
			if (end == std::string_view::npos)
				break;
			start = end + 1;
		}
		return { chord, chord.virtualKey != 0 };
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
		value += MenuToggleKeyName(a_chord.virtualKey);
		return value;
	}

	void HotkeyRegistry::InitializeOverrides(
		std::map<std::string, std::string> a_overrides) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		m_overrides = std::move(a_overrides);
		RecomputeBindingsLocked();
	}

	void HotkeyRegistry::SetReservedVirtualKey(uint32_t a_virtualKey) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		m_reservedVirtualKey = a_virtualKey;
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

		try
		{
			Action action;
			action.client = a_client;
			action.callback = a_descriptor->callback;
			action.userData = a_descriptor->userData;
			if (!ReadString(a_descriptor->id, kActionIdCapacity, action.id) ||
				!ValidHotkeyActionId(action.id))
				return DMUI_RESULT_MALFORMED_ACTION_ID;
			if (!ReadString(
					a_descriptor->displayName,
					kDisplayNameCapacity,
					action.displayName) ||
				!ValidDisplayName(action.displayName))
				return DMUI_RESULT_INVALID_DESCRIPTOR;
			if (!ReadString(
					a_descriptor->suggestedDefaultChord,
					kChordCapacity,
					action.suggestedDefaultChord))
				return DMUI_RESULT_UNKNOWN_CHORD;
			const auto parsed = ParseHotkeyChord(action.suggestedDefaultChord);
			if (!parsed.recognized)
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
		a_binding->state = action->state;
		const auto chord = action->state == DMUI_HOTKEY_BINDING_BOUND ?
			SerializeHotkeyChord(action->effective) :
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
		if (m_renderThread != std::this_thread::get_id())
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
		std::string_view a_chord) noexcept
	{
		const auto parsed = ParseHotkeyChord(a_chord);
		if (!parsed.recognized)
			return DMUI_RESULT_UNKNOWN_CHORD;
		try
		{
			const std::scoped_lock lock{ m_mutex };
			const auto found = std::ranges::find_if(m_actions, [&](const auto& a_action) {
				return a_action.live && a_action.id == a_id;
			});
			if (found == m_actions.end())
				return DMUI_RESULT_ACTION_NOT_FOUND;
			const auto previous = m_overrides.find(found->id);
			const auto hadPrevious = previous != m_overrides.end();
			const auto previousChord = hadPrevious ? previous->second : std::string{};
			m_overrides[found->id] = SerializeHotkeyChord(parsed.chord);
			RecomputeBindingsLocked();
			if (found->state == DMUI_HOTKEY_BINDING_BOUND ||
				found->state == DMUI_HOTKEY_BINDING_UNBOUND_USER)
				return DMUI_RESULT_OK;
			if (hadPrevious)
				m_overrides[found->id] = previousChord;
			else
				m_overrides.erase(found->id);
			RecomputeBindingsLocked();
			return DMUI_RESULT_DUPLICATE_ACTION_ID;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	bool HotkeyRegistry::RemoveOverride(std::string_view a_id) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		const auto erased = m_overrides.erase(std::string{ a_id }) != 0;
		if (erased)
			RecomputeBindingsLocked();
		return erased;
	}

	HotkeyMessageResult HotkeyRegistry::HandleKey(
		uint32_t a_virtualKey,
		uint32_t a_modifiers,
		bool a_pressed,
		bool a_repeat) noexcept
	{
		if (a_virtualKey >= m_activeKeys.size())
			return HotkeyMessageResult::kPassThrough;
		const std::scoped_lock lock{ m_mutex };
		auto& active = m_activeKeys[a_virtualKey];
		if (!a_pressed)
		{
			if (active.action == DMUI_INVALID_HOTKEY_ACTION_HANDLE)
				return HotkeyMessageResult::kPassThrough;
			if (active.queued)
			{
				const auto tail =
					(m_eventHead + m_eventCount) % m_events.size();
				m_events[tail] = { active.action, false };
				++m_eventCount;
				--m_reservedReleaseCount;
			}
			active = {};
			return HotkeyMessageResult::kConsumed;
		}
		if (active.action != DMUI_INVALID_HOTKEY_ACTION_HANDLE)
			return HotkeyMessageResult::kConsumed;
		if (a_repeat)
			return HotkeyMessageResult::kPassThrough;
		const HotkeyChord pressed{ a_virtualKey, a_modifiers };
		const auto found = std::ranges::find_if(m_actions, [&](const auto& a_action) {
			return a_action.live &&
				!a_action.callbackFailed &&
				a_action.state == DMUI_HOTKEY_BINDING_BOUND &&
				a_action.effective == pressed;
		});
		if (found == m_actions.end())
			return HotkeyMessageResult::kPassThrough;
		if (m_eventCount + m_reservedReleaseCount + 2 > m_events.size())
		{
			active = { found->handle, false };
			return HotkeyMessageResult::kConsumedPairDropped;
		}
		const auto tail = (m_eventHead + m_eventCount) % m_events.size();
		m_events[tail] = { found->handle, true };
		++m_eventCount;
		++m_reservedReleaseCount;
		active = { found->handle, true };
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
			std::vector<HotkeyActionSnapshot> result;
			result.reserve(m_actions.size() + m_overrides.size());
			for (const auto& action : m_actions)
			{
				if (!action.live)
					continue;
				const auto override = m_overrides.find(action.id);
				result.push_back({
					action.id,
					action.displayName,
					action.suggestedDefaultChord,
					override != m_overrides.end() ? override->second : std::string{},
					action.state == DMUI_HOTKEY_BINDING_BOUND ?
						SerializeHotkeyChord(action.effective) :
						std::string{ "none" },
					action.state,
					true
				});
			}
			for (const auto& [id, chord] : m_overrides)
			{
				if (std::ranges::any_of(m_actions, [&](const auto& a_action) {
						return a_action.live && a_action.id == id;
					}))
					continue;
				result.push_back({
					id,
					id,
					{},
					chord,
					chord,
					DMUI_HOTKEY_BINDING_UNBOUND_NEVER_SET,
					false
				});
			}
			std::ranges::sort(result, {}, &HotkeyActionSnapshot::id);
			return result;
		}
		catch (...)
		{
			return {};
		}
	}

	std::map<std::string, std::string> HotkeyRegistry::Overrides() const noexcept
	{
		try
		{
			const std::scoped_lock lock{ m_mutex };
			return m_overrides;
		}
		catch (...)
		{
			return {};
		}
	}

	void HotkeyRegistry::RecomputeBindingsLocked() noexcept
	{
		for (auto& action : m_actions)
		{
			action.effective = {};
			action.state = DMUI_HOTKEY_BINDING_UNBOUND_NEVER_SET;
		}
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
		std::set<std::pair<uint32_t, uint32_t>> occupied;
		for (auto* action : ordered)
		{
			const auto override = m_overrides.find(action->id);
			if (override == m_overrides.end())
				continue;
			const auto parsed = ParseHotkeyChord(override->second);
			if (!parsed.recognized)
			{
				action->state = DMUI_HOTKEY_BINDING_UNBOUND_INVALID_OVERRIDE;
				continue;
			}
			if (parsed.chord.IsNone())
			{
				action->state = DMUI_HOTKEY_BINDING_UNBOUND_USER;
				continue;
			}
			const auto key = std::pair{
				parsed.chord.virtualKey,
				parsed.chord.modifiers
			};
			if (parsed.chord.virtualKey == m_reservedVirtualKey ||
				occupied.contains(key))
			{
				action->state = DMUI_HOTKEY_BINDING_UNBOUND_OVERRIDE_CONFLICT;
				continue;
			}
			action->effective = parsed.chord;
			action->state = DMUI_HOTKEY_BINDING_BOUND;
			occupied.insert(key);
		}
		for (auto* action : ordered)
		{
			if (m_overrides.contains(action->id))
				continue;
			if (action->suggestedDefault.IsNone())
			{
				action->state = DMUI_HOTKEY_BINDING_UNBOUND_NEVER_SET;
				continue;
			}
			const auto key = std::pair{
				action->suggestedDefault.virtualKey,
				action->suggestedDefault.modifiers
			};
			if (action->suggestedDefault.virtualKey == m_reservedVirtualKey ||
				occupied.contains(key))
			{
				action->state = DMUI_HOTKEY_BINDING_UNBOUND_DEFAULT_CONFLICT;
				continue;
			}
			action->effective = action->suggestedDefault;
			action->state = DMUI_HOTKEY_BINDING_BOUND;
			occupied.insert(key);
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
		void BindRenderThread() noexcept
		{
			RegistryInstance().BindRenderThread();
		}

		void InitializeOverrides(std::map<std::string, std::string> a_overrides) noexcept
		{
			RegistryInstance().InitializeOverrides(std::move(a_overrides));
		}

		void SetReservedVirtualKey(uint32_t a_virtualKey) noexcept
		{
			RegistryInstance().SetReservedVirtualKey(a_virtualKey);
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

		HotkeyMessageResult HandleKey(
			uint32_t a_virtualKey,
			uint32_t a_modifiers,
			bool a_pressed,
			bool a_repeat) noexcept
		{
			return RegistryInstance().HandleKey(
				a_virtualKey, a_modifiers, a_pressed, a_repeat);
		}

		void DispatchQueued() noexcept
		{
			RegistryInstance().DispatchQueued();
		}

		std::vector<HotkeyActionSnapshot> Snapshot() noexcept
		{
			return RegistryInstance().Snapshot();
		}

		std::map<std::string, std::string> Overrides() noexcept
		{
			return RegistryInstance().Overrides();
		}

		DMUI_Result SetOverride(
			std::string_view a_id,
			std::string_view a_chord) noexcept
		{
			return RegistryInstance().SetOverride(a_id, a_chord);
		}

		bool RemoveOverride(std::string_view a_id) noexcept
		{
			return RegistryInstance().RemoveOverride(a_id);
		}
	}
}
