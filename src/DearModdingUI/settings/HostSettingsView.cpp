#include <DearModdingUI/settings/HostSettingsView.h>

#include "HostSettingsColorControls.h"

#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/localization/Localization.h>
#include <DearModdingUI/controls/SettingsTable.h>
#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/presentation/Theme.h>
#include <Platform/settings/GameColors.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <span>
#include <string_view>

namespace DearModdingUI
{
	namespace
	{
		constexpr Localization::Phrase kDefaultGreenText{ "$DMUI_Settings_DefaultGreen", "Default green" };

		using HostSettingsViewDetail::ColorPreset;

		[[nodiscard]] std::array<ColorPreset, 6> AccentPresets() noexcept
		{
			return {
				ColorPreset{
					Localization::Text(kDefaultGreenText),
					Localization::Text(kDefaultGreenText),
					{ 0x42, 0xFA, 0x60 }
				},
				ColorPreset{
					Localization::Text(
						"$DMUI_Settings_AccessibleBlue",
						"Accessible blue"),
					Localization::Text(
						"$DMUI_Settings_OkabeBlue",
						"Okabe-Ito blue, distinguishable across common color-vision deficiencies"),
					{ 0x00, 0x72, 0xB2 }
				},
				ColorPreset{
					Localization::Text(
						"$DMUI_Settings_AccessibleOrange",
						"Accessible orange"),
					Localization::Text(
						"$DMUI_Settings_OkabeOrange",
						"Okabe-Ito orange, distinguishable across common color-vision deficiencies"),
					{ 0xE6, 0x9F, 0x00 }
				},
				ColorPreset{
					Localization::Text(
						"$DMUI_Settings_AccessibleSkyBlue",
						"Accessible sky blue"),
					Localization::Text(
						"$DMUI_Settings_OkabeSkyBlue",
						"Okabe-Ito sky blue, distinguishable across common color-vision deficiencies"),
					{ 0x56, 0xB4, 0xE9 }
				},
				ColorPreset{
					Localization::Text(
						"$DMUI_Settings_AccessibleVermillion",
						"Accessible vermillion"),
					Localization::Text(
						"$DMUI_Settings_OkabeVermillion",
						"Okabe-Ito vermillion, distinguishable across common color-vision deficiencies"),
					{ 0xD5, 0x5E, 0x00 }
				},
				ColorPreset{
					Localization::Text(
						"$DMUI_Settings_AccessiblePurple",
						"Accessible purple"),
					Localization::Text(
						"$DMUI_Settings_OkabePurple",
						"Okabe-Ito purple, distinguishable across common color-vision deficiencies"),
					{ 0xCC, 0x79, 0xA7 }
				}
			};
		}

		HostSettingsDraftState g_settingsDraft;
		uint64_t g_observedPageRevision{ 0 };

		[[nodiscard]] float ControlWidth() noexcept
		{
			return (std::min)(
				ImGui::GetContentRegionAvail().x,
				ImGui::GetFontSize() * 22.0f);
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
			const auto result = SettingsTable::BeginRow(
				DMUI_INVALID_CLIENT_HANDLE,
				a_id,
				a_label,
				a_description);
			if (result.result != DMUI_RESULT_OK || !result.visible)
				return false;

			a_drawValue();
			bool resetPressed{};
			const auto endResult = SettingsTable::EndRow(
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

		[[nodiscard]] bool DrawColorSettingRow(
			const char* a_id,
			const char* a_label,
			const char* a_description,
			HostAccentColor& a_color,
			HostAccentColor a_defaultColor,
			std::span<const ColorPreset> a_presets = {}) noexcept
		{
			auto changed = false;
			if (DrawSettingsRow(
					a_id,
					a_label,
					a_description,
					true,
					[&]() noexcept {
						changed |= HostSettingsViewDetail::
							DrawColorSettingControl(
								a_color,
								a_presets,
								ControlWidth());
					},
					[&]() noexcept {
						return a_color != a_defaultColor;
					}))
			{
				a_color = a_defaultColor;
				changed = true;
			}
			return changed;
		}

		void DrawFeedbackExamples() noexcept
		{
			static size_t selected{};
			constexpr std::array severities{
				DMUI_FIELD_FEEDBACK_SEVERITY_INFO,
				DMUI_FIELD_FEEDBACK_SEVERITY_WARNING,
				DMUI_FIELD_FEEDBACK_SEVERITY_ERROR
			};
			const std::array messages{
				Localization::Text(
					"$DMUI_Settings_FeedbackExampleInfo",
					"Changes apply after the menu reopens."),
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
				SettingsTable::RowLayout::kLabelValue);
			if (begun.result != DMUI_RESULT_OK || !begun.visible)
				return;

			const auto available = (std::max)(ImGui::GetContentRegionAvail().x, 1.0f);
			const auto right = ImGui::GetCursorScreenPos().x + available;
			const auto& style = ImGui::GetStyle();
			for (size_t index = 0; index < severities.size(); ++index)
			{
				const auto* label = FieldFeedback::SeverityLabel(severities[index]);
				const auto width = (std::min)(
					ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f,
					available);
				if (index > 0 &&
					ImGui::GetItemRectMax().x + style.ItemSpacing.x + width <= right)
					ImGui::SameLine();
				auto color = FieldFeedback::SeverityColor(severities[index]);
				ImGui::PushStyleColor(ImGuiCol_Text, color);
				if (selected == index)
				{
					color.w = 0.24f;
					ImGui::PushStyleColor(ImGuiCol_Button, color);
				}
				const auto pressed = ImGui::Button(label, { width, 0.0f });
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
			DrawSectionHeader("$DMUI_Settings_Appearance", "Appearance");
			if (!BeginSettingsSection("##DearModdingUI.AppearanceSettings"))
				return;

			auto& settings = g_settingsDraft.draft;
			const auto& defaults = DefaultSettings();
			auto changed = false;
			const auto accentPresets = AccentPresets();

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
						ImGui::SetNextItemWidth(ControlWidth());
						if (ImGui::BeginCombo(
								"##Value",
								SidebarLayoutLabel(selectedLayout->kind)))
						{
							for (const auto& layout : SIDEBAR_LAYOUTS)
							{
								if (!layout.production)
									continue;
								const auto selected =
									layout.kind == settings.sidebarLayout;
								if (ImGui::Selectable(
										SidebarLayoutLabel(layout.kind),
										selected))
								{
									changed |= CommitSidebarLayout(layout.kind);
								}
								if (ImGui::IsItemHovered())
								{
									ImGui::SetTooltip(
										"%s",
										SidebarLayoutDescription(layout.kind));
								}
								if (selected)
									ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
					},
					[&]() noexcept {
						return settings.sidebarLayout !=
							defaults.sidebarLayout;
					}))
			{
				changed |= CommitSidebarLayout(defaults.sidebarLayout);
			}

			changed |= DrawColorSettingRow(
				"AccentColor",
				Localization::Text(
					"$DMUI_Settings_AccentColor",
					"Accent color"),
				Localization::Text(
					"$DMUI_Settings_AccentColorHelp",
					"Retints selections, controls, links, and every Phosphor menu icon in colored mode."),
				settings.accentColor,
				defaults.accentColor,
				accentPresets);

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
					changed |= HostSettingsViewDetail::DrawGameColorSyncControls(
						settings.accentColor,
						ReadGameColor(GameColorSource::kHUD),
						ReadGameColor(GameColorSource::kPipboy),
						ControlWidth());
				},
				[]() noexcept { return false; });

			if (DrawSettingsRow(
					"IconColorMode",
					Localization::Text(
						"$DMUI_Settings_IconColorMode",
						"Icon color mode"),
					Localization::Text(
						"$DMUI_Settings_IconColorModeHelp",
						"Colored icons use the accent above; monochrome icons use the active text color."),
					true,
					[&]() noexcept {
						auto iconMode =
							settings.iconColorMode ==
									Theme::IconColorMode::kMonochrome ?
							1 :
							0;
						const char* const iconModes[]{
							Localization::Text(
								"$DMUI_Settings_IconModeColored",
								"Colored (accent)"),
							Localization::Text(
								"$DMUI_Settings_IconModeMonochrome",
								"Monochrome (text)")
						};
						ImGui::SetNextItemWidth(ControlWidth());
						if (ImGui::Combo(
								"##Value",
								&iconMode,
								iconModes,
								static_cast<int>(std::size(iconModes))))
						{
							settings.iconColorMode = iconMode == 1 ?
								Theme::IconColorMode::kMonochrome :
								Theme::IconColorMode::kColored;
							changed = true;
						}
					},
					[&]() noexcept {
						return settings.iconColorMode != defaults.iconColorMode;
					}))
			{
				settings.iconColorMode = defaults.iconColorMode;
				changed = true;
			}

			if (DrawSettingsRow(
					"LogoColors",
					Localization::Text(
						"$DMUI_Settings_LogoColors",
						"Logo colors"),
					Localization::Text(
						"$DMUI_Settings_LogoColorsHelp",
						"Uses the original logo colors or the current accent, independently of icon color mode."),
					true,
					[&]() noexcept {
						auto mode = settings.logoColors == LogoColorMode::kAccent ? 1 : 0;
						const char* const modes[]{
							Localization::Text(
								"$DMUI_Settings_LogoOriginalColors",
								"Original colors"),
							Localization::Text(
								"$DMUI_Settings_LogoAccent",
								"Accent")
						};
						ImGui::SetNextItemWidth(ControlWidth());
						if (ImGui::Combo("##Value", &mode, modes, static_cast<int>(std::size(modes))))
						{
							settings.logoColors = mode == 1 ?
								LogoColorMode::kAccent : LogoColorMode::kOriginal;
							changed = true;
						}
					},
					[&]() noexcept {
						return settings.logoColors != defaults.logoColors;
					}))
			{
				settings.logoColors = defaults.logoColors;
				changed = true;
			}

			if (DrawSettingsRow(
					"WindowBackgroundOpacity",
					Localization::Text(
						"$DMUI_Settings_WindowBackgroundOpacity",
						"Window background opacity"),
					Localization::Text(
						"$DMUI_Settings_WindowBackgroundOpacityHelp",
						"Raises or lowers the darkness of the host window without changing client content."),
					true,
					[&]() noexcept {
						auto opacityPercent =
							settings.windowBackgroundOpacity * 100.0f;
						ImGui::SetNextItemWidth(ControlWidth());
						if (ImGui::SliderFloat(
								"##Value",
								&opacityPercent,
								kMinWindowBackgroundOpacity * 100.0f,
								kMaxWindowBackgroundOpacity * 100.0f,
								"%.0f%%",
								ImGuiSliderFlags_AlwaysClamp))
						{
							settings.windowBackgroundOpacity =
								opacityPercent / 100.0f;
							changed = true;
						}
					},
					[&]() noexcept {
						return settings.windowBackgroundOpacity !=
							defaults.windowBackgroundOpacity;
					}))
			{
				settings.windowBackgroundOpacity =
					defaults.windowBackgroundOpacity;
				changed = true;
			}

			changed |= DrawColorSettingRow(
				"PaletteBackgroundColor",
				Localization::Text(
					"$DMUI_Settings_PaletteBackground",
					"Command palette background"),
				Localization::Text(
					"$DMUI_Settings_PaletteBackgroundHelp",
					"Sets the elevated surface color used by the command palette."),
				settings.paletteBackgroundColor,
				defaults.paletteBackgroundColor);

			if (DrawSettingsRow(
					"PaletteBackgroundOpacity",
					Localization::Text(
						"$DMUI_Settings_PaletteOpacity",
						"Command palette opacity"),
					Localization::Text(
						"$DMUI_Settings_PaletteOpacityHelp",
						"Controls how faintly the dimmed host panel shows through the palette."),
					true,
					[&]() noexcept {
						auto paletteOpacityPercent =
							settings.paletteBackgroundOpacity * 100.0f;
						ImGui::SetNextItemWidth(ControlWidth());
						if (ImGui::SliderFloat(
								"##Value",
								&paletteOpacityPercent,
								kMinPaletteBackgroundOpacity * 100.0f,
								kMaxPaletteBackgroundOpacity * 100.0f,
								"%.0f%%",
								ImGuiSliderFlags_AlwaysClamp))
						{
							settings.paletteBackgroundOpacity =
								paletteOpacityPercent / 100.0f;
							changed = true;
						}
					},
					[&]() noexcept {
						return settings.paletteBackgroundOpacity !=
							defaults.paletteBackgroundOpacity;
					}))
			{
				settings.paletteBackgroundOpacity =
					defaults.paletteBackgroundOpacity;
				changed = true;
			}

			if (DrawSettingsRow(
					"BackgroundBlur",
					Localization::Text(
						"$DMUI_Settings_BackgroundBlur",
						"Background blur"),
					Localization::Text(
						"$DMUI_Settings_BackgroundBlurHelp",
						"Blurs the game behind the host window; disabling it avoids the blur passes."),
					true,
					[&]() noexcept {
						changed |= ImGui::Checkbox(
							"##Value",
							&settings.backgroundBlur);
					},
					[&]() noexcept {
						return settings.backgroundBlur != defaults.backgroundBlur;
					}))
			{
				settings.backgroundBlur = defaults.backgroundBlur;
				changed = true;
			}

			if (DrawSettingsRow(
					"BackgroundBlurStrength",
					Localization::Text(
						"$DMUI_Settings_BlurStrength",
						"Blur strength"),
					Localization::Text(
						"$DMUI_Settings_BlurStrengthHelp",
						"Adjusts the per-frame blur sample spread without reallocating graphics resources."),
					true,
					[&]() noexcept {
						ImGui::BeginDisabled(!settings.backgroundBlur);
						ImGui::SetNextItemWidth(ControlWidth());
						if (ImGui::SliderFloat(
								"##Value",
								&settings.backgroundBlurStrength,
								kMinBackgroundBlurStrength,
								kMaxBackgroundBlurStrength,
								"%.2f",
								ImGuiSliderFlags_AlwaysClamp))
							changed = true;
						ImGui::EndDisabled();
					},
					[&]() noexcept {
						return settings.backgroundBlurStrength !=
							defaults.backgroundBlurStrength;
					}))
			{
				settings.backgroundBlurStrength =
					defaults.backgroundBlurStrength;
				changed = true;
			}

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
						ImGui::SetNextItemWidth(ControlWidth());
						if (ImGui::BeginCombo(
								"##Value",
								FieldFeedbackLayoutLabel(selectedFeedbackLayout->kind)))
						{
							for (const auto& layout : FIELD_FEEDBACK_LAYOUTS)
							{
								const auto selected =
									layout.kind == settings.feedbackPlacement;
								if (ImGui::Selectable(
										FieldFeedbackLayoutLabel(layout.kind),
										selected))
								{
									settings.feedbackPlacement = layout.kind;
									changed = true;
								}
								if (ImGui::IsItemHovered())
								{
									ImGui::SetTooltip(
										"%s",
										FieldFeedbackLayoutDescription(layout.kind));
								}
								if (selected)
									ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
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

			changed |= DrawColorSettingRow(
				"FieldFeedbackInfoColor",
				Localization::Text(
					"$DMUI_Settings_FeedbackInfo",
					"Field feedback: info"),
				Localization::Text(
					"$DMUI_Settings_FeedbackInfoHelp",
					"Color used only for informational field feedback."),
				settings.feedbackInfoColor,
				defaults.feedbackInfoColor,
				accentPresets);
			changed |= DrawColorSettingRow(
				"FieldFeedbackWarningColor",
				Localization::Text(
					"$DMUI_Settings_FeedbackWarning",
					"Field feedback: warning"),
				Localization::Text(
					"$DMUI_Settings_FeedbackWarningHelp",
					"Color used only for warning field feedback."),
				settings.feedbackWarningColor,
				defaults.feedbackWarningColor,
				accentPresets);
			changed |= DrawColorSettingRow(
				"FieldFeedbackErrorColor",
				Localization::Text(
					"$DMUI_Settings_FeedbackError",
					"Field feedback: error"),
				Localization::Text(
					"$DMUI_Settings_FeedbackErrorHelp",
					"Color used only for error field feedback."),
				settings.feedbackErrorColor,
				defaults.feedbackErrorColor,
				accentPresets);

			if (changed)
				PreviewDraft();
			DrawFeedbackExamples();
			(void)SettingsTable::End(DMUI_INVALID_CLIENT_HANDLE);
		}

		void DrawReadability() noexcept
		{
			DrawSectionHeader("$DMUI_Settings_Readability", "Readability");
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
						ImGui::SetNextItemWidth(ControlWidth());
						(void)ImGui::SliderFloat(
							"##Value",
							&settings.uiScale,
							Theme::kMinUserScale,
							Theme::kMaxUserScale,
							"%.2fx",
							ImGuiSliderFlags_AlwaysClamp);
					},
					[&]() noexcept {
						return settings.uiScale != defaults.uiScale;
					}))
				settings.uiScale = defaults.uiScale;

			if (DrawSettingsRow(
					"BodyFontFamily",
					Localization::Text(
						"$DMUI_Settings_BodyFontFamily",
						"Body font family (requires Apply)"),
					Localization::Text(
						"$DMUI_Settings_BodyFontFamilyHelp",
						"Lists font-family folders in Data/F4SE/Plugins/DearModdingUI/Fonts; Apply rebuilds the selected family once."),
					true,
					[&]() noexcept {
						const auto& families =
							Theme::AvailableBodyFontFamilies();
						const auto resolvedFamily =
							Theme::ResolveBodyFontFamily(
								settings.bodyFontFamily);
						ImGui::SetNextItemWidth(ControlWidth());
						if (ImGui::BeginCombo(
								"##Value",
								resolvedFamily.data()))
						{
							for (const auto& family : families)
							{
								const auto selected = family == resolvedFamily;
								if (ImGui::Selectable(
										family.c_str(),
										selected))
									settings.bodyFontFamily = family;
								if (selected)
									ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
						const auto effectiveFamily =
							Theme::EffectiveBodyFontFamily();
						ImGui::TextDisabled(
							"%s",
							Localization::Format(
								"$DMUI_Settings_AppliedThisFrame",
								"Applied this frame: {}",
								effectiveFamily).c_str());
					},
					[&]() noexcept {
						return settings.bodyFontFamily !=
							defaults.bodyFontFamily;
					}))
			{
				settings.bodyFontFamily = defaults.bodyFontFamily;
			}
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
			DrawSectionHeader("$DMUI_Settings_InputAndBehavior", "Input and behavior");
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
								if (const auto captured = DrawKeyCapture(
										"##Value", binding.c_str(), ControlWidth(), slot, gamepad))
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
							(void)ImGui::Checkbox(
								"##Value",
								&settings.fallSoulsMode);
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
			const auto actions = Hotkeys::Snapshot();
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
				if (ImGui::IsItemHovered() && ImGui::BeginTooltip())
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
					if (const auto captured = DrawKeyCapture(
							"##Binding", chord.c_str(), -1.0f, slot, true,
							action.id.c_str(), action.registered, warning))
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
			DrawSectionHeader("$DMUI_Settings_HostFacts", "Host facts (read-only)");
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
