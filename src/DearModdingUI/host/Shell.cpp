#include <DearModdingUI/host/Shell.h>

#include <DearModdingUI/presentation/BackgroundBlur.h>
#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/host/MenuDismissal.h>
#include <DearModdingUI/navigation/NavigationController.h>
#include <DearModdingUI/navigation/NavigationPresentation.h>
#if defined(DMUI_PREVIEW)
#include <DearModdingUI/navigation/SidebarComparison.h>
#endif
#include <DearModdingUI/host/Status.h>
#include <DearModdingUI/presentation/Theme.h>
#include <DearModdingUI/VisualDecisions.h>
#include <DearModdingUI/navigation/CommandPalette.h>
#include <DearModdingUI/pages/HostPageViews.h>
#include <DearModdingUI/controls/ChromeGeometry.h>
#include <DearModdingUI/navigation/SidebarView.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace DearModdingUI
{
	namespace
	{
		inline constexpr char kStatusDetailsPopupId[] =
			"Status details###DearModdingUIStatusDetails";

		struct ShellState : ClientSelectionState
		{
			SidebarViewState sidebar;
			CommandPaletteState palette;
			HostPageViewState hostPages;
			NavigationPresentationState presentation;
			std::optional<StatusMessage> statusDetails;
			SidebarLayoutKind sidebarLayout{ DEFAULT_SIDEBAR_LAYOUT };
#if defined(DMUI_PREVIEW)
			std::optional<SidebarLayoutKind> previewSidebarLayoutOverride;
			std::optional<NavigationPresentationKind>
				previewPresentationOverride;
#endif
		};

		[[nodiscard]] ShellState& State() noexcept
		{
			static ShellState state;
			return state;
		}

		[[nodiscard]] NavigationPresentationKind ActivePresentationKind(
			const ShellState& a_state) noexcept
		{
#if defined(DMUI_PREVIEW)
			return a_state.previewPresentationOverride.value_or(
				NavigationPresentationKind::Grouped);
#else
			(void)a_state;
			return NavigationPresentationKind::Grouped;
#endif
		}

		[[nodiscard]] NavigationResult ApplyShellNavigationRequest(
			const NavigationModel& a_model,
			const NavigationRequest& a_request,
			ShellState& a_state)
		{
			auto result = ApplyNavigationRequest(
				a_model,
				a_request,
				a_state);
			if (!result.accepted)
			{
				(void)SetHostStatus(
					DMUI_STATUS_SEVERITY_WARNING,
					"Navigation request was rejected.");
				return result;
			}
			HostSettings::SetPageActive(
				result.hostPage == HostPageKind::kSettings);
			if (result.revealSelection &&
				result.client != DMUI_INVALID_CLIENT_HANDLE)
			{
				RevealNavigationClient(
					ActivePresentationKind(a_state),
					a_model,
					result.client,
					a_state.presentation);
			}
			RevealSidebarSelection(
				a_state.sidebarLayout,
				a_model,
				a_state,
				a_state.sidebar);
			return result;
		}

		void NavigateToHostPage(
			HostPageKind a_page,
			ShellState& a_state) noexcept
		{
			(void)ApplyShellNavigationRequest(
				Navigation(),
				NavigationRequest::Host(a_page),
				a_state);
		}

#if defined(DMUI_PREVIEW)
		void ApplyPreviewExpandedClients(
			const NavigationModel& a_model,
			ShellState& a_state)
		{
			if (!a_state.sidebar.previewExpandedClients)
				return;
			for (const auto& client : a_model.clients)
			{
				a_state.sidebar.modExpansion[client.id] =
					std::ranges::find(
						*a_state.sidebar.previewExpandedClients,
						client.id) !=
					a_state.sidebar.previewExpandedClients->end();
			}
			if (a_state.sidebarLayout == SidebarLayoutKind::DrillDown)
			{
				if (a_state.sidebar.previewExpandedClients->empty())
				a_state.sidebar.drillDown = {};
				else if (const auto client = std::ranges::find(
						a_model.clients,
						a_state.sidebar.previewExpandedClients->front(),
						&NavigationClient::id);
					client != a_model.clients.end())
				{
					(void)ApplyShellNavigationRequest(
						a_model,
						NavigationRequest::Client(client->handle),
						a_state);
				}
			}
			a_state.sidebar.previewExpandedClients.reset();
		}
#endif

		[[nodiscard]] bool DrawHeader(
			const NavigationModel& a_model,
			const ShellState& a_state,
			bool a_drawClose) noexcept
		{
			const auto* client = a_model.FindClient(a_state.activeClient);
			const auto* hostPage = a_state.activeHostPage ?
				FindHostNavigationPage(*a_state.activeHostPage) :
				nullptr;
			const auto breadcrumb = BuildHostBreadcrumb(
				"Evil Modding",
				hostPage ?
					hostPage->displayName :
					client ?
						std::string_view{ client->displayName } :
						std::string_view{});
			constexpr auto extentPolicy =
				TitleRowButtonExtentPolicy::kHostChrome;
			const auto extent = ResolveTitleRowButtonExtent(
				extentPolicy,
				ImGui::GetFontSize(),
				TitleBarButtonPadding());
			const auto hasGlyph = HasIconGlyph(PhosphorGlyph::kX);
			constexpr auto closeLabel = "Close";
			const TitleRowButton closeButton{
				"##DearModdingUI.HostCloseButton",
				ActionButtonWidth(
					hasGlyph,
					ImGui::CalcTextSize(closeLabel).x,
					extent,
					ImGui::GetStyle().FramePadding.x),
				hasGlyph ? PhosphorGlyph::kX : char32_t{},
				closeLabel,
				"Close menu"
			};
			return DrawTitleRow({
				.title = breadcrumb.c_str(),
				.titleScale = Theme::kHeaderFallbackTextScale,
				.titleInsetX = BulletRunContentInset(
					ImGui::GetStyle().FramePadding.x,
					ImGui::GetFontSize()),
				.buttons = {
					&closeButton,
					a_drawClose ? size_t{ 1 } : size_t{}
				},
				.buttonExtentPolicy = extentPolicy
			}).has_value();
		}

		void DrawFailure(const NavigationPage& a_page) noexcept
		{
			{
				const Theme::FontGuard font{ Theme::FontRole::kHeading };
				ImGui::TextColored(
					Theme::kStatusPaletteDefaults.error,
					"%s could not be displayed",
					a_page.displayName.c_str());
			}
			ImGui::Spacing();
			ImGui::TextWrapped(
				"The mod's page callback failed and has been disabled for "
				"this session. Other pages remain available.");
		}

		[[nodiscard]] bool ActionHasGlyph(
			const RegisteredAction& a_action,
			char32_t& a_glyph) noexcept
		{
			a_glyph = a_action.iconSelection.GlyphOr({});
			return HasIconGlyph(a_glyph);
		}

		void DrawPageHeader(const NavigationPage& a_page) noexcept
		{
			constexpr auto extentPolicy =
				TitleRowButtonExtentPolicy::kTitleBar;
			const auto extent = ResolveTitleRowButtonExtent(
				extentPolicy,
				ImGui::GetFontSize(),
				TitleBarButtonPadding());
			std::vector<TitleRowButton> buttons;
			std::vector<DMUI_ActionHandle> actions;
			for (const auto& action : OrderedActions())
			{
				if (action.client != a_page.client)
					continue;
				char32_t glyph{};
				const auto hasGlyph = ActionHasGlyph(action, glyph);
				const auto failed = ActionFailed(action.handle);
				buttons.push_back({
					action.id.c_str(),
					ActionButtonWidth(
						hasGlyph,
						ImGui::CalcTextSize(
							action.displayLabel.c_str()).x,
						extent,
						ImGui::GetStyle().FramePadding.x),
					hasGlyph ? glyph : char32_t{},
					action.displayLabel.c_str(),
					failed ?
						"Action disabled after its callback failed." :
						(action.tooltip.empty() ?
							action.displayLabel.c_str() :
							action.tooltip.c_str()),
					!failed
				});
				actions.push_back(action.handle);
			}
			const auto pressed = DrawTitleRow({
				.title = a_page.displayName.c_str(),
				.titleScale = Theme::kFeatureTitleScale,
				.buttons = buttons,
				.buttonExtentPolicy = extentPolicy,
				.summary = a_page.summary.empty() ?
					nullptr :
					a_page.summary.c_str()
			});
			if (pressed)
				(void)InvokeAction(actions[*pressed]);
		}

		void DrawContent(
			const NavigationModel& a_model,
			ShellState& a_state) noexcept
		{
			ImGui::TableNextColumn();
			if (!ImGui::BeginChild(
					"##DearModdingPageFrame",
					{},
					ImGuiChildFlags_Borders))
			{
				ImGui::EndChild();
				return;
			}
			if (a_state.activeHostPage)
			{
				DrawHostPage(*a_state.activeHostPage, a_state.hostPages);
				ImGui::EndChild();
				return;
			}
			const auto* page = a_model.FindPage(a_state.activePage);
			if (!page)
			{
				ImGui::TextDisabled("Please select a page from the left.");
				ImGui::EndChild();
				return;
			}
			DrawPageHeader(*page);
			const auto presentation =
				DecidePagePresentation(page, PageFailed(page->handle));
			if (presentation == PagePresentation::kFailure)
				DrawFailure(*page);
			else if (presentation == PagePresentation::kContent)
			{
				ImGui::PushID(static_cast<int>(page->handle));
				if (!DrawPage(page->handle))
				{
					ImGui::Spacing();
					ImGui::SeparatorEx(
						ImGuiSeparatorFlags_Horizontal,
						SeparatorThickness());
					ImGui::Spacing();
					DrawFailure(*page);
				}
				ImGui::PopID();
			}
			ImGui::EndChild();
		}

		[[nodiscard]] bool DrawFooterStatus(
			const StatusMessage& a_status,
			float a_runMaxX) noexcept
		{
			const auto availableWidth = (std::max)(
				a_runMaxX - ImGui::GetCursorScreenPos().x,
				0.0f);
			const auto presentation = FitStatusText(
				a_status.attributedText,
				availableWidth,
				[](std::string_view a_value) {
					return ImGui::CalcTextSize(
						a_value.data(),
						a_value.data() + a_value.size()).x;
				});
			ImGui::Bullet();
			ImGui::PushStyleColor(
				ImGuiCol_Text,
				Theme::StatusTextColor(a_status.severity));
			ImGui::TextUnformatted(
				presentation.visible.data(),
				presentation.visible.data() +
					presentation.visible.size());
			ImGui::PopStyleColor();
			const auto hasDetails =
				a_status.severity == DMUI_STATUS_SEVERITY_ERROR;
			if (presentation.truncated &&
				ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
				ImGui::SetTooltip("%s", presentation.full.c_str());
			if (!hasDetails)
				return false;
			if (ImGui::IsItemHovered())
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			return ImGui::IsItemClicked(ImGuiMouseButton_Left);
		}

		void DrawStatusDetails(ShellState& a_state) noexcept
		{
			if (!a_state.statusDetails)
				return;
			const auto* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowSize(
				{
					(std::min)(
						620.0f * Theme::Scale(),
						viewport->WorkSize.x * 0.9f),
					0.0f
				},
				ImGuiCond_Appearing);
			auto open = true;
			auto closeRequested = false;
			if (BeginPopupModalWithRoundedTitleBarButtons(
					kStatusDetailsPopupId,
					&open,
					ImGuiWindowFlags_AlwaysAutoResize |
						ImGuiWindowFlags_NoSavedSettings))
			{
				const auto& status = *a_state.statusDetails;
				ImGui::TextDisabled("Error from %s", status.owner.c_str());
				ImGui::Spacing();
				ImGui::TextWrapped("%s", status.message.c_str());
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();
				if (ImGui::Button("Copy details"))
					ImGui::SetClipboardText(status.attributedText.c_str());
				ImGui::SameLine();
				if (ImGui::Button("Close"))
				{
					ImGui::CloseCurrentPopup();
					closeRequested = true;
				}
				ImGui::EndPopup();
			}
			if (!open || closeRequested)
				a_state.statusDetails.reset();
		}

		void DrawFooter(
			const NavigationModel& a_model,
			ShellState& a_state) noexcept
		{
			const auto status = CurrentStatus();
			const auto start = ImGui::GetCursorScreenPos();
			const auto maxX = start.x + ImGui::GetContentRegionAvail().x;
			const auto iconSize = HostChromeIconSize(ImGui::GetFontSize());
			const auto settingsExtent = HostChromeButtonExtent(
				ImGui::GetFontSize(),
				TitleBarButtonPadding());
			const auto dismissExtent = TitleBarButtonExtent(
				ImGui::GetFontSize(),
				TitleBarButtonPadding());
			const auto rowHeight =
				(std::max)(ImGui::GetFrameHeight(), settingsExtent);
			const auto hasGear = HasIconGlyph(PhosphorGlyph::kGear);
			constexpr auto settingsLabel = "Settings";
			const auto settingsWidth = ActionButtonWidth(
				hasGear,
				ImGui::CalcTextSize(settingsLabel).x,
				settingsExtent,
				ImGui::GetStyle().FramePadding.x);
			const auto persistent = status && status->persistent;
			const auto hasDismiss = HasIconGlyph(PhosphorGlyph::kX);
			constexpr auto dismissLabel = "Dismiss";
			const auto dismissWidth = ActionButtonWidth(
				hasDismiss,
				ImGui::CalcTextSize(dismissLabel).x,
				dismissExtent,
				ImGui::GetStyle().FramePadding.x);
			const auto controls = ResolveFooterControlsLayout(
				start.x,
				maxX,
				settingsWidth,
				persistent ? dismissWidth : 0.0f,
				ImGui::GetStyle().ItemSpacing.x);

			ImGui::PushClipRect(
				start,
				{ controls.runMaxX, start.y + rowHeight },
				true);
			ImGui::SetCursorScreenPos({
				start.x,
				start.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f
			});
			DrawBulletText("Host: Evil Modding");
			if (const auto* client = a_model.FindClient(a_state.activeClient))
			{
				ImGui::SameLine();
				const auto mod = "Mod: " + client->displayName;
				DrawBulletText(mod.c_str());
				ImGui::SameLine();
				const auto version = std::format(
					"Version: {}.{}",
					client->version >> 16,
					client->version & 0xFFFFu);
				DrawBulletText(version.c_str());
			}
			if (status)
			{
				ImGui::SameLine();
				if (DrawFooterStatus(*status, controls.runMaxX))
				{
					a_state.statusDetails = *status;
					ImGui::OpenPopup(kStatusDetailsPopupId);
				}
			}
			ImGui::PopClipRect();

			if (persistent && controls.dismissMaxX > controls.dismissMinX &&
				DrawCompactChromeButton(
					"##DearModdingUI.StatusDismissButton",
					{
						controls.dismissMinX,
						start.y + (rowHeight - dismissExtent) * 0.5f
					},
					{
						controls.dismissMaxX - controls.dismissMinX,
						dismissExtent
					},
					hasDismiss ? PhosphorGlyph::kX : char32_t{},
					hasDismiss ? nullptr : dismissLabel,
					"Dismiss status",
					hasDismiss ?
						IconColor(ImGui::GetColorU32(ImGuiCol_Text)) :
						ImGui::GetColorU32(ImGuiCol_Text)))
			{
				(void)DismissStatus(status->generation);
			}
			if (DrawCompactChromeButton(
					"##DearModdingUI.HostSettingsButton",
					{
						controls.settingsMinX,
						start.y + (rowHeight - settingsExtent) * 0.5f
					},
					{ settingsWidth, settingsExtent },
					hasGear ? PhosphorGlyph::kGear : char32_t{},
					hasGear ? nullptr : settingsLabel,
					"Interface settings",
					hasGear ?
						IconColor(ImGui::GetColorU32(ImGuiCol_Text)) :
						ImGui::GetColorU32(ImGuiCol_Text),
					a_state.activeHostPage == HostPageKind::kSettings,
					iconSize))
			{
				NavigateToHostPage(HostPageKind::kSettings, a_state);
			}
			ImGui::SetCursorScreenPos(start);
			ImGui::Dummy({ maxX - start.x, rowHeight });
			DrawStatusDetails(a_state);
		}

		void SaveLayout() noexcept
		{
			const auto& io = ImGui::GetIO();
			if (io.IniFilename)
				ImGui::SaveIniSettingsToDisk(io.IniFilename);
		}

		void CloseShellAndSaveLayout() noexcept
		{
			(void)SetMenuVisible(false);
			SaveLayout();
		}
	}

#if defined(DMUI_PREVIEW)
	void ConfigurePreviewHostPage(HostPageKind a_page) noexcept
	{
		NavigateToHostPage(a_page, State());
	}

	void ConfigurePreviewSidebarComparison(
		std::optional<SidebarLayoutKind> a_layoutOverride,
		bool a_overrideExpandedClients,
		std::span<const std::string> a_expandedClients,
		std::optional<NavigationPresentationKind> a_presentationOverride,
		DMUI_ClientOrigin a_destinationOrigin)
	{
		auto& state = State();
		state.previewSidebarLayoutOverride = a_layoutOverride;
		state.previewPresentationOverride = a_presentationOverride;
		state.presentation.destinations.selectedOrigin = a_destinationOrigin;
		state.sidebar.drillDown = {};
		if (a_overrideExpandedClients)
		{
			state.sidebar.previewExpandedClients.emplace(
				a_expandedClients.begin(),
				a_expandedClients.end());
		}
		else
			state.sidebar.previewExpandedClients.reset();
	}
#endif

	void DrawShell() noexcept
	{
		auto& state = State();
		HostSettings::SetPageActive(
			state.activeHostPage == HostPageKind::kSettings);
		Theme::ApplyStyle();
		const auto sidebarLayout = ResolveSidebarLayout(
			HostSettings::EffectivePreview().sidebarLayout,
#if defined(DMUI_PREVIEW)
			state.previewSidebarLayoutOverride
#else
			std::nullopt
#endif
		);
		if (state.sidebarLayout != sidebarLayout)
		{
			state.sidebarLayout = sidebarLayout;
			state.sidebar.drillDown = {};
			ActivateSidebarLayout(
				state.sidebarLayout,
				Navigation(),
				state,
				state.sidebar);
		}

		const auto& model = Navigation();
		const auto requested = SelectedPage();
		if (requested != DMUI_INVALID_PAGE_HANDLE)
		{
			(void)ApplyShellNavigationRequest(
				model,
				NavigationRequest::Page(requested),
				state);
			ClearPageSelection(requested);
		}
		else if (!state.activeHostPage && !model.FindPage(state.activePage))
		{
			const auto fallback = model.FirstPage();
			if (fallback != DMUI_INVALID_PAGE_HANDLE)
			{
				(void)ApplyShellNavigationRequest(
					model,
					NavigationRequest::Page(fallback),
					state);
			}
		}
		SetActivePage(
			state.activeHostPage ?
				DMUI_INVALID_PAGE_HANDLE :
				state.activePage);
#if defined(DMUI_PREVIEW)
		ApplyPreviewExpandedClients(model, state);
#endif

		const auto* viewport = ImGui::GetMainViewport();
		ImGui::DockSpaceOverViewport(
			0,
			viewport,
			ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::SetNextWindowPos(
			{ viewport->Size.x * 0.5f, viewport->Size.y * 0.5f },
			ImGuiCond_FirstUseEver,
			{ 0.5f, 0.5f });
		ImGui::SetNextWindowSize(
			{ viewport->Size.x * 0.90f, viewport->Size.y * 0.90f },
			ImGuiCond_FirstUseEver);
		ImGuiWindowClass windowClass{};
		windowClass.ClassId = ImHashStr("DearModdingUI.Host");
		windowClass.DockingAllowUnclassed = true;
		ImGui::SetNextWindowClass(&windowClass);

		auto open = true;
		auto windowFlags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoScrollbar;
		static bool wasDocked = false;
		if (!wasDocked)
			windowFlags |= ImGuiWindowFlags_NoTitleBar;
		const auto visible = BeginWithRoundedTitleBarButtons(
			"Evil Modding###DearModdingUI.Host",
			&open,
			windowFlags);
		wasDocked = ImGui::IsWindowDocked();

		const auto position = ImGui::GetWindowPos();
		const auto size = ImGui::GetWindowSize();
		const auto framebufferScale =
			ImGui::GetIO().DisplayFramebufferScale;
		BackgroundBlur::SetHostWindow(
			(position.x - viewport->Pos.x) * framebufferScale.x,
			(position.y - viewport->Pos.y) * framebufferScale.y,
			(position.x + size.x - viewport->Pos.x) * framebufferScale.x,
			(position.y + size.y - viewport->Pos.y) * framebufferScale.y,
			ImGui::GetStyle().WindowRounding *
				(std::max)(framebufferScale.x, framebufferScale.y));

		if (visible)
		{
			const auto drawHeaderClose = ShouldDrawHeaderClose(
				ImGui::IsWindowDocked(),
				(ImGui::GetCurrentWindow()->Flags &
					ImGuiWindowFlags_NoTitleBar) != 0);
			if (DrawHeader(model, state, drawHeaderClose))
				open = false;
			const auto footerHeight = ReservedFooterHeight(
				(std::max)(
					ImGui::GetFrameHeight(),
					HostChromeButtonExtent(
						ImGui::GetFontSize(),
						TitleBarButtonPadding())),
				ImGui::GetStyle().ItemSpacing.y,
				ImGui::GetStyle().WindowPadding.y,
				SeparatorThickness());
			ImGui::BeginChild(
				"Dear Modding Menus Table",
				{ 0.0f, -footerHeight });
			if (ImGui::BeginTable(
					"Dear Modding Menus Table",
					2,
					ImGuiTableFlags_SizingStretchProp |
						ImGuiTableFlags_Resizable))
			{
				ImGui::TableSetupColumn(
					"##DearModdingList",
					ImGuiTableColumnFlags_None,
					3.5f);
				ImGui::TableSetupColumn(
					"##DearModdingPage",
					ImGuiTableColumnFlags_None,
					6.5f);
				const auto statuses =
					RollupClientStatuses(CurrentClientStatuses());
				const auto sidebar = DrawSidebar(
					model,
					statuses,
					state,
					state.sidebarLayout,
					ActivePresentationKind(state),
					state.presentation,
					state.sidebar);
				if (sidebar.openPalette)
					state.palette.RequestOpen();
				if (sidebar.request)
				{
					(void)ApplyShellNavigationRequest(
						model,
						*sidebar.request,
						state);
					SetActivePage(
						state.activeHostPage ?
							DMUI_INVALID_PAGE_HANDLE :
							state.activePage);
				}
				DrawContent(model, state);
				ImGui::EndTable();
			}
			ImGui::EndChild();

			if (const auto* activation =
					DrawCommandPalette(model, state, state.palette))
			{
				if (activation->kind == NavigationItemKind::kClient)
					(void)ApplyShellNavigationRequest(
						model,
						NavigationRequest::Client(activation->client),
						state);
				else if (activation->kind == NavigationItemKind::kPage)
					(void)ApplyShellNavigationRequest(
						model,
						NavigationRequest::Page(activation->page),
						state);
				else
					(void)InvokeAction(activation->action);
			}

			ImGui::Spacing();
			ImGui::SeparatorEx(
				ImGuiSeparatorFlags_Horizontal,
				SeparatorThickness());
			const auto footerPosition = ImGui::GetCursorScreenPos();
			ImGui::SetCursorScreenPos({
				footerPosition.x,
				footerPosition.y + FooterRowAdjustmentY(
					ImGui::GetStyle().ItemSpacing.y,
					ImGui::GetStyle().WindowPadding.y)
			});
			DrawFooter(model, state);
		}
		ImGui::End();
		if (!open)
			CloseShellAndSaveLayout();
	}

	void ApplyMenuEscapeDismissal() noexcept
	{
		if (ConsumeMenuEscapeTarget(MenuEscapeTarget::kInteraction))
			return;
		if (DismissCapturedMenuPopup())
			return;
		if (!ConsumeMenuEscapeTarget(MenuEscapeTarget::kHost))
			return;
		CloseShellAndSaveLayout();
	}
}
