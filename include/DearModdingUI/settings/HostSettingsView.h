#pragma once

#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/SettingsActions.h>

namespace DearModdingUI
{
	struct HostSettingsDraftState
	{
		HostInterfaceSettings committed;
		HostInterfaceSettings draft;
		bool active{ false };
	};

	[[nodiscard]] inline HostSettingsDraftState BeginHostSettingsDraft(
		const HostInterfaceSettings& a_committed)
	{
		return { a_committed, a_committed, true };
	}

	[[nodiscard]] inline bool HostSettingsDraftDiffers(
		const HostSettingsDraftState& a_state) noexcept
	{
		return a_state.active && a_state.draft != a_state.committed;
	}

	inline void CommitHostSettingsSidebarLayout(
		HostSettingsDraftState& a_state,
		SidebarLayoutKind a_layout) noexcept
	{
		if (!a_state.active)
			return;
		const auto layout = NormalizeUserSidebarLayout(a_layout);
		a_state.committed.sidebarLayout = layout;
		a_state.draft.sidebarLayout = layout;
	}

	inline void RevertHostSettingsDraft(
		HostSettingsDraftState& a_state)
	{
		if (a_state.active)
			a_state.draft = a_state.committed;
	}

	inline void ResetHostSettingsDraft(
		HostSettingsDraftState& a_state)
	{
		if (a_state.active)
			a_state.draft = DefaultHostInterfaceSettings();
	}

	inline void LeaveHostSettingsDraft(
		HostSettingsDraftState& a_state)
	{
		RevertHostSettingsDraft(a_state);
		a_state.active = false;
	}

	[[nodiscard]] bool HostSettingsTitleActionEnabled(
		SettingsAction a_action) noexcept;
	void InvokeHostSettingsTitleAction(
		SettingsAction a_action) noexcept;
	void DrawHostSettingsControls() noexcept;
}
