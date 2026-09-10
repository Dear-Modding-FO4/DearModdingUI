#include "GeneralTestSuiteInternal.h"

#include <algorithm>

namespace DmuiTests::Detail
{
	SettingsExercise::SettingsExercise(DiagnosticContext& a_context) noexcept :
		m_context(a_context)
	{}

	void SettingsExercise::RecordEdit(
		EditCounters& a_counters,
		std::string_view a_id,
		bool a_changed,
		bool a_completed) noexcept
	{
		if (a_changed)
		{
			++a_counters.changed;
			a_counters.dirty = true;
		}
		if (a_completed)
		{
			++a_counters.completed;
			if (a_counters.dirty)
			{
				++a_counters.saves;
				++m_simulatedSaves;
				a_counters.dirty = false;
			}
			m_context.Info(
				"dmui-test-client: edit id={} event=completed "
				"changed={} completed={} resets={} saves={} total-saves={}"sv,
				a_id,
				a_changed,
				a_counters.completed,
				a_counters.resets,
				a_counters.saves,
				m_simulatedSaves);
		}
	}

	template <class DrawCallback, class ResetCallback>
	bool SettingsExercise::DrawEditableRow(
		const char* a_id,
		const char* a_label,
		const char* a_description,
		bool a_isDefault,
		DrawCallback&& a_draw,
		EditCounters& a_counters,
		ResetCallback&& a_reset)
	{
		auto& client = m_context.Client();
		const auto visible =
			client.BeginSettingsRow(a_id, a_label, a_description);
		if (!visible)
			return false;
		if (!*visible)
			return true;

		const auto changed = a_draw();
		const auto completed = dmui::ui::IsItemDeactivatedAfterEdit();
		RecordEdit(a_counters, a_id, changed, completed);
		const auto reset = client.EndSettingsRow(true, !a_isDefault);
		if (!reset)
			return false;
		if (*reset && !a_isDefault)
		{
			a_reset();
			++a_counters.resets;
			RecordEdit(a_counters, a_id, true, true);
			m_context.Info(
				"dmui-test-client: edit id={} event=reset "
				"completed={} resets={} saves={} total-saves={}"sv,
				a_id,
				a_counters.completed,
				a_counters.resets,
				a_counters.saves,
				m_simulatedSaves);
		}
		return true;
	}

	template <size_t N>
	void SettingsExercise::CopyText(
		std::array<char, N>& a_destination,
		std::string_view a_text) noexcept
	{
		a_destination.fill('\0');
		const auto length = (std::min)(a_text.size(), N - 1);
		std::memcpy(a_destination.data(), a_text.data(), length);
	}

	void SettingsExercise::Draw() noexcept
	{
		auto& client = m_context.Client();
		(void)client.DrawSectionHeader(
			"Stable UI edits and simulated persistence");
		const auto table = client.BeginSettingsTable("native-edits");
		if (!table || !*table)
			return;

		if (!DrawEditableRow(
				"short-text",
				"Native text",
				"Live stable InputText; save is simulated on completion.",
				std::strcmp(m_shortText.data(), "smoke") == 0,
				[this] {
					return dmui::ui::InputText(
						"##Value",
						m_shortText.data(),
						m_shortText.size());
				},
				m_textEdits,
				[this] { CopyText(m_shortText, "smoke"); }))
		{
			(void)client.EndSettingsTable();
			return;
		}

		if (!DrawEditableRow(
				"slider",
				"Native slider",
				"Stable scalar slider, range 0..100.",
				m_sliderValue == 50.0f,
				[this] {
					const float minimum{};
					const float maximum{ 100.0f };
					return dmui::ui::SliderScalar(
						"##Value",
						&m_sliderValue,
						&minimum,
						&maximum,
						"%.1f",
						dmui::ui::SliderFlags::kAlwaysClamp);
				},
				m_sliderEdits,
				[this] { m_sliderValue = 50.0f; }))
		{
			(void)client.EndSettingsTable();
			return;
		}

		if (!DrawEditableRow(
				"multiline",
				"Native multiline",
				"Three-line stable editor; no disk writes are performed.",
				std::strcmp(m_multiline.data(), "line one\nline two") == 0,
				[this] {
					return dmui::ui::InputTextMultiline(
						"##Value",
						m_multiline.data(),
						m_multiline.size(),
						{
							0.0f,
							dmui::ui::GetTextLineHeightWithSpacing() * 3.0f
						});
				},
				m_multilineEdits,
				[this] { CopyText(m_multiline, "line one\nline two"); }))
		{
			(void)client.EndSettingsTable();
			return;
		}

		const auto counters = client.BeginSettingsRow(
			"edit-counters",
			"Lifecycle counters",
			"changed / completed / simulated saves",
			dmui::RowPresentation::Layout::kFullSpan);
		if (!counters)
		{
			(void)client.EndSettingsTable();
			return;
		}
		if (*counters)
		{
			dmui::ui::Text(
				"changed/completed/reset/saved: text %llu/%llu/%llu/%llu | "
				"slider %llu/%llu/%llu/%llu | multiline "
				"%llu/%llu/%llu/%llu | total saves %llu",
				m_textEdits.changed,
				m_textEdits.completed,
				m_textEdits.resets,
				m_textEdits.saves,
				m_sliderEdits.changed,
				m_sliderEdits.completed,
				m_sliderEdits.resets,
				m_sliderEdits.saves,
				m_multilineEdits.changed,
				m_multilineEdits.completed,
				m_multilineEdits.resets,
				m_multilineEdits.saves,
				m_simulatedSaves);
			(void)client.EndSettingsRow(false, false);
		}
		(void)client.EndSettingsTable();
	}

	uint64_t SettingsExercise::EventCount() const noexcept
	{
		return
			m_textEdits.completed + m_textEdits.resets +
			m_sliderEdits.completed + m_sliderEdits.resets +
			m_multilineEdits.completed + m_multilineEdits.resets;
	}

	uint64_t SettingsExercise::SimulatedSaves() const noexcept
	{
		return m_simulatedSaves;
	}

	SettingsExercise::Snapshot SettingsExercise::CurrentSnapshot() const noexcept
	{
		return {
			m_textEdits.completed + m_textEdits.resets,
			m_sliderEdits.completed + m_sliderEdits.resets,
			m_multilineEdits.completed + m_multilineEdits.resets
		};
	}
}
