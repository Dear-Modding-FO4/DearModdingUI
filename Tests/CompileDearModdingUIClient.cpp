#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <DearModdingUI/Client.h>

#include <string>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<dmui::Client>);
static_assert(!std::is_move_constructible_v<dmui::Client>);
static_assert(!std::is_copy_constructible_v<dmui::FontGuard>);
static_assert(!std::is_move_constructible_v<dmui::FontGuard>);

namespace
{
	int g_counter{ 0 };

	[[maybe_unused]] void Exercise() noexcept
	{
		static dmui::Client client{
			"example.author.mod",
			"Example Mod",
			dmui::Version{ 1, 0 }
		};
		if (!client.Connect())
			return;
		const auto label = g_counter;
		const auto page = client.AddPage("settings", "Settings", "General", [label] {
			ImGui::TextUnformatted("hello");
			(void)label;
		});
		if (page)
			(void)client.SelectPage(*page);
		(void)client.AddAction(
			"copy",
			"Copy",
			"clipboard-text",
			"Copy something.",
			[label] { (void)label; });
		(void)client.AddFrameObserver([label] { (void)label; });
		const auto hotkey = client.AddHotkeyAction(
			"example.author.mod.Toggle",
			"Toggle example",
			"Shift+F11",
			[label](bool) { (void)label; });
		if (hotkey)
			(void)client.QueryHotkeyBinding(*hotkey);
		const auto videoMemory = client.QueryVideoMemory();
		if (videoMemory)
			(void)videoMemory->budget;
		(void)client.SetStatus(DMUI_STATUS_SEVERITY_SUCCESS, "ready");
		const auto colors = client.GetThemeColors();
		if (colors)
			(void)dmui::ToImVec4(colors->accent);
		const dmui::FontGuard font{ client, DMUI_FONT_ROLE_BODY };
		(void)client.DrawSectionHeader("Section");
		std::string search;
		(void)client.DrawSearchInput("search", "Search...", search);
		bool expanded{ true };
		(void)client.DrawCollapsingSectionHeader(
			"section",
			"Section",
			0,
			expanded,
			1);
		(void)client.DrawSettingsActionButton(
			"apply",
			{},
			{ 24.0f, 24.0f },
			DMUI_SETTINGS_ACTION_APPLY,
			"Apply",
			"Apply changes.",
			true);
		const auto extent = client.SettingsActionButtonExtent();
		if (extent)
		{
			(void)client.SettingsActionButtonWidth(
				DMUI_SETTINGS_ACTION_APPLY,
				"Apply",
				*extent);
		}
		const auto settingsTable = client.BeginSettingsTable("settings");
		if (settingsTable.value_or(false))
		{
			const auto row = client.BeginSettingsRow(
				"enabled",
				"Enabled",
				"Enables the example.");
			if (row.value_or(false))
			{
				bool enabled{};
				(void)ImGui::Checkbox("##Value", &enabled);
				(void)client.EndSettingsRow(true, enabled);
			}
			(void)client.EndSettingsTable();
		}
		(void)client.IsMenuVisible();
		(void)client.QueryState();
		(void)client.UnavailableReason();
	}
}
