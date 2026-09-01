#include <DearModdingUI/Hotkeys.h>
#include "Harness.h"

#include <algorithm>
#include <map>
#include <string>
#include <thread>

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI;

		struct CallbackState
		{
			uint32_t pressed{ 0 };
			uint32_t released{ 0 };
			std::array<bool, kHotkeyEventQueueCapacity> edges{};
			size_t edgeCount{ 0 };
		};

		void DMUI_CALL HotkeyCallback(
			DMUI_HotkeyActionHandle,
			uint32_t a_pressed,
			void* a_userData) noexcept
		{
			auto& state = *static_cast<CallbackState*>(a_userData);
			if (a_pressed)
				++state.pressed;
			else
				++state.released;
			state.edges[state.edgeCount++] = a_pressed != 0;
		}

		[[nodiscard]] DMUI_HotkeyActionDescriptor Descriptor(
			const char* a_id,
			const char* a_chord,
			CallbackState& a_state) noexcept
		{
			return {
				sizeof(DMUI_HotkeyActionDescriptor),
				a_id,
				a_id,
				a_chord,
				&HotkeyCallback,
				&a_state
			};
		}

		[[nodiscard]] DMUI_HotkeyActionHandle Register(
			HotkeyRegistry& a_registry,
			DMUI_ClientHandle a_client,
			const char* a_id,
			const char* a_chord,
			CallbackState& a_state)
		{
			auto descriptor = Descriptor(a_id, a_chord, a_state);
			DMUI_HotkeyActionHandle handle{};
			require(a_registry.Register(a_client, &descriptor, &handle) == DMUI_RESULT_OK,
				"hotkey registration failed");
			return handle;
		}

		[[nodiscard]] DMUI_HotkeyBindingInfo Query(
			const HotkeyRegistry& a_registry,
			DMUI_ClientHandle a_client,
			DMUI_HotkeyActionHandle a_action)
		{
			DMUI_HotkeyBindingInfo binding{};
			binding.structSize = sizeof(binding);
			require(a_registry.Query(a_client, a_action, &binding) == DMUI_RESULT_OK,
				"hotkey query failed");
			return binding;
		}
	}

	void run_hotkey_checks(Runner& runner)
	{
		runner.test("hotkey action ids require a namespace and valid segments", [] {
			require(ValidHotkeyActionId("Addictol.Telemetry.ToggleOverlay"),
				"a valid namespaced id was rejected");
			require(!ValidHotkeyActionId("ToggleOverlay"), "an unnamespaced id was accepted");
			require(!ValidHotkeyActionId("Addictol..Toggle"), "an empty segment was accepted");
			require(!ValidHotkeyActionId("1Addictol.Toggle"), "a numeric segment start was accepted");
			require(!ValidHotkeyActionId("Addictol.Toggle Overlay"), "a space was accepted");
			HotkeyRegistry registry;
			CallbackState state;
			auto malformed = Descriptor("ToggleOverlay", "F11", state);
			DMUI_HotkeyActionHandle handle{};
			require(registry.Register(1, &malformed, &handle) ==
					DMUI_RESULT_MALFORMED_ACTION_ID,
				"registration did not report a malformed id");
			auto unknown = Descriptor("Example.Toggle", "Meta+F11", state);
			require(registry.Register(1, &unknown, &handle) ==
					DMUI_RESULT_UNKNOWN_CHORD,
				"registration did not report an unknown chord");
		});

		runner.test("hotkey chord strings round trip including none", [] {
			for (const auto chord : { "F11", "Shift+F11", "Ctrl+Alt+Home", "none" })
			{
				const auto parsed = ParseHotkeyChord(chord);
				require(parsed.recognized, std::string{ chord } + " was rejected");
				require(ParseHotkeyChord(SerializeHotkeyChord(parsed.chord)).chord ==
						parsed.chord,
					std::string{ chord } + " did not round trip");
			}
			require(!ParseHotkeyChord("Meta+F11").recognized, "an unknown modifier was accepted");
			require(!ParseHotkeyChord("Shift+").recognized, "a missing key was accepted");
		});

		runner.test("hotkey binding states track defaults and user clearing", [] {
			HotkeyRegistry registry;
			CallbackState state;
			const auto action = Register(
				registry, 1, "Example.Toggle", "F10", state);
			auto binding = Query(registry, 1, action);
			require(binding.state == DMUI_HOTKEY_BINDING_BOUND, "default was not bound");
			require(std::string{ binding.chord } == "F10", "bound chord was not reported");
			require(registry.SetOverride("Example.Toggle", "none") == DMUI_RESULT_OK,
				"explicit unbind failed");
			binding = Query(registry, 1, action);
			require(binding.state == DMUI_HOTKEY_BINDING_UNBOUND_USER,
				"user-cleared state was not reported");
			require(registry.RemoveOverride("Example.Toggle"), "override removal failed");
			require(Query(registry, 1, action).state == DMUI_HOTKEY_BINDING_BOUND,
				"default was not restored");
			const auto unset = Register(
				registry, 1, "Example.Unset", "none", state);
			require(Query(registry, 1, unset).state ==
					DMUI_HOTKEY_BINDING_UNBOUND_NEVER_SET,
				"never-set state was not reported");
		});

		runner.test("duplicate hotkey action ids are rejected globally", [] {
			HotkeyRegistry registry;
			CallbackState state;
			(void)Register(registry, 1, "Example.Toggle", "F10", state);
			auto duplicate = Descriptor("Example.Toggle", "F11", state);
			DMUI_HotkeyActionHandle handle{};
			require(registry.Register(2, &duplicate, &handle) ==
					DMUI_RESULT_DUPLICATE_ACTION_ID,
				"a cross-client duplicate was accepted");
		});

		runner.test("taken defaults produce a distinct unbound state", [] {
			HotkeyRegistry registry;
			CallbackState state;
			const auto later = Register(registry, 1, "Zulu.Toggle", "F10", state);
			const auto earlier = Register(registry, 2, "Alpha.Toggle", "F10", state);
			require(Query(registry, 2, earlier).state == DMUI_HOTKEY_BINDING_BOUND,
				"stable id ordering did not select the winner");
			require(Query(registry, 1, later).state ==
					DMUI_HOTKEY_BINDING_UNBOUND_DEFAULT_CONFLICT,
				"the taken default was not distinguished");
		});

		runner.test("orphaned hotkey overrides are retained and surfaced", [] {
			HotkeyRegistry registry;
			registry.InitializeOverrides({
				{ "RemovedMod.Toggle", "Shift+F11" }
			});
			CallbackState state;
			(void)Register(registry, 1, "PresentMod.Toggle", "F10", state);
			const auto snapshot = registry.Snapshot();
			const auto orphan = std::ranges::find(
				snapshot, std::string{ "RemovedMod.Toggle" }, &HotkeyActionSnapshot::id);
			require(orphan != snapshot.end(), "the orphaned override was dropped");
			require(!orphan->registered, "the orphan was shown as registered");
			require(orphan->overrideChord == "Shift+F11", "the orphan chord changed");
			require(registry.Overrides().contains("RemovedMod.Toggle"),
				"the orphan was not retained for persistence");
		});

		runner.test("hotkey callbacks defer both edges to dispatch", [] {
			HotkeyRegistry registry;
			CallbackState state;
			(void)Register(registry, 1, "Example.Toggle", "Shift+F11", state);
			require(registry.HandleKey(0x7A, kHotkeyModifierShift, true, false) ==
					HotkeyMessageResult::kConsumed,
				"the bound press was not consumed");
			require(registry.HandleKey(0x7A, kHotkeyModifierShift, true, true) ==
					HotkeyMessageResult::kConsumed,
				"the bound repeat was not consumed");
			require(registry.HandleKey(0x7A, 0, false, false) ==
					HotkeyMessageResult::kConsumed,
				"the matching release was not consumed");
			require(state.pressed == 0, "the press ran on the message thread");
			registry.DispatchQueued();
			require(state.pressed == 1, "the press was not dispatched");
			require(state.released == 1, "the release was not dispatched");
			require(state.edgeCount == 2 && state.edges[0] && !state.edges[1],
				"same-frame edges were collapsed, repeated, or reordered");
		});

		runner.test("unregister stops hotkey dispatch", [] {
			HotkeyRegistry registry;
			registry.BindRenderThread();
			CallbackState state;
			const auto action = Register(registry, 1, "Example.Toggle", "F10", state);
			require(registry.Unregister(1, action) == DMUI_RESULT_OK,
				"hotkey unregister failed");
			require(registry.HandleKey(0x79, 0, true, false) ==
					HotkeyMessageResult::kPassThrough,
				"an unregistered press was swallowed");
			require(registry.HandleKey(0x79, 0, false, false) ==
					HotkeyMessageResult::kPassThrough,
				"an unregistered release was swallowed");
			registry.DispatchQueued();
			require(state.edgeCount == 0, "an unregistered action dispatched");
			DMUI_HotkeyBindingInfo stale{};
			stale.structSize = sizeof(stale);
			require(registry.Query(1, action, &stale) == DMUI_RESULT_ACTION_NOT_FOUND,
				"a stale handle resolved after unregister");
			const auto replacement = Register(
				registry, 1, "Example.Toggle", "F10", state);
			require(replacement > action, "an unregistered handle was reused");
		});

		runner.test("unregister rejects another client action", [] {
			HotkeyRegistry registry;
			registry.BindRenderThread();
			CallbackState state;
			const auto action = Register(registry, 1, "Example.Toggle", "F10", state);
			require(registry.Unregister(2, action) == DMUI_RESULT_ACTION_NOT_FOUND,
				"another client unregistered the action");
			require(Query(registry, 1, action).state == DMUI_HOTKEY_BINDING_BOUND,
				"rejected unregister removed the action");
		});

		runner.test("unregister rejects a non-render thread", [] {
			HotkeyRegistry registry;
			registry.BindRenderThread();
			CallbackState state;
			const auto action = Register(registry, 1, "Example.Toggle", "F10", state);
			DMUI_Result result{ DMUI_RESULT_OK };
			std::thread worker{ [&] {
				result = registry.Unregister(1, action);
			} };
			worker.join();
			require(result == DMUI_RESULT_WRONG_THREAD,
				"non-render thread unregister was accepted");
			require(Query(registry, 1, action).state == DMUI_HOTKEY_BINDING_BOUND,
				"wrong-thread unregister removed the action");
		});

		runner.test("unregister invalidates queued hotkey events", [] {
			HotkeyRegistry registry;
			registry.BindRenderThread();
			CallbackState state;
			const auto action = Register(registry, 1, "Example.Toggle", "F10", state);
			require(registry.HandleKey(0x79, 0, true, false) ==
					HotkeyMessageResult::kConsumed,
				"the bound press was not queued");
			require(registry.Unregister(1, action) == DMUI_RESULT_OK,
				"hotkey unregister failed");
			registry.DispatchQueued();
			require(state.edgeCount == 0, "a queued event survived unregister");
			require(registry.HandleKey(0x79, 0, false, false) ==
					HotkeyMessageResult::kConsumed,
				"the canceled pair release was not swallowed");
			registry.DispatchQueued();
			require(state.edgeCount == 0, "a dead action release was dispatched");
		});

		runner.test("persisted override survives hotkey re-registration", [] {
			HotkeyRegistry registry;
			registry.BindRenderThread();
			CallbackState state;
			auto action = Register(registry, 1, "Example.Toggle", "F10", state);
			require(registry.SetOverride("Example.Toggle", "Shift+F11") == DMUI_RESULT_OK,
				"override setup failed");
			require(registry.Unregister(1, action) == DMUI_RESULT_OK,
				"hotkey unregister failed");
			const auto snapshot = registry.Snapshot();
			const auto orphan = std::ranges::find(
				snapshot, std::string{ "Example.Toggle" }, &HotkeyActionSnapshot::id);
			require(orphan != snapshot.end() && !orphan->registered,
				"the override was not retained as an orphan");
			require(std::ranges::count(
					snapshot, std::string{ "Example.Toggle" }, &HotkeyActionSnapshot::id) == 1,
				"the tombstone duplicated the orphan row");
			action = Register(registry, 1, "Example.Toggle", "F10", state);
			const auto binding = Query(registry, 1, action);
			require(binding.state == DMUI_HOTKEY_BINDING_BOUND &&
					std::string{ binding.chord } == "Shift+F11",
				"the retained override was not reapplied");
		});

		runner.test("unregister frees a conflicted chord", [] {
			HotkeyRegistry registry;
			registry.BindRenderThread();
			CallbackState state;
			const auto winner = Register(registry, 1, "Alpha.Toggle", "F10", state);
			const auto conflicted = Register(registry, 2, "Zulu.Toggle", "F10", state);
			require(Query(registry, 2, conflicted).state ==
					DMUI_HOTKEY_BINDING_UNBOUND_DEFAULT_CONFLICT,
				"the setup action was not conflicted");
			require(registry.Unregister(1, winner) == DMUI_RESULT_OK,
				"hotkey unregister failed");
			require(Query(registry, 2, conflicted).state == DMUI_HOTKEY_BINDING_BOUND,
				"the freed chord was not reassigned");
		});

		runner.test("unbound hotkey chords pass through untouched", [] {
			HotkeyRegistry registry;
			CallbackState state;
			(void)Register(registry, 1, "Example.Toggle", "F10", state);
			require(registry.HandleKey(0x7A, 0, true, false) ==
					HotkeyMessageResult::kPassThrough,
				"an unbound press was swallowed");
			require(registry.HandleKey(0x7A, 0, false, false) ==
					HotkeyMessageResult::kPassThrough,
				"an unbound release was swallowed");
			registry.DispatchQueued();
			require(state.edgeCount == 0, "an unbound chord was dispatched");
		});

		runner.test("hotkey queue overflow drops only complete pairs", [] {
			HotkeyRegistry registry;
			CallbackState state;
			(void)Register(registry, 1, "Example.Toggle", "F10", state);
			for (size_t index = 0; index < kHotkeyEventQueueCapacity / 2; ++index)
			{
				require(registry.HandleKey(0x79, 0, true, false) ==
						HotkeyMessageResult::kConsumed,
					"an in-capacity press was dropped");
				require(registry.HandleKey(0x79, 0, false, false) ==
						HotkeyMessageResult::kConsumed,
					"an in-capacity release was dropped");
			}
			require(registry.HandleKey(0x79, 0, true, false) ==
					HotkeyMessageResult::kConsumedPairDropped,
				"overflow did not drop the new whole pair");
			require(registry.HandleKey(0x79, 0, false, false) ==
					HotkeyMessageResult::kConsumed,
				"the dropped pair release was not swallowed");
			registry.DispatchQueued();
			require(state.pressed == kHotkeyEventQueueCapacity / 2 &&
					state.released == kHotkeyEventQueueCapacity / 2,
				"overflow delivered half a pair");
			require(state.edgeCount == kHotkeyEventQueueCapacity,
				"the bounded queue exceeded its capacity");
		});
	}
}
