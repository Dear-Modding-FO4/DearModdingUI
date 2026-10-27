#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/ColorEdit.h>
#include <DearModdingUI/KeyCapture.h>
#include <DearModdingUI/Numeric.h>
#include <DearModdingUI/controls/ColorEdit.h>
#include <DearModdingUI/controls/KeyCapture.h>
#include <DearModdingUI/controls/Numeric.h>
#include <DearModdingUI/localization/Localization.h>
#include <DearModdingUI/controls/TextInput.h>
#include <DearModdingUI/controls/Toggle.h>
#include <DearModdingUI/host/RenderExecution.h>
#include "../support/ImGuiTestContext.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;
	using namespace support::host;

	namespace
	{
		inline constexpr DMUI_ClientHandle kResizeTestClient{ 0x51u };

		void SetTestClipboard(std::string& a_text)
		{
			auto& platform = ImGui::GetPlatformIO();
			platform.Platform_ClipboardUserData = &a_text;
			platform.Platform_GetClipboardTextFn =
				[](ImGuiContext* a_context) {
					return static_cast<const std::string*>(
							   a_context->PlatformIO.
								   Platform_ClipboardUserData)
						->c_str();
				};
		}

		[[nodiscard]] bool Edited(const DMUI_TextEditState& a_state) noexcept
		{
			return (a_state.events & DMUI_UI_TEXT_EDIT_EVENTS_EDITED) != 0;
		}

		void BeginFieldFrame(
			support::ImGuiTestContext& a_imgui,
			const char* a_name)
		{
			a_imgui.BeginWindow(
				a_name,
				{ 0.0f, 0.0f },
				{ 640.0f, 180.0f },
				ImGuiWindowFlags_NoDecoration |
					ImGuiWindowFlags_NoSavedSettings,
				ImGuiCond_Always);
			ImGui::SetCursorScreenPos({ 20.0f, 20.0f });
		}

		// Activates the field with a click, then runs a ctrl+V paste.
		template <class Frame>
		void ClickThenPaste(support::ImGuiTestContext& a_imgui, Frame&& a_frame)
		{
			a_frame();
			auto& io = ImGui::GetIO();
			io.AddMousePosEvent(80.0f, 32.0f);
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
			a_frame();
			io.AddMousePosEvent(80.0f, 32.0f);
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
			a_frame();
			require(ImGui::GetCurrentContext()->ActiveId != 0,
				"field did not retain focus after mouse release");
			io.AddKeyEvent(ImGuiMod_Ctrl, true);
			io.AddKeyEvent(ImGuiKey_V, true);
			a_frame();
			io.AddKeyEvent(ImGuiKey_V, false);
			io.AddKeyEvent(ImGuiMod_Ctrl, false);
			(void)a_imgui;
		}

		// A fixed buffer bounds input by its capacity; a growable one by maxBytes.
		[[nodiscard]] std::pair<std::string, bool> EditField(
			std::string a_initial,
			size_t a_maximumBytes,
			bool a_growable,
			const char* a_input)
		{
			std::string clipboard{ a_input };
			support::ImGuiTestContext imgui{
				{
					.disableInputTrickle = true,
					.disableErrorRecovery = true
				}
			};
			SetTestClipboard(clipboard);
			std::vector<char> fixed((std::max)(a_maximumBytes, a_initial.size()) + 1);
			std::ranges::copy(a_initial, fixed.begin());
			std::string growable{ a_initial };
			auto edited = false;
			ClickThenPaste(imgui, [&] {
				BeginFieldFrame(imgui, "##FieldLimit");
				DMUI_TextEditState state{};
				if (a_growable)
				{
					bool changed{};
					require(
						DrawTextField(
							"limit",
							growable,
							{
								.hint = "Search...",
								.style = DMUI_TEXT_FIELD_STYLE_SEARCH,
								.maxBytes = static_cast<uint32_t>(a_maximumBytes)
							},
							changed) == DMUI_RESULT_OK,
						"limited field failed to draw");
					edited |= changed;
				}
				else
				{
					DMUI_TextBuffer buffer{ fixed.data(), fixed.size(), nullptr, nullptr };
					require(
						DrawTextField(
							"limit",
							buffer,
							{ .hint = "Search...", .style = DMUI_TEXT_FIELD_STYLE_SEARCH },
							state) == DMUI_RESULT_OK,
						"fixed field failed to draw");
					edited |= Edited(state);
				}
				imgui.EndWindow(true);
			});
			return { a_growable ? growable : std::string{ fixed.data() }, edited };
		}

		// Presses and releases the left button at a point across three frames.
		template <class Frame>
		void ClickAt(const ImVec2& a_point, Frame&& a_frame)
		{
			auto& io = ImGui::GetIO();
			io.AddMousePosEvent(a_point.x, a_point.y);
			a_frame();
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
			a_frame();
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
			a_frame();
		}

		// Draws one numeric control per frame inside the field frame.
		struct NumericHarness
		{
			explicit NumericHarness(DMUI_NumericDescriptor a_descriptor) :
				descriptor(a_descriptor),
				value(a_descriptor.value)
			{
				Frame();
				height = ImGui::GetFrameHeight();
			}

			void Frame()
			{
				BeginFieldFrame(imgui, "##NumericFrame");
				ImGui::BeginDisabled(disabled);
				auto current = descriptor;
				current.value = value;
				const auto result = DrawNumeric(current);
				ImGui::EndDisabled();
				if (result.changed != 0)
				{
					value = result.value;
					++changes;
				}
				imgui.EndWindow(true);
			}

			// A click at a fraction of the control width, then release.
			void ClickAt(float a_fraction)
			{
				vmm_tests::ClickAt(
					{ 20.0f + descriptor.width * a_fraction, 20.0f + height * 0.5f },
					[&] { Frame(); });
			}

			// Focuses the field, types text over its selected content, and commits it.
			void Type(const char* a_text)
			{
				auto& io = ImGui::GetIO();
				vmm_tests::ClickAt({ 40.0f, 20.0f + height * 0.5f }, [&] { Frame(); });
				io.AddInputCharactersUTF8(a_text);
				Frame();
				Frame();
				io.AddKeyEvent(ImGuiKey_Enter, true);
				Frame();
				io.AddKeyEvent(ImGuiKey_Enter, false);
				Frame();
			}

			support::ImGuiTestContext imgui{
				{ .disableInputTrickle = true, .disableErrorRecovery = true }
			};
			DMUI_NumericDescriptor descriptor;
			DMUI_NumericValue value;
			float height{};
			bool disabled{};
			int changes{};
		};

		[[nodiscard]] dmui::NumericOptions<int32_t> IntegerControl(
			dmui::NumericPresentation a_presentation)
		{
			return {
				.id = "numeric",
				.presentation = a_presentation,
				.value = 30,
				.minimum = 0,
				.maximum = 100,
				.step = 10,
				.clamp = true,
				.width = 200.0f
			};
		}

		struct RejectingTextBuffer
		{
			explicit RejectingTextBuffer(std::string_view a_text)
			{
				storage.resize(16);
				std::ranges::copy(a_text, storage.begin());
			}

			[[nodiscard]] DMUI_TextBuffer Descriptor() noexcept
			{
				return {
					.data = storage.data(),
					.capacity = storage.size(),
					.resize = Resize,
					.userData = this
				};
			}

			static DMUI_Result DMUI_CALL Resize(
				void* a_userData,
				size_t,
				char**,
				size_t*) noexcept
			{
				auto& self =
					*static_cast<RejectingTextBuffer*>(a_userData);
				++self.resizeCalls;
				self.renderExecutionActive = RenderExecution::IsActive();
				self.drawingClientVisible =
					RenderExecution::IsActiveClient(
						kResizeTestClient,
						true);
				return DMUI_RESULT_BUFFER_TOO_SMALL;
			}

			std::vector<char> storage;
			size_t resizeCalls{};
			bool renderExecutionActive{};
			bool drawingClientVisible{};
		};

		[[nodiscard]] DMUI_Result DrawBufferFrame(
			support::ImGuiTestContext& a_imgui,
			DMUI_TextBuffer& a_buffer,
			bool& a_changed)
		{
			BeginFieldFrame(a_imgui, "##GrowableField");
			DMUI_TextEditState state{};
			const auto result = DrawTextField(
				"growable",
				a_buffer,
				{ .hint = "Search...", .style = DMUI_TEXT_FIELD_STYLE_SEARCH },
				state);
			a_changed = Edited(state);
			a_imgui.EndWindow(true);
			return result;
		}

		// Types text and returns what the field holds afterward.
		[[nodiscard]] std::string TypeInto(
			TextFieldSpec a_spec,
			const char* a_text)
		{
			support::ImGuiTestContext imgui{
				{
					.disableInputTrickle = true,
					.disableErrorRecovery = true
				}
			};
			std::string text;
			const auto frame = [&] {
				BeginFieldFrame(imgui, "##FieldType");
				bool edited{};
				require(
					DrawTextField("type", text, a_spec, edited) == DMUI_RESULT_OK,
					"field failed to draw");
				imgui.EndWindow(true);
			};
			frame();
			auto& io = ImGui::GetIO();
			io.AddMousePosEvent(80.0f, 32.0f);
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
			frame();
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
			frame();
			io.AddInputCharactersUTF8(a_text);
			frame();
			frame();
			return text;
		}
	}

	void run_host_control_checks(Runner& runner)
	{
		runner.test("toggle flips on click, stays put when disabled, and validates", [] {
			support::ImGuiTestContext imgui{
				{ .disableInputTrickle = true, .disableErrorRecovery = true }
			};
			auto value = false;
			auto changes = 0;
			auto disabled = false;
			const auto frame = [&] {
				BeginFieldFrame(imgui, "##ToggleFrame");
				ImGui::BeginDisabled(disabled);
				const auto result = DrawToggle({ .id = "toggle", .value = value ? 1u : 0u });
				ImGui::EndDisabled();
				value = result.value != 0;
				changes += result.changed != 0 ? 1 : 0;
				imgui.EndWindow(true);
			};
			frame();
			const auto half = ImGui::GetFrameHeight() * 0.5f;
			const ImVec2 center{ 20.0f + half, 20.0f + half };
			ClickAt(center, frame);
			require(value && changes == 1, "a click did not check the toggle");
			ClickAt(center, frame);
			require(!value && changes == 2, "a second click did not clear the toggle");
			disabled = true;
			ClickAt(center, frame);
			require(!value && changes == 2, "a disabled toggle changed");

			const DMUI_ToggleDescriptor ok{ "toggle", nullptr, 1u, 0u };
			DMUI_ToggleDescriptor noId = ok;
			noId.id = nullptr;
			DMUI_ToggleDescriptor emptyId = ok;
			emptyId.id = "";
			DMUI_ToggleDescriptor reserved = ok;
			reserved.reserved = 1u;
			require(ValidateToggle(&ok) == DMUI_RESULT_OK &&
					ValidateToggle(nullptr) == DMUI_RESULT_INVALID_ARGUMENT &&
					ValidateToggle(&noId) == DMUI_RESULT_INVALID_ARGUMENT &&
					ValidateToggle(&emptyId) == DMUI_RESULT_INVALID_ARGUMENT &&
					ValidateToggle(&reserved) == DMUI_RESULT_INVALID_ARGUMENT,
				"a malformed toggle descriptor was accepted");
		});

		runner.test("numeric slider moves with its pointer, snaps to the step, and respects disabled", [] {
			NumericHarness harness{ IntegerControl(dmui::NumericPresentation::kSlider).Abi() };
			harness.ClickAt(0.5f);
			require(harness.value.i == 50 && harness.changes == 1,
				"a mid-slider click did not land on the nearest step");
			harness.ClickAt(0.999f);
			require(harness.value.i == 100, "the slider end did not reach the maximum");
			harness.ClickAt(0.0f);
			require(harness.value.i == 0, "the slider start did not reach the minimum");
			const auto changes = harness.changes;
			harness.disabled = true;
			harness.ClickAt(0.5f);
			require(harness.value.i == 0 && harness.changes == changes,
				"a disabled slider changed");
		});

		runner.test("numeric input clamps and snaps typed values only when asked", [] {
			auto options = IntegerControl(dmui::NumericPresentation::kInput);
			{
				NumericHarness harness{ options.Abi() };
				harness.Type("500");
				require(harness.value.i == 100, "a typed value above the maximum was not clamped");
			}
			{
				NumericHarness harness{ options.Abi() };
				harness.Type("-5");
				require(harness.value.i == 0, "a typed value below the minimum was not clamped");
			}
			{
				NumericHarness harness{ options.Abi() };
				harness.Type("47");
				require(harness.value.i == 50, "a typed value was not snapped to the step");
			}
			options.clamp = false;
			options.step.reset();
			{
				NumericHarness harness{ options.Abi() };
				harness.Type("500");
				require(harness.value.i == 500, "a typed value was clamped without the clamp flag");
			}
			{
				// Snapping back to the input is not an edit.
				options.step = 10;
				options.value = 40;
				NumericHarness harness{ options.Abi() };
				harness.Type("42");
				require(harness.changes == 0 && harness.value.i == 40,
					"an edit that snapped back to the input reported a change");
			}
		});

		runner.test("numeric floats snap from their origin and keep the unit out of the value", [] {
			const auto descriptor = dmui::NumericOptions<float>{
				.id = "float",
				.presentation = dmui::NumericPresentation::kInput,
				.value = 0.5f,
				.minimum = 0.0f,
				.maximum = 2.0f,
				.step = 0.25f,
				.stepOrigin = 0.125f,
				.format = "%.3f",
				.unit = "x",
				.clamp = true,
				.width = 200.0f
			}.Abi();
			NumericHarness harness{ descriptor };
			harness.Type("0.8");
			require(harness.value.f == 0.875,
				"a float was not snapped to origin plus a multiple of the step");
		});

		runner.test("numeric values keep full width across every data type", [] {
			const auto keeps = [](DMUI_NumericType a_type, DMUI_NumericValue a_value) {
				DMUI_NumericDescriptor descriptor{};
				descriptor.id = "wide";
				descriptor.type = a_type;
				descriptor.presentation = DMUI_NUMERIC_PRESENTATION_INPUT;
				descriptor.value = a_value;
				descriptor.width = 200.0f;
				require(ValidateNumeric(&descriptor) == DMUI_RESULT_OK,
					"a representable value was rejected");
				NumericHarness harness{ descriptor };
				harness.Frame();
				return harness.changes == 0 &&
					std::memcmp(&harness.value, &a_value, sizeof(a_value)) == 0;
			};
			require(keeps(DMUI_NUMERIC_TYPE_S8, dmui::ToNumericValue<int64_t>(-128)) &&
					keeps(DMUI_NUMERIC_TYPE_U8, dmui::ToNumericValue<uint64_t>(255)) &&
					keeps(DMUI_NUMERIC_TYPE_S16, dmui::ToNumericValue<int64_t>(-32768)) &&
					keeps(DMUI_NUMERIC_TYPE_U16, dmui::ToNumericValue<uint64_t>(65535)) &&
					keeps(DMUI_NUMERIC_TYPE_S32, dmui::ToNumericValue<int64_t>(INT32_MIN)) &&
					keeps(DMUI_NUMERIC_TYPE_U32, dmui::ToNumericValue<uint64_t>(UINT32_MAX)) &&
					keeps(DMUI_NUMERIC_TYPE_S64, dmui::ToNumericValue<int64_t>(INT64_MIN)) &&
					keeps(DMUI_NUMERIC_TYPE_U64, dmui::ToNumericValue<uint64_t>(UINT64_MAX)) &&
					keeps(DMUI_NUMERIC_TYPE_FLOAT, dmui::ToNumericValue<double>(0.1f)) &&
					keeps(DMUI_NUMERIC_TYPE_DOUBLE, dmui::ToNumericValue<double>(0.1)),
				"a numeric value lost width or precision through the native widget");
		});

		runner.test("numeric formats are rebuilt for the argument width", [] {
			const auto build = [](const char* a_format, const char* a_unit, DMUI_NumericType a_type) {
				return BuildNumericFormat(a_format, a_unit, a_type).value_or("<rejected>");
			};
			require(build("%.0f", "%", DMUI_NUMERIC_TYPE_FLOAT) == "%.0f%%" &&
					build(nullptr, nullptr, DMUI_NUMERIC_TYPE_DOUBLE) == "%.3f" &&
					build("%5.2f", "x", DMUI_NUMERIC_TYPE_FLOAT) == "%5.2fx",
				"float formats were not preserved with an escaped unit");
			require(build("%lld", nullptr, DMUI_NUMERIC_TYPE_S32) == "%d" &&
					build("%d", nullptr, DMUI_NUMERIC_TYPE_S64) == "%lld" &&
					build("%llu", "ms", DMUI_NUMERIC_TYPE_U64) == "%llums" &&
					build("%hhu", nullptr, DMUI_NUMERIC_TYPE_U8) == "%u" &&
					build("0x%X", nullptr, DMUI_NUMERIC_TYPE_U32) == "0x%X" &&
					build("100%% %d", nullptr, DMUI_NUMERIC_TYPE_S16) == "100%% %d",
				"integer length modifiers or literals were not rebuilt");
			require(build("%d", nullptr, DMUI_NUMERIC_TYPE_FLOAT) == "<rejected>" &&
					build("%f", nullptr, DMUI_NUMERIC_TYPE_U32) == "<rejected>" &&
					build("%s", nullptr, DMUI_NUMERIC_TYPE_S32) == "<rejected>" &&
					build("%d%d", nullptr, DMUI_NUMERIC_TYPE_S32) == "<rejected>" &&
					build("%1234d", nullptr, DMUI_NUMERIC_TYPE_S32) == "<rejected>" &&
					build("%", nullptr, DMUI_NUMERIC_TYPE_S32) == "<rejected>",
				"a format that does not match the type was rebuilt");
		});

		runner.test("color edit round-trips RGBA and applies a swatch without disturbing alpha", [] {
			for (uint32_t byte = 0; byte <= 255; ++byte)
			{
				const auto packed = byte << 24u | ((byte * 7u) & 0xFFu) << 16u |
					(255u - byte) << 8u | ((byte * 13u) & 0xFFu);
				require(PackRgba(UnpackRgba(packed)) == packed,
					"a packed color changed through the float round trip");
			}

			const std::array presets{
				dmui::ColorPreset{ "First", 0x112233FFu, "first" },
				dmui::ColorPreset{ "Second", 0x445566FFu, "second" }
			};
			support::ImGuiTestContext imgui{
				{ .disableInputTrickle = true, .disableErrorRecovery = true }
			};
			uint32_t rgba{ 0xAABBCC80u };
			auto alpha = false;
			auto changes = 0;
			auto completed = 0;
			ImVec2 lastSwatch;
			const auto frame = [&] {
				BeginFieldFrame(imgui, "##ColorFrame");
				const auto result = DrawColorEdit(dmui::ColorEditOptions{
					.id = "color",
					.rgba = rgba,
					.alpha = alpha,
					.presets = presets,
					.presetsLabel = "Presets",
					.width = 300.0f
				}.Abi());
				// The last item is the final swatch.
				const auto min = ImGui::GetItemRectMin();
				const auto max = ImGui::GetItemRectMax();
				lastSwatch = { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f };
				if (result.changed != 0)
				{
					rgba = result.rgba;
					++changes;
				}
				completed += result.completed != 0 ? 1 : 0;
				imgui.EndWindow(true);
			};
			frame();
			frame();
			require(changes == 0 && rgba == 0xAABBCC80u,
				"an untouched color edit reported a change");
			ClickAt(lastSwatch, frame);
			require(changes == 1 && completed == 1 && rgba == 0x44556680u,
				"a swatch did not replace the color while keeping its alpha");
			rgba = 0xAABBCC80u;
			alpha = true;
			frame();
			ClickAt(lastSwatch, frame);
			require(rgba == 0x445566FFu,
				"a swatch did not apply its own alpha when alpha is edited");
		});

		runner.test("color edit descriptors are validated", [] {
			const std::array presets{ dmui::ColorPreset{ "One", 0x010203FFu } };
			const auto ok = dmui::ColorEditOptions{
				.id = "color",
				.presets = presets,
				.presetsLabel = "Presets"
			}.Abi();
			require(ValidateColorEdit(&ok) == DMUI_RESULT_OK,
				"a valid color edit was rejected");
			const auto rejects = [&](auto a_change) {
				auto candidate = ok;
				a_change(candidate);
				return ValidateColorEdit(&candidate) == DMUI_RESULT_INVALID_ARGUMENT;
			};
			DMUI_ColorPreset unnamed{ nullptr, nullptr, 0u, 0u };
			require(ValidateColorEdit(nullptr) == DMUI_RESULT_INVALID_ARGUMENT &&
					rejects([](auto& a_value) { a_value.id = nullptr; }) &&
					rejects([](auto& a_value) { a_value.id = ""; }) &&
					rejects([](auto& a_value) { a_value.reserved = 1u; }) &&
					rejects([](auto& a_value) { a_value.flags = 0x80u; }) &&
					rejects([](auto& a_value) { a_value.width = -1.0f; }) &&
					rejects([](auto& a_value) { a_value.presets = nullptr; }) &&
					rejects([&](auto& a_value) { a_value.presets = &unnamed; }) &&
					rejects([](auto& a_value) { a_value.presetCount = kMaxColorPresets + 1u; }),
				"a malformed color edit descriptor was accepted");
		});

		runner.test("key capture binds the next chord, clears, and honors read-only", [] {
			Hotkeys::ReleaseActiveKeys();
			Hotkeys::SetContext({ HostInputMode::kShell });
			support::ImGuiTestContext imgui{
				{ .disableInputTrickle = true, .disableErrorRecovery = true }
			};
			KeyCaptureSpec spec{
				.id = "capture",
				.binding = "Ctrl+F5",
				.width = 300.0f
			};
			std::optional<HotkeyChord> result;
			const auto frame = [&] {
				BeginFieldFrame(imgui, "##KeyFrame");
				if (const auto captured = DrawKeyCapture(spec))
					result = captured;
				imgui.EndWindow(true);
			};
			frame();
			const auto half = ImGui::GetFrameHeight() * 0.5f;
			const ImVec2 field{ 60.0f, 20.0f + half };
			const ImVec2 unbind{ 20.0f + spec.width - half, 20.0f + half };

			ClickAt(field, frame);
			require(Hotkeys::IsCapturing(), "pressing the field did not start a capture");
			(void)Hotkeys::HandleKey(0x40, kHotkeyModifierAlt, true, false);
			(void)Hotkeys::HandleKey(0x40, 0, false, false);
			frame();
			require(result && SerializeHotkeyChord(*result) == "Alt+F6" &&
					!Hotkeys::IsCapturing(),
				"a released chord was not reported or ended the capture");

			result.reset();
			ClickAt(unbind, frame);
			require(result && result->IsNone() && !Hotkeys::IsCapturing(),
				"the unbind button did not report a cleared binding");

			spec.allowClear = false;
			result.reset();
			ClickAt(unbind, frame);
			require(!result && !Hotkeys::IsCapturing(),
				"a field without a clear button still cleared");

			spec.readOnly = true;
			ClickAt(field, frame);
			require(!Hotkeys::IsCapturing(), "a read-only field started a capture");
			Hotkeys::ReleaseActiveKeys();
			Hotkeys::SetContext({});
		});

		runner.test("key capture text reflects binding, warning, display text, and capture state", [] {
			KeyCaptureSpec spec;
			const std::string notSet = HotkeyNotSetText();
			require(KeyCaptureText(spec, false) == notSet,
				"an unbound field did not read as not set");
			spec.binding = "none";
			require(KeyCaptureText(spec, false) == notSet, "none did not read as unbound");
			spec.warning = "Taken by another action";
			require(KeyCaptureText(spec, false) ==
					Localization::Text("$DMUI_Controls_CheckBinding", "Check binding"),
				"an unbound field with a conflict did not ask to check the binding");
			spec.binding = "Ctrl+F5";
			require(KeyCaptureText(spec, false) == FormatHotkeyChord("Ctrl+F5"),
				"a bound field did not show its formatted chord despite a warning");
			spec.displayText = "Legacy label";
			require(KeyCaptureText(spec, false) == "Legacy label",
				"display text did not replace the formatted binding");
			require(KeyCaptureText(spec, true) !=
					KeyCaptureText({ .slot = HotkeySlot::kGamepad }, true) &&
					KeyCaptureText(spec, true) != "Legacy label",
				"capturing did not prompt per slot");
		});

		runner.test("key capture descriptors are validated and mapped", [] {
			const auto ok = dmui::KeyCaptureOptions{
				.id = "capture",
				.binding = "F5",
				.slot = dmui::KeyCaptureSlot::kGamepad,
				.readOnly = true,
				.width = 120.0f
			}.Abi();
			const auto spec = MakeKeyCaptureSpec(ok);
			require(ValidateKeyCapture(&ok) == DMUI_RESULT_OK &&
					spec.slot == HotkeySlot::kGamepad && spec.allowClear &&
					spec.readOnly && spec.width == 120.0f,
				"a valid key capture descriptor was rejected or mis-mapped");
			const auto rejects = [&](auto a_change) {
				auto candidate = ok;
				a_change(candidate);
				return ValidateKeyCapture(&candidate) == DMUI_RESULT_INVALID_ARGUMENT;
			};
			require(ValidateKeyCapture(nullptr) == DMUI_RESULT_INVALID_ARGUMENT &&
					rejects([](auto& a_value) { a_value.id = nullptr; }) &&
					rejects([](auto& a_value) { a_value.id = ""; }) &&
					rejects([](auto& a_value) { a_value.reserved = 1u; }) &&
					rejects([](auto& a_value) { a_value.flags = 0x80u; }) &&
					rejects([](auto& a_value) { a_value.slot = 2u; }) &&
					rejects([](auto& a_value) { a_value.width = -1.0f; }),
				"a malformed key capture descriptor was accepted");
		});

		runner.test("text field rejects malformed options before drawing", [] {
			support::ImGuiTestContext imgui{
				{ .disableErrorRecovery = true }
			};
			BeginFieldFrame(imgui, "##FieldValidation");
			std::vector<char> storage(16);
			DMUI_TextBuffer buffer{ storage.data(), storage.size(), nullptr, nullptr };
			DMUI_TextEditState state{};
			const auto draw = [&](const char* a_id, const TextFieldSpec& a_spec) {
				return DrawTextField(a_id, buffer, a_spec, state);
			};
			require(draw("ok", {}) == DMUI_RESULT_OK,
				"a default single-line field was rejected");
			require(draw(nullptr, {}) == DMUI_RESULT_INVALID_ARGUMENT &&
					draw("", {}) == DMUI_RESULT_INVALID_ARGUMENT,
				"a field without an id was accepted");
			require(draw("search", { .style = DMUI_TEXT_FIELD_STYLE_SEARCH, .lines = 2 }) ==
						DMUI_RESULT_INVALID_ARGUMENT &&
					draw("history", {
						.editFlags = DMUI_UI_TEXT_EDIT_FLAGS_HISTORY_KEYS,
						.lines = 2 }) == DMUI_RESULT_INVALID_ARGUMENT,
				"a multiline search or history field was accepted");
			require(draw("height", { .size = { 0.0f, -1.0f } }) ==
						DMUI_RESULT_INVALID_ARGUMENT &&
					draw("limit", { .maxBytes = static_cast<uint32_t>(INT_MAX) }) ==
						DMUI_RESULT_INVALID_ARGUMENT &&
					draw("style", { .style = 7u }) == DMUI_RESULT_INVALID_ARGUMENT,
				"an out-of-range height, limit, or style was accepted");
			imgui.EndWindow();
		});

		runner.test("growable text field accepts a large UTF8 paste across borrowed frames", [] {
			std::string clipboard(4094, 'a');
			clipboard += "\xC3\xA9";
			support::ImGuiTestContext imgui{
				{
					.disableInputTrickle = true,
					.disableErrorRecovery = true
				}
			};
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameDraw
			};
			RenderExecution::ClientGuard client{
				kResizeTestClient,
				true
			};
			SetTestClipboard(clipboard);
			std::string query;
			const auto drawFrame = [&] {
				BeginFieldFrame(imgui, "##GrowableStringField");
				bool edited{};
				const auto result = DrawTextField(
					"growable",
					query,
					{ .hint = "Search...", .style = DMUI_TEXT_FIELD_STYLE_SEARCH },
					edited);
				imgui.EndWindow(true);
				return result;
			};

			require(drawFrame() == DMUI_RESULT_OK,
				"growable field failed before activation");
			auto& io = ImGui::GetIO();
			io.AddMousePosEvent(80.0f, 32.0f);
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
			require(drawFrame() == DMUI_RESULT_OK,
				"growable field failed during activation");
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
			require(
				drawFrame() == DMUI_RESULT_OK && query.empty() &&
					ImGui::GetCurrentContext()->ActiveId != 0 &&
					ImGui::GetCurrentContext()->InputTextState.ID ==
						ImGui::GetCurrentContext()->ActiveId,
				"growable field did not retain focus after mouse release");
			io.AddKeyEvent(ImGuiMod_Ctrl, true);
			io.AddKeyEvent(ImGuiKey_V, true);
			require(drawFrame() == DMUI_RESULT_OK && query == clipboard,
				"single large UTF8 paste did not grow the buffer");

			io.AddKeyEvent(ImGuiKey_V, false);
			io.AddKeyEvent(ImGuiMod_Ctrl, false);
			require(drawFrame() == DMUI_RESULT_OK && query == clipboard,
				"recreated borrowed storage lost the active edit");

			RejectingTextBuffer deactivatedOwner{ "safe" };
			auto deactivatedBuffer = deactivatedOwner.Descriptor();
			bool changed{};
			BeginFieldFrame(imgui, "##GrowableStringField");
			ImGui::SetActiveID(
				ImGui::GetID("##OtherInput"),
				ImGui::GetCurrentWindow());
			DMUI_TextEditState deactivatedState{};
			const auto deactivatedResult = DrawTextField(
				"growable",
				deactivatedBuffer,
				{ .hint = "Search...", .style = DMUI_TEXT_FIELD_STYLE_SEARCH },
				deactivatedState);
			changed = Edited(deactivatedState);
			imgui.EndWindow(true);
			require(
				deactivatedResult == DMUI_RESULT_BUFFER_TOO_SMALL &&
					!changed &&
					std::string_view{ deactivatedBuffer.data } == "safe" &&
					deactivatedOwner.resizeCalls == 1 &&
					deactivatedOwner.renderExecutionActive &&
					!deactivatedOwner.drawingClientVisible,
				"deactivated writeback hid failure or changed replacement storage");

			RejectingTextBuffer rejectingOwner{ "abcdef" };
			auto rejecting = rejectingOwner.Descriptor();
			require(
				DrawBufferFrame(imgui, rejecting, changed) == DMUI_RESULT_OK,
				"rejecting field failed before activation");
			io.AddMousePosEvent(80.0f, 32.0f);
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
			require(
				DrawBufferFrame(imgui, rejecting, changed) == DMUI_RESULT_OK,
				"rejecting field failed during activation");
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
			require(
				DrawBufferFrame(imgui, rejecting, changed) == DMUI_RESULT_OK,
				"rejecting field lost focus after mouse release");
			io.AddKeyEvent(ImGuiMod_Ctrl, true);
			io.AddKeyEvent(ImGuiKey_V, true);
			require(
				DrawBufferFrame(imgui, rejecting, changed) ==
						DMUI_RESULT_BUFFER_TOO_SMALL &&
					!changed &&
					std::string_view{ rejecting.data } == "abcdef" &&
					rejectingOwner.resizeCalls == 1 &&
					rejectingOwner.renderExecutionActive &&
					!rejectingOwner.drawingClientVisible,
				"failed resize truncated text or hid its explicit result");
			io.AddKeyEvent(ImGuiKey_V, false);
			io.AddKeyEvent(ImGuiMod_Ctrl, false);
			require(
				DrawBufferFrame(imgui, rejecting, changed) == DMUI_RESULT_OK &&
					!changed && rejectingOwner.resizeCalls == 1,
				"rejected paste was replayed on the following frame");
		});

		runner.test("text field limits input by capacity or maxBytes on UTF8 boundaries", [] {
			for (const auto growable : { false, true })
			{
				const auto at256 = EditField(std::string(256, 'a'), 512, growable, "b");
				require(
					at256.second && at256.first.size() == 257 &&
						at256.first.find('b') != std::string::npos,
					"256-byte text was truncated below the limit");

				const auto completeUtf8 =
					EditField(std::string(510, 'a'), 512, growable, "\xC3\xA9");
				require(
					completeUtf8.second &&
						completeUtf8.first.size() == 512 &&
						completeUtf8.first.find("\xC3\xA9") != std::string::npos,
					"complete multibyte input was not preserved at the boundary");

				const auto rejectedUtf8 =
					EditField(std::string(511, 'a'), 512, growable, "\xC3\xA9");
				require(
					!rejectedUtf8.second &&
						rejectedUtf8.first == std::string(511, 'a'),
					"partial multibyte input was accepted or truncated");

				const auto partialInput =
					EditField(std::string(511, 'a'), 512, growable, "b\xC3\xA9");
				require(
					partialInput.second && partialInput.first.size() == 512 &&
						std::ranges::count(partialInput.first, 'a') == 511 &&
						partialInput.first.find('b') != std::string::npos,
					"bounded input lost existing text or split the final UTF8 sequence");
			}

			const auto longer = EditField(std::string(600, 'a'), 512, true, "b");
			require(
				!longer.second && longer.first == std::string(600, 'a'),
				"a limit truncated or extended text that already exceeded it");
		});

		runner.test("multiline text fields accept newlines and single lines do not", [] {
			const auto multiline = TypeInto({ .lines = 3 }, "a\nb");
			const auto single = TypeInto({}, "a\nb");
			require(multiline == "a\nb",
				"a multiline field dropped a typed newline");
			require(single == "ab",
				"a single-line field accepted a newline");
		});
	}
}
