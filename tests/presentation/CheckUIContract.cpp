#include <DearModdingUI/controls/TextSpans.h>
#include <DearModdingUI/host/UIAdapter.h>
#include <DearModdingUI/host/ImGuiRecovery.h>
#include <DearModdingUI/host/UILayoutState.h>
#include <DearModdingUI/presentation/Theme.h>
#include <DearModdingUI/UIBindings.generated.h>
#include "../Harness.h"
#include "../support/ImGuiTestContext.h"

#include <DearModdingUI/UI.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace vmm_tests
{
	namespace
	{
		[[nodiscard]] DMUI_Result AcceptClient(
			DMUI_ClientHandle) noexcept
		{
			return DMUI_RESULT_OK;
		}

		class ImGuiFrame
		{
		public:
			ImGuiFrame()
			{
				m_imgui.BeginWindow("##StableUIContractTest", { 20.0f, 20.0f }, { 640.0f, 480.0f });
			}

			~ImGuiFrame()
			{
				m_imgui.EndWindow();
			}

		private:
			support::ImGuiTestContext m_imgui;
		};

		std::string s_capturedText;

		DMUI_Result DMUI_CALL CaptureText(
			DMUI_ClientHandle,
			const char* a_text,
			size_t a_length,
			const DMUI_TextSpan*,
			uint32_t,
			const DMUI_TextOptions*) noexcept
		{
			try
			{
				s_capturedText.assign(a_text, a_length);
				return DMUI_RESULT_OK;
			}
			catch (...)
			{
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			}
		}
	}

	void run_ui_contract_checks(Runner& runner)
	{
		runner.test("host style variables share push pop ordering and callback recovery", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{ &AcceptClient };
			const auto& api = DearModdingUI::UI::API();
			DMUI_StyleMetrics before{}, scoped{}, after{};
			(void)api.getStyleMetrics(1u, &before);
			const auto nativeDepth = GImGui->StyleVarStack.Size;
			auto recovery = DearModdingUI::ImGuiRecoverySnapshot::Capture();
			require(api.pushStyleVarFloat(1u, DMUI_UI_STYLE_VAR_SECTION_GAP, 31.0f) == DMUI_RESULT_OK &&
					api.pushStyleVarFloat(1u, DMUI_UI_STYLE_VAR_ALPHA, 0.4f) == DMUI_RESULT_OK &&
					api.pushStyleVarVec2(1u, DMUI_UI_STYLE_VAR_PANEL_PADDING, { 18.0f, 14.0f }) == DMUI_RESULT_OK,
				"mixed host and native styles could not be pushed");
			(void)api.getStyleMetrics(1u, &scoped);
			require(scoped.sectionGap == 31.0f && scoped.alpha == 0.4f &&
					scoped.panelPadding.x == 18.0f && scoped.panelPadding.y == 14.0f,
				"scoped metrics did not expose the active overrides");
			require(api.popStyleVar(1u, 3) == DMUI_RESULT_OK, "mixed style pop failed");
			(void)api.getStyleMetrics(1u, &after);
			require(after.sectionGap == before.sectionGap && after.alpha == before.alpha &&
					after.panelPadding.x == before.panelPadding.x && after.panelPadding.y == before.panelPadding.y,
				"mixed style pop did not restore the theme");
			require(api.pushStyleVarVec2(1u, DMUI_UI_STYLE_VAR_SECTION_GAP, { 1, 1 }) ==
					DMUI_RESULT_INVALID_ARGUMENT &&
					api.pushStyleVarFloat(1u, DMUI_UI_STYLE_VAR_PANEL_PADDING, 1) == DMUI_RESULT_INVALID_ARGUMENT &&
					api.popStyleVar(1u, 1) == DMUI_RESULT_INVALID_ARGUMENT,
				"wrong shapes or callback-boundary underflow were accepted");
			require(api.pushStyleVarVec2(1u, DMUI_UI_STYLE_VAR_ALPHA, { 0.5f, 0.5f }) ==
					DMUI_RESULT_INVALID_ARGUMENT &&
					api.pushStyleVarFloat(1u, DMUI_UI_STYLE_VAR_FRAME_PADDING, 2.0f) ==
					DMUI_RESULT_INVALID_ARGUMENT &&
					GImGui->StyleVarStack.Size == nativeDepth,
				"mismatched native style-var shape reached ImGui");
			(void)api.pushStyleVarFloat(1u, DMUI_UI_STYLE_VAR_SECTION_GAP, 41.0f);
			(void)api.pushStyleVarVec2(1u, DMUI_UI_STYLE_VAR_PANEL_PADDING, { 2, 3 });
			(void)api.pushStyleVarFloat(1u, DMUI_UI_STYLE_VAR_ALPHA, 0.2f);
			require(recovery->RecoverFailure().Repaired(), "abandoned styles were not reported");
			(void)api.getStyleMetrics(1u, &after);
			require(after.sectionGap == before.sectionGap && after.alpha == before.alpha &&
					after.panelPadding.x == before.panelPadding.x && after.panelPadding.y == before.panelPadding.y &&
					GImGui->StyleVarStack.Size == nativeDepth &&
					DearModdingUI::UI::GetLayoutDepths() == DearModdingUI::UI::LayoutDepths{},
				"failed callback leaked native or host style state");
		});

		runner.test("frame time follows the ImGui clock and spinners draw one validated item", [] {
			support::ImGuiTestContext imgui;
			const DearModdingUI::UI::Testing::ValidationOverride validation{ &AcceptClient };
			const auto& api = DearModdingUI::UI::API();
			double previous{ -1.0 };
			for (const auto delta : { 0.016f, 0.25f, 0.1f })
			{
				ImGui::GetIO().DeltaTime = delta;
				imgui.BeginWindow("##FrameTime", { 20.0f, 20.0f }, { 300.0f, 200.0f });
				double time{};
				float elapsed{};
				require(api.getTime(1u, &time) == DMUI_RESULT_OK &&
						api.getDeltaTime(1u, &elapsed) == DMUI_RESULT_OK,
					"frame time was not reported");
				require(elapsed == delta && time == ImGui::GetTime() && time > previous &&
						(previous < 0.0 || std::abs((time - previous) - delta) < 1e-6),
					"the clock did not advance by the frame delta");
				previous = time;
				if (delta == 0.1f)
				{
					DMUI_SpinnerDescriptor spinner{ "busy", 0.0f, 0u };
					const auto vertices = ImGui::GetWindowDrawList()->VtxBuffer.Size;
					const auto cursor = ImGui::GetCursorScreenPos().y;
					require(api.spinner(1u, &spinner) == DMUI_RESULT_OK &&
							ImGui::GetWindowDrawList()->VtxBuffer.Size > vertices &&
							ImGui::GetItemRectSize().x == ImGui::GetFrameHeight() &&
							ImGui::GetItemRectSize().y == ImGui::GetFrameHeight() &&
							ImGui::GetCursorScreenPos().y ==
								cursor + ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y,
						"the spinner was not one frame-sized drawn item");
					spinner.size = 40.0f;
					require(api.spinner(1u, &spinner) == DMUI_RESULT_OK &&
							ImGui::GetItemRectSize().x == 40.0f,
						"an explicit spinner size was ignored");
					const auto after = ImGui::GetCursorScreenPos().y;
					const DMUI_SpinnerDescriptor noId{ nullptr, 0.0f, 0u }, emptyId{ "", 0.0f, 0u },
						negative{ "busy", -1.0f, 0u }, infinite{ "busy", INFINITY, 0u },
						reserved{ "busy", 0.0f, 1u };
					for (const auto* bad : { &noId, &emptyId, &negative, &infinite, &reserved })
						require(api.spinner(1u, bad) == DMUI_RESULT_INVALID_ARGUMENT,
							"a malformed spinner descriptor was accepted");
					require(api.spinner(1u, nullptr) == DMUI_RESULT_INVALID_ARGUMENT &&
							api.getTime(1u, nullptr) == DMUI_RESULT_INVALID_ARGUMENT &&
							api.getDeltaTime(1u, nullptr) == DMUI_RESULT_INVALID_ARGUMENT &&
							ImGui::GetCursorScreenPos().y == after,
						"null arguments were accepted or drew an item");
				}
				imgui.EndWindow();
			}
		});

		runner.test("panels isolate draw lists and recover abandoned child brackets", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{ &AcceptClient };
			const auto& api = DearModdingUI::UI::API();
			auto* parent = ImGui::GetCurrentWindow();
			auto* parentDraw = ImGui::GetWindowDrawList();
			const auto nativeDepth = GImGui->StyleVarStack.Size;
			auto recovery = DearModdingUI::ImGuiRecoverySnapshot::Capture();
			require(api.endPanel(1u) == DMUI_RESULT_INVALID_ARGUMENT, "unmatched panel end was accepted");
			uint32_t visible{};
			require(api.beginPanel(1u, "Outer", { 240, 180 }, 0, &visible) == DMUI_RESULT_OK && visible,
				"panel did not open");
			auto* child = ImGui::GetCurrentWindow();
			require(child != parent && child->DrawList != parentDraw &&
					(child->Flags & (ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) ==
						(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse),
				"panel did not establish its own non-scrolling draw window");
			const auto clip = child->DrawList->_ClipRectStack.back();
			require(clip.x >= child->Pos.x && clip.y >= child->Pos.y &&
					clip.z <= child->Pos.x + child->Size.x && clip.w <= child->Pos.y + child->Size.y,
				"panel draw list was not clipped to its window");
			{
				const DearModdingUI::UI::DrawListClipScope clips;
				require(api.drawListPushClipRect(1u, DMUI_DRAW_TARGET_WINDOW,
						{ clip.x, clip.y }, { clip.z, clip.w }, 1) == DMUI_RESULT_OK &&
						api.endPanel(1u) == DMUI_RESULT_INVALID_ARGUMENT,
					"panel end consumed an outstanding client clip");
			}
			(void)api.pushStyleVarFloat(1u, DMUI_UI_STYLE_VAR_SECTION_GAP, 2.0f);
			require(api.beginPanel(1u, "Nested", { 100, 70 }, DMUI_UI_PANEL_FLAGS_SCROLLABLE, &visible) ==
					DMUI_RESULT_OK && visible, "nested panel did not open");
			require((ImGui::GetCurrentWindow()->Flags & ImGuiWindowFlags_NoScrollWithMouse) == 0,
				"scrollable panel disabled wheel scrolling");
			(void)api.pushStyleVarFloat(1u, DMUI_UI_STYLE_VAR_ALPHA, 0.5f);
			require(api.endPanel(2u) == DMUI_RESULT_INVALID_ARGUMENT, "another client ended a panel");
			const auto repaired = recovery->RecoverFailure();
			require(repaired.Repaired() && ImGui::GetCurrentWindow() == parent &&
					GImGui->StyleVarStack.Size == nativeDepth &&
					DearModdingUI::UI::GetLayoutDepths() == DearModdingUI::UI::LayoutDepths{},
				"failed callback left a child window or host bracket open");
			require(api.beginPanel(1u, "NextCallback", { 100, 60 }, 0, &visible) == DMUI_RESULT_OK &&
					visible && api.endPanel(1u) == DMUI_RESULT_OK,
				"panel recovery poisoned the next callback");
		});

		runner.test("draw-list colors preserve RGBA packing and live style alpha", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{ &AcceptClient };
			dmui::ui::detail::ScopedContext context{ &DearModdingUI::UI::API(), 1u };
			require(dmui::ui::ColorConvertFloat4ToU32({ 1.0f, 0.5f, 0.25f, 0.0f }) == 0xFF804000u,
				"float colors used native byte order instead of RRGGBBAA");
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 0.8f, 0.2f, 0.4f, 0.6f });
			const auto verify = [](dmui::ui::Color32 a_color, ImU32 a_expected) {
				auto* draw = ImGui::GetWindowDrawList();
				const auto first = draw->VtxBuffer.Size;
				dmui::ui::WindowDrawList().AddRectFilled({ 50, 50 }, { 70, 70 }, a_color);
				require(draw->VtxBuffer.Size > first && draw->VtxBuffer[first].col == a_expected,
					"client color conversion changed channels or lost style alpha");
			};
			verify(dmui::ui::GetColorU32(dmui::ui::Color::kText, 0.5f),
				ImGui::GetColorU32(ImGuiCol_Text, 0.5f));
			const auto tokens = dmui::ui::GetThemeTokens();
			require(tokens.has_value(), "theme tokens were unavailable in a draw callback");
			const auto expected = DearModdingUI::Theme::Current().tokens;
			const auto same = [](const DMUI_Vec4& a_actual, const ImVec4& a_expected) {
				return a_actual.x == a_expected.x && a_actual.y == a_expected.y &&
					a_actual.z == a_expected.z && a_actual.w == a_expected.w;
			};
			require(same(tokens->text, expected.text) && same(tokens->muted, expected.muted) &&
					same(tokens->accent, expected.accent) &&
					same(tokens->accentMuted, expected.accentMuted) &&
					same(tokens->info, expected.info) && same(tokens->success, expected.success) &&
					same(tokens->warning, expected.warning) && same(tokens->error, expected.error) &&
					same(tokens->disabled, expected.disabled) &&
					same(tokens->restartNeeded, expected.restartNeeded) &&
					same(tokens->currentHotkey, expected.currentHotkey) &&
					same(tokens->rowAlt, expected.rowAlt),
				"theme tokens were mapped to the wrong host slots");
			const auto severityColor = [](DMUI_Severity a_severity) {
				const auto color = DearModdingUI::Theme::StatusTextColor(a_severity);
				return DMUI_Vec4{ color.x, color.y, color.z, color.w };
			};
			require(same(severityColor(DMUI_SEVERITY_INFO), expected.info) &&
					same(severityColor(DMUI_SEVERITY_SUCCESS), expected.success) &&
					same(severityColor(DMUI_SEVERITY_WARNING), expected.warning) &&
					same(severityColor(DMUI_SEVERITY_ERROR), expected.error),
				"severities did not resolve to their theme tokens");
			const auto accent = tokens->accent;
			verify(dmui::ui::GetColorU32(&DMUI_ThemeTokens::accent),
				ImGui::GetColorU32(ImVec4{ accent.x, accent.y, accent.z, accent.w }));
			ImGui::PopStyleColor();
			verify(dmui::ui::GetColorU32(dmui::ui::Color::kText), ImGui::GetColorU32(ImGuiCol_Text));
			ImGui::PopStyleVar();
			require(context.Result() == DMUI_RESULT_OK, "color queries failed in a draw callback");

			auto* list = ImGui::GetWindowDrawList();
			const auto start = list->VtxBuffer.Size;
			const auto pos = dmui::ui::GetCursorScreenPos();
			const auto draw = dmui::ui::WindowDrawList();
			draw.AddRectFilled(pos, { pos.x + 80, pos.y + 35 }, 0x123456FF);
			const auto widgetStart = list->VtxBuffer.Size;
			(void)dmui::ui::Button({ .label = "Widget", .size = { 80, 35 } });
			const auto widgetEnd = list->VtxBuffer.Size;
			draw.AddRectFilled(pos, { pos.x + 10, pos.y + 10 }, 0xABCDEF80);
			require(widgetStart > start && widgetEnd > widgetStart && list->VtxBuffer.Size > widgetEnd &&
				list->VtxBuffer[start].col == IM_COL32(0x12, 0x34, 0x56, 255) &&
				list->VtxBuffer[widgetEnd].col == IM_COL32(0xAB, 0xCD, 0xEF, 128),
				"geometry was buffered/reordered or packed color changed");
		});

		runner.test("stable list clippers clip rows and unwind callback-owned nesting", [] {
			using namespace DearModdingUI;
			const UI::Testing::ValidationOverride validation{ &AcceptClient };
			support::ImGuiTestContext imgui{ { .disableErrorRecovery = true } };
			int errors{};
			imgui.Get()->ErrorCallback =
				[](ImGuiContext*, void* a_data, const char*) {
					++*static_cast<int*>(a_data);
				};
			imgui.Get()->ErrorCallbackUserData = &errors;
			imgui.BeginWindow(
				"##ListClipperTest", { 20.0f, 20.0f }, { 640.0f, 480.0f });
			const auto baseline = GImGui->ClipperTempDataStacked;
			const auto& api = UI::API();
			{
				const UI::ListClipperScope callbackScope;
				uint64_t token{};
				require(api.listClipperBegin(1u, 1000, -1.0f, &token) == DMUI_RESULT_OK,
					"stable API could not begin a measured-height clipper");
				int32_t submitted{};
				for (;;)
				{
					uint32_t stepping{};
					int32_t start{}, end{};
					require(api.listClipperStep(1u, token, &stepping, &start, &end) ==
							DMUI_RESULT_OK,
						"stable clipper failed to step");
					if (!stepping)
						break;
					require(start >= 0 && start < end && end <= 1000,
						"stable clipper returned an invalid half-open range");
					submitted += end - start;
					for (auto i = start; i < end; ++i)
						ImGui::Text("%d: row", i);
				}
				require(submitted > 1 && submitted < 1000,
					"measured clipper did not cull offscreen rows");
				require(GImGui->ClipperTempDataStacked == baseline &&
						api.listClipperEnd(1u, token) == DMUI_RESULT_OK,
					"completed Step did not release the clipper");
			}

			ImGui::SetCursorPosY(30.0f);
			ImVec2 abandonedCursor{};
			const auto callback = [&] {
				const UI::ListClipperScope callbackScope;
				uint64_t outer{}, inner{};
				const auto height = ImGui::GetTextLineHeightWithSpacing();
				require(api.listClipperBegin(1u, 1000, height, &outer) == DMUI_RESULT_OK &&
						api.listClipperBegin(1u, 1000, height, &inner) == DMUI_RESULT_OK,
					"nested stable clippers could not begin");
				require(api.listClipperEnd(1u, outer) == DMUI_RESULT_UNBALANCED_BRACKET,
					"out-of-order End accepted an outer clipper");
				uint32_t stepping{};
				int32_t start{}, end{};
				require(api.listClipperStep(1u, inner, &stepping, &start, &end) ==
							DMUI_RESULT_OK && stepping,
					"out-of-order End corrupted the inner clipper");
				ImGui::BeginChild("##AbandonedClipperWindow", { 300.0f, 150.0f });
				abandonedCursor = ImGui::GetCursorScreenPos();
				require(api.listClipperEnd(1u, inner) == DMUI_RESULT_UNBALANCED_BRACKET,
					"End accepted a clipper in another window");
			};
			callback();
			const auto cursor = ImGui::GetCursorScreenPos();
			const bool cleaned = GImGui->ClipperTempDataStacked == baseline;
			ImGui::EndChild();
			imgui.EndWindow(true);
			require(cleaned, "callback scope retained abandoned ImGui clipper data");
			require(cursor.x == abandonedCursor.x && cursor.y == abandonedCursor.y,
				"callback cleanup sought the cursor in the client's remaining window");
			require(errors == 0, "clipper callback isolation left ImGui frame errors");
		});

		runner.test("stable UI rejects unknown and mutually exclusive flags", [] {
			ImGuiCol color{};
			require(
				DearModdingUI::UI::Bindings::TranslateColor(
					DMUI_UI_COLOR_TEXT, color) == DMUI_RESULT_OK &&
					color == ImGuiCol_Text &&
					DearModdingUI::UI::Bindings::TranslateColor(
						UINT32_C(999999), color) == DMUI_RESULT_INVALID_ARGUMENT,
				"stable colors did not translate by name or accepted an unknown value");
			ImGuiHoveredFlags hovered{};
			require(
				DearModdingUI::UI::Bindings::TranslateHoveredFlags(
					UINT32_C(0x80000000),
					hovered) == DMUI_RESULT_INVALID_ARGUMENT,
				"unknown stable flag bit reached native ImGui");
			ImGuiTableFlags table{};
			require(
				DearModdingUI::UI::Bindings::TranslateTableFlags(
					DMUI_UI_TABLE_FLAGS_SIZING_FIXED_FIT |
						DMUI_UI_TABLE_FLAGS_SIZING_STRETCH_PROP,
					table) == DMUI_RESULT_INVALID_ARGUMENT,
				"mutually exclusive stable flags reached native ImGui");
		});

		runner.test("input callback flags and reserved text fields are rejected", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{
				&AcceptClient
			};
			const auto& api = DearModdingUI::UI::API();
			char text[8]{};
			DMUI_TextBuffer buffer{ text, sizeof(text), nullptr, nullptr };
			DMUI_TextFieldOptions options{};
			options.flags = DMUI_UI_INPUT_TEXT_FLAGS_REJECTED_CALLBACK_MASK;
			DMUI_TextEditState state{};
			require(
				api.inputText(1u, "text", &buffer, &options, &state) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"unsupported native input callback flags were accepted");
			options = {};
			options.reserved = 1u;
			require(
				api.inputText(1u, "text", &buffer, &options, &state) ==
						DMUI_RESULT_INVALID_ARGUMENT &&
					api.inputText(1u, "text", &buffer, nullptr, &state) ==
						DMUI_RESULT_INVALID_ARGUMENT,
				"a reserved field or missing options were accepted");
		});

		runner.test("numeric descriptors are validated before drawing", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{
				&AcceptClient
			};
			const auto& api = DearModdingUI::UI::API();
			const auto ok = dmui::NumericOptions<int32_t>{
				.id = "numeric",
				.presentation = dmui::NumericPresentation::kSlider,
				.value = 5,
				.minimum = 0,
				.maximum = 10,
				.format = "%d",
				.unit = "ms"
			}.Abi();
			DMUI_NumericResult result{};
			require(api.numeric(1u, &ok, &result) == DMUI_RESULT_OK &&
					result.value.i == 5 && result.changed == 0u,
				"a valid numeric descriptor was rejected or edited");
			require(api.numeric(1u, nullptr, &result) == DMUI_RESULT_INVALID_ARGUMENT &&
					api.numeric(1u, &ok, nullptr) == DMUI_RESULT_INVALID_ARGUMENT,
				"a missing descriptor or result was accepted");

			const auto rejects = [&](const char* a_reason, auto a_change) {
				auto candidate = ok;
				a_change(candidate);
				require(api.numeric(1u, &candidate, &result) == DMUI_RESULT_INVALID_ARGUMENT,
					a_reason);
			};
			rejects("a missing id was accepted", [](auto& a_value) { a_value.id = nullptr; });
			rejects("an empty id was accepted", [](auto& a_value) { a_value.id = ""; });
			rejects("a reserved field was accepted", [](auto& a_value) { a_value.reserved = 1u; });
			rejects("an unknown flag was accepted", [](auto& a_value) { a_value.flags |= 0x80u; });
			rejects("an unknown type was accepted", [](auto& a_value) { a_value.type = 0u; });
			rejects("an unknown presentation was accepted",
				[](auto& a_value) { a_value.presentation = 3u; });
			rejects("a slider without a bound was accepted",
				[](auto& a_value) { a_value.flags &= ~DMUI_NUMERIC_HAS_MAXIMUM; });
			rejects("a value outside the type was accepted", [](auto& a_value) {
				a_value.type = DMUI_NUMERIC_TYPE_U8;
				a_value.value.u = 256u;
				a_value.minimum.u = 0u;
				a_value.maximum.u = 255u;
			});
			rejects("a non-positive step was accepted", [](auto& a_value) {
				a_value.flags |= DMUI_NUMERIC_HAS_STEP;
				a_value.step.i = 0;
			});
			rejects("a non-finite width was accepted", [](auto& a_value) {
				a_value.width = std::numeric_limits<float>::infinity();
			});
			rejects("a non-finite float value was accepted", [](auto& a_value) {
				a_value.type = DMUI_NUMERIC_TYPE_FLOAT;
				a_value.value.f = std::numeric_limits<double>::quiet_NaN();
				a_value.minimum.f = 0.0;
				a_value.maximum.f = 1.0;
			});
			for (const auto* format : { "%s", "%d %d", "%n", "no conversion", "%*d", "%f" })
			{
				rejects("a format that does not fit the type was accepted",
					[&](auto& a_value) { a_value.format = format; });
			}
		});

		runner.test("toggle, color edit, and key capture entries validate before drawing", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{
				&AcceptClient
			};
			const auto& api = DearModdingUI::UI::API();

			const DMUI_ToggleDescriptor toggle{ "toggle", nullptr, 1u, 0u };
			DMUI_ToggleResult toggled{};
			require(api.toggle(1u, &toggle, &toggled) == DMUI_RESULT_OK &&
					toggled.value == 1u && toggled.changed == 0u &&
					api.toggle(1u, nullptr, &toggled) == DMUI_RESULT_INVALID_ARGUMENT &&
					api.toggle(1u, &toggle, nullptr) == DMUI_RESULT_INVALID_ARGUMENT,
				"the toggle entry mishandled a valid or malformed call");

			DMUI_ColorEditDescriptor color{};
			color.id = "color";
			color.rgba = 0x11223344u;
			DMUI_ColorEditResult colored{};
			require(api.colorEdit(1u, &color, &colored) == DMUI_RESULT_OK &&
					colored.rgba == 0x11223344u && colored.changed == 0u,
				"an untouched color edit was rejected or changed");
			color.id = nullptr;
			require(api.colorEdit(1u, &color, &colored) == DMUI_RESULT_INVALID_ARGUMENT &&
					api.colorEdit(1u, nullptr, &colored) == DMUI_RESULT_INVALID_ARGUMENT,
				"the color edit entry accepted a malformed descriptor");

			DMUI_KeyCaptureDescriptor capture{};
			capture.id = "capture";
			capture.binding = "Ctrl+F5";
			capture.flags = DMUI_KEY_CAPTURE_READ_ONLY;
			DMUI_KeyCaptureResult captured{};
			require(api.keyCapture(1u, &capture, &captured) == DMUI_RESULT_OK &&
					captured.changed == 0u && captured.chord[0] == 0,
				"a read-only key capture was rejected or reported a chord");
			capture.id = nullptr;
			require(api.keyCapture(1u, &capture, &captured) == DMUI_RESULT_INVALID_ARGUMENT &&
					api.keyCapture(1u, nullptr, &captured) == DMUI_RESULT_INVALID_ARGUMENT,
				"the key capture entry accepted a malformed descriptor");
		});

		runner.test("section and choice descriptors are validated before drawing", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{
				&AcceptClient
			};
			const auto& api = DearModdingUI::UI::API();
			uint32_t visible{ 1u };
			DMUI_SectionDescriptor section{};
			section.id = "section";
			section.title = "Title";
			require(api.beginSection(1u, nullptr, &visible) == DMUI_RESULT_INVALID_ARGUMENT &&
					visible == 0u,
				"a missing section descriptor was accepted");
			auto emptyId = section;
			emptyId.id = "";
			auto reserved = section;
			reserved.reserved = 1u;
			auto unknownFlag = section;
			unknownFlag.flags = 0x80u;
			for (const auto* candidate : { &emptyId, &reserved, &unknownFlag })
			{
				require(api.beginSection(1u, candidate, &visible) == DMUI_RESULT_INVALID_ARGUMENT &&
						DearModdingUI::UI::GetLayoutDepths() == DearModdingUI::UI::LayoutDepths{},
					"a malformed section opened a bracket");
			}
			require(api.endSection(1u) == DMUI_RESULT_INVALID_ARGUMENT,
				"an unmatched section end was accepted");
			require(api.beginSection(1u, &section, &visible) == DMUI_RESULT_OK && visible == 1u &&
					api.endSection(2u) == DMUI_RESULT_INVALID_ARGUMENT &&
					api.endSection(1u) == DMUI_RESULT_OK &&
					DearModdingUI::UI::GetLayoutDepths() == DearModdingUI::UI::LayoutDepths{},
				"a section did not balance its own client's end");

			const std::array<DMUI_ChoiceOption, 2> options{ {
				{ "One", nullptr, 1u, 0u },
				{ "Two", "Second", 1u, 0u }
			} };
			DMUI_ChoiceDescriptor choice{};
			choice.id = "choice";
			choice.options = options.data();
			choice.optionCount = 2u;
			choice.selected = 5;
			DMUI_ChoiceResult result{};
			require(api.choice(1u, &choice, &result) == DMUI_RESULT_OK &&
					result.selected == 5 && result.changed == 0u,
				"an unmatched selection was altered or rejected");
			auto noOptions = choice;
			noOptions.options = nullptr;
			auto noId = choice;
			noId.id = nullptr;
			auto noLabel = choice;
			const std::array<DMUI_ChoiceOption, 1> unlabeled{ { { nullptr, nullptr, 1u, 0u } } };
			noLabel.options = unlabeled.data();
			noLabel.optionCount = 1u;
			auto reservedChoice = choice;
			reservedChoice.reserved = 1u;
			auto nanWidth = choice;
			nanWidth.width = std::numeric_limits<float>::quiet_NaN();
			for (const auto* candidate : { &noOptions, &noLabel, &reservedChoice, &nanWidth })
			{
				require(api.choice(1u, candidate, &result) == DMUI_RESULT_INVALID_ARGUMENT,
					"a malformed choice was drawn");
			}
			require(api.choice(1u, &noId, &result) == DMUI_RESULT_INVALID_ARGUMENT &&
					api.choice(1u, &choice, nullptr) == DMUI_RESULT_INVALID_ARGUMENT,
				"a choice without an id or result was accepted");
		});

		runner.test("scope ends still dispatch after a sticky UI failure", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{
				&AcceptClient
			};
			const auto& api = DearModdingUI::UI::API();
			const auto baseline = ImGui::GetCurrentWindow()->IDStack.Size;
			dmui::ui::detail::ScopedContext context{ &api, 1u };
			dmui::ui::PushID("balanced");
			(void)dmui::ui::Button({});
			dmui::ui::PopID();
			require(
				context.Result() == DMUI_RESULT_INVALID_ARGUMENT &&
					ImGui::GetCurrentWindow()->IDStack.Size == baseline,
				"sticky UI failure prevented a required scope unwind");
		});

		runner.test("client text formatting is dynamic and untruncated", [] {
			auto api = DearModdingUI::UI::API();
			api.text = &CaptureText;
			s_capturedText.clear();
			const std::string payload(8192, 'x');
			dmui::ui::detail::ScopedContext context{ &api, 1u };
			dmui::ui::Text("prefix:%s:suffix", payload.c_str());
			require(
				context.Result() == DMUI_RESULT_OK &&
					s_capturedText.size() == payload.size() + 14u &&
					s_capturedText.starts_with("prefix:") &&
					s_capturedText.ends_with(":suffix"),
				"client-side text formatting truncated or crossed the ABI");
		});


		runner.test("text operation rejects malformed input before drawing", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{
				&AcceptClient
			};
			const auto& api = DearModdingUI::UI::API();
			const DMUI_TextOptions wrap{ DMUI_TEXT_OPTION_WRAP, 0.0f };
			const std::string_view text{ "valid" };
			DMUI_TextSpan whole{};
			whole.length = text.size();
			const auto draw = [&](std::string_view a_text, const DMUI_TextSpan* a_spans,
									  uint32_t a_count, const DMUI_TextOptions* a_options) {
				return api.text(1u, a_text.data(), a_text.size(), a_spans, a_count, a_options);
			};
			require(draw(text, &whole, 1u, &wrap) == DMUI_RESULT_OK &&
					draw(text, nullptr, 0u, nullptr) == DMUI_RESULT_OK &&
					draw("", nullptr, 0u, &wrap) == DMUI_RESULT_OK,
				"valid or empty styled text was rejected");
			require(draw(text, nullptr, 1u, &wrap) == DMUI_RESULT_INVALID_ARGUMENT &&
					api.text(1u, nullptr, 0u, nullptr, 0u, nullptr) ==
						DMUI_RESULT_INVALID_ARGUMENT,
				"null spans or text were accepted");
			const std::string_view broken{ "\xC3" };
			const std::string_view embedded{ "a\0b", 3 };
			DMUI_TextSpan one{};
			one.length = 1;
			require(draw(broken, &one, 1u, &wrap) == DMUI_RESULT_INVALID_ARGUMENT &&
					draw(embedded, &one, 1u, &wrap) == DMUI_RESULT_INVALID_ARGUMENT,
				"invalid UTF-8 was accepted by the styled path");
			// The size check must precede any read of the oversized text.
			const std::string huge(DearModdingUI::kMaxStyledTextBytes + 1u, 'x');
			require(draw(huge, nullptr, 0u, &wrap) == DMUI_RESULT_INVALID_ARGUMENT,
				"oversized styled text reached the renderer");
			const std::vector<DMUI_TextSpan> tooMany(DearModdingUI::kMaxTextSpans + 1u);
			require(draw(text, tooMany.data(), static_cast<uint32_t>(tooMany.size()), &wrap) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"span count above the limit was accepted");
			const DMUI_TextOptions unknownOption{ 0x80u, 0.0f };
			const DMUI_TextOptions negativeWidth{ DMUI_TEXT_OPTION_WRAP, -1.0f };
			require(draw(text, nullptr, 0u, &unknownOption) == DMUI_RESULT_INVALID_ARGUMENT &&
					draw(text, &whole, 1u, &negativeWidth) == DMUI_RESULT_INVALID_ARGUMENT,
				"invalid text options were accepted");
		});

		runner.test("mixed text spans wrap across runs as one item", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{
				&AcceptClient
			};
			const auto& api = DearModdingUI::UI::API();
			dmui::ui::detail::ScopedContext context{ &api, 1u };
			const auto measure = [](const char* a_text) {
				return ImGui::CalcTextSize(a_text).x;
			};
			const auto lineHeight = ImGui::GetTextLineHeight();
			// "cccc" and "dddd" touch across runs, so they wrap as one word.
			const auto wrapWidth = measure("aaaa bbbb cccc") + 1.0f;
			const auto before = ImGui::GetCursorScreenPos().y;
			dmui::StyledText styled;
			styled.Append("aaaa bbbb ", dmui::TextStyle{}.Tone(dmui::TextTone::kWarning))
				.Append("cccc", dmui::TextStyle{}.Bold().Italic())
				.Append("dddd eeee", dmui::TextStyle{}.Color({ 1.0f, 0.0f, 0.0f, 1.0f }));
			dmui::ui::Text(styled, { .wrap = true, .wrapWidth = wrapWidth });
			const auto size = ImGui::GetItemRectSize();
			require(context.Result() == DMUI_RESULT_OK &&
					size.x == measure("ccccdddd eeee") && size.y == lineHeight * 2.0f,
				"runs did not wrap into the expected two-line item");
			require(ImGui::GetCursorScreenPos().y ==
					before + size.y + ImGui::GetStyle().ItemSpacing.y,
				"spans advanced the cursor as more than one item");

			const auto longWord = "supercalifragilisticexpialidocious";
			dmui::ui::Text(longWord, dmui::TextStyle{}, { .wrap = true, .wrapWidth = 60.0f });
			const auto broken = ImGui::GetItemRectSize();
			require(context.Result() == DMUI_RESULT_OK && broken.x <= 60.0f &&
					broken.y >= lineHeight * 2.0f &&
					std::fmod(broken.y, lineHeight) < 0.001f,
				"an overlong word did not break per codepoint within the width");

			dmui::ui::Text("aaaa", dmui::TextStyle{}, { .wrap = true });
			const auto flat = ImGui::GetItemRectSize();
			dmui::ui::Text("aaaa", dmui::TextStyle{}, { .wrap = true, .bullet = true });
			require(ImGui::GetItemRectSize().x ==
					flat.x + ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x * 2.0f,
				"bullet did not indent the item like a native bullet");
		});

	}
}
