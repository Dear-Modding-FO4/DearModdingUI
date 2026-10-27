#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/controls/SettingsTable.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
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
			SettingsTable::BracketState state;
			require(state.BeginField(owner) == DMUI_RESULT_UNBALANCED_BRACKET,
				"field began without a table");
			require(state.EndTable(owner) == DMUI_RESULT_UNBALANCED_BRACKET,
				"idle table ended");
			require(state.BeginTable(owner) == DMUI_RESULT_OK,
				"table did not begin");
			require(state.BeginTable(owner) == DMUI_RESULT_UNBALANCED_BRACKET,
				"settings table nested");
			require(state.BeginField(other) == DMUI_RESULT_UNBALANCED_BRACKET,
				"another owner began a field");
			require(state.BeginField(owner) == DMUI_RESULT_OK,
				"field did not begin");
			require(state.EndTable(owner) == DMUI_RESULT_UNBALANCED_BRACKET,
				"table ended with an open field");
			require(state.EndField(other) == DMUI_RESULT_UNBALANCED_BRACKET,
				"another owner ended a field");
			require(state.EndField(owner) == DMUI_RESULT_OK,
				"field did not end");
			require(state.EndTable(owner) == DMUI_RESULT_OK,
				"table did not end");
			require(state.CurrentPhase() == SettingsTable::Phase::kIdle,
				"balanced table did not return to idle");
			require(state.BeginTable(owner) == DMUI_RESULT_OK &&
					state.BeginField(owner) == DMUI_RESULT_OK,
				"state could not be reused");
			state.Reset();
			require(state.CurrentPhase() == SettingsTable::Phase::kIdle &&
					state.Owner() == DMUI_INVALID_CLIENT_HANDLE,
				"forced reset retained bracket state");
		});

		runner.test("field options reject invalid layout and feedback", [] {
			require(SettingsTable::ValidateFieldEndOptions(nullptr) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null field end options were accepted");
			DMUI_FieldEndOptions endOptions{};
			require(SettingsTable::ValidateFieldEndOptions(&endOptions) ==
					DMUI_RESULT_OK,
				"exact field end options were rejected");

			DMUI_FieldBeginOptions fieldOptions{};
			fieldOptions.layout = DMUI_FIELD_LAYOUT_FULL_SPAN;
			require(SettingsTable::ValidateFieldBeginOptions(&fieldOptions) ==
					DMUI_RESULT_OK,
				"full-span field options were rejected");
			fieldOptions.layout = 2;
			require(SettingsTable::ValidateFieldBeginOptions(&fieldOptions) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"unknown field layout was accepted");

			DMUI_FieldFeedback feedback{};
			require(SettingsTable::ValidateFieldFeedback(nullptr) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null field feedback was accepted");
			feedback.severity = DMUI_SEVERITY_ERROR + 1u;
			require(SettingsTable::ValidateFieldFeedback(&feedback) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"unknown field feedback severity was accepted");
			feedback.severity = DMUI_SEVERITY_INFO;
			const std::string oversized(
				FieldFeedback::kMaximumMessageBytes + 1,
				'x');
			feedback.message = oversized.c_str();
			require(SettingsTable::ValidateFieldFeedback(&feedback) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"oversized field feedback was accepted");
			feedback.message = nullptr;
			require(SettingsTable::ValidateFieldFeedback(&feedback) ==
					DMUI_RESULT_OK,
				"explicit field feedback clear was rejected");
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

		runner.test("declarative numeric controls normalize values", [] {
			const dmui::DoubleSettingControl slider{
				.range = dmui::NumericSettingRange<double>{
					.minimum = 0.0,
					.maximum = 1.0
				},
				.format = "%.2f"
			};
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

		runner.test("declarative color settings hold packed RGBA and force opacity without alpha", [] {
			dmui::SettingDescriptor setting{
				.id = "tint",
				.control = dmui::ColorSettingControl{},
				.defaultValue = uint64_t{ 0x112233FFu }
			};
			const auto presentation =
				dmui::ResolveSettingControlPresentation(setting.control);
			require(presentation.kind == dmui::SettingControlKind::kColor &&
					presentation.editable &&
					dmui::SettingValueMatchesControl(
						setting.control, setting.defaultValue) &&
					!dmui::SettingValueMatchesControl(
						setting.control, dmui::SettingValue{ true }),
				"a color control did not resolve to an editable packed value");
			const dmui::SettingValue wide{ uint64_t{ 0x1122334455u } };
			require(std::get<uint64_t>(dmui::NormalizeSettingValue(setting, wide)) ==
					0x223344FFu,
				"an opaque color kept bits above 32 or a partial alpha");
			setting.control = dmui::ColorSettingControl{ .alpha = true };
			require(std::get<uint64_t>(dmui::NormalizeSettingValue(setting, wide)) ==
					0x22334455u,
				"an alpha color lost its alpha byte");
		});

		runner.test("settings page text prefers translations and falls back to English", [] {
			const auto translated = [](const char* a_key, DMUI_Result& a_result)
				-> std::optional<std::string> {
				a_result = DMUI_RESULT_OK;
				if (std::string_view{ a_key } == "$DMUI_SettingsPage_Revert")
					return "Verwerfen";
				a_result = DMUI_RESULT_TEXT_NOT_FOUND;
				return std::nullopt;
			};
			auto isFinal = false;
			auto text = dmui::LoadSettingsPageText(translated, isFinal);
			require(isFinal && text.revert == "Verwerfen" && text.resetAll == "Reset all" &&
					text.apply == "Apply ({})" && text.modifiedOnly == "Modified only",
				"translated and untranslated settings text did not mix per key");

			text = dmui::LoadSettingsPageText(
				[](const char*, DMUI_Result& a_result) -> std::optional<std::string> {
					a_result = DMUI_RESULT_HOST_NOT_READY;
					return std::nullopt;
				},
				isFinal);
			require(!isFinal && text.unavailable == "Unavailable",
				"an unready host was cached instead of retried");
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
