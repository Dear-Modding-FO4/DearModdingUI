#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/host/MenuToggleChord.h>
#include <DearModdingUI/host/RenderExecution.h>
#include "../Harness.h"

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
			require(a_registry.Query(a_client, a_action, &binding) == DMUI_RESULT_OK,
				"hotkey query failed");
			return binding;
		}
	}

	void run_hotkey_checks(Runner& runner)
	{
		runner.test("gamepad capture ignores the activation button already held", [] {
			HotkeyRegistry registry;
			registry.SetContext({ true, false, false, false });
			(void)registry.HandleKey(KeyCatalog::kPadA, 0, true, false);
			registry.BeginCapture(HotkeySlot::kGamepad);
			(void)registry.HandleKey(KeyCatalog::kPadA, 0, true, true);
			(void)registry.HandleKey(KeyCatalog::kPadA, 0, false, false);
			require(registry.IsCapturing() && !registry.TakeCapture(),
				"the A that opened capture was bound on release");
			(void)registry.HandleKey(KeyCatalog::kPadY, 0, true, false);
			(void)registry.HandleKey(KeyCatalog::kPadY, 0, false, false);
			const auto captured = registry.TakeCapture();
			require(captured && SerializeHotkeyChord(*captured) == "PadY",
				"fresh gamepad press failed after pre-held A was released");
		});
		runner.test("key catalog tokens preserve identity and fit host chords", [] {
			for (size_t index = 0; index < KeyCatalog::kKeys.size(); ++index)
			{
				const auto& key = KeyCatalog::kKeys[index];
				require(KeyCatalog::Parse(key.token) == key.code &&
						KeyCatalog::Find(key.code) == &key,
					"catalog key did not round trip");
				require(!key.token.empty() && key.token.size() <= 16 &&
						key.token.size() + std::string_view{ "Ctrl+Alt+Shift+" }.size() <= 31 &&
						key.token.find_first_of("+ \t\r\n\v\f") == std::string_view::npos,
					"catalog token cannot fit the chord grammar or ABI buffer");
				for (size_t other = index + 1; other < KeyCatalog::kKeys.size(); ++other)
					require(!EqualsIgnoringCase(key.token, KeyCatalog::kKeys[other].token),
						"catalog tokens are ambiguous");
				if (IsHostBindableKey(key.code))
				{
					const HotkeyChord chord{ key.code,
						HotkeySlotForKey(key.code) == HotkeySlot::kGamepad ? 0u : 7u };
					const auto parsed = ParseHotkeyChord(SerializeHotkeyChord(chord));
					require(parsed.recognized && parsed.chord == chord,
						"bindable catalog key was lost in host parsing");
				}
			}
			for (const auto& [token, code] : std::array{
					 std::pair{ "f11", 0x57u }, std::pair{ "End", 0xCFu },
					 std::pair{ "PgUp", 0xC9u }, std::pair{ "PgDn", 0xD1u },
					 std::pair{ "a", 0x1Eu }, std::pair{ "0", 0x0Bu },
					 std::pair{ "9", 0x0Au }, std::pair{ "Esc", 0x01u } })
				require(KeyCatalog::Parse(token) == code,
					"legacy name or alias changed identity");
			for (const auto token : { "Mouse1", "Mouse2", "Escape", "LeftShift" })
				require(!ParseHotkeyChord(token).recognized,
					"host accepted a key without a supported binding producer");
			require(ParseHotkeyChord("Pause").recognized,
				"Pause is still excluded from keyboard bindings");
			const auto pageDown = ParseMenuToggleChord("Pgdn");
			require(
				pageDown.recognized && pageDown.chord == HotkeyChord{ 0xD1, 0 } &&
					SerializeHotkeyChord(pageDown.chord) == "PageDown" &&
					ParseMenuToggleChord("pageup").chord == HotkeyChord{ 0xC9, 0 },
				"Page Up/Down names or aliases were not accepted");
		});

		runner.test("key capture owns edges before actions and cancels cleanly", [] {
			HotkeyRegistry registry;
			CallbackState state;
			(void)Register(registry, 1, "Example.Capture", "Ctrl+F5", state);
			registry.SetContext({ true, false, false, false });
			registry.BeginCapture();
			require(registry.HandleKey(0x1D, kHotkeyModifierControl, true, false) ==
					HotkeyMessageResult::kPassThrough && registry.IsCapturing(),
				"modifier press ended capture or was consumed");
			require(registry.HandleKey(0x3F, kHotkeyModifierControl, true, true) ==
					HotkeyMessageResult::kPassThrough && registry.IsCapturing(),
				"repeat was captured");
			(void)registry.HandleKey(0x3F, 0, false, false);
			require(registry.HandleKey(0x3F, kHotkeyModifierControl, true, false) ==
					HotkeyMessageResult::kConsumed && registry.IsCapturing() &&
					!registry.TakeCapture(),
				"capture did not consume the fresh press");
			require(registry.HandleKey(0x3F, 0, true, true) ==
					HotkeyMessageResult::kConsumed &&
					registry.HandleKey(0x3F, 0, false, false) ==
						HotkeyMessageResult::kConsumed,
				"capture did not retain the repeat and release");
			const auto captured = registry.TakeCapture();
			require(captured && *captured == HotkeyChord{ 0x3F, kHotkeyModifierControl } &&
					!registry.TakeCapture() && !registry.IsCapturing(),
				"capture lost modifiers or was delivered twice");
			registry.DispatchQueued();
			require(state.edgeCount == 0, "capture also fired the bound action");
			registry.BeginCapture();
			require(registry.CancelCapture() && !registry.CancelCapture() &&
					!registry.TakeCapture(),
				"cancel did not clear capture exactly once");
			registry.BeginCapture();
			registry.SetContext({});
			require(registry.HandleKey(0x3E, 0, true, false) ==
					HotkeyMessageResult::kPassThrough &&
					!registry.IsCapturing() && !registry.TakeCapture(),
				"hidden host captured a key");
		});

		runner.test("hotkey registration validates ids, chords, and global identity", [] {
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
			auto valid = Descriptor("Example.Toggle", "F10", state);
			require(registry.Register(1, &valid, &handle) == DMUI_RESULT_OK,
				"valid hotkey registration failed");
			auto duplicate = Descriptor("Example.Toggle", "F11", state);
			require(registry.Register(2, &duplicate, &handle) ==
					DMUI_RESULT_DUPLICATE_ACTION_ID,
				"a cross-client duplicate was accepted");
		});

		runner.test("hotkey chord strings round trip including none", [] {
			for (const auto chord : {
					 "F11",
					 "Shift+F11",
					 "Ctrl+Alt+Home",
					 "A",
					 "Ctrl+7",
					 "none" })
			{
				const auto parsed = ParseHotkeyChord(chord);
				require(parsed.recognized, std::string{ chord } + " was rejected");
				require(ParseHotkeyChord(SerializeHotkeyChord(parsed.chord)).chord ==
						parsed.chord,
					std::string{ chord } + " did not round trip");
			}
			require(!ParseHotkeyChord("Meta+F11").recognized, "an unknown modifier was accepted");
			require(!ParseHotkeyChord("Shift+").recognized, "a missing key was accepted");
			require(!ParseHotkeyChord("F11+").recognized, "a trailing separator was accepted");
			const auto combo = ParseHotkeyChord("PadRB+PadLB+PadBack");
			require(combo.recognized &&
					SerializeHotkeyChord(combo.chord) == "PadBack+PadLB+PadRB" &&
					ParseHotkeyChord("PadLB+PadBack+PadRB").chord == combo.chord,
				"multi-key identity was not canonical");
			for (const auto invalid : { "F1+F1", "A+B+C+D", "A+PadA", "Mouse3+PadLB",
					 "Ctrl+PadLB", "A++B", "Ctrl+Alt+Shift+PgDn+PgUp+End" })
				require(!ParseHotkeyChord(invalid).recognized,
					std::string{ invalid } + " was accepted");
			const auto capacity = ParseHotkeyChord("Ctrl+Alt+Shift+NumpadDecimal+F1");
			require(capacity.recognized && SerializeHotkeyChord(capacity.chord).size() == 31,
				"largest ABI-compatible chord was rejected");
			const auto keyboardCombo = ParseHotkeyChord("Shift+Mouse3+F5+A");
			require(keyboardCombo.recognized &&
					SerializeHotkeyChord(keyboardCombo.chord) == "Shift+A+F5+Mouse3",
				"keyboard/mouse combination lost ordering");
		});

		runner.test("dual slots match held combinations and release the triggering key", [] {
			HotkeyRegistry registry;
			CallbackState state;
			const auto action = Register(registry, 1, "Example.Combo", "Ctrl+Mouse3", state);
			require(registry.SetOverride("Example.Combo", "PadLB+PadRB+PadBack", HotkeySlot::kGamepad) ==
					DMUI_RESULT_OK,
				"gamepad override was not accepted");
			std::array<uint32_t, 3> keys{ KeyCatalog::kPadBack, KeyCatalog::kPadLB, KeyCatalog::kPadRB };
			do
			{
				require(registry.HandleKey(keys[0], 0, true, false) == HotkeyMessageResult::kPassThrough &&
						registry.HandleKey(keys[1], 0, true, false) == HotkeyMessageResult::kPassThrough,
					"subset fired a three-button chord");
				require(registry.HandleKey(keys[2], 0, true, false) == HotkeyMessageResult::kConsumed &&
						registry.HandleKey(keys[2], 0, true, true) == HotkeyMessageResult::kConsumed,
					"last press did not activate or retain repeats");
				registry.DispatchQueued();
				require(state.pressed == state.released + 1, "press did not reach the action");
				(void)registry.HandleKey(keys[0], 0, false, false);
				require(registry.HandleKey(keys[0], 0, true, false) == HotkeyMessageResult::kPassThrough,
					"repressing a non-trigger key duplicated the active slot");
				(void)registry.HandleKey(keys[0], 0, false, false);
				registry.DispatchQueued();
				require(state.pressed == state.released + 1, "non-trigger release ended activation");
				require(registry.HandleKey(keys[2], 0, false, false) == HotkeyMessageResult::kConsumed,
					"trigger release was not owned");
				(void)registry.HandleKey(keys[1], 0, false, false);
				registry.DispatchQueued();
				require(state.pressed == state.released, "trigger release was not dispatched");
			} while (std::next_permutation(keys.begin(), keys.end()));
			require(registry.HandleKey(KeyCatalog::kMouseButtonOffset + 2, kHotkeyModifierControl, true, false) ==
					HotkeyMessageResult::kConsumed,
				"keyboard/mouse slot stopped working after assigning gamepad");
			(void)registry.HandleKey(KeyCatalog::kPadBack, 0, true, false);
			(void)registry.HandleKey(KeyCatalog::kPadLB, 0, true, false);
			require(registry.HandleKey(KeyCatalog::kPadRB, kHotkeyModifierControl, true, false) ==
					HotkeyMessageResult::kConsumed,
				"keyboard activation or modifiers blocked the gamepad slot");
			registry.DispatchQueued();
			require(state.pressed == 8 && state.released == 6,
				"slots could not activate independently");
			(void)registry.HandleKey(KeyCatalog::kMouseButtonOffset + 2, 0, false, false);
			registry.ReleaseActiveKeys();
			registry.DispatchQueued();
			require(state.pressed == 8 && state.released == 8 &&
					std::string{ Query(registry, 1, action).chord } == "Ctrl+Mouse3",
				"dual slots changed callback identity or ABI query slot");
		});

		runner.test("slot conflicts and reserved toggles share one matching owner", [] {
			HotkeyRegistry registry;
			CallbackState state;
			registry.SetReservedChord(ParseHotkeyChord("Ctrl+F5").chord);
			registry.SetReservedChord(ParseHotkeyChord("PadLB+PadRB+PadBack").chord, HotkeySlot::kGamepad);
			const auto action = Register(registry, 1, "Example.First", "Ctrl+F5", state);
			(void)Register(registry, 2, "Example.Second", "F6", state);
			require(registry.SetOverride("Example.Second", "Ctrl+F5") ==
						DMUI_RESULT_DUPLICATE_ACTION_ID,
				"a conflicting edit bypassed the toggle chord reservation");
			require(Query(registry, 1, action).state == DMUI_HOTKEY_BINDING_UNBOUND_DEFAULT_CONFLICT &&
					registry.SetOverride("Example.First", "PadBack+PadRB+PadLB", HotkeySlot::kGamepad) ==
						DMUI_RESULT_DUPLICATE_ACTION_ID,
				"reserved toggle did not block a same-slot client");
			require(registry.SetOverride("Example.First", "PadA", HotkeySlot::kGamepad) == DMUI_RESULT_OK &&
					registry.SetOverride("Example.Second", "PadA", HotkeySlot::kGamepad) ==
						DMUI_RESULT_DUPLICATE_ACTION_ID &&
					registry.Snapshot()[1].bindings[0].state == DMUI_HOTKEY_BINDING_BOUND,
				"gamepad conflict changed the independent keyboard slot");
			require(registry.HandleKey(0x3F, 0, true, false) == HotkeyMessageResult::kPassThrough,
				"toggle ignored exact modifiers");
			(void)registry.HandleKey(0x3F, 0, false, false);
			require(registry.HandleKey(0x3F, kHotkeyModifierControl, true, false) ==
					HotkeyMessageResult::kMenuToggle &&
					registry.HandleKey(0x3F, kHotkeyModifierControl, true, true) ==
						HotkeyMessageResult::kConsumed &&
					registry.HandleKey(0x3F, 0, false, false) == HotkeyMessageResult::kConsumed,
				"keyboard toggle did not own exactly one press pair");
			(void)registry.HandleKey(KeyCatalog::kPadLB, 0, true, false);
			(void)registry.HandleKey(KeyCatalog::kPadRB, 0, true, false);
			require(registry.HandleKey(KeyCatalog::kPadBack, 0, true, false) == HotkeyMessageResult::kMenuToggle &&
					registry.HandleKey(KeyCatalog::kPadBack, 0, true, true) == HotkeyMessageResult::kConsumed &&
					registry.HandleKey(KeyCatalog::kPadBack, 0, false, false) == HotkeyMessageResult::kConsumed,
				"gamepad toggle did not own exactly one press pair");
			registry.DispatchQueued();
			require(state.edgeCount == 0, "reserved toggle called a client");
			registry.SetReservedChord({ 0x3F, 0 });
			require(Query(registry, 1, action).state == DMUI_HOTKEY_BINDING_BOUND,
				"changing the toggle chord did not recompute exact conflicts");
		});

		runner.test("capture accumulates only the selected slot and consumes remaining releases", [] {
			HotkeyRegistry registry;
			registry.SetReservedChord(ParseHotkeyChord("PadLB+PadRB+PadBack").chord, HotkeySlot::kGamepad);
			registry.SetContext({ true });
			registry.BeginCapture(HotkeySlot::kGamepad);
			for (const auto code : { 275u, 271u, 274u })
				require(registry.HandleKey(code, 7, true, false) == HotkeyMessageResult::kConsumed &&
						registry.IsCapturing() && !registry.TakeCapture(),
					"gamepad capture completed before release");
			require(registry.HandleKey(KeyCatalog::kPadBack, 0, false, false) == HotkeyMessageResult::kConsumed &&
					registry.TakeCapture() == ParseHotkeyChord("PadLB+PadRB+PadBack").chord &&
					!registry.IsCapturing(),
				"first release lost the held set or retained keyboard modifiers");
			for (const auto code : { 274u, 275u })
				require(registry.HandleKey(code, 0, false, false) == HotkeyMessageResult::kConsumed,
					"remaining capture release escaped");
			registry.BeginCapture();
			require(registry.HandleKey(KeyCatalog::kPadA, 0, true, false) == HotkeyMessageResult::kPassThrough &&
					registry.HandleKey(KeyCatalog::kMouseButtonOffset, 0, true, false) == HotkeyMessageResult::kPassThrough &&
					registry.IsCapturing(),
				"keyboard capture accepted gamepad or Mouse1");
			require(registry.HandleKey(KeyCatalog::kMouseButtonOffset + 2, kHotkeyModifierAlt, true, false) == HotkeyMessageResult::kConsumed &&
					registry.HandleKey(KeyCatalog::kMouseButtonOffset + 2, 0, false, false) == HotkeyMessageResult::kConsumed &&
					registry.TakeCapture() == ParseHotkeyChord("Alt+Mouse3").chord,
				"mouse capture did not preserve press modifiers");
			registry.BeginCapture();
			for (const auto code : { 0x1Eu, 0x30u, 0x2Eu, 0x20u })
				(void)registry.HandleKey(code, 0, true, false);
			(void)registry.HandleKey(0x1E, 0, false, false);
			require(!registry.IsCapturing() && !registry.TakeCapture(),
				"over-capacity capture silently bound a subset");
			registry.ReleaseActiveKeys();
			registry.SetContext({ true });
			registry.BeginCapture();
			for (const auto code : { 0xD1u, 0xC9u, 0xCFu })
				(void)registry.HandleKey(code, 7, true, false);
			(void)registry.HandleKey(0xCF, 0, false, false);
			require(!registry.TakeCapture() && !registry.IsCapturing(),
				"captured canonical chord exceeded the ABI buffer");
		});

		runner.test("hotkey contexts are checked before consuming presses", [] {
			HotkeyRegistry registry;
			CallbackState state;
			auto descriptor = Descriptor("Example.Context", "Ctrl+P", state);
			descriptor.contextPolicy =
				DMUI_HOTKEY_CONTEXT_GAMEPLAY_UNOBSTRUCTED;
			DMUI_HotkeyActionHandle action{};
			require(registry.Register(1, &descriptor, &action) == DMUI_RESULT_OK,
				"contextual hotkey registration failed");
			registry.SetContext({ false, false, false, false });
			require(registry.HandleKey(0x19, kHotkeyModifierControl, true, false) ==
					HotkeyMessageResult::kPassThrough,
				"unsafe gameplay context consumed a press");
			registry.SetContext({ false, false, false, true });
			require(registry.HandleKey(0, kHotkeyModifierControl, true, false) ==
					HotkeyMessageResult::kPassThrough,
				"unknown scan code matched chord padding");
			(void)registry.HandleKey(0x19, 0, false, false);
			require(registry.HandleKey(0x19, kHotkeyModifierControl, true, false) ==
					HotkeyMessageResult::kConsumed,
				"safe gameplay context did not consume a press");
			registry.SetContext({ true, false, false, false });
			require(registry.HandleKey(0x19, 0, false, false) ==
					HotkeyMessageResult::kConsumed,
				"an owned release was lost after the context changed");
			registry.DispatchQueued();
			require(state.pressed == 1 && state.released == 1,
				"context transition broke the owned edge pair");
		});

		runner.test("disabled hotkeys yield but retain owned releases", [] {
			HotkeyRegistry registry;
			CallbackState state;
			const auto action = Register(
				registry, 1, "Example.Enabled", "G", state);
			require(registry.HandleKey(0x22, 0, true, false) ==
					HotkeyMessageResult::kConsumed,
				"enabled alphanumeric action did not consume");
			require(registry.SetEnabled(1, action, false) == DMUI_RESULT_OK,
				"hotkey disable failed");
			require(registry.HandleKey(0x22, 0, false, false) ==
					HotkeyMessageResult::kConsumed,
				"disable lost the owned release");
			require(registry.HandleKey(0x22, 0, true, false) ==
					HotkeyMessageResult::kPassThrough,
				"disabled action consumed a new press");
			registry.DispatchQueued();
			require(state.pressed == 1 && state.released == 1,
				"disable changed an already-owned pair");
		});

		runner.test("focus loss synthesizes ordered owned releases", [] {
			HotkeyRegistry registry;
			CallbackState state;
			(void)Register(registry, 1, "Example.Focus", "H", state);
			require(registry.HandleKey(0x23, 0, true, false) ==
					HotkeyMessageResult::kConsumed,
				"focus test press was not consumed");
			registry.ReleaseActiveKeys();
			require(registry.HandleKey(0x23, 0, false, false) ==
					HotkeyMessageResult::kPassThrough,
				"physical release after reconciliation was swallowed");
			registry.DispatchQueued();
			require(state.edgeCount == 2 &&
					state.edges[0] &&
					!state.edges[1],
				"focus reconciliation did not preserve edge order");
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

		runner.test("default conflicts reassign when the winning action unregisters", [] {
			HotkeyRegistry registry;
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameObservation
			};
			(void)execution.NoteBinding(1);
			CallbackState state;
			const auto later = Register(registry, 1, "Zulu.Toggle", "F10", state);
			const auto earlier = Register(registry, 2, "Alpha.Toggle", "F10", state);
			require(Query(registry, 2, earlier).state == DMUI_HOTKEY_BINDING_BOUND,
				"stable id ordering did not select the winner");
			require(Query(registry, 1, later).state ==
					DMUI_HOTKEY_BINDING_UNBOUND_DEFAULT_CONFLICT,
				"the taken default was not distinguished");
			require(registry.Unregister(2, earlier) == DMUI_RESULT_OK,
				"winning hotkey unregister failed");
			require(Query(registry, 1, later).state == DMUI_HOTKEY_BINDING_BOUND,
				"the freed chord was not reassigned");
		});

		runner.test("persisted overrides survive orphaning and re-registration", [] {
			HotkeyRegistry registry;
			registry.InitializeOverrides({
				{ "RemovedMod.Toggle", "Shift+F11" }
			});
			registry.InitializeOverrides({
				{ "RemovedPad.Toggle", "PadA" },
				{ "Example.Toggle", "PadB" }
			}, HotkeySlot::kGamepad);
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameObservation
			};
			(void)execution.NoteBinding(1);
			CallbackState state;
			auto action = Register(registry, 1, "Example.Toggle", "F10", state);
			require(registry.SetOverride("Example.Toggle", "Ctrl+F11") ==
					DMUI_RESULT_OK,
				"override setup failed");
			require(registry.Unregister(1, action) == DMUI_RESULT_OK,
				"hotkey unregister failed");
			const auto snapshot = registry.Snapshot();
			const auto removed = std::ranges::find(
				snapshot, std::string{ "RemovedMod.Toggle" }, &HotkeyActionSnapshot::id);
			const auto orphanedAction = std::ranges::find(
				snapshot, std::string{ "Example.Toggle" }, &HotkeyActionSnapshot::id);
			require(removed != snapshot.end() && !removed->registered &&
					removed->bindings[0].overrideChord == "Shift+F11",
				"pre-existing orphaned override was dropped or changed");
			require(orphanedAction != snapshot.end() &&
					!orphanedAction->registered &&
					orphanedAction->bindings[0].overrideChord == "Ctrl+F11" &&
					orphanedAction->bindings[1].overrideChord == "PadB",
				"unregistered action was not retained as an orphan");
			require(std::ranges::count(
					snapshot, std::string{ "Example.Toggle" }, &HotkeyActionSnapshot::id) == 1,
				"the tombstone duplicated the orphan row");
			require(registry.Overrides().contains("RemovedMod.Toggle") &&
					registry.Overrides().contains("Example.Toggle") &&
					registry.Overrides(HotkeySlot::kGamepad).contains("RemovedPad.Toggle"),
				"orphaned overrides were not retained for persistence");
			const auto stale = action;
			action = Register(registry, 1, "Example.Toggle", "F10", state);
			require(registry.HandleKey(KeyCatalog::kPadB, 0, true, false) == HotkeyMessageResult::kConsumed,
				"re-registration lost the gamepad override");
			(void)registry.HandleKey(KeyCatalog::kPadB, 0, false, false);
			const auto binding = Query(registry, 1, action);
			require(action > stale &&
					binding.state == DMUI_HOTKEY_BINDING_BOUND &&
					std::string{ binding.chord } == "Ctrl+F11",
				"re-registration reused a handle or lost its persisted override");
		});

		runner.test("hotkey callbacks defer both edges to dispatch", [] {
			HotkeyRegistry registry;
			CallbackState state;
			(void)Register(registry, 1, "Example.Toggle", "Shift+F11", state);
			require(registry.HandleKey(0x57, kHotkeyModifierShift, true, false) ==
					HotkeyMessageResult::kConsumed,
				"the bound press was not consumed");
			require(registry.HandleKey(0x57, kHotkeyModifierShift, true, true) ==
					HotkeyMessageResult::kConsumed,
				"the bound repeat was not consumed");
			require(registry.HandleKey(0x57, 0, false, false) ==
					HotkeyMessageResult::kConsumed,
				"the matching release was not consumed");
			require(state.pressed == 0, "the press ran on the message thread");
			registry.DispatchQueued();
			require(state.pressed == 1, "the press was not dispatched");
			require(state.released == 1, "the release was not dispatched");
			require(state.edgeCount == 2 && state.edges[0] && !state.edges[1],
				"same-frame edges were collapsed, repeated, or reordered");
		});

		runner.test("unregister enforces client ownership and render execution", [] {
			HotkeyRegistry registry;
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameObservation
			};
			(void)execution.NoteBinding(1);
			CallbackState state;
			const auto action = Register(registry, 1, "Example.Toggle", "F10", state);
			require(registry.Unregister(2, action) == DMUI_RESULT_ACTION_NOT_FOUND,
				"another client unregistered the action");
			require(Query(registry, 1, action).state == DMUI_HOTKEY_BINDING_BOUND,
				"rejected unregister removed the action");
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
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameObservation
			};
			(void)execution.NoteBinding(1);
			CallbackState state;
			const auto action = Register(registry, 1, "Example.Toggle", "F10", state);
			require(registry.HandleKey(0x44, 0, true, false) ==
					HotkeyMessageResult::kConsumed,
				"the bound press was not queued");
			require(registry.Unregister(1, action) == DMUI_RESULT_OK,
				"hotkey unregister failed");
			registry.DispatchQueued();
			require(state.edgeCount == 0, "a queued event survived unregister");
			require(registry.HandleKey(0x44, 0, false, false) ==
					HotkeyMessageResult::kConsumed,
				"the canceled pair release was not swallowed");
			registry.DispatchQueued();
			require(state.edgeCount == 0, "a dead action release was dispatched");
			DMUI_HotkeyBindingInfo stale{};
			require(registry.Query(1, action, &stale) ==
					DMUI_RESULT_ACTION_NOT_FOUND,
				"a stale handle resolved after unregister");
			require(registry.HandleKey(0x44, 0, true, false) ==
					HotkeyMessageResult::kPassThrough,
				"an unregistered action consumed a new press");
		});

		runner.test("hotkey queue overflow drops only complete pairs", [] {
			HotkeyRegistry registry;
			CallbackState state;
			(void)Register(registry, 1, "Example.Toggle", "F10", state);
			for (size_t index = 0; index < kHotkeyEventQueueCapacity / 2; ++index)
			{
				require(registry.HandleKey(0x44, 0, true, false) ==
						HotkeyMessageResult::kConsumed,
					"an in-capacity press was dropped");
				require(registry.HandleKey(0x44, 0, false, false) ==
						HotkeyMessageResult::kConsumed,
					"an in-capacity release was dropped");
			}
			require(registry.HandleKey(0x44, 0, true, false) ==
					HotkeyMessageResult::kConsumedPairDropped,
				"overflow did not drop the new whole pair");
			require(registry.HandleKey(0x44, 0, false, false) ==
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
