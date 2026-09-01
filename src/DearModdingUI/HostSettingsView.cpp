#include <DearModdingUI/HostSettingsView.h>

#include <DearModdingUI/HostSettings.h>
#include <DearModdingUI/Hotkeys.h>
#include <DearModdingUI/SettingsTable.h>
#include <DearModdingUI/Shell.h>
#include <DearModdingUI/Theme.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <string_view>

namespace DearModdingUI
{
	namespace
	{
		struct AccentPreset
		{
			const char* name;
			const char* description;
			HostAccentColor color;
		};

		inline constexpr std::array kAccentPresets{
			AccentPreset{
				"Default green",
				"Default green",
				{ 0x42, 0xFA, 0x60 }
			},
			AccentPreset{
				"Accessible blue",
				"Okabe-Ito blue, distinguishable across common color-vision deficiencies",
				{ 0x00, 0x72, 0xB2 }
			},
			AccentPreset{
				"Accessible orange",
				"Okabe-Ito orange, distinguishable across common color-vision deficiencies",
				{ 0xE6, 0x9F, 0x00 }
			},
			AccentPreset{
				"Accessible sky blue",
				"Okabe-Ito sky blue, distinguishable across common color-vision deficiencies",
				{ 0x56, 0xB4, 0xE9 }
			},
			AccentPreset{
				"Accessible vermillion",
				"Okabe-Ito vermillion, distinguishable across common color-vision deficiencies",
				{ 0xD5, 0x5E, 0x00 }
			},
			AccentPreset{
				"Accessible purple",
				"Okabe-Ito purple, distinguishable across common color-vision deficiencies",
				{ 0xCC, 0x79, 0xA7 }
			}
		};
		HostSettingsDraftState g_settingsDraft;
		uint64_t g_observedPanelRevision{ 0 };

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
				g_observedPanelRevision);
			Theme::ApplyStyle();
		}

		void EnsureDraft() noexcept
		{
			const auto revision = HostSettings::PanelRevision();
			if (g_settingsDraft.active &&
				g_observedPanelRevision == revision)
				return;

			if (g_settingsDraft.active)
				LeaveHostSettingsDraft(g_settingsDraft);
			g_settingsDraft = BeginHostSettingsDraft(
				HostSettings::Current());
			g_observedPanelRevision = revision;
			PreviewDraft();
		}

		void ApplyDraft() noexcept
		{
			const auto committed = g_settingsDraft.committed;
			auto commit = ApplyHostSettingsDraft(g_settingsDraft);
			if (!commit.settings)
				return;

			if (!HostSettings::Apply(*commit.settings))
			{
				g_settingsDraft.committed = committed;
				return;
			}
			g_settingsDraft = BeginHostSettingsDraft(
				HostSettings::Current());
			PreviewDraft();
		}

		void DrawAccentPresets(
			HostInterfaceSettings& a_settings,
			bool& a_changed) noexcept
		{
			ImGui::TextUnformatted("Color-vision-friendly presets");
			const auto swatchSize = ImGui::GetFrameHeight();
			for (size_t index = 0; index < kAccentPresets.size(); ++index)
			{
				const auto& preset = kAccentPresets[index];
				ImGui::PushID(static_cast<int>(index));
				if (index > 0)
					ImGui::SameLine();
				if (ImGui::ColorButton(
						preset.name,
						HostAccentToImVec4(preset.color),
						ImGuiColorEditFlags_NoAlpha,
						{ swatchSize, swatchSize }))
				{
					a_settings.accentColor = preset.color;
					a_changed = true;
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", preset.description);
				ImGui::PopID();
			}
		}

		void DrawAppearance() noexcept
		{
			DrawSectionHeader("Appearance");
			if (!BeginSettingsSection("##DearModdingUI.AppearanceSettings"))
				return;

			auto& settings = g_settingsDraft.draft;
			const auto& defaults = DefaultSettings();
			auto changed = false;

			if (DrawSettingsRow(
					"AccentColor",
					"Accent color",
					"Retints selections, controls, links, and every Phosphor menu icon in colored mode.",
					true,
					[&]() noexcept {
						auto accent = HostAccentToImVec4(settings.accentColor);
						ImGui::SetNextItemWidth(ControlWidth());
						if (ImGui::ColorPicker3(
								"##Value",
								&accent.x,
								ImGuiColorEditFlags_NoAlpha |
									ImGuiColorEditFlags_DisplayRGB |
									ImGuiColorEditFlags_InputRGB |
									ImGuiColorEditFlags_PickerHueBar))
						{
							settings.accentColor = HostAccentFromImVec4(accent);
							changed = true;
						}
						DrawAccentPresets(settings, changed);
					},
					[&]() noexcept {
						return settings.accentColor != defaults.accentColor;
					}))
			{
				settings.accentColor = defaults.accentColor;
				changed = true;
			}

			if (DrawSettingsRow(
					"IconColorMode",
					"Icon color mode",
					"Colored icons use the accent above; monochrome icons use the active text color.",
					true,
					[&]() noexcept {
						auto iconMode =
							settings.iconColorMode ==
									Theme::IconColorMode::kMonochrome ?
							1 :
							0;
						constexpr const char* iconModes[]{
							"Colored (accent)",
							"Monochrome (text)"
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
					"WindowBackgroundOpacity",
					"Window background opacity",
					"Raises or lowers the darkness of the host window without changing client content.",
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

			if (DrawSettingsRow(
					"PaletteBackgroundColor",
					"Command palette background",
					"Sets the neutral background color used by the command palette.",
					true,
					[&]() noexcept {
						auto paletteBackground =
							HostAccentToImVec4(settings.paletteBackgroundColor);
						ImGui::SetNextItemWidth(ControlWidth());
						if (ImGui::ColorEdit3(
								"##Value",
								&paletteBackground.x,
								ImGuiColorEditFlags_NoAlpha |
									ImGuiColorEditFlags_DisplayRGB |
									ImGuiColorEditFlags_InputRGB |
									ImGuiColorEditFlags_PickerHueBar))
						{
							settings.paletteBackgroundColor =
								HostAccentFromImVec4(paletteBackground);
							changed = true;
						}
					},
					[&]() noexcept {
						return settings.paletteBackgroundColor !=
							defaults.paletteBackgroundColor;
					}))
			{
				settings.paletteBackgroundColor =
					defaults.paletteBackgroundColor;
				changed = true;
			}

			if (DrawSettingsRow(
					"PaletteBackgroundOpacity",
					"Command palette opacity",
					"Controls how much of the blurred game remains visible through the palette.",
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
					"Background blur",
					"Blurs the game behind the host window and command palette; disabling it avoids the blur passes.",
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
					"Blur strength",
					"Adjusts the per-frame blur sample spread without reallocating graphics resources.",
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

			(void)SettingsTable::End(DMUI_INVALID_CLIENT_HANDLE);
			if (changed)
				PreviewDraft();
		}

		void DrawReadability() noexcept
		{
			DrawSectionHeader("Readability");
			if (!BeginSettingsSection("##DearModdingUI.ReadabilitySettings"))
				return;

			auto& settings = g_settingsDraft.draft;
			const auto& defaults = DefaultSettings();

			if (DrawSettingsRow(
					"UiScale",
					"UI scale (requires Apply)",
					"Multiplies resolution-derived sizing; Apply rebuilds typography once before the next frame.",
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
					"Body font family (requires Apply)",
					"Lists font-family folders in Data/F4SE/Plugins/DearModdingUI/Fonts; Apply rebuilds the selected family once.",
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
							"Applied this frame: %.*s",
							static_cast<int>(effectiveFamily.size()),
							effectiveFamily.data());
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

		void DrawInput() noexcept
		{
			DrawSectionHeader("Input");
			auto& settings = g_settingsDraft.draft;
			const auto& defaults = DefaultSettings();
			if (BeginSettingsSection("##DearModdingUI.InputSettings"))
			{
				if (DrawSettingsRow(
						"MenuToggleKey",
						"Menu toggle key",
						"Opens and closes the shared menu. Apply saves the key for this session and future launches.",
						true,
						[&]() noexcept {
							const auto selectedKey =
								ParseMenuToggleKey(settings.menuToggleKey);
							const auto selectedName =
								MenuToggleKeyName(selectedKey.virtualKey);
							ImGui::SetNextItemWidth(ControlWidth());
							if (ImGui::BeginCombo(
									"##Value",
									selectedName.data()))
							{
								for (const auto& key : kMenuToggleKeys)
								{
									const auto selected =
										key.virtualKey ==
										selectedKey.virtualKey;
									if (ImGui::Selectable(
											key.name.data(),
											selected))
										settings.menuToggleKey = key.name;
									if (selected)
										ImGui::SetItemDefaultFocus();
								}
								ImGui::EndCombo();
							}
						},
						[&]() noexcept {
							return settings.menuToggleKey !=
								defaults.menuToggleKey;
						}))
				{
					settings.menuToggleKey = defaults.menuToggleKey;
				}
				(void)SettingsTable::End(DMUI_INVALID_CLIENT_HANDLE);
			}

			ImGui::Spacing();
			ImGui::TextUnformatted("Client hotkeys");
			DrawHelp(
				"Bindings are owned by DearModdingUI. Changes below are saved immediately.");
			const auto actions = Hotkeys::Snapshot();
			if (actions.empty())
			{
				ImGui::TextDisabled("No client actions or saved overrides.");
				return;
			}

			if (!ImGui::BeginTable(
					"##DearModdingUI.Hotkeys",
					3,
					ImGuiTableFlags_BordersInnerH |
						ImGuiTableFlags_RowBg |
						ImGuiTableFlags_SizingStretchProp))
				return;
			ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 1.5f);
			ImGui::TableSetupColumn("Binding", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableHeadersRow();
			for (const auto& action : actions)
			{
				ImGui::PushID(action.id.c_str());
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				if (action.registered)
				{
					ImGui::TextUnformatted(action.displayName.c_str());
					ImGui::TextDisabled("%s", action.id.c_str());
				}
				else
					ImGui::TextUnformatted(action.id.c_str());

				ImGui::TableSetColumnIndex(1);
				if (action.registered)
				{
					const auto preview = action.state == DMUI_HOTKEY_BINDING_BOUND ?
						action.effectiveChord.c_str() :
						"Unbound";
					ImGui::SetNextItemWidth(-1.0f);
					if (ImGui::BeginCombo("##Binding", preview))
					{
						if (ImGui::Selectable(
								"Unbound",
								action.state == DMUI_HOTKEY_BINDING_UNBOUND_USER))
							(void)HostSettings::SetHotkeyOverride(action.id, "none");
						for (uint32_t modifiers = 0; modifiers < 8; ++modifiers)
						{
							for (const auto& key : kMenuToggleKeys)
							{
								const auto chord = SerializeHotkeyChord({
									key.virtualKey,
									modifiers
								});
								const auto selected =
									action.state == DMUI_HOTKEY_BINDING_BOUND &&
									action.effectiveChord == chord;
								if (ImGui::Selectable(chord.c_str(), selected))
									(void)HostSettings::SetHotkeyOverride(
										action.id, chord);
								if (selected)
									ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}
				}
				else
				{
					ImGui::TextUnformatted(action.overrideChord.c_str());
					if (ImGui::SmallButton("Remove saved override"))
						(void)HostSettings::RemoveHotkeyOverride(action.id);
				}

				ImGui::TableSetColumnIndex(2);
				switch (action.state)
				{
				case DMUI_HOTKEY_BINDING_BOUND:
					ImGui::TextUnformatted(action.registered ? "Bound" : "Not registered");
					break;
				case DMUI_HOTKEY_BINDING_UNBOUND_USER:
					ImGui::TextUnformatted("Cleared by user");
					break;
				case DMUI_HOTKEY_BINDING_UNBOUND_DEFAULT_CONFLICT:
					ImGui::TextUnformatted("Suggested default is taken");
					break;
				case DMUI_HOTKEY_BINDING_UNBOUND_OVERRIDE_CONFLICT:
					ImGui::TextUnformatted("Saved binding conflicts");
					break;
				case DMUI_HOTKEY_BINDING_UNBOUND_INVALID_OVERRIDE:
					ImGui::TextUnformatted("Saved binding is invalid");
					break;
				default:
					ImGui::TextUnformatted(
						action.registered ? "No suggested binding" : "Not registered");
					break;
				}
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
			DrawSectionHeader("Host facts (read-only)");
			ImGui::TextDisabled("Values resolved by the DearModdingUI host.");
			ImGui::Spacing();
			if (!BeginSettingsSection("##DearModdingUI.HostFacts"))
				return;

			const auto* body = Theme::GetFonts().body;
			char typography[32]{};
			std::snprintf(
				typography,
				sizeof(typography),
				"%.0f px",
				body ? body->LegacySize : ImGui::GetFontSize());
			DrawReadOnlyHostFact(
				"ResolvedTypographySize",
				"Resolved typography size",
				typography,
				"Derived from the backbuffer height and the applied UI scale at a frame boundary.");

			char scale[32]{};
			std::snprintf(
				scale,
				sizeof(scale),
				"%.2fx",
				Theme::Scale());
			DrawReadOnlyHostFact(
				"EffectiveUiScale",
				"Effective UI scale",
				scale,
				"Derived from resolution and [Additional] fMenuUiScale.");
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
			ResetHostSettingsDraft(g_settingsDraft);
			PreviewDraft();
			break;
		}
	}

	void DrawHostSettingsControls() noexcept
	{
		EnsureDraft();
		DrawAppearance();
		ImGui::Spacing();
		DrawReadability();
		ImGui::Spacing();
		DrawInput();
		ImGui::Spacing();
		DrawReadOnlyFacts();
	}
}
