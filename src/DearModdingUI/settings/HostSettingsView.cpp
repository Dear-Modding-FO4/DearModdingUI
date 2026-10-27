#include <DearModdingUI/settings/HostSettingsView.h>

#include "HostSettingsColorControls.h"

#include <DearModdingUI/ColorEdit.h>
#include <DearModdingUI/Numeric.h>
#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/controls/TextSpans.h>
#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/localization/Localization.h>
#include <DearModdingUI/controls/SettingsTable.h>
#include <DearModdingUI/controls/Button.h>
#include <DearModdingUI/controls/Choice.h>
#include <DearModdingUI/controls/ColorEdit.h>
#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/controls/KeyCapture.h>
#include <DearModdingUI/controls/Numeric.h>
#include <DearModdingUI/controls/Section.h>
#include <DearModdingUI/controls/Tooltip.h>
#include <DearModdingUI/controls/Toggle.h>
#include <DearModdingUI/host/Status.h>
#include <DearModdingUI/presentation/Theme.h>
#include <DearModdingUI/presentation/ThemePreset.h>
#include <Platform/settings/GameColors.h>
#include <Support/Revision.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace DearModdingUI
{
	namespace
	{
		constexpr Localization::Phrase kDefaultGreenText{ "$DMUI_Settings_DefaultGreen", "Default green" };

		[[nodiscard]] std::array<dmui::ColorPreset, 6> AccentPresets() noexcept
		{
			return {
				dmui::ColorPreset{
					Localization::Text(kDefaultGreenText),
					0x42FA60FFu,
					Localization::Text(kDefaultGreenText)
				},
				dmui::ColorPreset{
					Localization::Text(
						"$DMUI_Settings_AccessibleBlue",
						"Accessible blue"),
					0x0072B2FFu,
					Localization::Text(
						"$DMUI_Settings_OkabeBlue",
						"Okabe-Ito blue, distinguishable across common color-vision deficiencies")
				},
				dmui::ColorPreset{
					Localization::Text(
						"$DMUI_Settings_AccessibleOrange",
						"Accessible orange"),
					0xE69F00FFu,
					Localization::Text(
						"$DMUI_Settings_OkabeOrange",
						"Okabe-Ito orange, distinguishable across common color-vision deficiencies")
				},
				dmui::ColorPreset{
					Localization::Text(
						"$DMUI_Settings_AccessibleSkyBlue",
						"Accessible sky blue"),
					0x56B4E9FFu,
					Localization::Text(
						"$DMUI_Settings_OkabeSkyBlue",
						"Okabe-Ito sky blue, distinguishable across common color-vision deficiencies")
				},
				dmui::ColorPreset{
					Localization::Text(
						"$DMUI_Settings_AccessibleVermillion",
						"Accessible vermillion"),
					0xD55E00FFu,
					Localization::Text(
						"$DMUI_Settings_OkabeVermillion",
						"Okabe-Ito vermillion, distinguishable across common color-vision deficiencies")
				},
				dmui::ColorPreset{
					Localization::Text(
						"$DMUI_Settings_AccessiblePurple",
						"Accessible purple"),
					0xCC79A7FFu,
					Localization::Text(
						"$DMUI_Settings_OkabePurple",
						"Okabe-Ito purple, distinguishable across common color-vision deficiencies")
				}
			};
		}

		HostSettingsDraftState g_settingsDraft;
		uint64_t g_observedPageRevision{ 0 };
		// Game colors change only in game menus, so each page visit reads them once.
		std::optional<HostAccentColor> g_hudColor;
		std::optional<HostAccentColor> g_pipboyColor;
		Addictol::Support::Derived<uint64_t, std::vector<HotkeyActionSnapshot>> g_hotkeys;

		[[nodiscard]] float ControlWidth() noexcept
		{
			return (std::min)(
				ImGui::GetContentRegionAvail().x,
				ImGui::GetFontSize() * 22.0f);
		}

		// Draws the row's dropdown; the chosen index, or -1 when nothing changed.
		[[nodiscard]] int DrawChoiceControl(
			std::span<const DMUI_ChoiceOption> a_options,
			int a_selected,
			const char* a_unmatchedLabel) noexcept
		{
			const auto result = DrawChoice({
				.id = "Value",
				.options = a_options.data(),
				.unmatchedLabel = a_unmatchedLabel,
				.optionCount = static_cast<uint32_t>(a_options.size()),
				.selected = a_selected,
				.width = ControlWidth()
			});
			return result.changed != 0 ? result.selected : -1;
		}

		// Edits a float row as a slider; a_scale shows it in other units, such as
		// percent, and true means the value changed.
		[[nodiscard]] bool DrawSlider(
			float& a_value,
			float a_minimum,
			float a_maximum,
			const char* a_format,
			const char* a_unit = nullptr,
			float a_scale = 1.0f) noexcept
		{
			const auto result = DrawNumeric(dmui::NumericOptions<float>{
				.id = "Value",
				.presentation = dmui::NumericPresentation::kSlider,
				.value = a_value * a_scale,
				.minimum = a_minimum * a_scale,
				.maximum = a_maximum * a_scale,
				.format = a_format,
				.unit = a_unit,
				.clamp = true,
				.width = ControlWidth()
			}.Abi());
			if (result.changed == 0)
				return false;
			a_value = dmui::FromNumericValue<float>(result.value) / a_scale;
			return true;
		}

		// Edits an RGB setting with optional preset swatches; true when it changed.
		[[nodiscard]] bool DrawColorSetting(
			HostAccentColor& a_color,
			std::span<const dmui::ColorPreset> a_presets) noexcept
		{
			const auto result = DrawColorEdit(dmui::ColorEditOptions{
				.id = "Value",
				.rgba = PackRgba(HostAccentToImVec4(a_color)),
				.presets = a_presets,
				.presetsLabel = Localization::Text(
					"$DMUI_Settings_ColorVisionPresets",
					"Color-vision-friendly presets"),
				.width = ControlWidth()
			}.Abi());
			if (result.changed == 0)
				return false;
			a_color = HostAccentFromImVec4(UnpackRgba(result.rgba));
			return true;
		}

		// Edits a row's checkbox in place; true when it was toggled.
		[[nodiscard]] bool DrawBoolSetting(bool& a_value) noexcept
		{
			const auto result = DrawToggle({ .id = "Value", .value = a_value });
			a_value = result.value != 0;
			return result.changed != 0;
		}

		[[nodiscard]] const HostInterfaceSettings& DefaultSettings() noexcept
		{
			static const HostInterfaceSettings settings;
			return settings;
		}

		[[nodiscard]] bool BeginSettingsSection(const char* a_id) noexcept
		{
			const auto result = SettingsTable::Begin(
				DMUI_INVALID_CLIENT_HANDLE,
				a_id);
			return result.result == DMUI_RESULT_OK && result.visible;
		}

		template <class DrawValue, class ResetEnabled>
		[[nodiscard]] bool DrawSettingsRow(
			const char* a_id,
			const char* a_label,
			const char* a_description,
			bool a_resetVisible,
			DrawValue&& a_drawValue,
			ResetEnabled&& a_resetEnabled) noexcept
		{
			const auto result = SettingsTable::BeginField(
				DMUI_INVALID_CLIENT_HANDLE,
				a_id,
				a_label,
				a_description,
				SettingsTable::FieldLayout::kLabelValue);
			if (result.result != DMUI_RESULT_OK || !result.visible)
				return false;

			a_drawValue();
			bool resetPressed{};
			const auto endResult = SettingsTable::EndField(
				DMUI_INVALID_CLIENT_HANDLE,
				{
					a_resetVisible,
					a_resetVisible && a_resetEnabled()
				},
				resetPressed);
			return endResult == DMUI_RESULT_OK && resetPressed;
		}

		void DrawHelp(const char* a_text) noexcept
		{
			const Theme::FontGuard font{ Theme::FontRole::kSubtext };
			ImGui::TextDisabled("%s", a_text);
			ImGui::Spacing();
		}

		void PreviewDraft() noexcept
		{
			HostSettings::SetPreview(
				PreviewHostInterfaceSettings(g_settingsDraft.draft),
				g_observedPageRevision);
			Theme::ApplyStyle();
		}

		void EnsureDraft() noexcept
		{
			const auto revision = HostSettings::PageRevision();
			if (g_settingsDraft.active &&
				g_observedPageRevision == revision)
				return;

			if (g_settingsDraft.active)
				LeaveHostSettingsDraft(g_settingsDraft);
			g_settingsDraft = BeginHostSettingsDraft(
				HostSettings::Current());
			g_observedPageRevision = revision;
			Theme::RefreshThemePresets();
			g_hudColor = ReadGameColor(GameColorSource::kHUD);
			g_pipboyColor = ReadGameColor(GameColorSource::kPipboy);
			PreviewDraft();
		}

		void ApplyDraft() noexcept
		{
			if (!HostSettingsDraftDiffers(g_settingsDraft))
				return;

			if (!HostSettings::Apply(g_settingsDraft.draft))
				return;
			g_settingsDraft = BeginHostSettingsDraft(
				HostSettings::Current());
			PreviewDraft();
		}

		[[nodiscard]] bool CommitSidebarLayout(
			SidebarLayoutKind a_layout) noexcept
		{
			if (!HostSettings::SetSidebarLayout(a_layout))
				return false;
			CommitHostSettingsSidebarLayout(g_settingsDraft, a_layout);
			return true;
		}

		// The control edits the effective value; Reset drops the override.
		template <class T, class DrawControl>
		[[nodiscard]] bool DrawOverrideRow(
			const char* a_id,
			const char* a_label,
			const char* a_description,
			std::optional<T>& a_override,
			const T& a_presetValue,
			DrawControl&& a_drawControl) noexcept
		{
			auto changed = false;
			if (DrawSettingsRow(
					a_id,
					a_label,
					a_description,
					true,
					[&]() noexcept {
						auto value = a_override.value_or(a_presetValue);
						if (a_drawControl(value))
						{
							a_override = std::move(value);
							changed = true;
						}
					},
					[&]() noexcept {
						return a_override.has_value();
					}))
			{
				a_override.reset();
				changed = true;
			}
			return changed;
		}

		// Code fonts may also pick the built-in face.
		void DrawFontFamilyRow(
			const char* a_id,
			const char* a_label,
			const char* a_description,
			std::optional<std::string>& a_override,
			const std::string& a_presetValue,
			Theme::FontRole a_role,
			std::string_view a_fallback,
			bool a_includeBuiltin) noexcept
		{
			(void)DrawOverrideRow(
				a_id,
				a_label,
				a_description,
				a_override,
				a_presetValue,
				[&](std::string& a_family) noexcept {
					const auto families = Theme::AvailableFontFamilies(a_includeBuiltin);
					const std::string resolvedFamily{
						Theme::ResolveFontFamily(a_family, a_fallback) };
					auto edited = false;
					std::vector<DMUI_ChoiceOption> options;
					options.reserve(families.size());
					for (const auto& family : families)
						options.push_back({ family.c_str(), nullptr, 1u, 0u });
					const auto current = std::ranges::find(families, resolvedFamily);
					const auto chosen = DrawChoiceControl(
						options,
						current != families.end() ?
							static_cast<int>(current - families.begin()) :
							-1,
						resolvedFamily.c_str());
					if (chosen >= 0)
					{
						a_family = families[static_cast<size_t>(chosen)];
						edited = true;
					}
					ImGui::TextDisabled(
						"%s",
						Localization::Format(
							"$DMUI_Settings_AppliedThisFrame",
							"Applied this frame: {}",
							Theme::EffectiveFontFamily(a_role)).c_str());
					return edited;
				});
		}

		void DrawFeedbackExamples() noexcept
		{
			static size_t selected{};
			constexpr std::array severities{
				DMUI_SEVERITY_INFO,
				DMUI_SEVERITY_SUCCESS,
				DMUI_SEVERITY_WARNING,
				DMUI_SEVERITY_ERROR
			};
			const std::array messages{
				Localization::Text(
					"$DMUI_Settings_FeedbackExampleInfo",
					"Changes apply after the menu reopens."),
				Localization::Text(
					"$DMUI_Settings_FeedbackExampleSuccess",
					"Saved to the active profile."),
				Localization::Text(
					"$DMUI_Settings_FeedbackExampleWarning",
					"This option may affect gameplay timing."),
				Localization::Text(
					"$DMUI_Settings_FeedbackExampleError",
					"Minimum cannot exceed maximum.")
			};
			const auto begun = SettingsTable::BeginField(
				DMUI_INVALID_CLIENT_HANDLE,
				"FeedbackPreview",
				Localization::Text(
					"$DMUI_Settings_FeedbackPreview",
					"Field feedback preview"),
				nullptr,
				SettingsTable::FieldLayout::kLabelValue);
			if (begun.result != DMUI_RESULT_OK || !begun.visible)
				return;

			const auto available = (std::max)(ImGui::GetContentRegionAvail().x, 1.0f);
			const auto right = ImGui::GetCursorScreenPos().x + available;
			const auto& style = ImGui::GetStyle();
			for (size_t index = 0; index < severities.size(); ++index)
			{
				const auto* label = StatusSeverityLabel(severities[index]);
				const auto width = (std::min)(
					MeasureButton({ .label = label }).x,
					available);
				if (index > 0 &&
					ImGui::GetItemRectMax().x + style.ItemSpacing.x + width <= right)
					ImGui::SameLine();
				auto color = Theme::StatusTextColor(severities[index]);
				ImGui::PushStyleColor(ImGuiCol_Text, color);
				if (selected == index)
				{
					color.w = 0.24f;
					ImGui::PushStyleColor(ImGuiCol_Button, color);
				}
				const auto pressed = DrawButton({
					.label = label,
					.size = { width, 0.0f }
				});
				ImGui::PopStyleColor(selected == index ? 2 : 1);
				if (pressed)
					selected = index;
			}
			(void)SettingsTable::SetFieldFeedback(
				DMUI_INVALID_CLIENT_HANDLE, severities[selected], messages[selected]);
			bool ignored{};
			(void)SettingsTable::EndField(
				DMUI_INVALID_CLIENT_HANDLE, { false, false }, ignored);
		}

		void DrawAppearance() noexcept
		{
			DrawLocalizedSection("$DMUI_Settings_Appearance", "Appearance");
			if (!BeginSettingsSection("##DearModdingUI.AppearanceSettings"))
				return;

			auto& settings = g_settingsDraft.draft;
			auto& appearance = settings.appearance;
			const auto& defaults = DefaultSettings();
			auto changed = false;
			const auto accentPresets = AccentPresets();
			const auto preset = Theme::ResolveThemePreset(settings.themePreset);
			const auto presetValues = Theme::DescribeAppearance(preset->spec);

			if (DrawSettingsRow(
					"ThemePreset",
					Localization::Text(
						"$DMUI_Settings_ThemePreset",
						"Theme preset"),
					Localization::Text(
						"$DMUI_Settings_ThemePresetHelp",
						"Lists theme files in Data/F4SE/Plugins/DearModdingUI/Themes. The settings below override the preset; Reset returns a row to it."),
					true,
					[&]() noexcept {
						const auto& presets = Theme::AvailableThemePresets();
						const auto selected = std::ranges::find(
							presets,
							settings.themePreset,
							&Theme::ThemePresetInfo::id);
						std::vector<DMUI_ChoiceOption> options;
						options.reserve(presets.size());
						for (const auto& entry : presets)
							options.push_back({ entry.name.c_str(), nullptr, 1u, 0u });
						const auto chosen = DrawChoiceControl(
							options,
							selected != presets.end() ?
								static_cast<int>(selected - presets.begin()) :
								-1,
							settings.themePreset.c_str());
						if (chosen >= 0)
						{
							settings.themePreset = presets[static_cast<size_t>(chosen)].id;
							changed = true;
						}
						if (!preset->failure.empty() || !preset->problems.empty())
						{
							const auto health = Theme::ClassifyThemePresetHealth(
								settings.themePreset,
								*preset);
							(void)DrawTextSpans(
								health.reason,
								dmui::TextStyle{}.Tone(dmui::TextTone::kWarning),
								{ .wrap = true });
						}
					},
					[&]() noexcept {
						return settings.themePreset != defaults.themePreset;
					}))
			{
				settings.themePreset = defaults.themePreset;
				changed = true;
			}

			const auto* selectedLayout =
				FindUserSidebarLayout(settings.sidebarLayout);
			if (!selectedLayout)
				selectedLayout = FindUserSidebarLayout(DEFAULT_SIDEBAR_LAYOUT);
			if (DrawSettingsRow(
					"SidebarLayout",
					Localization::Text(
						"$DMUI_Settings_SidebarLayout",
						"Sidebar layout (saved immediately)"),
					SidebarLayoutDescription(selectedLayout->kind),
					true,
					[&]() noexcept {
						std::vector<DMUI_ChoiceOption> options;
						std::vector<SidebarLayoutKind> kinds;
						for (const auto& layout : SIDEBAR_LAYOUTS)
						{
							if (!layout.production)
								continue;
							options.push_back({
								SidebarLayoutLabel(layout.kind),
								SidebarLayoutDescription(layout.kind),
								1u,
								0u
							});
							kinds.push_back(layout.kind);
						}
						const auto current = std::ranges::find(kinds, settings.sidebarLayout);
						const auto chosen = DrawChoiceControl(
							options,
							current != kinds.end() ?
								static_cast<int>(current - kinds.begin()) :
								-1,
							SidebarLayoutLabel(selectedLayout->kind));
						if (chosen >= 0)
							changed |= CommitSidebarLayout(kinds[static_cast<size_t>(chosen)]);
					},
					[&]() noexcept {
						return settings.sidebarLayout !=
							defaults.sidebarLayout;
					}))
			{
				changed |= CommitSidebarLayout(defaults.sidebarLayout);
			}

			changed |= DrawOverrideRow(
				"AccentColor",
				Localization::Text(
					"$DMUI_Settings_AccentColor",
					"Accent color"),
				Localization::Text(
					"$DMUI_Settings_AccentColorHelp",
					"Retints selections, controls, links, and every Phosphor menu icon in colored mode."),
				appearance.accentColor,
				*presetValues.accentColor,
				[&](HostAccentColor& a_color) noexcept {
					return DrawColorSetting(a_color, accentPresets);
				});

			(void)DrawSettingsRow(
				"SyncGameColor",
				Localization::Text(
					"$DMUI_Settings_SyncAccent",
					"Sync accent with game"),
				Localization::Text(
					"$DMUI_Settings_SyncAccentHelp",
					"Copies the current HUD or Pip-Boy color once. Use Apply to save or Revert to discard."),
				false,
				[&]() noexcept {
					auto accent = appearance.accentColor.value_or(
						*presetValues.accentColor);
					if (HostSettingsViewDetail::DrawGameColorSyncControls(
							accent,
							g_hudColor,
							g_pipboyColor,
							ControlWidth()))
					{
						appearance.accentColor = accent;
						changed = true;
					}
				},
				[]() noexcept { return false; });

			changed |= DrawOverrideRow(
				"IconColorMode",
				Localization::Text(
					"$DMUI_Settings_IconColorMode",
					"Icon color mode"),
				Localization::Text(
					"$DMUI_Settings_IconColorModeHelp",
					"Colored icons use the accent above; monochrome icons use the active text color."),
				appearance.iconColorMode,
				*presetValues.iconColorMode,
				[&](Theme::IconColorMode& a_mode) noexcept {
					const std::array<DMUI_ChoiceOption, 2> iconModes{ {
						{
							Localization::Text(
								"$DMUI_Settings_IconModeColored",
								"Colored (accent)"),
							nullptr, 1u, 0u
						},
						{
							Localization::Text(
								"$DMUI_Settings_IconModeMonochrome",
								"Monochrome (text)"),
							nullptr, 1u, 0u
						}
					} };
					const auto chosen = DrawChoiceControl(
						iconModes,
						a_mode == Theme::IconColorMode::kMonochrome ? 1 : 0,
						nullptr);
					if (chosen < 0)
						return false;
					a_mode = chosen == 1 ?
						Theme::IconColorMode::kMonochrome :
						Theme::IconColorMode::kColored;
					return true;
				});

			changed |= DrawOverrideRow(
				"LogoColors",
				Localization::Text(
					"$DMUI_Settings_LogoColors",
					"Logo colors"),
				Localization::Text(
					"$DMUI_Settings_LogoColorsHelp",
					"Uses the original logo colors or the current accent, independently of icon color mode."),
				appearance.logoColors,
				*presetValues.logoColors,
				[&](LogoColorMode& a_mode) noexcept {
					const std::array<DMUI_ChoiceOption, 2> modes{ {
						{
							Localization::Text(
								"$DMUI_Settings_LogoOriginalColors",
								"Original colors"),
							nullptr, 1u, 0u
						},
						{
							Localization::Text(
								"$DMUI_Settings_LogoAccent",
								"Accent"),
							nullptr, 1u, 0u
						}
					} };
					const auto chosen = DrawChoiceControl(
						modes,
						a_mode == LogoColorMode::kAccent ? 1 : 0,
						nullptr);
					if (chosen < 0)
						return false;
					a_mode = chosen == 1 ?
						LogoColorMode::kAccent : LogoColorMode::kOriginal;
					return true;
				});

			changed |= DrawOverrideRow(
				"WindowBackgroundOpacity",
				Localization::Text(
					"$DMUI_Settings_WindowBackgroundOpacity",
					"Window background opacity"),
				Localization::Text(
					"$DMUI_Settings_WindowBackgroundOpacityHelp",
					"Raises or lowers the darkness of the host window without changing client content."),
				appearance.windowBackgroundOpacity,
				*presetValues.windowBackgroundOpacity,
				[&](float& a_opacity) noexcept {
					return DrawSlider(
						a_opacity,
						kMinWindowBackgroundOpacity,
						kMaxWindowBackgroundOpacity,
						"%.0f",
						"%",
						100.0f);
				});

			changed |= DrawOverrideRow(
				"PaletteBackgroundColor",
				Localization::Text(
					"$DMUI_Settings_PaletteBackground",
					"Command palette background"),
				Localization::Text(
					"$DMUI_Settings_PaletteBackgroundHelp",
					"Sets the elevated surface color used by the command palette."),
				appearance.paletteBackgroundColor,
				*presetValues.paletteBackgroundColor,
				[&](HostPaletteColor& a_color) noexcept {
					return DrawColorSetting(a_color, {});
				});

			changed |= DrawOverrideRow(
				"PaletteBackgroundOpacity",
				Localization::Text(
					"$DMUI_Settings_PaletteOpacity",
					"Command palette opacity"),
				Localization::Text(
					"$DMUI_Settings_PaletteOpacityHelp",
					"Controls how faintly the dimmed host panel shows through the palette."),
				appearance.paletteBackgroundOpacity,
				*presetValues.paletteBackgroundOpacity,
				[&](float& a_opacity) noexcept {
					return DrawSlider(
						a_opacity,
						kMinPaletteBackgroundOpacity,
						kMaxPaletteBackgroundOpacity,
						"%.0f",
						"%",
						100.0f);
				});

			changed |= DrawOverrideRow(
				"BackgroundBlur",
				Localization::Text(
					"$DMUI_Settings_BackgroundBlur",
					"Background blur"),
				Localization::Text(
					"$DMUI_Settings_BackgroundBlurHelp",
					"Blurs the game behind the host window; disabling it avoids the blur passes."),
				appearance.backgroundBlur,
				*presetValues.backgroundBlur,
				[&](bool& a_blur) noexcept {
					return DrawBoolSetting(a_blur);
				});

			const auto blurEnabled = appearance.backgroundBlur.value_or(
				*presetValues.backgroundBlur);
			changed |= DrawOverrideRow(
				"BackgroundBlurStrength",
				Localization::Text(
					"$DMUI_Settings_BlurStrength",
					"Blur strength"),
				Localization::Text(
					"$DMUI_Settings_BlurStrengthHelp",
					"Adjusts the per-frame blur sample spread without reallocating graphics resources."),
				appearance.backgroundBlurStrength,
				*presetValues.backgroundBlurStrength,
				[&](float& a_strength) noexcept {
					ImGui::BeginDisabled(!blurEnabled);
					const auto edited = DrawSlider(
						a_strength,
						kMinBackgroundBlurStrength,
						kMaxBackgroundBlurStrength,
						"%.2f");
					ImGui::EndDisabled();
					return edited;
				});

			const auto* selectedFeedbackLayout =
				FindFieldFeedbackLayout(settings.feedbackPlacement);
			if (!selectedFeedbackLayout)
				selectedFeedbackLayout = FindFieldFeedbackLayout(
					DEFAULT_FIELD_FEEDBACK_LAYOUT);
			if (DrawSettingsRow(
					"FieldFeedbackLayout",
					Localization::Text(
						"$DMUI_Settings_FeedbackPlacement",
						"Field feedback placement"),
					FieldFeedbackLayoutDescription(selectedFeedbackLayout->kind),
					true,
					[&]() noexcept {
						std::vector<DMUI_ChoiceOption> options;
						for (const auto& layout : FIELD_FEEDBACK_LAYOUTS)
						{
							options.push_back({
								FieldFeedbackLayoutLabel(layout.kind),
								FieldFeedbackLayoutDescription(layout.kind),
								1u,
								0u
							});
						}
						const auto current = std::ranges::find(
							FIELD_FEEDBACK_LAYOUTS,
							settings.feedbackPlacement,
							&decltype(FIELD_FEEDBACK_LAYOUTS)::value_type::kind);
						const auto chosen = DrawChoiceControl(
							options,
							current != FIELD_FEEDBACK_LAYOUTS.end() ?
								static_cast<int>(current - FIELD_FEEDBACK_LAYOUTS.begin()) :
								-1,
							FieldFeedbackLayoutLabel(selectedFeedbackLayout->kind));
						if (chosen >= 0)
						{
							settings.feedbackPlacement =
								FIELD_FEEDBACK_LAYOUTS[static_cast<size_t>(chosen)].kind;
							changed = true;
						}
					},
					[&]() noexcept {
						return settings.feedbackPlacement !=
							defaults.feedbackPlacement;
					}))
			{
				settings.feedbackPlacement = defaults.feedbackPlacement;
				changed = true;
			}

			if (changed)
				PreviewDraft();
			DrawFeedbackExamples();
			(void)SettingsTable::End(DMUI_INVALID_CLIENT_HANDLE);
		}

		void DrawReadability() noexcept
		{
			DrawLocalizedSection("$DMUI_Settings_Readability", "Readability");
			if (!BeginSettingsSection("##DearModdingUI.ReadabilitySettings"))
				return;

			auto& settings = g_settingsDraft.draft;
			const auto& defaults = DefaultSettings();

			if (DrawSettingsRow(
					"UiScale",
					Localization::Text(
						"$DMUI_Settings_UiScale",
						"UI scale (requires Apply)"),
					Localization::Text(
						"$DMUI_Settings_UiScaleHelp",
						"Multiplies resolution-derived sizing; Apply rebuilds typography once before the next frame."),
					true,
					[&]() noexcept {
						(void)DrawSlider(
							settings.uiScale,
							Theme::kMinUserScale,
							Theme::kMaxUserScale,
							"%.2f",
							"x");
					},
					[&]() noexcept {
						return settings.uiScale != defaults.uiScale;
					}))
				settings.uiScale = defaults.uiScale;

			const auto preset = Theme::ResolveThemePreset(settings.themePreset);
			const auto presetValues = Theme::DescribeAppearance(preset->spec);
			DrawFontFamilyRow(
				"BodyFontFamily",
				Localization::Text(
					"$DMUI_Settings_BodyFontFamily",
					"Body font family (requires Apply)"),
				Localization::Text(
					"$DMUI_Settings_BodyFontFamilyHelp",
					"Lists font-family folders in Data/F4SE/Plugins/DearModdingUI/Fonts; Apply rebuilds the selected family once."),
				settings.appearance.bodyFontFamily,
				*presetValues.bodyFontFamily,
				Theme::FontRole::kBody,
				kDefaultBodyFontFamily,
				false);
			DrawFontFamilyRow(
				"MonospaceFontFamily",
				Localization::Text(
					"$DMUI_Settings_MonospaceFontFamily",
					"Monospace font family (requires Apply)"),
				Localization::Text(
					"$DMUI_Settings_MonospaceFontFamilyHelp",
					"Used for code and logs. Lists the same font-family folders plus the built-in face; Apply rebuilds the selected family once."),
				settings.appearance.monospaceFontFamily,
				*presetValues.monospaceFontFamily,
				Theme::FontRole::kMonospace,
				kDefaultMonospaceFontFamily,
				true);
			(void)SettingsTable::End(DMUI_INVALID_CLIENT_HANDLE);
		}

		[[nodiscard]] const char* HotkeyWarning(
			DMUI_HotkeyBindingState a_state) noexcept
		{
			switch (a_state)
			{
			case DMUI_HOTKEY_BINDING_UNBOUND_DEFAULT_CONFLICT:
				return Localization::Text(
						"$DMUI_Settings_HotkeyConflictDefault",
						"Conflict: suggested default is already assigned.");
			case DMUI_HOTKEY_BINDING_UNBOUND_OVERRIDE_CONFLICT:
				return Localization::Text(
						"$DMUI_Settings_HotkeyConflictOverride",
						"Conflict: saved binding is assigned to another action or menu toggle.");
			case DMUI_HOTKEY_BINDING_UNBOUND_INVALID_OVERRIDE:
				return Localization::Text(
						"$DMUI_Settings_HotkeyInvalidOverride",
						"Invalid: saved binding is not recognized for this input device.");
			default:
				return nullptr;
			}
		}

		void DrawInput() noexcept
		{
			DrawLocalizedSection("$DMUI_Settings_InputAndBehavior", "Input and behavior");
			DrawHelp(Localization::Text("$DMUI_Settings_TypingNeedsKeyboard", "Typing needs a keyboard."));
			auto& settings = g_settingsDraft.draft;
			const auto& defaults = DefaultSettings();
			if (BeginSettingsSection("##DearModdingUI.InputSettings"))
			{
				for (const auto slot : kHotkeySlots)
				{
					const auto gamepad = slot == HotkeySlot::kGamepad;
					auto& binding = gamepad ? settings.menuToggleGamepad : settings.menuToggleKey;
					const auto& defaultBinding = gamepad ? defaults.menuToggleGamepad : defaults.menuToggleKey;
					if (DrawSettingsRow(
							gamepad ? "MenuToggleGamepad" : "MenuToggleKey",
							gamepad ?
								Localization::Text("$DMUI_Settings_MenuToggleController", "Menu toggle (controller)") :
								Localization::Text("$DMUI_Settings_MenuToggleKeyboard", "Menu toggle (keyboard)"),
							Localization::Text(
								"$DMUI_Settings_MenuToggleHelp",
								"Opens and closes the shared menu. Apply saves the binding for this session and future launches."),
							true,
							[&]() noexcept {
								if (const auto captured = DrawKeyCapture({
											.id = "Value",
											.binding = binding.c_str(),
											.slot = slot,
											.allowClear = gamepad,
											.width = ControlWidth()
										}))
									binding = SerializeHotkeyChord(*captured);
							},
							[&]() noexcept { return binding != defaultBinding; }))
						binding = defaultBinding;
				}
				if (DrawSettingsRow(
						"FallSoulsMode",
						Localization::Text(
							"$DMUI_Settings_FallSoulsMode",
							"FallSouls mode"),
						Localization::Text(
							"$DMUI_Settings_FallSoulsModeHelp",
							"Keep the game running while this menu is open. Applies the next time the menu opens."),
						true,
						[&]() noexcept {
							(void)DrawBoolSetting(settings.fallSoulsMode);
						},
						[&]() noexcept {
							return settings.fallSoulsMode !=
								defaults.fallSoulsMode;
						}))
				{
					settings.fallSoulsMode = defaults.fallSoulsMode;
				}
				(void)SettingsTable::End(DMUI_INVALID_CLIENT_HANDLE);
			}

			ImGui::Spacing();
			ImGui::TextUnformatted(Localization::Text("$DMUI_Settings_ClientHotkeys", "Client hotkeys"));
			DrawHelp(
				Localization::Text(
					"$DMUI_Settings_ClientHotkeysHelp",
					"Bindings are owned by DearModdingUI. Changes below are saved immediately."));
			const auto& actions = g_hotkeys.Get(Hotkeys::Revision(), &Hotkeys::Snapshot);
			if (actions.empty())
			{
				ImGui::TextDisabled(
					"%s",
					Localization::Text(
						"$DMUI_Settings_NoClientActions",
						"No client actions or saved overrides."));
				return;
			}

			if (!ImGui::BeginTable(
					"##DearModdingUI.Hotkeys",
					3,
					ImGuiTableFlags_BordersInnerH |
						ImGuiTableFlags_RowBg |
						ImGuiTableFlags_SizingStretchProp))
				return;
			const std::array columns{
				Localization::Text("$DMUI_Settings_HotkeyColumnAction", "Action"),
				Localization::Text("$DMUI_Settings_HotkeyColumnKeyboard", "Keyboard / mouse"),
				Localization::Text("$DMUI_Settings_HotkeyColumnController", "Controller")
			};
			ImGui::TableSetupColumn(columns[0], ImGuiTableColumnFlags_WidthStretch, 1.5f);
			ImGui::TableSetupColumn(columns[1], ImGuiTableColumnFlags_WidthStretch, 1.15f);
			ImGui::TableSetupColumn(columns[2], ImGuiTableColumnFlags_WidthStretch, 1.15f);
			ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(0, 0, 0, 0));
			for (const auto* label : columns)
			{
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", label);
			}
			for (const auto& action : actions)
			{
				ImGui::PushID(action.id.c_str());
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				const auto rowStart = ImGui::GetCursorScreenPos();
				ImGui::AlignTextToFramePadding();
				if (action.registered)
					ImGui::TextUnformatted(action.displayName.c_str());
				else
					ImGui::TextDisabled("%s", action.id.c_str());
				if (Tooltip::BeginItem())
				{
					ImGui::TextUnformatted(action.id.c_str());
					if (!action.registered)
						ImGui::TextUnformatted(Localization::Text(
							"$DMUI_Settings_NotRegisteredClearable",
							"Not registered. Saved bindings can be cleared."));
					ImGui::EndTooltip();
				}

				for (const auto slot : kHotkeySlots)
				{
					const auto index = static_cast<size_t>(slot);
					ImGui::TableSetColumnIndex(static_cast<int>(index) + 1);
					const auto& binding = action.bindings[index];
					ImGui::PushID(static_cast<int>(slot));
					const auto& chord = binding.state == DMUI_HOTKEY_BINDING_BOUND ?
						binding.effectiveChord :
						binding.state == DMUI_HOTKEY_BINDING_UNBOUND_DEFAULT_CONFLICT ?
							action.suggestedDefaultChord : binding.overrideChord;
					const auto* warning = !action.registered && !chord.empty() ?
						Localization::Text(
							"$DMUI_Settings_NotRegisteredInactive",
							"Not registered. This saved binding is inactive and can be cleared.") :
						HotkeyWarning(binding.state);
					if (const auto captured = DrawKeyCapture({
							.id = "Binding",
							.binding = chord.c_str(),
							.details = action.id.c_str(),
							.warning = warning,
							.slot = slot,
							.readOnly = !action.registered
						}))
					{
						if (action.registered)
							(void)HostSettings::SetHotkeyOverride(
								action.id, SerializeHotkeyChord(*captured), slot);
						else
							(void)HostSettings::RemoveHotkeyOverride(action.id, slot);
					}
					ImGui::PopID();
				}
				if (ImGui::IsWindowHovered() &&
					ImGui::IsMouseHoveringRect(rowStart, ImGui::GetItemRectMax()))
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
						ImGui::GetColorU32(ImGuiCol_HeaderHovered));
				ImGui::PopID();
			}
			ImGui::EndTable();
		}

		void DrawReadOnlyHostFact(
			const char* a_id,
			const char* a_label,
			const char* a_value,
			const char* a_source) noexcept
		{
			(void)DrawSettingsRow(
				a_id,
				a_label,
				a_source,
				false,
				[&]() noexcept {
					ImGui::TextUnformatted(a_value);
				},
				[]() noexcept {
					return false;
				});
		}

		void DrawReadOnlyFacts() noexcept
		{
			DrawLocalizedSection("$DMUI_Settings_HostFacts", "Host facts (read-only)");
			ImGui::TextDisabled(
				"%s",
				Localization::Text(
					"$DMUI_Settings_HostFactsHelp",
					"Values resolved by the DearModdingUI host."));
			ImGui::Spacing();
			if (!BeginSettingsSection("##DearModdingUI.HostFacts"))
				return;

			const auto typography = Localization::Format(
				"$DMUI_Settings_PixelSize",
				"{:.0f} px",
				Theme::FontSize(Theme::FontRole::kBody));
			DrawReadOnlyHostFact(
				"ResolvedTypographySize",
				Localization::Text(
					"$DMUI_Settings_ResolvedTypographySize",
					"Resolved typography size"),
				typography.c_str(),
				Localization::Text(
					"$DMUI_Settings_ResolvedTypographySizeHelp",
					"Derived from the backbuffer height and the applied UI scale at a frame boundary."));

			char scale[32]{};
			std::snprintf(
				scale,
				sizeof(scale),
				"%.2fx",
				Theme::Scale());
			DrawReadOnlyHostFact(
				"EffectiveUiScale",
				Localization::Text(
					"$DMUI_Settings_EffectiveUiScale",
					"Effective UI scale"),
				scale,
				Localization::Text(
					"$DMUI_Settings_EffectiveUiScaleHelp",
					"Derived from resolution and [Additional] fMenuUiScale."));
			(void)SettingsTable::End(DMUI_INVALID_CLIENT_HANDLE);
		}
	}

	bool HostSettingsTitleActionEnabled(SettingsAction a_action) noexcept
	{
		EnsureDraft();
		return SettingsActionEnabled(
			a_action,
			HostSettingsDraftDiffers(g_settingsDraft));
	}

	void InvokeHostSettingsTitleAction(
		SettingsAction a_action) noexcept
	{
		EnsureDraft();
		switch (a_action)
		{
		case SettingsAction::kApply:
			ApplyDraft();
			break;
		case SettingsAction::kRevert:
			RevertHostSettingsDraft(g_settingsDraft);
			PreviewDraft();
			break;
		case SettingsAction::kReset:
		{
			const auto committedLayout =
				g_settingsDraft.committed.sidebarLayout;
			ResetHostSettingsDraft(g_settingsDraft);
			if (!CommitSidebarLayout(g_settingsDraft.draft.sidebarLayout))
				g_settingsDraft.draft.sidebarLayout = committedLayout;
			PreviewDraft();
			break;
		}
		}
	}

	void DrawHostSettingsControls() noexcept
	{
		EnsureDraft();
		DrawAppearance();
		Theme::SectionSpacing();
		DrawReadability();
		Theme::SectionSpacing();
		DrawInput();
		Theme::SectionSpacing();
		DrawReadOnlyFacts();
	}
}
