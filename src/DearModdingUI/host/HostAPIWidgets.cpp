#include "HostAPIEntries.h"
#include "HostContext.h"

#include <Platform/files/ExternalOpen.h>
#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/controls/Faq.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/controls/LinkRow.h>
#include <DearModdingUI/controls/SettingsTable.h>
#include <DearModdingUI/presentation/Theme.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace DearModdingUI::HostAPIInternal
{
	using namespace HostInternal;

	[[nodiscard]] DMUI_Result DMUI_CALL ApiGetThemeColors(DMUI_ClientHandle a_client,
														  DMUI_ThemeColors *a_colors) noexcept
	{
		if (!a_colors)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_colors->structSize < DMUI_THEME_COLORS_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;

		*a_colors = Theme::ColorSnapshot();
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiPushFont(DMUI_ClientHandle a_client,
													DMUI_FontRole a_role) noexcept
	{
		if (a_role >= DMUI_FONT_ROLE_COUNT)
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		const auto *context = ImGui::GetCurrentContext();
		if (!context)
			return DMUI_RESULT_HOST_NOT_READY;

		try
		{
			g_clientFontPushes.push_back({a_client, context->FontStack.Size + 1});
		}
		catch (const std::bad_alloc &)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
		catch (...)
		{
			return DMUI_RESULT_CALLBACK_FAILED;
		}
		if (!Theme::PushFont(static_cast<Theme::FontRole>(a_role)))
		{
			g_clientFontPushes.pop_back();
			return DMUI_RESULT_HOST_NOT_READY;
		}
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiPopFont(DMUI_ClientHandle a_client) noexcept
	{
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		const auto *context = ImGui::GetCurrentContext();
		if (!context || g_clientFontPushes.empty() ||
			g_clientFontPushes.back().client != a_client ||
			g_clientFontPushes.back().depth != context->FontStack.Size)
			return DMUI_RESULT_INVALID_ARGUMENT;

		Theme::PopFont();
		g_clientFontPushes.pop_back();
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiDrawSectionHeader(DMUI_ClientHandle a_client,
															 const char *a_text,
															 uint32_t a_glyph) noexcept
	{
		if (!a_text)
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		DrawSectionHeader(a_text, static_cast<char32_t>(a_glyph));
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiDrawBulletText(DMUI_ClientHandle a_client,
														  const char *a_text) noexcept
	{
		if (!a_text)
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		DrawBulletText(a_text);
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiDrawSearchInput(DMUI_ClientHandle a_client,
														   const char *a_id, const char *a_hint,
														   char *a_buffer, size_t a_capacity,
														   uint32_t *a_changed) noexcept
	{
		if (!a_id || !a_hint || !a_buffer || !a_capacity || !a_changed)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_changed = 0u;

		size_t length = 0;
		while (length < a_capacity && a_buffer[length])
			++length;
		if (length == a_capacity)
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;

		try
		{
			std::string search{a_buffer, length};
			DrawSearchInput(a_id, a_hint, search);
			*a_changed =
				search.size() != length || std::memcmp(search.data(), a_buffer, length) != 0 ? 1u
																							 : 0u;
			const auto outputLength = (std::min)(search.size(), a_capacity - 1);
			std::memcpy(a_buffer, search.data(), outputLength);
			a_buffer[outputLength] = '\0';
			return DMUI_RESULT_OK;
		}
		catch (const std::bad_alloc &)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
		catch (...)
		{
			return DMUI_RESULT_CALLBACK_FAILED;
		}
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiDrawCollapsingSectionHeader(
		DMUI_ClientHandle a_client, const char *a_key, const char *a_text, uint32_t a_glyph,
		uint32_t *a_expanded, size_t a_count) noexcept
	{
		if (!a_key || !a_text || !a_expanded)
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;

		auto expanded = *a_expanded != 0;
		DrawCollapsingSectionHeader(a_key, a_text, static_cast<char32_t>(a_glyph), expanded,
									a_count);
		*a_expanded = expanded ? 1u : 0u;
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiDrawLinkRow(DMUI_ClientHandle a_client, const char *a_id,
													   const DMUI_LinkDescriptor *a_links,
													   size_t a_count) noexcept
	{
		const auto arguments = ValidateLinkRowArguments(a_client, a_id, a_links, a_count);
		if (arguments != DMUI_RESULT_OK || a_count == 0)
			return arguments;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;

		try
		{
			std::vector<LinkRowEntry> links;
			links.reserve(a_count);
			for (size_t index = 0; index < a_count; ++index)
			{
				const auto &link = a_links[index];
				ExternalOpenRequest external;
				if (link.enabled != 0 && link.external)
				{
					const auto result = ValidateExternalOpenDescriptor(link.external, external);
					if (result != DMUI_RESULT_OK)
						return result;
				}
				links.push_back({link.label, std::move(external), link.note ? link.note : "",
								 static_cast<char32_t>(link.glyph), link.enabled != 0,
								 link.action});
			}
			return DrawLinkRow(a_id, links);
		}
		catch (const std::bad_alloc &)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
		catch (...)
		{
			return DMUI_RESULT_CALLBACK_FAILED;
		}
	}

	[[nodiscard]] DMUI_Result DMUI_CALL
	ApiOpenExternal(DMUI_ClientHandle a_client, const DMUI_ExternalOpenDescriptor *a_descriptor,
					uint32_t *a_nativeError) noexcept
	{
		if (a_nativeError)
			*a_nativeError = 0;
		if (a_client == DMUI_INVALID_CLIENT_HANDLE || !a_descriptor)
			return DMUI_RESULT_INVALID_ARGUMENT;
		auto &service = GetService();
		const auto state = service.state.load(std::memory_order_acquire);
		if (state != DMUI_HOST_STATE_READY)
			return StateResult(state);
		const auto clientResult = service.registry.ValidateClient(a_client);
		if (clientResult != DMUI_RESULT_OK)
			return clientResult;
		static const ExternalOpener opener;
		return opener.Open(a_descriptor, a_nativeError);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiDrawFaq(DMUI_ClientHandle a_client, const char *a_id,
												   const DMUI_FaqEntry *a_entries,
												   size_t a_count) noexcept
	{
		const auto arguments = ValidateFaqArguments(a_client, a_id, a_entries, a_count);
		if (arguments != DMUI_RESULT_OK || a_count == 0)
			return arguments;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;

		try
		{
			std::vector<FaqRowEntry> entries;
			entries.reserve(a_count);
			for (size_t index = 0; index < a_count; ++index)
			{
				entries.push_back({a_entries[index].question, a_entries[index].answer});
			}
			DrawFaq(a_id, entries);
			return DMUI_RESULT_OK;
		}
		catch (const std::bad_alloc &)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
		catch (...)
		{
			return DMUI_RESULT_CALLBACK_FAILED;
		}
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiDrawSettingsActionButton(
		DMUI_ClientHandle a_client, const char *a_id, DMUI_Vec2 a_origin, DMUI_Vec2 a_size,
		DMUI_SettingsAction a_action, const char *a_fallbackLabel, const char *a_tooltip,
		uint32_t a_enabled, uint32_t *a_pressed) noexcept
	{
		if (!a_id || !a_pressed || a_action > DMUI_SETTINGS_ACTION_APPLY)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_pressed = 0u;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;

		*a_pressed = DrawSettingsActionButton(a_id, {a_origin.x, a_origin.y}, {a_size.x, a_size.y},
											  static_cast<SettingsAction>(a_action),
											  a_fallbackLabel, a_tooltip, a_enabled != 0)
						 ? 1u
						 : 0u;
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiSettingsActionButtonWidth(DMUI_ClientHandle a_client,
																	 DMUI_SettingsAction a_action,
																	 const char *a_fallbackLabel,
																	 float a_buttonExtent,
																	 float *a_width) noexcept
	{
		if (!a_width || a_action > DMUI_SETTINGS_ACTION_APPLY)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_width = 0.0f;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;

		*a_width = SettingsActionButtonWidth(static_cast<SettingsAction>(a_action), a_fallbackLabel,
											 a_buttonExtent);
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiSettingsActionButtonExtent(DMUI_ClientHandle a_client,
																	  float *a_extent) noexcept
	{
		if (!a_extent)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_extent = 0.0f;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;

		*a_extent = SettingsActionButtonExtent();
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiBeginSettingsTable(DMUI_ClientHandle a_client,
															  const char *a_id,
															  uint32_t *a_visible) noexcept
	{
		if (!a_id || !a_visible)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_visible = 0u;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		if (!SettingsTable::AcceptsClient(a_client))
			return DMUI_RESULT_WRONG_THREAD;

		const auto result = SettingsTable::Begin(a_client, a_id);
		*a_visible = result.visible ? 1u : 0u;
		return result.result;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiBeginSettingsRow(DMUI_ClientHandle a_client,
															const char *a_id, const char *a_label,
															const char *a_description,
															uint32_t *a_visible) noexcept
	{
		if (!a_id || !a_label || !a_visible)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_visible = 0u;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		if (!SettingsTable::AcceptsClient(a_client))
			return DMUI_RESULT_WRONG_THREAD;

		const auto result = SettingsTable::BeginRow(a_client, a_id, a_label, a_description);
		*a_visible = result.visible ? 1u : 0u;
		return result.result;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL
	ApiBeginSettingsRowEx(DMUI_ClientHandle a_client, const char *a_id, const char *a_label,
						  const char *a_description, const DMUI_SettingsRowBeginOptions *a_options,
						  uint32_t *a_visible) noexcept
	{
		if (!a_visible)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_visible = 0u;
		const auto optionsValidation = SettingsTable::ValidateRowBeginOptions(a_options);
		if (optionsValidation != DMUI_RESULT_OK)
			return optionsValidation;
		if (!a_id || !a_label)
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		if (!SettingsTable::AcceptsClient(a_client))
			return DMUI_RESULT_WRONG_THREAD;

		const auto result =
			SettingsTable::BeginRow(a_client, a_id, a_label, a_description,
									a_options->layout == DMUI_SETTINGS_ROW_LAYOUT_FULL_SPAN
										? SettingsTable::RowLayout::kFullSpan
										: SettingsTable::RowLayout::kLabelValue);
		*a_visible = result.visible ? 1u : 0u;
		return result.result;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiEndSettingsRow(DMUI_ClientHandle a_client,
														  const DMUI_SettingsRowOptions *a_options,
														  uint32_t *a_resetPressed) noexcept
	{
		if (!a_resetPressed)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_resetPressed = 0u;
		const auto optionsValidation = SettingsTable::ValidateRowOptions(a_options);
		if (optionsValidation != DMUI_RESULT_OK)
			return optionsValidation;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		if (!SettingsTable::AcceptsClient(a_client))
			return DMUI_RESULT_WRONG_THREAD;

		bool resetPressed{};
		const auto result = SettingsTable::EndRow(
			a_client, {a_options->resetVisible != 0, a_options->resetEnabled != 0}, resetPressed);
		if (result == DMUI_RESULT_OK)
			*a_resetPressed = resetPressed ? 1u : 0u;
		return result;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiEndSettingsTable(DMUI_ClientHandle a_client) noexcept
	{
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		if (!SettingsTable::AcceptsClient(a_client))
			return DMUI_RESULT_WRONG_THREAD;
		return SettingsTable::End(a_client);
	}
} // namespace DearModdingUI::HostAPIInternal
