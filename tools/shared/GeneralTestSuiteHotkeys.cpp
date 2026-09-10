#include "GeneralTestSuiteInternal.h"

namespace DmuiTests::Detail
{
	HotkeyExercise::HotkeyExercise(DiagnosticContext& a_context) noexcept :
		m_context(a_context)
	{
		for (size_t index = 0; index < kHotkeyDescriptors.size(); ++index)
		{
			const auto& descriptor = kHotkeyDescriptors[index];
			auto& probe = m_probes[index];
			probe.id = descriptor.id;
			probe.name = descriptor.name;
			probe.suggested = descriptor.suggested;
			probe.policy = descriptor.policy;
		}
	}

	bool HotkeyExercise::Register(
		std::function<void()> a_toggleOverlay,
		std::function<void()> a_scheduleNotification,
		std::string& a_failedId,
		DMUI_Result& a_result) noexcept
	{
		m_toggleOverlay = std::move(a_toggleOverlay);
		m_scheduleNotification = std::move(a_scheduleNotification);
		auto& client = m_context.Client();
		bool complete{ true };
		a_result = DMUI_RESULT_OK;
		for (size_t index = 0; index < m_probes.size(); ++index)
		{
			auto& probe = m_probes[index];
			const auto handle = client.AddHotkeyAction(
				probe.id,
				probe.name,
				probe.suggested,
				[this, index](bool a_pressed) {
					OnHotkey(index, a_pressed);
				},
				probe.policy);
			if (!handle)
			{
				const auto result = client.LastResult();
				if (complete)
				{
					a_failedId = probe.id;
					a_result = result;
				}
				complete = false;
				m_context.Error(
					"dmui-test-client: hotkey registration id={} "
					"result={} suggested={} policy={} effective=unavailable"sv,
					probe.id,
					DMUI_ResultToString(result),
					probe.suggested,
					static_cast<uint32_t>(probe.policy));
				continue;
			}
			probe.handle = *handle;
			const auto binding = client.QueryHotkeyBinding(probe.handle);
			probe.lastResult = client.LastResult();
			if (!binding)
			{
				if (complete)
				{
					a_failedId = "initial hotkey binding query";
					a_result = probe.lastResult;
				}
				complete = false;
				m_context.Error(
					"dmui-test-client: hotkey registration id={} "
					"result=OK suggested={} policy={} binding-result={} "
					"effective=unavailable"sv,
					probe.id,
					probe.suggested,
					static_cast<uint32_t>(probe.policy),
					DMUI_ResultToString(probe.lastResult));
				continue;
			}
			probe.binding = *binding;
			m_context.Info(
				"dmui-test-client: hotkey registration id={} "
				"result=OK suggested={} policy={} effective={} state={}"sv,
				probe.id,
				probe.suggested,
				static_cast<uint32_t>(probe.policy),
				probe.binding.chord[0] ? probe.binding.chord : "none",
				BindingStateName(probe.binding.state));
		}
		return complete;
	}

	void HotkeyExercise::OnHotkey(size_t a_index, bool a_pressed) noexcept
	{
		auto& probe = m_probes[a_index];
		if (a_pressed)
			++probe.presses;
		else
			++probe.releases;
		m_context.Info(
			"dmui-test-client: hotkey id={} event={} down={} up={}"sv,
			probe.id,
			a_pressed ? "press" : "release",
			probe.presses.load(),
			probe.releases.load());
		if (!a_pressed)
			return;
		if (a_index == 0 && m_toggleOverlay)
			m_toggleOverlay();
		else if (a_index == 1 && m_scheduleNotification)
			m_scheduleNotification();
	}

	void HotkeyExercise::Observe() noexcept
	{
		auto& client = m_context.Client();
		for (auto& probe : m_probes)
		{
			if (probe.handle == DMUI_INVALID_HOTKEY_ACTION_HANDLE)
				continue;
			if (const auto binding = client.QueryHotkeyBinding(probe.handle))
			{
				const auto changed =
					binding->state != probe.binding.state ||
					std::strcmp(binding->chord, probe.binding.chord) != 0;
				probe.binding = *binding;
				probe.lastResult = client.LastResult();
				if (changed)
				{
					m_context.Info(
						"dmui-test-client: hotkey binding "
						"id={} effective={} state={} result={}"sv,
						probe.id,
						probe.binding.chord[0] ?
							probe.binding.chord :
							"none",
						BindingStateName(probe.binding.state),
						DMUI_ResultToString(probe.lastResult));
				}
			}
			else
			{
				probe.lastResult = client.LastResult();
			}
		}
	}

	void HotkeyExercise::Draw() noexcept
	{
		auto& client = m_context.Client();
		(void)client.DrawSectionHeader("Official contextual hotkeys");
		const auto table = client.BeginSettingsTable("hotkeys");
		if (!table || !*table)
			return;
		for (auto& probe : m_probes)
		{
			const auto visible = client.BeginSettingsRow(
				probe.id,
				probe.name,
				"Enablement uses the official host manager.");
			if (!visible)
			{
				(void)client.EndSettingsTable();
				return;
			}
			if (!*visible)
				continue;
			auto enabled = probe.enabled;
			if (dmui::ui::Checkbox("##Enabled", &enabled))
			{
				const auto changed =
					client.SetHotkeyActionEnabled(probe.handle, enabled);
				m_result = client.LastResult();
				probe.lastResult = m_result;
				if (changed)
					probe.enabled = enabled;
				m_context.Info(
					"dmui-test-client: hotkey enable "
					"id={} requested={} applied={} result={}"sv,
					probe.id,
					enabled,
					changed,
					DMUI_ResultToString(m_result));
			}
			dmui::ui::SameLine();
			dmui::ui::Text(
				"%s | %s | down/up %llu/%llu",
				probe.binding.chord[0] ? probe.binding.chord : "none",
				BindingStateName(probe.binding.state),
				probe.presses.load(),
				probe.releases.load());
			if (!client.EndSettingsRow(false, false))
			{
				(void)client.EndSettingsTable();
				return;
			}
		}
		(void)client.EndSettingsTable();
		dmui::ui::TextDisabled(
			"Defaults: Ctrl+Shift+F10/F11 gameplay-unobstructed. "
			"HOST_INPUT_INACTIVE, optional ALWAYS, letter A, and "
			"digit 7 probes default to NONE; bind them in the host manager.");
	}

	uint64_t HotkeyExercise::EdgeCount() const noexcept
	{
		uint64_t result{};
		for (const auto& probe : m_probes)
			result += probe.presses.load() + probe.releases.load();
		return result;
	}

	DMUI_Result HotkeyExercise::Result() const noexcept
	{
		return m_result;
	}

	const std::array<HotkeyProbe, kHotkeyDescriptors.size()>&
		HotkeyExercise::Probes() const noexcept
	{
		return m_probes;
	}
}
