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
				m_imgui.BeginWindow("##StableUIContractTest");
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
			ImGuiDataType dataType{};
			require(
				DearModdingUI::UI::Bindings::TranslateDataType(
					DMUI_UI_DATA_TYPE_S32,
					dataType) == DMUI_RESULT_OK &&
					dataType == ImGuiDataType_S32,
				"stable scalar type did not translate by its symbolic mapping");
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

		runner.test("UI query writes only complete caller prefixes", [] {
			struct FullInfo
			{
				DMUI_UIAPIInfo info{};
				uint64_t canary{ UINT64_C(0xD00DFEEDCAFEBABE) };
			} full;
			full.info.structSize = sizeof(full.info);
			require(
				DearModdingUI::UI::Query(
					DMUI_UI_ABI_CURRENT,
					DMUI_UI_REVISION_CURRENT,
					DMUI_UI_API_REQUIRED_SIZE,
					&full.info) == DMUI_RESULT_OK &&
					full.info.api == &DearModdingUI::UI::API() &&
					full.canary == UINT64_C(0xD00DFEEDCAFEBABE),
				"full UI query corrupted caller-owned storage");

			struct alignas(DMUI_UIAPIInfo) PrefixInfo
			{
				std::array<std::byte, DMUI_UI_API_INFO_PREFIX_SIZE> bytes{};
				uint64_t canary{ UINT64_C(0x123456789ABCDEF0) };
			} prefix;
			auto* info = reinterpret_cast<DMUI_UIAPIInfo*>(prefix.bytes.data());
			info->structSize = DMUI_UI_API_INFO_PREFIX_SIZE;
			require(
				DearModdingUI::UI::Query(
					DMUI_UI_ABI_CURRENT,
					DMUI_UI_REVISION_CURRENT,
					DMUI_UI_API_REQUIRED_SIZE,
					info) == DMUI_RESULT_STRUCT_TOO_SMALL &&
					info->structSize == DMUI_UI_API_INFO_PREFIX_SIZE &&
					info->abiVersion == DMUI_UI_ABI_CURRENT &&
					info->revision == DMUI_UI_REVISION_CURRENT &&
					info->tableSize == DMUI_UI_API_CURRENT_SIZE &&
					prefix.canary == UINT64_C(0x123456789ABCDEF0),
				"UI query overwrote an incomplete caller prefix");
		});

		runner.test("style metrics preserve legacy and appended prefixes", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{
				&AcceptClient
			};
			const auto& api = DearModdingUI::UI::API();

			struct alignas(DMUI_StyleMetrics) LegacyMetrics
			{
				std::array<std::byte, DMUI_STYLE_METRICS_0_1_SIZE> bytes{};
				uint32_t canary{ UINT32_C(0xA1B2C3D4) };
			} legacy;
			auto* legacyMetrics =
				reinterpret_cast<DMUI_StyleMetrics*>(legacy.bytes.data());
			legacyMetrics->structSize = DMUI_STYLE_METRICS_0_1_SIZE;
			require(
				api.getStyleMetrics(1u, legacyMetrics) == DMUI_RESULT_OK &&
					legacyMetrics->structSize == DMUI_STYLE_METRICS_0_1_SIZE &&
					legacy.canary == UINT32_C(0xA1B2C3D4),
				"legacy style-metrics prefix overwrote the appended field");

			struct FullMetrics
			{
				DMUI_StyleMetrics metrics{};
				uint32_t canary{ UINT32_C(0xC4D3E2F1) };
			} full;
			full.metrics.structSize = sizeof(full.metrics);
			require(
				api.getStyleMetrics(1u, &full.metrics) == DMUI_RESULT_OK &&
					full.metrics.structSize == sizeof(full.metrics) &&
					full.metrics.fontSizeBase > 0.0f &&
					full.canary == UINT32_C(0xC4D3E2F1),
				"appended style metric was not written prefix-safely");
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

		runner.test("missing optional UI tail does not hide available operations", [] {
			ImGuiFrame frame;
			const DearModdingUI::UI::Testing::ValidationOverride validation{
				&AcceptClient
			};
			auto api = DearModdingUI::UI::API();
			api.structSize = DMUI_UI_API_REQUIRED_SIZE;
			api.plotLines = nullptr;
			dmui::ui::detail::ScopedContext context{ &api, 1u };
			dmui::ui::TextUnformatted("required operation remains available");
			require(context.Result() == DMUI_RESULT_OK,
				"available required operation failed on an older table prefix");
			const float samples[]{ 1.0f, 2.0f };
			dmui::ui::PlotLines("optional", samples, 2);
			require(context.Result() == DMUI_RESULT_UNSUPPORTED_ABI,
				"missing optional operation was confused with an empty result");

			auto missingOptional = DearModdingUI::UI::API();
			missingOptional.plotLines = nullptr;
			dmui::ui::detail::ScopedContext missingContext{
				&missingOptional,
				1u
			};
			dmui::ui::PlotLines("optional-null", samples, 2);
			require(missingContext.Result() == DMUI_RESULT_UNSUPPORTED_ABI,
				"null optional operation was confused with an empty result");
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
