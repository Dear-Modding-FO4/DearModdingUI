#include <DearModdingUI/host/UIAdapter.h>
#include <DearModdingUI/host/ImGuiRecovery.h>
#include <DearModdingUI/UIBindings.generated.h>
#include "../Harness.h"
#include "../support/ImGuiTestContext.h"

#include <DearModdingUI/UI.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

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
			size_t a_length) noexcept
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
			const auto accent = dmui::ui::GetThemeColors().accent;
			verify(dmui::ui::GetColorU32(&DMUI_ThemeColors::accent),
				ImGui::GetColorU32(ImVec4{ accent.x, accent.y, accent.z, accent.w }));
			ImGui::PopStyleColor();
			verify(dmui::ui::GetColorU32(dmui::ui::Color::kText), ImGui::GetColorU32(ImGuiCol_Text));
			ImGui::PopStyleVar();
			require(context.Result() == DMUI_RESULT_OK, "color queries failed in a draw callback");

			const auto original = ImGui::GetStyle().Colors[ImGuiCol_Text];
			require(DearModdingUI::UI::API().pushStyleColorU32(
						1u, DMUI_UI_COLOR_TEXT, UINT32_C(0xFF804020)) == DMUI_RESULT_OK,
				"stable packed color was rejected");
			const auto translated = ImGui::GetStyle().Colors[ImGuiCol_Text];
			require(translated.x == 1.0f &&
					translated.y > 0.50f && translated.y < 0.51f &&
					translated.z > 0.25f && translated.z < 0.26f &&
					translated.w > 0.12f && translated.w < 0.13f,
				"stable packed color was not decoded as 0xRRGGBBAA");
			require(DearModdingUI::UI::API().popStyleColor(1u, 1) == DMUI_RESULT_OK &&
					ImGui::GetStyle().Colors[ImGuiCol_Text].x == original.x &&
					ImGui::GetStyle().Colors[ImGuiCol_Text].w == original.w,
				"stable packed color scope did not restore native state");

			auto* list = ImGui::GetWindowDrawList();
			const auto start = list->VtxBuffer.Size;
			const auto pos = dmui::ui::GetCursorScreenPos();
			const auto draw = dmui::ui::WindowDrawList();
			draw.AddRectFilled(pos, { pos.x + 80, pos.y + 35 }, 0x123456FF);
			const auto widgetStart = list->VtxBuffer.Size;
			(void)dmui::ui::Button("Widget", { 80, 35 });
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
			ImGuiComboFlags combo{};
			require(
				DearModdingUI::UI::Bindings::TranslateComboFlags(
					DMUI_UI_COMBO_FLAGS_HEIGHT_SMALL |
						DMUI_UI_COMBO_FLAGS_HEIGHT_LARGE,
					combo) == DMUI_RESULT_INVALID_ARGUMENT,
				"mutually exclusive stable flags reached native ImGui");
		});

		runner.test("input callbacks and unsafe scalar storage are rejected", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{
				&AcceptClient
			};
			const auto& api = DearModdingUI::UI::API();
			char text[8]{};
			uint32_t changed{};
			require(
				api.inputText(
					1u,
					"##text",
					text,
					sizeof(text),
					DMUI_UI_INPUT_TEXT_FLAGS_REJECTED_CALLBACK_MASK,
					&changed) == DMUI_RESULT_INVALID_ARGUMENT,
				"unsupported native input callback flags were accepted");
			int32_t value{};
			require(
				api.inputScalar(
					1u,
					"##scalar",
					DMUI_UI_DATA_TYPE_S32,
					&value,
					sizeof(value) - 1u,
					nullptr,
					0u,
					nullptr,
					0u,
					nullptr,
					DMUI_UI_INPUT_TEXT_FLAGS_NONE,
					&changed) == DMUI_RESULT_INVALID_ARGUMENT,
				"undersized scalar storage reached native ImGui");
			alignas(8) std::array<std::byte, 8> misaligned{};
			require(
				api.inputScalar(
					1u,
					"##misaligned",
					DMUI_UI_DATA_TYPE_S32,
					misaligned.data() + 1,
					sizeof(int32_t),
					nullptr,
					0u,
					nullptr,
					0u,
					nullptr,
					DMUI_UI_INPUT_TEXT_FLAGS_NONE,
					&changed) == DMUI_RESULT_INVALID_ARGUMENT,
				"misaligned scalar storage reached native ImGui");
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
			(void)dmui::ui::Button(nullptr);
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

	}
}
