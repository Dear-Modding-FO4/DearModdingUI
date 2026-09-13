#include "SettingFeedbackFixtures.h"

#include <DearModdingUI/controls/FieldFeedback.h>

#include <array>

namespace DmuiTestFixtures
{
	namespace
	{
		using namespace DearModdingUI;

		constexpr const char* kLongCompatibilityWarning{
			"Another installed mod can change activation distance, line-of-sight "
			"checks, detection thresholds, crime reporting, animation timing, or "
			"menu behavior while this option is enabled. DearModdingUI displays "
			"this client-authored warning exactly as supplied and does not decide "
			"whether the combination is valid. Review the other mod's configuration "
			"and test several crowded interiors before keeping the change. If "
			"symptoms appear only after loading an existing save, restore the "
			"original values, close the menu, and repeat the same scenario before "
			"reporting a conflict. This deliberately long example demonstrates "
			"natural wrapping at narrow widths without overlapping the value "
			"control, reset action, following rows, or container padding."
		};

		template<class DrawControl>
		void DrawSettingRow(
			dmui::Client& a_client,
			const char* a_id,
			const char* a_label,
			const char* a_description,
			bool a_isDefault,
			std::optional<dmui::FieldFeedbackSeverity> a_severity,
			const char* a_message,
			DrawControl&& a_drawControl)
		{
			dmui::FieldScope row{
				a_client,
				a_id,
				a_label,
				a_description
			};
			if (!row.Visible())
				return;
			a_drawControl();
			if (a_severity)
				(void)row.SetFeedback(*a_severity, a_message);
			const auto reset = row.End(true, !a_isDefault);
			if (reset && *reset)
				a_drawControl(true);
		}
	}

	bool SettingFeedbackFixture::Register(std::string& a_error)
	{
		m_client = std::make_unique<dmui::Client>(
			"setting-feedback", "Setting feedback", dmui::Version{ 0, 1 },
			"sliders-horizontal");
		if (!m_client->Connect())
		{
			a_error = "Could not connect setting feedback fixture: ";
			a_error += DMUI_ResultToString(m_client->LastResult());
			return false;
		}

		struct Page
		{
			const char* id;
			const char* name;
			Placement placement;
		};
		constexpr std::array pages{
			Page{ "label", "A. Under the label", Placement::kLabel },
			Page{ "control", "B. Under the control", Placement::kControl },
			Page{ "strip", "C. Full-width strip", Placement::kStrip }
		};
		int32_t sortKey{};
		for (const auto& page : pages)
		{
			if (!m_client->AddPage(
					{
						.id = page.id,
						.displayName = page.name,
						.summary = "Production field feedback layouts",
						.sortKey = sortKey++
					},
					[this, placement = page.placement] { Draw(placement); }))
			{
				a_error = "Could not register setting feedback page: ";
				a_error += DMUI_ResultToString(m_client->LastResult());
				return false;
			}
		}
		return true;
	}

	void SettingFeedbackFixture::Draw(Placement a_placement)
	{
		using namespace DearModdingUI;
		const auto layout = a_placement == Placement::kControl ?
			FieldFeedbackPlacement::kUnderControl :
			a_placement == Placement::kStrip ?
				FieldFeedbackPlacement::kFullWidthStrip :
				FieldFeedbackPlacement::kUnderLabel;
		const FieldFeedback::PreviewLayoutOverride previewLayout{ layout };

		(void)m_client->DrawSectionHeader("Pickpocket");
		dmui::SettingsTableScope table{ *m_client, "##FeedbackSettings" };
		if (table.Visible())
		{
			DrawSettingRow(
				*m_client,
				"minimum",
				"Minimum pickpocket chance",
				"The lowest chance of a successful attempt.",
				m_minimum == 0,
				m_showMessages ?
					std::optional{ dmui::FieldFeedbackSeverity::kError } :
					std::nullopt,
				"Minimum cannot exceed maximum.",
				[this](bool reset = false) {
					if (reset)
						m_minimum = 0;
					else
					{
						const int minimum = 0;
						const int maximum = 100;
						(void)dmui::ui::SliderScalar(
							"##Value", &m_minimum, &minimum, &maximum, "%d%%");
					}
				});
			DrawSettingRow(
				*m_client,
				"maximum",
				"Maximum pickpocket chance",
				"The highest chance of a successful attempt.",
				m_maximum == 90,
				std::nullopt,
				nullptr,
				[this](bool reset = false) {
					if (reset)
						m_maximum = 90;
					else
					{
						const int minimum = 0;
						const int maximum = 100;
						(void)dmui::ui::SliderScalar(
							"##Value", &m_maximum, &minimum, &maximum, "%d%%");
					}
				});
			DrawSettingRow(
				*m_client,
				"reach",
				"Pickpocket reach",
				"How close you must be to the target, in game units.",
				m_reach == 150,
				m_showMessages ?
					std::optional{ dmui::FieldFeedbackSeverity::kWarning } :
					std::nullopt,
				"Long reach may allow pickpocketing through walls.",
				[this](bool reset = false) {
					if (reset)
						m_reach = 150;
					else
					{
						const int minimum = 50;
						const int maximum = 500;
						(void)dmui::ui::SliderScalar(
							"##Value", &m_reach, &minimum, &maximum, "%d");
					}
				});
			DrawSettingRow(
				*m_client,
				"compatibility",
				"Compatibility warning",
				"A deliberately long client-authored example for comparing wrapping.",
				true,
				m_showMessages ?
					std::optional{ dmui::FieldFeedbackSeverity::kWarning } :
					std::nullopt,
				kLongCompatibilityWarning,
				[](bool = false) {
					dmui::ui::TextUnformatted(
						"Uses the current client-owned configuration");
				});
			DrawSettingRow(
				*m_client,
				"chance",
				"Show success chance",
				"Display the chance before attempting to pickpocket.",
				m_showChance,
				m_showMessages ?
					std::optional{ dmui::FieldFeedbackSeverity::kInfo } :
					std::nullopt,
				"Restart the game for changes to take effect.",
				[this](bool reset = false) {
					if (reset)
						m_showChance = true;
					else
						(void)dmui::ui::Checkbox("##Value", &m_showChance);
				});
			(void)table.End();
		}

		dmui::ui::Spacing();
		(void)m_client->DrawSectionHeader("Standalone custom UI");
		{
			dmui::FieldScope field{
				*m_client,
				"standalone-distance",
				"Activation distance",
				"This field is not registered as a setting and owns no persistence binding."
			};
			if (field.Visible())
			{
				const int minimum = 50;
				const int maximum = 1000;
				(void)dmui::ui::SliderScalar(
					"##Value",
					&m_standaloneDistance,
					&minimum,
					&maximum,
					"%d units");
				if (m_showMessages)
				{
					(void)field.SetFeedback(
						dmui::FieldFeedbackSeverity::kInfo,
						"The client supplied this frame-local message after drawing the current value.");
				}
			}
		}

		dmui::ui::Spacing();
		(void)dmui::ui::Checkbox(
			"Show simulated client messages",
			&m_showMessages);
		(void)dmui::DrawStyledText(
			*m_client,
			"Interactive fixture: values and messages are client-owned simulation only; no validation or disk writes occur.",
			{ .tone = dmui::TextTone::kMuted, .wrapped = true });
	}
}
