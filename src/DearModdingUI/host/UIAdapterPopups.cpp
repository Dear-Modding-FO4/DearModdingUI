#include "UIAdapterInternal.h"
#include <DearModdingUI/UIBindings.generated.h>
#include <DearModdingUI/host/ModalCoordinator.h>
#include <DearModdingUI/host/RenderExecution.h>

namespace DearModdingUI::UI::Bindings
{
	namespace
	{
		ModalCoordinator::Owner Owner(DMUI_ClientHandle a_client) noexcept
		{
			return { a_client, RenderExecution::ActivePage() };
		}

		template <class Operation>
		DMUI_Result PopupOperation(DMUI_ClientHandle a_client, Operation&& a_operation) noexcept
		{
			const auto validation = AdapterInternal::Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			if (!RenderExecution::ActivePage())
				return DMUI_RESULT_INVALID_ARGUMENT;
			try
			{
				return a_operation(Owner(a_client));
			}
			catch (...)
			{
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			}
		}

		DMUI_Result Begin(DMUI_ClientHandle a_client, const char* a_id,
			bool a_modal, uint32_t a_hasCloseButton, uint32_t* a_open,
			DMUI_UIWindowFlags a_flags, uint32_t* a_visible) noexcept
		{
			if (!a_id || !*a_id || !a_visible || (a_modal && !a_open))
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_visible = 0;
			ImGuiWindowFlags flags{};
			const auto translated = TranslateWindowFlags(a_flags, flags);
			if (translated != DMUI_RESULT_OK)
				return translated;
			return PopupOperation(a_client, [&](ModalCoordinator::Owner a_owner) {
				bool open = !a_open || *a_open != 0;
				*a_visible = ModalCoordinator::Begin(a_owner, a_id, a_modal,
					a_open ? &open : nullptr, flags, a_hasCloseButton != 0) ? 1u : 0u;
				if (a_open)
					*a_open = open ? 1u : 0u;
				return DMUI_RESULT_OK;
			});
		}
	}

	DMUI_Result DMUI_CALL OpenPopup(DMUI_ClientHandle a_client, const char* a_id) noexcept
	{
		if (!a_id || !*a_id)
			return DMUI_RESULT_INVALID_ARGUMENT;
		return PopupOperation(a_client, [&](ModalCoordinator::Owner a_owner) {
			ModalCoordinator::Open(a_owner, a_id);
			return DMUI_RESULT_OK;
		});
	}

	DMUI_Result DMUI_CALL BeginPopup(DMUI_ClientHandle a_client, const char* a_id,
		DMUI_UIWindowFlags a_flags, uint32_t* a_visible) noexcept
	{
		return Begin(a_client, a_id, false, 0, nullptr, a_flags, a_visible);
	}

	DMUI_Result DMUI_CALL BeginPopupModal(DMUI_ClientHandle a_client, const char* a_id,
		uint32_t a_hasCloseButton, uint32_t* a_open, DMUI_UIWindowFlags a_flags,
		uint32_t* a_visible) noexcept
	{
		return Begin(a_client, a_id, true, a_hasCloseButton, a_open, a_flags, a_visible);
	}

	DMUI_Result DMUI_CALL EndPopup(DMUI_ClientHandle a_client) noexcept
	{
		return PopupOperation(a_client, [](ModalCoordinator::Owner a_owner) {
			if (AdapterInternal::HasWindowDrawListClip())
				return DMUI_RESULT_INVALID_ARGUMENT;
			return ModalCoordinator::End(a_owner) ?
				DMUI_RESULT_OK : DMUI_RESULT_INVALID_ARGUMENT;
		});
	}

	DMUI_Result DMUI_CALL CloseCurrentPopup(DMUI_ClientHandle a_client) noexcept
	{
		return PopupOperation(a_client, [](ModalCoordinator::Owner a_owner) {
			return ModalCoordinator::CloseCurrent(a_owner) ?
				DMUI_RESULT_OK : DMUI_RESULT_INVALID_ARGUMENT;
		});
	}

	DMUI_Result DMUI_CALL IsPopupOpen(DMUI_ClientHandle a_client, const char* a_id,
		uint32_t* a_open) noexcept
	{
		if (!a_id || !*a_id || !a_open)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_open = 0;
		return PopupOperation(a_client, [&](ModalCoordinator::Owner a_owner) {
			*a_open = ModalCoordinator::IsOpen(a_owner, a_id) ? 1u : 0u;
			return DMUI_RESULT_OK;
		});
	}
}
