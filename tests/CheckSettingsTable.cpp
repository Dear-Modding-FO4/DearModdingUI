#include <DearModdingUI/ImGuiRecovery.h>
#include <DearModdingUI/SettingsTable.h>
#include <DearModdingUI/Shell.h>
#include <DearModdingUI/Theme.h>
#include <DearModdingUI/UIAdapter.h>
#include "Harness.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <DearModdingUI/Presentation.h>
#include <DearModdingUI/Client.h>

#include <utility>

namespace DearModdingUI
{
	namespace
	{
		std::string_view s_sectionHeaderText;
		char32_t s_sectionHeaderGlyph{};
	}

	void DrawSectionHeader(const char* a_text, char32_t a_glyph) noexcept
	{
		s_sectionHeaderText = a_text;
		s_sectionHeaderGlyph = a_glyph;
	}

	float SettingsActionButtonWidth(
		SettingsAction,
		const char*,
		float a_buttonExtent) noexcept
	{
		return a_buttonExtent;
	}

	bool DrawSettingsActionButton(
		const char*,
		const ImVec2&,
		const ImVec2&,
		SettingsAction,
		const char*,
		const char*,
		bool) noexcept
	{
		return false;
	}
}

namespace DearModdingUI::Theme
{
	FontGuard::FontGuard(FontRole, float) noexcept {}
	FontGuard::~FontGuard() noexcept = default;
}

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI;

		[[nodiscard]] DMUI_Result AcceptUIClient(
			DMUI_ClientHandle) noexcept
		{
			return DMUI_RESULT_OK;
		}

		class ImGuiTestFrame
		{
		public:
			explicit ImGuiTestFrame(ImVec2 a_position = { 60.0f, 60.0f })
			{
				m_context = ImGui::CreateContext();
				auto& io = ImGui::GetIO();
				io.DisplaySize = { 1280.0f, 720.0f };
				io.DeltaTime = 1.0f / 60.0f;
				io.IniFilename = nullptr;
				io.ConfigErrorRecoveryEnableAssert = false;
				io.ConfigErrorRecoveryEnableDebugLog = false;
				io.ConfigErrorRecoveryEnableTooltip = false;
				(void)io.Fonts->Build();
				m_context->ErrorCallback = [](ImGuiContext*, void* a_userData, const char*) {
					++*static_cast<int*>(a_userData);
				};
				m_context->ErrorCallbackUserData = &m_errors;
				ImGui::NewFrame();
				ImGui::SetNextWindowPos(a_position);
				ImGui::SetNextWindowSize({ 640.0f, 480.0f });
				(void)ImGui::Begin("##SettingsTableTest");
				ImGui::ErrorRecoveryStoreState(&m_recovery);
				m_idDepth = ImGui::GetCurrentWindow()->IDStack.Size;
				m_colorDepth = m_context->ColorStack.Size;
				m_disabledDepth = m_context->DisabledStackSize;
				m_wrapDepth =
					ImGui::GetCurrentWindow()->DC.TextWrapPosStack.Size;
				m_beginPopupDepth = m_context->BeginPopupStack.Size;
			}

			~ImGuiTestFrame()
			{
				ImGui::ErrorRecoveryTryToRecoverState(&m_recovery);
				ImGui::End();
				ImGui::EndFrame();
				ImGui::DestroyContext(m_context);
			}

			[[nodiscard]] int Errors() const noexcept
			{
				return m_errors;
			}

			[[nodiscard]] bool IsAtBaseline() const noexcept
			{
				const auto* context = ImGui::GetCurrentContext();
				return context &&
					context->CurrentTable == nullptr &&
					context->CurrentWindow->IDStack.Size == m_idDepth &&
					context->ColorStack.Size == m_colorDepth &&
					context->DisabledStackSize == m_disabledDepth &&
					context->CurrentWindow->DC.TextWrapPosStack.Size ==
						m_wrapDepth &&
					context->BeginPopupStack.Size == m_beginPopupDepth;
			}

		private:
			UI::Testing::ValidationOverride m_uiValidation{ &AcceptUIClient };
			dmui::ui::detail::ScopedContext m_uiContext{
				&UI::API(),
				1u
			};
			ImGuiContext* m_context{ nullptr };
			ImGuiErrorRecoveryState m_recovery;
			int m_idDepth{ 0 };
			int m_colorDepth{ 0 };
			int m_disabledDepth{ 0 };
			int m_wrapDepth{ 0 };
			int m_beginPopupDepth{ 0 };
			int m_errors{ 0 };
		};

		[[nodiscard]] DMUI_ThemeColors TestTheme() noexcept
		{
			return {
				sizeof(DMUI_ThemeColors),
				{ 1.0f, 0.0f, 0.0f, 1.0f },
				{ 2.0f, 0.0f, 0.0f, 1.0f },
				{ 3.0f, 0.0f, 0.0f, 1.0f },
				{ 4.0f, 0.0f, 0.0f, 1.0f },
				{ 5.0f, 0.0f, 0.0f, 1.0f },
				{ 6.0f, 0.0f, 0.0f, 1.0f },
				{ 7.0f, 0.0f, 0.0f, 1.0f },
				{ 8.0f, 0.0f, 0.0f, 1.0f },
				{ 9.0f, 0.0f, 0.0f, 1.0f },
				{ 10.0f, 0.0f, 0.0f, 1.0f },
				{ 11.0f, 0.0f, 0.0f, 1.0f },
				{ 12.0f, 0.0f, 0.0f, 1.0f },
				{ 13.0f, 0.0f, 0.0f, 1.0f },
				{ 14.0f, 0.0f, 0.0f, 1.0f }
			};
		}
	}

	void run_settings_table_checks(Runner& runner)
	{
		runner.test("native section headings inherit icons without overriding explicit glyphs", [] {
			for (const auto& [label, icon] : {
					 std::pair{ "Appearance", "palette" },
					 std::pair{ "Readability", "text-aa" },
					 std::pair{ "Input", "keyboard" },
					 std::pair{ "Host facts (read-only)", "info" } })
			{
				DrawSectionHeader(label);
				require(s_sectionHeaderText == label &&
						s_sectionHeaderGlyph == FindPhosphorIconGlyphOrZero(icon),
					"native section heading bypassed the shared icon classifier");
			}
			DrawSectionHeader("Appearance", PhosphorGlyph::kGear);
			require(s_sectionHeaderGlyph == PhosphorGlyph::kGear,
				"section classification replaced an explicit icon");
			DrawSectionHeader("Appearance", 0);
			require(s_sectionHeaderGlyph == 0,
				"section classification replaced an explicit no-icon choice");
			DrawSectionHeader("Unclassified Heading");
			require(s_sectionHeaderGlyph == PhosphorGlyph::kQuestion,
				"native section heading lost the classifier fallback");
		});

		runner.test("presentation tones resolve every theme role", [] {
			const auto theme = TestTheme();
			const dmui::TextTone tones[]{
				dmui::TextTone::kSuccess,
				dmui::TextTone::kWarning,
				dmui::TextTone::kError,
				dmui::TextTone::kInfo,
				dmui::TextTone::kMuted,
				dmui::TextTone::kAccent,
				dmui::TextTone::kAccentMuted,
				dmui::TextTone::kStatusDisable,
				dmui::TextTone::kStatusError,
				dmui::TextTone::kStatusWarning,
				dmui::TextTone::kStatusRestartNeeded,
				dmui::TextTone::kStatusCurrentHotkey,
				dmui::TextTone::kStatusSuccess,
				dmui::TextTone::kStatusInfo
			};
			for (size_t index = 0; index < std::size(tones); ++index)
			{
				const auto resolved =
					dmui::ResolveTextColor(theme, tones[index]);
				require(
					resolved.result == DMUI_RESULT_OK &&
						resolved.color &&
						resolved.color->x == static_cast<float>(index + 1),
					"theme tone selected the wrong field");
			}
			const auto inherited = dmui::ResolveTextColor(
				DMUI_ThemeColors{},
				dmui::TextTone::kInherit);
			require(inherited && !inherited.color,
				"inherit tone required a theme color");
			auto truncated = theme;
			truncated.structSize = DMUI_THEME_COLORS_0_1_SIZE - 1;
			require(
				dmui::ResolveTextColor(
					truncated,
					dmui::TextTone::kAccent).result ==
					DMUI_RESULT_STRUCT_TOO_SMALL,
				"truncated theme snapshot was accepted");
			require(
				dmui::ResolveTextColor(
					theme,
					static_cast<dmui::TextTone>(255)).result ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"unknown text tone was accepted");
		});

		runner.test("styled text balances color and wrapping stacks", [] {
			ImGuiTestFrame frame;
			auto text = std::string(4096, 'x');
			text += " 100% ##literal ";
			text += "\xE2\x98\x83";
			require(
				dmui::DrawStyledText(
					text,
					TestTheme(),
					{
						.tone = dmui::TextTone::kStatusRestartNeeded,
						.wrapped = true
					}) == DMUI_RESULT_OK,
				"styled text draw failed");
			require(
				dmui::DrawStyledText(
					{},
					DMUI_ThemeColors{},
					{}) == DMUI_RESULT_OK,
				"empty inherited text required a theme");
			require(
				dmui::DrawStyledText(
					"text",
					TestTheme(),
					{ .fontRole = DMUI_FONT_ROLE_BODY }) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"snapshot-only styled text accepted a client font role");
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"styled text changed the ImGui stack");
		});

		runner.test("client text helpers fail before drawing when disconnected", [] {
			ImGuiTestFrame frame;
			dmui::Client client{
				"tests.presentation",
				"Presentation tests",
				{ 1, 0 }
			};
			const auto start = ImGui::GetCursorScreenPos();
			require(
				!dmui::DrawStyledText(
					client,
					"not drawn",
					{ .tone = dmui::TextTone::kAccent }) &&
					client.LastResult() == DMUI_RESULT_CLIENT_NOT_FOUND,
				"disconnected styled text did not report its failure");
			require(
				!dmui::DrawLabeledValue(
					client,
					"Label",
					"not drawn",
					{
						.valueStyle = {
							.fontRole = DMUI_FONT_ROLE_BODY,
							.tone = dmui::TextTone::kSuccess
						}
					}) &&
					client.LastResult() == DMUI_RESULT_CLIENT_NOT_FOUND,
				"disconnected labeled value did not preflight");
			const auto end = ImGui::GetCursorScreenPos();
			require(start.x == end.x && start.y == end.y,
				"failed text helper drew a partial value");
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"failed text helper changed the ImGui stack");
		});

		runner.test("labeled value preserves its label when value font push fails", [] {
			ImGuiTestFrame frame;
			dmui::Client client{
				"tests.presentation.font-failure",
				"Presentation font failure",
				{ 1, 0 }
			};
			require(client.Connect(), "fixture client did not connect");
			const auto start = ImGui::GetCursorScreenPos();
			require(
				!dmui::DrawLabeledValue(
					client,
					"Must not render",
					"Value",
					{
						.valueStyle = {
							.fontRole = DMUI_FONT_ROLE_HEADING
						}
					}) &&
					client.LastResult() == DMUI_RESULT_UNSUPPORTED_ABI,
				"unsupported font push did not fail explicitly");
			const auto end = ImGui::GetCursorScreenPos();
			require(end.x > start.x && end.y == start.y,
				"failed value-font acquisition dropped the caller-font label");
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"failed labeled value changed the ImGui stack");
		});

		runner.test("disabled and tooltip scopes end idempotently", [] {
			ImGuiTestFrame frame;
			{
				const dmui::DisabledScope disabled{ false };
				ImGui::TextUnformatted("enabled");
			}
			const auto start = ImGui::GetCursorScreenPos();
			ImGui::GetIO().MousePos = { start.x + 1.0f, start.y + 1.0f };
			ImGui::Dummy({ 40.0f, 20.0f });
			{
				dmui::TooltipScope tooltip{
					dmui::ui::HoveredFlags::kAllowWhenDisabled
				};
				if (tooltip.Visible())
				ImGui::TextUnformatted("rich tooltip");
				const auto ended = tooltip.End();
				require(!tooltip.End(), "tooltip ended twice");
				require(!tooltip.Visible(), "ended tooltip remained visible");
				require(!tooltip.Hovered() || ended,
					"hovered tooltip did not open");
			}
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"presentation scope changed the ImGui stack");
		});

		runner.test("choice draw preserves unknown and empty values", [] {
			ImGuiTestFrame frame;
			const std::array options{
				dmui::ChoiceOption<int>{
					.value = 1,
					.label = "100% ## duplicate",
					.key = "first"
				},
				dmui::ChoiceOption<int>{
					.value = 2,
					.label = "100% ## duplicate",
					.key = "second",
					.enabled = false
				}
			};
			ImGui::PushID("known");
			ImGui::OpenPopupEx(
				ImHashStr("##ComboPopup", 0, ImGui::GetID("##Choice")),
				ImGuiPopupFlags_None);
			ImGui::PopID();
			ImGui::LogToBuffer();
			const auto known = dmui::DrawChoice(
				"known",
				1,
				options,
				"Unavailable",
				"Quality mode ## literal");
			const std::string logged{
				ImGui::GetCurrentContext()->LogBuffer.c_str()
			};
			ImGui::LogFinish();
			const auto& popups = ImGui::GetCurrentContext()->OpenPopupStack;
			require(popups.Size == 1 && popups[0].Window &&
					popups[0].Window->Active &&
					popups[0].Window->DC.CursorMaxPos.y >
						popups[0].Window->DC.CursorStartPos.y,
				"choice regression did not draw the actual combo popup");
			require(
				!known.changed && !known.completed && !known.selected,
				"drawing a choice changed its current value");
			require(
				logged.contains("Quality mode ## literal"),
				"choice display label was hidden or treated as identity");
			const auto unknown = dmui::DrawChoice(
				"unknown",
				99,
				options,
				"Unknown selection");
			require(
				!unknown.changed && !unknown.completed && !unknown.selected,
				"unknown choice was silently clamped");
			const auto empty = dmui::DrawChoice(
				"empty",
				99,
				{});
			require(
				!empty.changed && !empty.completed && !empty.selected,
				"empty choice produced a selection");
			const auto unchanged =
				dmui::presentation_detail::ResolveChoiceActivation(
					1,
					options[0],
					true);
			const auto disabled =
				dmui::presentation_detail::ResolveChoiceActivation(
					1,
					options[1],
					true);
			auto enabled = options[1];
			enabled.enabled = true;
			const auto changed =
				dmui::presentation_detail::ResolveChoiceActivation(
					1,
					enabled,
					true);
			require(
				!unchanged.changed && !unchanged.completed &&
					!unchanged.selected &&
					!disabled.changed && !disabled.completed &&
					!disabled.selected &&
					changed.changed && changed.completed &&
					changed.selected == 2,
				"choice activation return semantics changed");
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"choice draw changed the ImGui stack");
		});

		runner.test("declarative choices separate unmatched labels from stored values", [] {
			ImGuiTestFrame frame;
			std::string stored{ "missing.xml" };
			size_t writes{};
			size_t edits{};
			dmui::SettingDescriptor setting;
			setting.control = dmui::ChoiceSettingControl{
				.options = { { "", "None" }, { "known.xml", "Known preset" } },
				.unmatchedLabel = "None"
			};
			setting.defaultValue = std::string{};
			setting.binding = dmui::BindSetting(
				[&] { return stored; },
				[&](std::string value) {
					++writes;
					stored = std::move(value);
					return stored;
				});
			setting.onEdit = [&](const dmui::SettingEditEvent& event) {
				++edits;
				require(event.changed && event.completed &&
						std::get<std::string>(event.value).empty(),
					"choice reset did not emit one completed empty-string change");
			};

			const auto drawAndCheck = [&](const char* id, std::string_view expected) {
				ImGui::PushID(id);
				ImGui::LogToBuffer();
				const auto drawn = dmui::setting_detail::DrawBoundSetting(
					setting, setting.binding.get());
				const std::string logged{ ImGui::GetCurrentContext()->LogBuffer.c_str() };
				ImGui::LogFinish();
				ImGui::PopID();
				require(!drawn.changed && !drawn.completed &&
						std::get<std::string>(drawn.value) == stored,
					"choice presentation changed the bound value");
				require(logged.contains(expected),
					"choice did not draw the requested preview label");
			};
			drawAndCheck("unmatched", "None");
			require(stored == "missing.xml" && writes == 0 && edits == 0 &&
					!dmui::IsSettingDefault(setting, setting.binding.get()),
				"unmatched presentation masked the stored non-default value");

			const auto cleared = dmui::ResetSettingToDefault(setting);
			require(cleared && std::get<std::string>(*cleared).empty() &&
					stored.empty() && writes == 1 && edits == 1,
				"reset did not clear the real unknown value");
			(void)dmui::ResetSettingToDefault(setting);
			require(writes == 1 && edits == 1,
				"already-default reset wrote or emitted a change");
			stored = "known.xml";
			drawAndCheck("matched", "Known preset");

			auto& control = std::get<dmui::ChoiceSettingControl>(setting.control);
			control = {};
			stored = "missing.xml";
			drawAndCheck("empty-list", "Unavailable");
			require(writes == 1 && edits == 1 && frame.IsAtBaseline() && frame.Errors() == 0,
				"empty-list presentation wrote storage or leaked ImGui state");
		});

		runner.test("ImGui recovery reports repaired stack depths", [] {
			ImGuiTestFrame frame;
			auto recovery = ImGuiRecoverySnapshot::Capture();
			require(recovery.has_value(), "recovery snapshot was not captured");
			ImGui::PushID("leaked-id");
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{});
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
			ImGui::PushFont(ImGui::GetFont());
			require(ImGui::BeginTable("leaked-table", 1),
				"diagnostic table did not begin");

			const auto repaired = recovery->RecoverAfterCallback();
			require(repaired.Repaired(), "stack repair was not reported");
			require(
				repaired.before.tables == repaired.after.tables + 1 &&
					repaired.before.ids == repaired.after.ids + 2 &&
					repaired.before.colors == repaired.after.colors + 1 &&
					repaired.before.styleVariables ==
						repaired.after.styleVariables + 1 &&
					repaired.before.fonts == repaired.after.fonts + 1,
				"reported stack depths did not describe the repair");
			require(frame.IsAtBaseline(),
				"reported recovery did not restore the ImGui baseline");
		});

		runner.test("settings row closes its nested controls table", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			ImGuiTestFrame frame;
			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				const auto table = SettingsTable::Begin(owner, "settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"settings table did not begin");
				const auto row = SettingsTable::BeginRow(
					owner, "row", "Setting", "Description");
				require(row.result == DMUI_RESULT_OK && row.visible,
					"settings row did not begin");
				bool resetPressed{};
				require(SettingsTable::EndRow(
							owner, { true, false }, resetPressed) ==
						DMUI_RESULT_OK,
					"settings row rejected its nested table depth");
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"settings table did not end");
			}
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"balanced row changed the ImGui stack");
		});

		runner.test("settings row accepts a deliberately empty label", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			ImGuiTestFrame frame;
			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				const auto table = SettingsTable::Begin(owner, "settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"settings table did not begin");
				const auto row = SettingsTable::BeginRow(
					owner, "prose", "", "");
				require(row.result == DMUI_RESULT_OK && row.visible,
					"settings row rejected an empty label");
				ImGui::TextUnformatted("Prose");
				bool resetPressed{};
				require(SettingsTable::EndRow(
							owner, { false, false }, resetPressed) ==
						DMUI_RESULT_OK,
					"unlabeled settings row did not end");
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"settings table did not end");
			}
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"unlabeled row changed the ImGui stack");
		});

		runner.test("settings descriptions wrap inside translated window columns", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			constexpr auto description =
				"This description must remain entirely inside the label column, "
				"wrapping onto additional lines rather than disappearing beneath "
				"the value control. Moving the window must not change its wrapping.";
			for (const auto position : { ImVec2{ 60.0f, 60.0f }, ImVec2{ 420.0f, 100.0f } })
			{
				ImGuiTestFrame frame{ position };
				{
					const SettingsTable::ClientCallbackGuard guard{ owner };
					const auto table = SettingsTable::Begin(owner, "wrapped-settings");
					require(table.result == DMUI_RESULT_OK && table.visible,
						"wrapped settings table did not begin");
					const auto tableId = ImGui::GetCurrentTable()->ID;
					const auto row = SettingsTable::BeginRow(
						owner, "wrapped", "Setting", description);
					require(row.result == DMUI_RESULT_OK && row.visible,
						"wrapped settings row did not begin");
					const auto* outer = ImGui::GetCurrentContext()->Tables.GetByKey(tableId);
					const auto& column = outer->Columns[0];
					const auto expected = ImGui::CalcTextSize(
						description, nullptr, false, column.WorkMaxX - column.WorkMinX);
					const auto labelMaxX = column.WorkMaxX;
					ImGui::Button("Value");
					bool resetPressed{};
					require(SettingsTable::EndRow(
								owner, { false, false }, resetPressed) == DMUI_RESULT_OK,
						"wrapped settings row did not end");
					const auto drawn = ImGui::GetCurrentContext()->LastItemData.Rect;
					require(drawn.Max.x <= labelMaxX + 0.5f &&
							drawn.GetHeight() >= expected.y - 0.5f &&
							expected.y > ImGui::GetFontSize(),
						"description wrapping escaped its column or lost text lines");
					require(SettingsTable::End(owner) == DMUI_RESULT_OK,
						"wrapped settings table did not end");
				}
				require(frame.IsAtBaseline() && frame.Errors() == 0,
					"description wrapping changed the ImGui stack");
			}
		});

		runner.test("full-span settings row uses the complete table width", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			ImGuiTestFrame frame;
			float rowWidth{};
			float valueColumnWidth{};
			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				const auto table = SettingsTable::Begin(owner, "settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"settings table did not begin");
				const auto row = SettingsTable::BeginRow(
					owner,
					"prose",
					"",
					"",
					SettingsTable::RowLayout::kFullSpan);
				require(row.result == DMUI_RESULT_OK && row.visible,
					"full-span settings row did not begin");
				rowWidth = ImGui::GetCurrentTable()->OuterRect.GetWidth();
				ImGui::TextWrapped("Full-width prose");
				bool resetPressed{};
				require(SettingsTable::EndRow(
							owner, { false, false }, resetPressed) ==
						DMUI_RESULT_OK,
					"full-span settings row did not end");

				const auto comparison = SettingsTable::BeginRow(
					owner, "value", "", "");
				require(comparison.result == DMUI_RESULT_OK && comparison.visible,
					"value-column settings row did not begin");
				valueColumnWidth = ImGui::GetCurrentTable()->OuterRect.GetWidth();
				ImGui::TextUnformatted("Value");
				require(SettingsTable::EndRow(
							owner, { false, false }, resetPressed) ==
						DMUI_RESULT_OK,
					"value-column settings row did not end");
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"settings table did not end");
			}
			require(rowWidth > valueColumnWidth * 1.8f,
				"full-span row remained constrained to the value column (" +
					std::to_string(rowWidth) + " vs " +
					std::to_string(valueColumnWidth) + ")");
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"full-span row changed the ImGui stack");
		});

		runner.test("settings row survives ImGui table pool growth", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			ImGuiTestFrame frame;
			auto& tables = ImGui::GetCurrentContext()->Tables;
			auto seed = 0;
			const auto seedTable = [&]() {
				ImGui::PushID(seed++);
				require(ImGui::BeginTable("seed", 1),
					"table pool seed did not begin");
				ImGui::EndTable();
				ImGui::PopID();
			};
			seedTable();
			while (tables.GetBufSize() + 1 < tables.Buf.Capacity)
				seedTable();
			const auto capacity = tables.Buf.Capacity;

			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				auto recovery = ImGuiRecoverySnapshot::Capture();
				require(recovery.has_value(), "recovery snapshot was not captured");
				const auto table = SettingsTable::Begin(owner, "settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"settings table did not begin");
				require(tables.Buf.Capacity == capacity,
					"outer settings table unexpectedly grew the table pool");
				const auto row = SettingsTable::BeginRow(
					owner, "row", "Setting", "Description");
				require(row.result == DMUI_RESULT_OK && row.visible,
					"settings row did not begin");
				require(tables.Buf.Capacity > capacity,
					"nested controls table did not grow the table pool");
				bool value{};
				(void)ImGui::Checkbox("##Value", &value);
				bool resetPressed{};
				require(SettingsTable::EndRow(
							owner, { true, false }, resetPressed) ==
						DMUI_RESULT_OK,
					"table pool growth invalidated the settings row");
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"table pool growth invalidated the settings table");
				const auto repaired = recovery->RecoverAfterCallback();
				require(!repaired.Repaired(),
					"table pool growth required ImGui recovery");
			}
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"table pool growth changed the ImGui stack");
		});

		runner.test("first-frame host controls need no ImGui recovery", [] {
			ImGuiTestFrame frame;
			auto recovery = ImGuiRecoverySnapshot::Capture();
			require(recovery.has_value(), "recovery snapshot was not captured");
			const auto table = SettingsTable::Begin(
				DMUI_INVALID_CLIENT_HANDLE,
				"host-settings");
			require(table.result == DMUI_RESULT_OK && table.visible,
				"host settings table did not begin");

			const auto drawRow = [](const char* a_id, auto&& a_draw) {
				const auto row = SettingsTable::BeginRow(
					DMUI_INVALID_CLIENT_HANDLE,
					a_id,
					"Setting",
					"Description");
				require(row.result == DMUI_RESULT_OK && row.visible,
					"host settings row did not begin");
				a_draw();
				bool resetPressed{};
				require(SettingsTable::EndRow(
							DMUI_INVALID_CLIENT_HANDLE,
							{ true, false },
							resetPressed) == DMUI_RESULT_OK,
					"host settings row did not end");
			};

			float color[]{ 0.25f, 0.50f, 0.75f };
			drawRow("color", [&]() {
				(void)ImGui::ColorPicker3("##Value", color);
				for (int index = 0; index < 3; ++index)
				{
					ImGui::PushID(index);
					if (index > 0)
						ImGui::SameLine();
					(void)ImGui::ColorButton(
						"Preset",
						{ color[0], color[1], color[2], 1.0f });
					ImGui::PopID();
				}
			});
			bool checked{};
			drawRow("checkbox", [&]() {
				(void)ImGui::Checkbox("##Value", &checked);
			});
			float scalar{ 0.5f };
			drawRow("slider", [&]() {
				(void)ImGui::SliderFloat(
					"##Value",
					&scalar,
					0.0f,
					1.0f);
			});

			require(SettingsTable::End(DMUI_INVALID_CLIENT_HANDLE) ==
					DMUI_RESULT_OK,
				"host settings table did not end");
			const auto repaired = recovery->RecoverAfterCallback();
			require(!repaired.Repaired() && frame.Errors() == 0,
				"balanced first-frame controls required ImGui recovery");
		});

		runner.test("first-frame page callback preserves its host table", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			ImGuiTestFrame frame;
			require(ImGui::BeginTable(
						"host-shell",
						2,
						ImGuiTableFlags_SizingStretchProp |
							ImGuiTableFlags_Resizable),
				"host shell table did not begin");
			ImGui::TableSetupColumn("Navigation", ImGuiTableColumnFlags_None, 3.0f);
			ImGui::TableSetupColumn("Page", ImGuiTableColumnFlags_None, 7.0f);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Navigation");
			ImGui::TableNextColumn();
			require(ImGui::BeginChild("page-frame", {}, ImGuiChildFlags_Borders),
				"page frame did not begin");
			ImGui::PushID(11);

			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				auto recovery = ImGuiRecoverySnapshot::Capture();
				require(recovery.has_value(), "recovery snapshot was not captured");
				const auto table = SettingsTable::Begin(owner, "settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"client settings table did not begin");
				const auto row = SettingsTable::BeginRow(
					owner, "row", "Setting", "Description");
				require(row.result == DMUI_RESULT_OK && row.visible,
					"client settings row did not begin");
				bool value{};
				(void)ImGui::Checkbox("##Value", &value);
				bool resetPressed{};
				require(SettingsTable::EndRow(
							owner, { true, false }, resetPressed) ==
						DMUI_RESULT_OK,
					"client settings row did not end");
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"client settings table did not end");
				const auto repaired = recovery->RecoverAfterCallback();
				require(!repaired.Repaired(),
					"balanced page callback required ImGui recovery");
			}

			ImGui::PopID();
			ImGui::EndChild();
			ImGui::EndTable();
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"page callback changed the host table stack");
		});

		runner.test("invisible settings row opens no bracket", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			ImGuiTestFrame frame;
			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				const auto table = SettingsTable::Begin(owner, "settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"settings table did not begin");
				auto* currentTable = ImGui::GetCurrentTable();
				currentTable->TempData->ReconcileColumnsRequests[1].Flags |=
					ImGuiTableColumnFlags_Disabled;
				const auto row = SettingsTable::BeginRow(
					owner, "row", "Setting", nullptr);
				require(row.result == DMUI_RESULT_OK && !row.visible,
					"disabled value column did not produce an invisible row");
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"invisible row left the table bracket open");
			}
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"invisible row changed the ImGui stack");
		});

		runner.test("settings bracket early returns remain recoverable", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			constexpr DMUI_ClientHandle other{ 8 };
			ImGuiTestFrame frame;
			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				require(SettingsTable::Begin(owner, "").result ==
						DMUI_RESULT_INVALID_ARGUMENT,
					"invalid table id was accepted");
				auto* window = ImGui::GetCurrentWindow();
				window->SkipItems = true;
				const auto invisibleTable =
					SettingsTable::Begin(owner, "clipped");
				window->SkipItems = false;
				require(invisibleTable.result == DMUI_RESULT_OK &&
						!invisibleTable.visible,
					"invisible table opened a bracket");
				const auto table = SettingsTable::Begin(owner, "settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"settings table did not begin");
				require(SettingsTable::Begin(owner, "nested").result ==
						DMUI_RESULT_UNBALANCED_BRACKET,
					"nested settings table was accepted");
				require(SettingsTable::BeginRow(
							owner, "", "Setting", nullptr).result ==
						DMUI_RESULT_INVALID_ARGUMENT,
					"invalid row id was accepted");
				require(SettingsTable::BeginRow(
							other, "other", "Setting", nullptr).result ==
						DMUI_RESULT_UNBALANCED_BRACKET,
					"foreign owner began a row");
				const auto row = SettingsTable::BeginRow(
					owner, "row", "Setting", nullptr);
				require(row.result == DMUI_RESULT_OK && row.visible,
					"valid row did not begin after an early return");
				require(SettingsTable::BeginRow(
							owner, "nested", "Setting", nullptr).result ==
						DMUI_RESULT_UNBALANCED_BRACKET,
					"nested settings row was accepted");
				require(SettingsTable::End(owner) ==
						DMUI_RESULT_UNBALANCED_BRACKET,
					"settings table ended with an open row");
				bool resetPressed{};
				ImGui::PushID("client-damage");
				require(SettingsTable::EndRow(
							owner, {}, resetPressed) ==
						DMUI_RESULT_UNBALANCED_BRACKET,
					"row ended through client stack damage");
				ImGui::PopID();
				require(SettingsTable::EndRow(
							other, {}, resetPressed) ==
						DMUI_RESULT_UNBALANCED_BRACKET,
					"foreign owner ended the row");
				require(SettingsTable::EndRow(
							owner, {}, resetPressed) ==
						DMUI_RESULT_OK,
					"row could not recover from a rejected end");
				require(SettingsTable::End(other) ==
						DMUI_RESULT_UNBALANCED_BRACKET,
					"foreign owner ended the settings table");
				require(SettingsTable::End(owner) == DMUI_RESULT_OK,
					"settings table did not end");
			}
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"early return changed the ImGui stack");
		});

		runner.test("abandoned settings row is unwound at callback exit", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			ImGuiTestFrame frame;
			{
				const SettingsTable::ClientCallbackGuard guard{ owner };
				const auto table = SettingsTable::Begin(owner, "settings");
				require(table.result == DMUI_RESULT_OK && table.visible,
					"settings table did not begin");
				const auto row = SettingsTable::BeginRow(
					owner, "row", "Setting", nullptr);
				require(row.result == DMUI_RESULT_OK && row.visible,
					"settings row did not begin");
			}
			require(frame.IsAtBaseline() && frame.Errors() == 0,
				"callback recovery reported or retained abandoned brackets");
		});
	}
}
