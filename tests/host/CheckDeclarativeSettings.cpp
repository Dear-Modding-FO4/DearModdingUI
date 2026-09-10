#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/SettingsActions.h>
#include <DearModdingUI/controls/SettingsTable.h>
#include <algorithm>
#include <array>
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

	void run_declarative_settings_checks(Runner& runner)
	{
		runner.test("settings brackets reject mismatched transitions", [] {
			constexpr DMUI_ClientHandle owner{ 7 };
			constexpr DMUI_ClientHandle other{ 8 };
			require(std::string_view{
						DMUI_ResultToString(DMUI_RESULT_UNBALANCED_BRACKET) } ==
					"UNBALANCED_BRACKET",
				"bracket error string was not published");
			SettingsTable::BracketState state;
			require(state.BeginRow(owner) == DMUI_RESULT_UNBALANCED_BRACKET,
				"row began without a table");
			require(state.EndTable(owner) == DMUI_RESULT_UNBALANCED_BRACKET,
				"idle table ended");
			require(state.BeginTable(owner) == DMUI_RESULT_OK,
				"table did not begin");
			require(state.BeginTable(owner) == DMUI_RESULT_UNBALANCED_BRACKET,
				"settings table nested");
			require(state.BeginRow(other) == DMUI_RESULT_UNBALANCED_BRACKET,
				"another owner began a row");
			require(state.BeginRow(owner) == DMUI_RESULT_OK,
				"row did not begin");
			require(state.EndTable(owner) == DMUI_RESULT_UNBALANCED_BRACKET,
				"table ended with an open row");
			require(state.EndRow(other) == DMUI_RESULT_UNBALANCED_BRACKET,
				"another owner ended a row");
			require(state.EndRow(owner) == DMUI_RESULT_OK,
				"row did not end");
			require(state.EndTable(owner) == DMUI_RESULT_OK,
				"table did not end");
			require(state.CurrentPhase() == SettingsTable::Phase::kIdle,
				"balanced table did not return to idle");
			require(state.BeginTable(owner) == DMUI_RESULT_OK &&
					state.BeginRow(owner) == DMUI_RESULT_OK,
				"state could not be reused");
			state.Reset();
			require(state.CurrentPhase() == SettingsTable::Phase::kIdle &&
					state.Owner() == DMUI_INVALID_CLIENT_HANDLE,
				"forced reset retained bracket state");
		});

		runner.test("settings row options enforce their versioned prefix", [] {
			require(SettingsTable::ValidateRowOptions(nullptr) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null row options were accepted");
			DMUI_SettingsRowOptions options{};
			options.structSize = DMUI_SETTINGS_ROW_OPTIONS_0_1_SIZE - 1;
			require(SettingsTable::ValidateRowOptions(&options) ==
					DMUI_RESULT_STRUCT_TOO_SMALL,
				"short row options were accepted");
			options.structSize = DMUI_SETTINGS_ROW_OPTIONS_0_1_SIZE;
			require(SettingsTable::ValidateRowOptions(&options) ==
					DMUI_RESULT_OK,
				"exact row options were rejected");
			options.structSize += sizeof(uint32_t);
			require(SettingsTable::ValidateRowOptions(&options) ==
					DMUI_RESULT_OK,
				"extended row options were rejected");
			DMUI_SettingsRowBeginOptions beginOptions{};
			beginOptions.structSize =
				DMUI_SETTINGS_ROW_BEGIN_OPTIONS_0_1_SIZE - 1;
			require(SettingsTable::ValidateRowBeginOptions(&beginOptions) ==
					DMUI_RESULT_STRUCT_TOO_SMALL,
				"short row begin options were accepted");
			beginOptions.structSize =
				DMUI_SETTINGS_ROW_BEGIN_OPTIONS_0_1_SIZE;
			beginOptions.layout = DMUI_SETTINGS_ROW_LAYOUT_FULL_SPAN;
			require(SettingsTable::ValidateRowBeginOptions(&beginOptions) ==
					DMUI_RESULT_OK,
				"full-span row begin options were rejected");
			beginOptions.layout = 2;
			require(SettingsTable::ValidateRowBeginOptions(&beginOptions) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"unknown row layout was accepted");
		});

		runner.test("declarative setting filters match metadata without reading values", [] {
			const dmui::SettingDescriptor setting{
				.id = "bHighResolution",
				.label = "High Resolution",
				.description = "Increases texture detail for distant objects.",
				.control = dmui::CheckboxSettingControl{},
				.defaultValue = false
			};
			require(
				dmui::MatchesSettingFilter(
					setting,
					"High Resolution",
					false,
					{ "HIGH RES", false }) &&
					dmui::MatchesSettingFilter(
						setting,
						"High Resolution",
						false,
						{ "bhigh", false }) &&
					dmui::MatchesSettingFilter(
						setting,
						"High Resolution",
						false,
						{ "DISTANT OBJECTS", false }),
				"case-insensitive metadata filtering lost a match");
			require(
				!dmui::MatchesSettingFilter(
					setting,
					"High Resolution",
					false,
					{ "shadows", false }) &&
					!dmui::MatchesSettingFilter(
						setting,
						"High Resolution",
						false,
						{ "", true }) &&
					dmui::MatchesSettingFilter(
						setting,
						"High Resolution",
						true,
						{ "", true }),
				"modified-only or negative filtering changed");

			const dmui::SettingGroup group{
				.id = "questions",
				.label = "Questions",
				.settings = {
					{ .id = "first", .label = "First" },
					{ .id = "second", .label = "Second" }
				},
				.rows = {
					dmui::SettingGroup::SettingIndex{ 0 },
					dmui::SettingGroup::DividerRow{},
					dmui::SettingGroup::SettingIndex{ 1 }
				}
			};
			const auto all = dmui::setting_detail::MatchingRows(group, {});
			const auto filtered =
				dmui::setting_detail::MatchingRows(group, { "second" });
			require(
				all.size() == 3 &&
					dmui::setting_detail::MatchingContentCount(all) == 2 &&
					filtered.size() == 1 &&
					dmui::setting_detail::MatchingContentCount(filtered) == 1,
				"divider rows changed heading counts or survived lone filtering");
		});

		runner.test("declarative pending count uses dirty state without value getters", [] {
			auto firstDirty = false;
			auto secondDirty = true;
			auto getterCalls = 0u;
			dmui::SettingsPage page{
				.groups = {
					{
						.id = "general",
						.label = "General",
						.settings = {
							{
								.id = "first",
								.label = "First",
								.control = dmui::CheckboxSettingControl{},
								.defaultValue = false,
								.binding = {
									.get = [&] {
										++getterCalls;
										return dmui::SettingValue{ false };
									}
								},
								.isDirty = [&] { return firstDirty; }
							},
							{
								.id = "second",
								.label = "Second",
								.control = dmui::CheckboxSettingControl{},
								.defaultValue = false,
								.binding = {
									.get = [&] {
										++getterCalls;
										return dmui::SettingValue{ false };
									}
								},
								.isDirty = [&] { return secondDirty; }
							}
						}
					}
				}
			};
			require(
				page.PendingCount() == 1 &&
					page.IsDirty() &&
					getterCalls == 0,
				"pending count read a bound value or lost dirty state");
			firstDirty = true;
			secondDirty = false;
			require(page.PendingCount() == 1 && getterCalls == 0,
				"pending count did not follow dynamic dirty state");
		});

		runner.test("declarative numeric controls select widgets and normalize values", [] {
			const dmui::DoubleSettingControl input;
			const dmui::DoubleSettingControl drag{
				.range = dmui::NumericSettingRange<double>{
					.minimum = 0.0
				}
			};
			const dmui::DoubleSettingControl slider{
				.range = dmui::NumericSettingRange<double>{
					.minimum = 0.0,
					.maximum = 1.0
				},
				.format = "%.2f"
			};
			require(
				dmui::ResolveNumericSettingWidget(input) ==
						dmui::NumericSettingWidget::kInput &&
					dmui::ResolveNumericSettingWidget(drag) ==
						dmui::NumericSettingWidget::kDrag &&
					dmui::ResolveNumericSettingWidget(slider) ==
						dmui::NumericSettingWidget::kSlider,
				"numeric range shape selected the wrong widget");
			require(
				dmui::ClampSettingNumber(
					std::numeric_limits<double>::quiet_NaN(),
					0.25,
					slider.range) == 0.25 &&
					dmui::ClampSettingNumber(2.0, 0.25, slider.range) == 1.0,
				"double recovery or clamping changed");
			require(
				dmui::ClampSettingNumber(
					int64_t{ -50 },
					int64_t{ 5 },
					std::optional{
						dmui::NumericSettingRange<int64_t>{
							.minimum = int64_t{ -10 },
							.maximum = int64_t{ 10 } } }) == -10 &&
					dmui::ClampSettingNumber(
						uint64_t{ 500 },
						uint64_t{ 5 },
						std::optional{
							dmui::NumericSettingRange<uint64_t>{
								.minimum = uint64_t{ 100 },
								.maximum = uint64_t{ 20 } } }) == 100,
				"signed, unsigned, or inverted bounds were not normalized");
			require(
				std::abs(
					dmui::QuantizeSettingNumber(
						0.61,
						std::optional{
							dmui::NumericQuantization<double>{ 0.2, 0.1 } }) -
					0.7) < 1.0e-12 &&
					dmui::QuantizeSettingNumber(
						int64_t{ -4 },
						std::optional{
							dmui::NumericQuantization<int64_t>{ 3, -10 } }) ==
						-4 &&
					dmui::QuantizeSettingNumber(
						uint64_t{ 18 },
						std::optional{
							dmui::NumericQuantization<uint64_t>{ 5, 3 } }) ==
						18,
				"numeric quantization stopped using its explicit origin");
		});

		runner.test("declarative defaults reset through accepted value bindings", [] {
			int64_t draft = 18;
			size_t setterCalls{};
			std::vector<dmui::SettingEditEvent> editEvents;
			auto setting = dmui::SettingDescriptor{
				.id = "threads",
				.label = "Worker threads",
				.control = dmui::SignedSettingControl{},
				.defaultValue = int64_t{ 8 },
				.binding = dmui::BindSetting(
					[&]() -> int64_t { return draft; },
					[&](int64_t a_value) -> int64_t {
						++setterCalls;
						draft = (std::min)(a_value, int64_t{ 16 });
						return draft;
					}),
				.onEdit = [&](const dmui::SettingEditEvent& a_event) {
					editEvents.push_back(a_event);
				}
			};
			static_assert(!std::is_nothrow_invocable_v<
				decltype(setting.binding.get)&>);
			static_assert(!std::is_nothrow_invocable_v<
				decltype(setting.binding.set)&,
				dmui::SettingValue>);
			require(
				!dmui::IsSettingDefault(
					setting,
					dmui::SettingValue{ draft }),
				"modified value was treated as its default");
			const auto reset = dmui::ResetSettingToDefault(setting);
			require(
				reset &&
					std::get<int64_t>(*reset) == 8 &&
					draft == 8 &&
					setterCalls == 1 &&
					editEvents.size() == 1 &&
					editEvents.front().changed &&
					editEvents.front().completed &&
					std::get<int64_t>(editEvents.front().value) == 8 &&
					dmui::IsSettingDefault(
						setting,
						dmui::SettingValue{ draft }),
				"reset did not emit one completed effective edit");
			require(
				dmui::ResetSettingToDefault(setting) &&
					setterCalls == 1 &&
					editEvents.size() == 1,
				"no-op reset wrote or emitted another completion");
			const auto accepted =
				setting.binding.set(dmui::SettingValue{ int64_t{ 99 } });
			require(
				std::get<int64_t>(accepted) == 16 &&
					draft == 16 &&
					setterCalls == 2 &&
					editEvents.size() == 1,
				"setter did not return the accepted clamped value");
			setting.isEnabled = [] { return false; };
			require(
				!dmui::ResetSettingToDefault(setting) &&
					draft == 16 &&
					setterCalls == 2 &&
					editEvents.size() == 1,
				"disabled reset wrote or emitted completion");
		});

		runner.test("unknown declarative controls resolve to a disabled fallback", [] {
			auto setterCalls = 0u;
			const dmui::SettingDescriptor setting{
				.id = "future",
				.label = "Future control",
				.control = dmui::UnsupportedSettingControl{ 0xFFFFu },
				.defaultValue = false,
				.binding = {
					.set = [&](dmui::SettingValue a_value) {
						++setterCalls;
						return a_value;
					}
				}
			};
			const auto presentation =
				dmui::ResolveSettingControlPresentation(setting.control);
			require(
				presentation.kind ==
						dmui::SettingControlKind::kUnsupported &&
					!presentation.supported &&
					!presentation.editable &&
					!presentation.resetVisible &&
					!dmui::SettingValueMatchesControl(
						setting.control,
						setting.defaultValue),
				"unknown kind did not select the noninteractive fallback");
			require(
				!dmui::ResetSettingToDefault(setting) &&
					setterCalls == 0,
				"unknown kind invoked an editable binding");
		});

	}
}
