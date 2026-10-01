#include <DearModdingUI/host/ModalCoordinator.h>
#include <DearModdingUI/controls/Controls.h>

#include <imgui/imgui_internal.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace DearModdingUI::ModalCoordinator
{
	namespace
	{
		enum class State { kPending, kOpen, kClosed };
		struct Popup
		{
			Owner owner;
			ImGuiID id;
			State state{ State::kPending };
			bool modal{};
			int frame{};
			ImVec2 anchor{};
		};
		std::vector<Popup> s_popups;
		std::optional<Owner> s_modalOwner;
		DMUI_DialogHandle s_dialog{};
		std::atomic<ImGuiID> s_dialogPopup{};
		bool s_dialogOpened{};
		std::atomic_bool s_closePages{};
		std::atomic_bool s_modalActive{};

		ImGuiID PageSeed(Owner a_owner) noexcept
		{
			auto seed = ImHashData(&a_owner.client, sizeof(a_owner.client));
			return ImHashData(&a_owner.page, sizeof(a_owner.page), seed);
		}

		ImGuiID PopupId(Owner a_owner, const char* a_id) noexcept
		{
			const auto local = ImGui::GetID(a_id);
			return ImHashData(&local, sizeof(local), PageSeed(a_owner));
		}

		bool NativeOpen(ImGuiID a_id) noexcept
		{
			return ImGui::GetCurrentContext() &&
				ImGui::IsPopupOpen(a_id, ImGuiPopupFlags_AnyPopupLevel);
		}

		ImGuiID ModalId(ImGuiID a_id) noexcept
		{
			char label[64]{};
			std::snprintf(label, sizeof(label), "###dmui.modal.%u", a_id);
			return ImHashStr(label);
		}

		ImGuiID NativeId(const Popup& a_popup) noexcept
		{
			return a_popup.modal ? ModalId(a_popup.id) : a_popup.id;
		}

		void Refresh() noexcept
		{
			for (auto& popup : s_popups)
				if (popup.state == State::kOpen && !NativeOpen(NativeId(popup)))
					popup.state = State::kClosed;
			if (s_dialog && (!s_dialogOpened || NativeOpen(DialogPopupId())))
				s_modalOwner = Owner{};
			else
			{
				const auto modal = std::ranges::find_if(s_popups, [](const Popup& a_popup) {
					return a_popup.modal && a_popup.state == State::kOpen;
				});
				s_modalOwner = modal == s_popups.end() ?
					std::nullopt : std::optional{ modal->owner };
			}
			s_modalActive.store(s_modalOwner.has_value(), std::memory_order_release);
		}

		void CloseToLevel(int a_level) noexcept
		{
			auto& stack = GImGui->OpenPopupStack;
			if (a_level < 0 || a_level >= stack.Size)
				return;
			auto* focus = a_level ? stack[a_level - 1].Window : stack[a_level].RestoreNavWindow;
			// Native fallback skips newly appearing parents and can close an extra level.
			ImGui::ClosePopupToLevel(a_level, false);
			if (focus)
				ImGui::FocusWindow(focus, ImGuiFocusRequestFlags_RestoreFocusedChild);
		}

		void CloseNative(ImGuiID a_id) noexcept
		{
			if (auto* context = ImGui::GetCurrentContext())
				for (int level = 0; level < context->OpenPopupStack.Size; ++level)
					if (context->OpenPopupStack[level].PopupId == a_id)
					{
						CloseToLevel(level);
						break;
					}
		}

		// Modal names are also window identities; never reuse a window across pages.
		bool BeginModal(ImGuiID a_id, const char* a_title, bool* a_open,
			ImGuiWindowFlags a_flags)
		{
			const auto title = std::string(a_title, ImGui::FindRenderedTextEnd(a_title)) +
				"###dmui.modal." + std::to_string(a_id);
			auto* parent = ImGui::GetCurrentWindow();
			const auto seed = parent->IDStack.back();
			// BeginPopupModal hashes its label, whereas OpenPopupEx accepts an ID.
			parent->IDStack.back() = 0;
			ImGui::SetNextWindowBgAlpha(1.0f);
			const auto visible = BeginPopupModalWithRoundedTitleBarButtons(
				title.c_str(), a_open, a_flags);
			parent->IDStack.back() = seed;
			return visible;
		}

		Popup* Find(Owner a_owner, const char* a_id) noexcept
		{
			const auto id = PopupId(a_owner, a_id);
			const auto found = std::ranges::find(s_popups, id, &Popup::id);
			return found == s_popups.end() ? nullptr : &*found;
		}

		bool CurrentOwned(Owner a_owner) noexcept
		{
			const auto* context = ImGui::GetCurrentContext();
			if (!context || context->BeginPopupStack.empty() ||
				context->BeginPopupStack.back().Window != context->CurrentWindow)
				return false;
			const auto id = context->BeginPopupStack.back().PopupId;
			return std::ranges::any_of(s_popups, [&](const Popup& a_popup) {
				return NativeId(a_popup) == id && a_popup.owner == a_owner;
			});
		}
	}

	bool ReserveDialog(DMUI_DialogHandle a_dialog) noexcept
	{
		Refresh();
		if (s_modalOwner || s_dialog ||
			(ImGui::GetCurrentContext() && ImGui::GetTopMostPopupModal()))
			return false;
		s_modalOwner = Owner{};
		s_modalActive.store(true, std::memory_order_release);
		s_dialog = a_dialog;
		s_dialogPopup = 0;
		s_dialogOpened = false;
		return true;
	}

	void ReleaseDialog(DMUI_DialogHandle a_dialog) noexcept
	{
		if (s_dialog != a_dialog)
			return;
		CloseNative(DialogPopupId());
		s_dialog = 0;
		s_dialogPopup = 0;
		s_dialogOpened = false;
		Refresh();
	}

	ImGuiID DialogPopupId() noexcept
	{
		const auto id = s_dialogPopup.load(std::memory_order_acquire);
		return id ? ModalId(id) : 0;
	}

	bool BeginDialog(const char* a_title, bool* a_open)
	{
		if (!s_dialog)
			return false;
		if (!s_dialogOpened)
		{
			s_dialogPopup = ImHashData(&s_dialog, sizeof(s_dialog), ImHashStr("dmui.dialog"));
			ImGui::OpenPopupEx(DialogPopupId());
			s_dialogOpened = true;
		}
		if (!NativeOpen(DialogPopupId()))
		{
			if (a_open)
				*a_open = false;
			return false;
		}
		return BeginModal(s_dialogPopup, a_title, a_open, ImGuiWindowFlags_AlwaysAutoResize);
	}

	void Open(Owner a_owner, const char* a_id)
	{
		Refresh();
		auto* popup = Find(a_owner, a_id);
		if (!popup)
		{
			s_popups.push_back({ a_owner, PopupId(a_owner, a_id) });
			popup = &s_popups.back();
		}
		if (popup->state == State::kOpen)
			return;
		popup->state = State::kPending;
		popup->frame = ImGui::GetFrameCount();
		popup->anchor = { ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y };
	}

	bool Begin(Owner a_owner, const char* a_id, bool a_modal, bool* a_open,
		ImGuiWindowFlags a_flags, bool a_hasCloseButton)
	{
		Refresh();
		auto* popup = Find(a_owner, a_id);
		if (!popup)
		{
			if (a_open)
				*a_open = false;
			return false;
		}
		if (a_open && !*a_open)
		{
			CloseNative(NativeId(*popup));
			popup->state = State::kClosed;
			Refresh();
			return false;
		}
		if (popup->state == State::kClosed)
		{
			if (a_open)
				*a_open = false;
			return false;
		}
		if (popup->state == State::kPending)
		{
			if ((s_modalOwner && *s_modalOwner != a_owner) ||
				(!s_modalOwner && ImGui::GetTopMostPopupModal()))
				return false;
			popup->modal = a_modal;
			if (a_modal)
			{
				s_modalOwner = a_owner;
				s_modalActive.store(true, std::memory_order_release);
			}
			ImGui::OpenPopupEx(NativeId(*popup));
			popup->state = State::kOpen;
			if (!a_modal)
				ImGui::SetNextWindowPos(popup->anchor, ImGuiCond_Appearing);
		}
		return a_modal ?
			BeginModal(popup->id, a_id, a_hasCloseButton ? a_open : nullptr, a_flags) :
			ImGui::BeginPopupEx(popup->id,
				a_flags | ImGuiWindowFlags_AlwaysAutoResize |
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
	}

	bool IsOpen(Owner a_owner, const char* a_id) noexcept
	{
		Refresh();
		const auto* popup = Find(a_owner, a_id);
		return popup && popup->state != State::kClosed;
	}

	bool HasBracket(Owner a_owner) noexcept
	{
		if (const auto* context = ImGui::GetCurrentContext())
			for (const auto& bracket : context->BeginPopupStack)
				for (const auto& popup : s_popups)
					if (NativeId(popup) == bracket.PopupId && popup.owner == a_owner)
						return true;
		return false;
	}

	bool End(Owner a_owner) noexcept
	{
		if (!CurrentOwned(a_owner))
			return false;
		ImGui::EndPopup();
		Refresh();
		return true;
	}

	bool CloseCurrent(Owner a_owner) noexcept
	{
		if (!CurrentOwned(a_owner))
			return false;
		CloseToLevel(GImGui->BeginPopupStack.Size - 1);
		Refresh();
		return true;
	}

	void ClosePage(DMUI_PageHandle a_page) noexcept
	{
		for (const auto& popup : s_popups)
			if (popup.owner.page == a_page)
				CloseNative(NativeId(popup));
		std::erase_if(s_popups, [=](const Popup& a_popup) { return a_popup.owner.page == a_page; });
		Refresh();
	}

	void ClosePages() noexcept
	{
		while (!s_popups.empty())
			ClosePage(s_popups.back().owner.page);
	}

	void NotifyMenuClosed() noexcept
	{
		s_closePages.store(true, std::memory_order_release);
	}

	void BeginFrame() noexcept
	{
		if (s_closePages.exchange(false, std::memory_order_acq_rel))
			ClosePages();
	}

	void TouchPage(DMUI_PageHandle a_page) noexcept
	{
		for (auto& popup : s_popups)
			if (popup.owner.page == a_page)
				popup.frame = ImGui::GetFrameCount();
	}

	void FinishFrame() noexcept
	{
		BeginFrame();
		for (;;)
		{
			const auto absent = std::ranges::find_if(s_popups, [](const Popup& a_popup) {
				return a_popup.frame != ImGui::GetFrameCount();
			});
			if (absent == s_popups.end())
				break;
			ClosePage(absent->owner.page);
		}
		Refresh();
	}

	void Dismiss(ImGuiID a_id, size_t a_depth) noexcept
	{
		auto* context = ImGui::GetCurrentContext();
		if (context && a_depth &&
			static_cast<size_t>(context->OpenPopupStack.Size) == a_depth &&
			context->OpenPopupStack.back().PopupId == a_id)
			CloseToLevel(static_cast<int>(a_depth - 1));
		Refresh();
	}

	bool HasModal() noexcept
	{
		return s_modalActive.load(std::memory_order_acquire);
	}
}
