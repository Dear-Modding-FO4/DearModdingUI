#include <DearModdingUI/host/UIAdapter.h>
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
		runner.test("cursor position round trips in window-local coordinates", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{ &AcceptClient };
			dmui::ui::detail::ScopedContext context{ &DearModdingUI::UI::API(), 1u };
			dmui::ui::SetCursorPos({ 35.0f, 45.0f });
			const auto position = dmui::ui::GetCursorPos();
			dmui::ui::SetCursorPosX(55.0f);
			dmui::ui::SetCursorPosY(65.0f);
			const auto updated = dmui::ui::GetCursorPos();
			dmui::ui::Dummy({ 1.0f, 1.0f });
			require(position.x == 35.0f && position.y == 45.0f &&
					updated.x == 55.0f && updated.y == 65.0f &&
					context.Result() == DMUI_RESULT_OK,
				"cursor operations changed coordinate space or the other axis");
		});

		runner.test("aligned text clamps alignment and ellipsizes within its item width", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{ &AcceptClient };
			dmui::ui::detail::ScopedContext context{ &DearModdingUI::UI::API(), 1u };
			auto* drawList = ImGui::GetWindowDrawList();
			const auto start = drawList->VtxBuffer.Size;
			const auto position = ImGui::GetCursorScreenPos();
			dmui::ui::TextAligned(-1.0f, 200.0f, "Align");
			const auto left = drawList->VtxBuffer[start].pos.x - position.x;
			const auto next = drawList->VtxBuffer.Size;
			dmui::ui::TextAligned(2.0f, 200.0f, "Align");
			const auto right = drawList->VtxBuffer[next].pos.x - position.x;
			const auto expected = 200.0f - ImGui::CalcTextSize("Align").x;
			require(std::abs((right - left) - expected) < 1.1f,
				"text alignment did not clamp to the item edges");
			const auto clippedStart = drawList->VtxBuffer.Size;
			dmui::ui::TextAligned(0.5f, 60.0f, "This text must be ellipsized, not overflow the item");
			const auto bounds = ImGui::GetItemRectMax();
			require(ImGui::GetItemRectSize().x == 60.0f &&
					drawList->VtxBuffer.Size > clippedStart &&
					drawList->VtxBuffer.Size - clippedStart < 48 * 4,
				"overflowing text was not shortened inside the requested width");
			for (auto i = clippedStart; i < drawList->VtxBuffer.Size; ++i)
				require(drawList->VtxBuffer[i].pos.x <= bounds.x + 1.0f,
					"ellipsized text escaped its item bounds");
			require(context.Result() == DMUI_RESULT_OK, "aligned text failed UI dispatch");
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

		runner.test("stable UI values translate by name instead of reinterpretation", [] {
			ImGuiCol color{};
			require(
				DearModdingUI::UI::Bindings::TranslateColor(
					DMUI_UI_COLOR_TEXT,
					color) == DMUI_RESULT_OK &&
					color == ImGuiCol_Text,
				"stable color did not translate by its symbolic mapping");
			require(
				DearModdingUI::UI::Bindings::TranslateColor(
					UINT32_C(999999),
					color) == DMUI_RESULT_INVALID_ARGUMENT,
				"unknown stable enum value reached native ImGui");
		});

		runner.test("stable UI rejects unknown and mutually exclusive flags", [] {
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



		runner.test("style vars reject mismatched value shapes before ImGui", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{
				&AcceptClient
			};
			const auto& api = DearModdingUI::UI::API();
			const auto baseline = ImGui::GetCurrentContext()->StyleVarStack.Size;
			require(
				api.pushStyleVarVec2(1u, DMUI_UI_STYLE_VAR_ALPHA, { 0.5f, 0.5f }) ==
						DMUI_RESULT_INVALID_ARGUMENT &&
					api.pushStyleVarFloat(1u, DMUI_UI_STYLE_VAR_FRAME_PADDING, 2.0f) ==
						DMUI_RESULT_INVALID_ARGUMENT &&
					ImGui::GetCurrentContext()->StyleVarStack.Size == baseline,
				"mismatched style-var shape reached native ImGui");

			const auto alpha = ImGui::GetStyle().Alpha;
			require(
				api.pushStyleVarFloat(1u, DMUI_UI_STYLE_VAR_ALPHA, 0.25f) == DMUI_RESULT_OK &&
					api.pushStyleVarVec2(1u, DMUI_UI_STYLE_VAR_FRAME_PADDING, { 3.0f, 1.0f }) ==
						DMUI_RESULT_OK &&
					ImGui::GetStyle().Alpha == 0.25f &&
					ImGui::GetStyle().FramePadding.x == 3.0f &&
					api.popStyleVar(1u, 2) == DMUI_RESULT_OK &&
					ImGui::GetStyle().Alpha == alpha &&
					ImGui::GetCurrentContext()->StyleVarStack.Size == baseline,
				"matching style vars did not push and pop through the table");
		});

		runner.test("false widget results stay distinct from UI dispatch errors", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{
				&AcceptClient
			};
			const auto& api = DearModdingUI::UI::API();
			dmui::ui::detail::ScopedContext context{ &api, 1u };
			require(!dmui::ui::Button("not clicked"),
				"offscreen button unexpectedly reported a click");
			require(context.Result() == DMUI_RESULT_OK,
				"normal false widget result was reported as a UI error");
		});

		runner.test("scope ends still dispatch after a sticky UI failure", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{
				&AcceptClient
			};
			auto api = DearModdingUI::UI::API();
			api.button = nullptr;
			const auto baseline = ImGui::GetCurrentWindow()->IDStack.Size;
			dmui::ui::detail::ScopedContext context{ &api, 1u };
			dmui::ui::PushID("balanced");
			(void)dmui::ui::Button("missing");
			dmui::ui::PopID();
			require(
				context.Result() == DMUI_RESULT_UNSUPPORTED_ABI &&
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

		runner.test("stable packed colors translate as RGBA", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{
				&AcceptClient
			};
			const auto& api = DearModdingUI::UI::API();
			const auto original = ImGui::GetStyle().Colors[ImGuiCol_Text];
			require(
				api.pushStyleColorU32(
					1u,
					DMUI_UI_COLOR_TEXT,
					UINT32_C(0xFF804020)) == DMUI_RESULT_OK,
				"stable packed color was rejected");
			const auto translated = ImGui::GetStyle().Colors[ImGuiCol_Text];
			require(
				translated.x == 1.0f &&
					translated.y > 0.50f && translated.y < 0.51f &&
					translated.z > 0.25f && translated.z < 0.26f &&
					translated.w > 0.12f && translated.w < 0.13f,
				"stable packed color was not decoded as 0xRRGGBBAA");
			require(api.popStyleColor(1u, 1) == DMUI_RESULT_OK,
				"stable packed color scope did not unwind");
			const auto restored = ImGui::GetStyle().Colors[ImGuiCol_Text];
			require(
				restored.x == original.x &&
					restored.y == original.y &&
					restored.z == original.z &&
					restored.w == original.w,
				"stable packed color scope did not restore native state");
		});
	}
}
