#include "HostAPIEntries.h"
#include "HostContext.h"

#include <Platform/files/ExternalOpen.h>
#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/controls/Faq.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <DearModdingUI/controls/LinkRow.h>
#include <DearModdingUI/controls/SettingsTable.h>
#include <DearModdingUI/controls/TextViewer.h>
#include <DearModdingUI/presentation/Theme.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace DearModdingUI::HostAPIInternal
{
	using namespace HostInternal;

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

	[[nodiscard]] DMUI_Result DMUI_CALL ApiDrawTextView(
		DMUI_ClientHandle a_client,
		const DMUI_TextViewDescriptor* a_descriptor,
		DMUI_TextViewState* a_state) noexcept
	{
		if (!a_descriptor || !a_state)
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		if (!RenderExecution::IsActiveClient(a_client, true))
			return DMUI_RESULT_WRONG_THREAD;
		if (!ImGui::GetCurrentContext() || !Theme::FontsReady())
			return DMUI_RESULT_HOST_NOT_READY;
		try
		{
			return DrawTextView(a_client, *a_descriptor, *a_state);
		}
		catch (const std::bad_alloc&)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
		catch (...)
		{
			return DMUI_RESULT_CALLBACK_FAILED;
		}
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
			const std::unique_ptr<bool[]> expanded{new bool[a_count]};
			for (size_t index = 0; index < a_count; ++index)
			{
				expanded[index] = *a_entries[index].expanded != 0;
				entries.push_back({a_entries[index].question, a_entries[index].answer,
								   &expanded[index]});
			}
			DrawFaq(a_id, entries);
			for (size_t index = 0; index < a_count; ++index)
				*a_entries[index].expanded = expanded[index] ? 1u : 0u;
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
