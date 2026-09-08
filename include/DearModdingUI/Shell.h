#pragma once

#include <DearModdingUI/SettingsActions.h>
#include <DearModdingUI/ExternalOpen.h>

#include <imgui/imgui.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace DearModdingUI
{
	enum class HostPageKind : uint32_t;

	struct LinkRowEntry
	{
		std::string_view label;
		ExternalOpenRequest external;
		std::string_view note;
		char32_t glyph{};
		bool enabled{ true };
		DMUI_LinkAction action{ DMUI_LINK_ACTION_COPY_TARGET };
	};

	struct FaqRowEntry
	{
		std::string_view question;
		std::string_view answer;
	};

	void ConfigurePreviewHostPage(HostPageKind a_page) noexcept;
	[[nodiscard]] float SettingsActionButtonExtent() noexcept;
	[[nodiscard]] float SettingsActionButtonWidth(
		SettingsAction a_action,
		const char* a_fallbackLabel,
		float a_buttonExtent) noexcept;
	[[nodiscard]] bool DrawSettingsActionButton(
		const char* a_id,
		const ImVec2& a_origin,
		const ImVec2& a_size,
		SettingsAction a_action,
		const char* a_fallbackLabel,
		const char* a_tooltip,
		bool a_enabled) noexcept;
	void DrawSearchInput(
		const char* a_id,
		const char* a_hint,
		std::string& a_search) noexcept;
	void DrawSectionHeader(const char* a_text, char32_t a_glyph = 0) noexcept;
	void DrawBulletText(const char* a_text) noexcept;
	[[nodiscard]] DMUI_Result DrawLinkRow(
		const char* a_id,
		std::span<const LinkRowEntry> a_links) noexcept;
	void DrawFaq(
		const char* a_id,
		std::span<const FaqRowEntry> a_entries) noexcept;
	void DrawCollapsingSectionHeader(
		const char* a_key,
		const char* a_text,
		char32_t a_glyph,
		bool& a_expanded,
		size_t a_count) noexcept;
	void DrawShell() noexcept;
	void ApplyMenuEscapeDismissal() noexcept;
}
