#include "../support/DearModdingUITestSupport.h"
#include "../support/ImGuiTestContext.h"

#include <DearModdingUI/navigation/SidebarView.h>
#include <imgui/imgui_internal.h>

#include <array>
#include <optional>

namespace
{
	const DearModdingUI::Registry* g_sidebarRegistry{};
}

namespace DearModdingUI
{
	bool PageFailed(DMUI_PageHandle a_page) noexcept
	{
		// The headless sidebar uses its fixture's registry instead of the game host.
		return !g_sidebarRegistry || g_sidebarRegistry->PageFailed(a_page);
	}
}

namespace vmm_tests
{
	using namespace DearModdingUI;
	using namespace support::host;

	namespace
	{
		enum class Header
		{
			Origin,
			Mod,
			Category
		};

		struct SidebarFrame
		{
			ImRect lastRow;
			std::optional<NavigationRequest> request;
		};

		class InteractiveSidebar
		{
		public:
			explicit InteractiveSidebar(
				Header a_header,
				DMUI_ClientOrigin a_origin = DMUI_CLIENT_ORIGIN_NATIVE,
				bool a_singlePage = false)
			{
				client = AddClient(
					m_registry, "sidebar.mod", "Sidebar Mod", m_callbacks,
					a_origin,
					a_origin == DMUI_CLIENT_ORIGIN_BRIDGED ? "MCM" : nullptr);
				AddCategory(m_registry, client, "general", "General");
				firstPage = AddPage(
					m_registry, client, "first", "First Page", "general", 0,
					DMUI_PAGE_KIND_SETTINGS, m_callbacks);
				lastPage = a_singlePage ? firstPage : AddPage(
					m_registry, client, "last", "Last Page", "general", 10,
					DMUI_PAGE_KIND_SETTINGS, m_callbacks);
				require(m_registry.Freeze(), "sidebar fixture did not freeze");
				g_sidebarRegistry = &m_registry;
				selection.activeClient = client;
				selection.activePage = lastPage;
				selection.activeHostPage.reset();
				const auto& model = m_registry.Navigation();
				browsing.originExpansion[SidebarOriginKey(model.sections.front())] = true;
				browsing.modExpansion["sidebar.mod"] = true;
				browsing.categoryExpansion[
					SidebarCategoryKey(model.clients.front(), "general")] = true;
				Expanded(a_header) = false;
			}

			~InteractiveSidebar()
			{
				g_sidebarRegistry = nullptr;
			}

			bool& Expanded(Header a_header)
			{
				const auto& model = m_registry.Navigation();
				switch (a_header)
				{
				case Header::Origin:
					return browsing.originExpansion.at(
						SidebarOriginKey(model.sections.front()));
				case Header::Mod:
					return browsing.modExpansion.at("sidebar.mod");
				case Header::Category:
					return browsing.categoryExpansion.at(
						SidebarCategoryKey(model.clients.front(), "general"));
				}
				throw Failure("unknown sidebar header");
			}

			SidebarFrame Frame(
				ImVec2 a_mouse,
				bool a_mouseDown,
				SidebarClientRowKind a_kind = SidebarClientRowKind::kTree)
			{
				auto& io = ImGui::GetIO();
				io.AddMousePosEvent(a_mouse.x, a_mouse.y);
				io.AddMouseButtonEvent(ImGuiMouseButton_Left, a_mouseDown);
				m_imgui.BeginWindow(
					"##SidebarInteractionTest", { 0.0f, 0.0f }, { 420.0f, 360.0f },
					ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings,
					ImGuiCond_Always);
				ImGui::SetCursorScreenPos({ 20.0f, 40.0f });
				SidebarNavigationIntent intent;
				const NavigationPresentation presentation{
					.sections = { { 0, true } }
				};
				const auto& model = m_registry.Navigation();
				DrawPresentedSidebarClients(
					{ model, {}, presentation, selection, browsing, intent }, a_kind);
				SidebarFrame result{
					.lastRow = { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() },
					.request = intent.request
				};
				if (intent.request)
				{
					require(
						ApplyNavigationRequest(model, *intent.request, selection).accepted,
						"sidebar offered an invalid navigation request");
					RevealSidebarSelection(
						a_kind == SidebarClientRowKind::kTree ? SidebarLayoutKind::Tree :
						a_kind == SidebarClientRowKind::kRail ? SidebarLayoutKind::IconRail :
							SidebarLayoutKind::TwoPane,
						model, selection, browsing);
				}
				m_imgui.EndWindow(true);
				return result;
			}

			DMUI_ClientHandle client{};
			DMUI_PageHandle firstPage{};
			DMUI_PageHandle lastPage{};
			ClientSelectionState selection;
			SidebarViewState browsing;

		private:
			Registry m_registry;
			CallbackState m_callbacks;
			support::ImGuiTestContext m_imgui{
				{ .displaySize = { 640.0f, 480.0f }, .disableInputTrickle = true }
			};
		};
	}

	void run_sidebar_interaction_checks(Runner& runner)
	{
		runner.test("origin mod and category rows toggle without navigating", [] {
			for (const auto origin :
				{ DMUI_CLIENT_ORIGIN_NATIVE, DMUI_CLIENT_ORIGIN_BRIDGED })
			{
				for (const auto header : { Header::Origin, Header::Mod, Header::Category })
				{
					for (const auto arrow : { false, true })
					{
						InteractiveSidebar ui{ header, origin };
						(void)ui.Frame({ -100.0f, -100.0f }, false);
						const auto initial = ui.Frame({ -100.0f, -100.0f }, false);
						const ImVec2 point{
							initial.lastRow.Min.x +
								ImGui::GetFontSize() * (arrow ? 0.5f : 2.5f),
							initial.lastRow.GetCenter().y
						};
						for (const auto expected : { true, false, true })
						{
							(void)ui.Frame(point, false);
							(void)ui.Frame(point, true);
							const auto released = ui.Frame(point, false);
							require(
								ui.Expanded(header) == expected,
								"expandable sidebar row did not toggle on consecutive clicks");
							require(
								!released.request &&
									ui.selection.activeClient == ui.client &&
									ui.selection.activePage == ui.lastPage,
								"container click navigated or reset the current page");
						}
					}
				}
			}
		});

		runner.test("single-page mods lists rails and page leaves retain navigation", [] {
			for (const auto kind :
				{ SidebarClientRowKind::kTree, SidebarClientRowKind::kList,
					SidebarClientRowKind::kRail })
			{
				InteractiveSidebar ui{ Header::Mod, DMUI_CLIENT_ORIGIN_NATIVE,
					kind == SidebarClientRowKind::kTree };
				SelectHostPage(HostPageKind::kHome, ui.selection);
				(void)ui.Frame({ -100.0f, -100.0f }, false, kind);
				const auto initial = ui.Frame({ -100.0f, -100.0f }, false, kind);
				const auto point = initial.lastRow.GetCenter();
				(void)ui.Frame(point, false, kind);
				(void)ui.Frame(point, true, kind);
				const auto released = ui.Frame(point, false, kind);
				require(
					released.request &&
						released.request->kind == NavigationRequestKind::Client &&
						ui.selection.activeClient == ui.client &&
						ui.selection.activePage == ui.firstPage,
					"non-expandable mod row did not select its landing page");
			}

			InteractiveSidebar ui{ Header::Category };
			ui.Expanded(Header::Category) = true;
			ui.selection.activePage = ui.firstPage;
			(void)ui.Frame({ -100.0f, -100.0f }, false);
			const auto initial = ui.Frame({ -100.0f, -100.0f }, false);
			const auto point = initial.lastRow.GetCenter();
			(void)ui.Frame(point, false);
			(void)ui.Frame(point, true);
			const auto released = ui.Frame(point, false);
			require(
				released.request &&
					released.request->kind == NavigationRequestKind::Page &&
					ui.selection.activePage == ui.lastPage,
				"page row did not select its page");
		});
	}
}
