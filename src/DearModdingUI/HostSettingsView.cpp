#include <DearModdingUI/HostSettingsView.h>

#include <DearModdingUI/HostSettings.h>
#include <DearModdingUI/Hotkeys.h>
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
			ImGui::Spacing();
		}

		void DrawAppearance() noexcept
		{
			DrawSectionHeader("Appearance");
			auto& settings = g_settingsDraft.draft;
			auto changed = false;

			ImGui::TextUnformatted("Accent color");
			DrawHelp(
				"Retints selections, controls, links, and every Phosphor menu icon in colored mode.");
			auto accent = HostAccentToImVec4(settings.accentColor);
			ImGui::SetNextItemWidth(ControlWidth());
			if (ImGui::ColorPicker3(
					"##DearModdingUI.AccentColor",
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

			auto iconMode =
				settings.iconColorMode == Theme::IconColorMode::kMonochrome ? 1 : 0;
			constexpr const char* iconModes[]{ "Colored (accent)", "Monochrome (text)" };
			ImGui::SetNextItemWidth(ControlWidth());
			if (ImGui::Combo(
					"Icon color mode",
					&iconMode,
					iconModes,
					static_cast<int>(std::size(iconModes))))
			{
				settings.iconColorMode = iconMode == 1 ?
					Theme::IconColorMode::kMonochrome :
					Theme::IconColorMode::kColored;
				changed = true;
			}
			DrawHelp(
				"Colored icons use the accent above; monochrome icons use the active text color.");

			auto opacityPercent = settings.windowBackgroundOpacity * 100.0f;
			ImGui::SetNextItemWidth(ControlWidth());
			if (ImGui::SliderFloat(
					"Window background opacity",
					&opacityPercent,
					kMinWindowBackgroundOpacity * 100.0f,
					kMaxWindowBackgroundOpacity * 100.0f,
					"%.0f%%",
					ImGuiSliderFlags_AlwaysClamp))
			{
				settings.windowBackgroundOpacity = opacityPercent / 100.0f;
				changed = true;
			}
			DrawHelp(
				"Raises or lowers the darkness of the host window without changing client content.");

			changed |= ImGui::Checkbox(
				"Background blur",
				&settings.backgroundBlur);
			DrawHelp(
				"Blurs the game only behind the host window; disabling it avoids the blur passes.");

			ImGui::BeginDisabled(!settings.backgroundBlur);
			ImGui::SetNextItemWidth(ControlWidth());
			if (ImGui::SliderFloat(
					"Blur strength",
					&settings.backgroundBlurStrength,
					kMinBackgroundBlurStrength,
					kMaxBackgroundBlurStrength,
					"%.2f",
					ImGuiSliderFlags_AlwaysClamp))
				changed = true;
			ImGui::EndDisabled();
			DrawHelp(
				"Adjusts the per-frame blur sample spread without reallocating graphics resources.");

			if (changed)
				PreviewDraft();
		}

		void DrawReadability() noexcept
		{
			DrawSectionHeader("Readability");
			auto& settings = g_settingsDraft.draft;

			ImGui::SetNextItemWidth(ControlWidth());
			ImGui::SliderFloat(
				"UI scale (requires Apply)",
				&settings.uiScale,
				Theme::kMinUserScale,
				Theme::kMaxUserScale,
				"%.2fx",
				ImGuiSliderFlags_AlwaysClamp);
			DrawHelp(
				"Multiplies resolution-derived sizing; Apply rebuilds typography once before the next frame.");
			const auto& families = Theme::AvailableBodyFontFamilies();
			const auto resolvedFamily =
				Theme::ResolveBodyFontFamily(settings.bodyFontFamily);
			ImGui::SetNextItemWidth(ControlWidth());
			if (ImGui::BeginCombo(
					"Body font family (requires Apply)",
					resolvedFamily.data()))
			{
				for (const auto& family : families)
				{
					const auto selected = family == resolvedFamily;
					if (ImGui::Selectable(family.c_str(), selected))
						settings.bodyFontFamily = family;
					if (selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			DrawHelp(
				"Lists font-family folders in Data/F4SE/Plugins/DearModdingUI/Fonts; Apply rebuilds the selected family once.");
			const auto effectiveFamily = Theme::EffectiveBodyFontFamily();
			ImGui::TextDisabled(
				"Applied this frame: %.*s",
				static_cast<int>(effectiveFamily.size()),
				effectiveFamily.data());
			ImGui::Spacing();
		}

		void DrawInput() noexcept
		{
			DrawSectionHeader("Input");
			auto& settings = g_settingsDraft.draft;
			const auto selectedKey = ParseMenuToggleKey(settings.menuToggleKey);
			const auto selectedName = MenuToggleKeyName(selectedKey.virtualKey);

			ImGui::SetNextItemWidth(ControlWidth());
			if (ImGui::BeginCombo("Menu toggle key", selectedName.data()))
			{
				for (const auto& key : kMenuToggleKeys)
				{
					const auto selected = key.virtualKey == selectedKey.virtualKey;
					if (ImGui::Selectable(key.name.data(), selected))
						settings.menuToggleKey = key.name;
					if (selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			DrawHelp(
				"Opens and closes the shared menu. Apply saves the key for this session and future launches.");

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
			const char* a_label,
			const char* a_value,
			const char* a_source) noexcept
		{
			{
				const Theme::FontGuard font{ Theme::FontRole::kHeading };
				ImGui::TextUnformatted(a_label);
			}
			ImGui::SameLine();
			ImGui::TextUnformatted(a_value);
			DrawHelp(a_source);
		}

		void DrawReadOnlyFacts() noexcept
		{
			DrawSectionHeader("Host facts (read-only)");
			ImGui::TextDisabled("Values resolved by the DearModdingUI host.");
			ImGui::Spacing();

			const auto* body = Theme::GetFonts().body;
			char typography[32]{};
			std::snprintf(
				typography,
				sizeof(typography),
				"%.0f px",
				body ? body->LegacySize : ImGui::GetFontSize());
			DrawReadOnlyHostFact(
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
				"Effective UI scale",
				scale,
				"Derived from resolution and [Additional] fMenuUiScale.");
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
