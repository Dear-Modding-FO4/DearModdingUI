#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/controls/ChromeGeometry.h>
#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <DearModdingUI/host/UIAdapter.h>
#include <DearModdingUI/TextView.h>
#include "../support/ImGuiTestContext.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace ImStb
{
#include <imgui/imstb_textedit.h>
}

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;
	using namespace support::host;

	namespace
	{
		inline constexpr DMUI_ClientHandle kResizeTestClient{ 0x51u };

		inline constexpr ImVec4 kRowHoverColor{
			0.13f,
			0.47f,
			0.91f,
			1.0f
		};

		struct RowFrame
		{
			RowResult row;
			bool hovered{};
			bool active{};
			bool highlightRendered{};
			ImRect highlightBounds;
			ImGuiID id{};
		};

		class InteractiveRow
		{
		public:
			InteractiveRow()
			{
				auto& io = ImGui::GetIO();
				io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
				ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered] =
					kRowHoverColor;
			}

			RowFrame Frame(
				const RowOptions& a_options,
				ImVec2 a_mouse,
				bool a_mouseDown,
				bool a_focusRow = false,
				bool a_disabled = false)
			{
				return RenderFrame(
					[&] { return DrawSelectableRow(a_options); },
					a_mouse, a_mouseDown, a_focusRow, a_disabled);
			}

			RowFrame HeadingFrame(
				const RuledHeadingOptions& a_options,
				ImVec2 a_mouse,
				bool a_mouseDown,
				bool a_focusRow = false)
			{
				return RenderFrame(
					[&] {
						DrawRuledHeading(a_options);
						return RowResult{
							.rect = { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() }
						};
					},
					a_mouse, a_mouseDown, a_focusRow, false);
			}

			void Key(ImGuiKey a_key, bool a_down)
			{
				ImGui::GetIO().AddKeyEvent(a_key, a_down);
			}

		private:
			template <class Draw>
			RowFrame RenderFrame(
				Draw a_draw,
				ImVec2 a_mouse,
				bool a_mouseDown,
				bool a_focusRow,
				bool a_disabled)
			{
				auto& io = ImGui::GetIO();
				io.AddMousePosEvent(a_mouse.x, a_mouse.y);
				io.AddMouseButtonEvent(ImGuiMouseButton_Left, a_mouseDown);
				m_imgui.BeginWindow(
					"##SelectableRowTest",
					{ 0.0f, 0.0f },
					{ 420.0f, 180.0f },
					ImGuiWindowFlags_NoDecoration |
						ImGuiWindowFlags_NoSavedSettings,
					ImGuiCond_Always);
				ImGui::SetCursorScreenPos({ 20.0f, 40.0f });
				if (a_disabled)
					ImGui::BeginDisabled();
				const auto row = a_draw();
				RowFrame result{
					.row = row,
					.hovered = ImGui::IsItemHovered(),
					.active = ImGui::IsItemActive(),
					.id = ImGui::GetItemID()
				};
				const auto color = ImGui::GetColorU32(kRowHoverColor);
				for (const auto& vertex :
					ImGui::GetWindowDrawList()->VtxBuffer)
				{
					if (vertex.col != color)
						continue;
					if (!result.highlightRendered)
					{
						result.highlightBounds = { vertex.pos, vertex.pos };
						result.highlightRendered = true;
					}
					else
					{
						result.highlightBounds.Min.x = (std::min)(
							result.highlightBounds.Min.x,
							vertex.pos.x);
						result.highlightBounds.Min.y = (std::min)(
							result.highlightBounds.Min.y,
							vertex.pos.y);
						result.highlightBounds.Max.x = (std::max)(
							result.highlightBounds.Max.x,
							vertex.pos.x);
						result.highlightBounds.Max.y = (std::max)(
							result.highlightBounds.Max.y,
							vertex.pos.y);
					}
				}
				if (a_focusRow)
				{
					ImGui::SetNavWindow(ImGui::GetCurrentWindow());
					ImGui::SetNavID(
						result.id,
						ImGuiNavLayer_Main,
						ImGui::GetCurrentFocusScope(),
						ImGui::WindowRectAbsToRel(
							ImGui::GetCurrentWindow(),
							row.rect));
					ImGui::GetCurrentContext()->NavCursorVisible = true;
				}
				if (a_disabled)
					ImGui::EndDisabled();
				m_imgui.EndWindow(true);
				return result;
			}

			support::ImGuiTestContext m_imgui{
				{
					.displaySize = { 640.0f, 360.0f },
					.disableInputTrickle = true,
					.disableErrorRecovery = true
				}
			};
		};

		[[nodiscard]] ImVec2 ArrowPoint(const ImRect& a_rect)
		{
			return {
				a_rect.Min.x + ImGui::GetFontSize() * 0.5f,
				a_rect.GetCenter().y
			};
		}

		[[nodiscard]] ImVec2 LabelPoint(const ImRect& a_rect)
		{
			return {
				a_rect.Min.x + ImGui::GetFontSize() * 2.5f,
				a_rect.GetCenter().y
			};
		}

		void RequireFullHighlight(const RowFrame& a_frame)
		{
			require(a_frame.hovered, "row hover API lost the hovered row");
			require(
				a_frame.highlightRendered,
				"row hover highlight was not rendered");
			constexpr auto epsilon = 0.01f;
			require(
				a_frame.highlightBounds.Min.x <=
						a_frame.row.rect.Min.x + epsilon &&
					a_frame.highlightBounds.Min.y <=
						a_frame.row.rect.Min.y + epsilon &&
					a_frame.highlightBounds.Max.x >=
						a_frame.row.rect.Max.x - epsilon &&
					a_frame.highlightBounds.Max.y >=
						a_frame.row.rect.Max.y - epsilon,
				"row hover highlight did not cover the full row");
		}

		void SetTestClipboard(std::string& a_text)
		{
			auto& platform = ImGui::GetPlatformIO();
			platform.Platform_ClipboardUserData = &a_text;
			platform.Platform_GetClipboardTextFn =
				[](ImGuiContext* a_context) {
					return static_cast<const std::string*>(
							   a_context->PlatformIO.
								   Platform_ClipboardUserData)
						->c_str();
				};
		}

		[[nodiscard]] std::pair<std::string, bool> EditSearch(
			std::string a_initial,
			size_t a_maximumBytes,
			const char* a_input)
		{
			std::string clipboard{ a_input };
			support::ImGuiTestContext imgui{
				{
					.disableInputTrickle = true,
					.disableErrorRecovery = true
				}
			};
			SetTestClipboard(clipboard);
			std::vector<char> buffer(a_maximumBytes + 1);
			std::ranges::copy(a_initial, buffer.begin());
			const auto beginFrame = [&] {
				imgui.BeginWindow(
					"##SearchCapacity",
					{ 0.0f, 0.0f },
					{ 640.0f, 180.0f },
					ImGuiWindowFlags_NoDecoration |
						ImGuiWindowFlags_NoSavedSettings,
					ImGuiCond_Always);
				ImGui::SetCursorScreenPos({ 20.0f, 20.0f });
			};
			beginFrame();
			(void)DrawSearchInput(
				"SearchCapacity",
				"Search...",
				buffer.data(),
				buffer.size());
			imgui.EndWindow(true);

			auto& io = ImGui::GetIO();
			io.AddMousePosEvent(80.0f, 32.0f);
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
			beginFrame();
			(void)DrawSearchInput(
				"SearchCapacity",
				"Search...",
				buffer.data(),
				buffer.size());
			imgui.EndWindow(true);

			io.AddMousePosEvent(80.0f, 32.0f);
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
			beginFrame();
			(void)DrawSearchInput(
				"SearchCapacity",
				"Search...",
				buffer.data(),
				buffer.size());
			const auto active = ImGui::IsItemActive();
			imgui.EndWindow(true);
			require(active,
				"fixed search did not retain focus after mouse release");

			io.AddKeyEvent(ImGuiMod_Ctrl, true);
			io.AddKeyEvent(ImGuiKey_V, true);
			beginFrame();
			const auto changed = DrawSearchInput(
				"SearchCapacity",
				"Search...",
				buffer.data(),
				buffer.size());
			imgui.EndWindow(true);
			io.AddKeyEvent(ImGuiKey_V, false);
			io.AddKeyEvent(ImGuiMod_Ctrl, false);
			return { std::string{ buffer.data() }, changed };
		}

		struct RejectingTextBuffer
		{
			explicit RejectingTextBuffer(std::string_view a_text)
			{
				storage.resize(16);
				std::ranges::copy(a_text, storage.begin());
			}

			[[nodiscard]] DMUI_TextBuffer Descriptor() noexcept
			{
				return {
					.structSize = sizeof(DMUI_TextBuffer),
					.data = storage.data(),
					.capacity = storage.size(),
					.resize = Resize,
					.userData = this
				};
			}

			static DMUI_Result DMUI_CALL Resize(
				void* a_userData,
				size_t,
				char**,
				size_t*) noexcept
			{
				auto& self =
					*static_cast<RejectingTextBuffer*>(a_userData);
				++self.resizeCalls;
				self.renderExecutionActive = RenderExecution::IsActive();
				self.drawingClientVisible =
					RenderExecution::IsActiveClient(
						kResizeTestClient,
						true);
				return DMUI_RESULT_BUFFER_TOO_SMALL;
			}

			std::vector<char> storage;
			size_t resizeCalls{};
			bool renderExecutionActive{};
			bool drawingClientVisible{};
		};

		[[nodiscard]] DMUI_Result DrawBufferSearchFrame(
			support::ImGuiTestContext& a_imgui,
			DMUI_TextBuffer& a_buffer,
			bool& a_changed,
			ImGuiID* a_inputId = nullptr)
		{
			a_imgui.BeginWindow(
				"##GrowableSearch",
				{ 0.0f, 0.0f },
				{ 640.0f, 180.0f },
				ImGuiWindowFlags_NoDecoration |
					ImGuiWindowFlags_NoSavedSettings,
				ImGuiCond_Always);
			ImGui::SetCursorScreenPos({ 20.0f, 20.0f });
			const auto result = DrawSearchInput(
				"GrowableSearch",
				"Search...",
				a_buffer,
				a_changed);
			if (a_inputId)
				*a_inputId = ImGui::GetItemID();
			a_imgui.EndWindow(true);
			return result;
		}

	}

	void run_host_control_checks(Runner& runner)
	{
		runner.test("text navigation preserves literal labels and wrapped button geometry", [] {
			support::ImGuiTestContext imgui;
			imgui.BeginWindow("##TextNavigation", { 0, 0 }, { 480, 240 });
			RenderExecution::Guard execution{ RenderExecution::Phase::kFrameDraw };
			RenderExecution::ClientGuard client{ 1, true };
			dmui::ui::detail::ScopedContext context{ &UI::API(), 1 };
			const auto& style = ImGui::GetStyle();
			const DMUI_StyleMetrics metrics{
				.structSize = sizeof(DMUI_StyleMetrics),
				.itemSpacing = { style.ItemSpacing.x, style.ItemSpacing.y },
				.framePadding = { style.FramePadding.x, style.FramePadding.y }
			};
			const std::array<size_t, 1> lines{ 0 };
			const dmui::TextViewRequest request{ .text = "abc", .lineOffsets = lines };
			dmui::TextViewState state;
			using Item = std::pair<std::string_view, size_t>;
			const std::array<Item, 2> items{
				std::pair{ "A", 0u }, std::pair{ "##B", 2u }
			};
			const auto origin = ImGui::GetCursorScreenPos();
			const auto firstWidth = ImGui::CalcTextSize("A").x + style.FramePadding.x * 2;
			const auto expectedSecondX = origin.x + firstWidth + style.ItemSpacing.x;
			const auto verticesBefore = ImGui::GetWindowDrawList()->VtxBuffer.Size;
			(void)dmui::DrawTextViewNavigation(
				"sections", std::span<const Item>{ items }, metrics, request, state,
				[](const auto& item) { return item; });
			require(
				std::abs(ImGui::GetItemRectMin().x - expectedSecondX) < 0.01f &&
					ImGui::GetItemRectSize().y >= ImGui::GetFrameHeight(),
				"literal text overlay changed the next button's layout anchor or height");
			size_t textVertices{};
			const auto* draw = ImGui::GetWindowDrawList();
			for (int i = verticesBefore; i < draw->VtxBuffer.Size; ++i)
				if (draw->VtxBuffer[i].col == ImGui::GetColorU32(ImGuiCol_Text))
					++textVertices;
			require(textVertices == 16, "double-hash navigation label was not drawn literally");

			const auto available = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available - 60.0f);
			const auto right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
			const std::array<Item, 1> longItem{
				std::pair{ "A long section label that must fit", 0u }
			};
			(void)dmui::DrawTextViewNavigation(
				"narrow", std::span<const Item>{ longItem }, metrics, request, state,
				[](const auto& item) { return item; });
			require(ImGui::GetItemRectMax().x <= right + 0.01f,
				"navigation button overflowed its narrow pane");
			require(context.Result() == DMUI_RESULT_OK,
				"navigation helper failed through the production UI adapter");
			imgui.EndWindow(true);
		});

		runner.test("host search rejects invalid capacity without changing the query", [] {
			std::string query(513, 'a');
			require(
				DrawSearchInput("search", "Search...", query, 512) ==
					DMUI_RESULT_INVALID_ARGUMENT &&
					query == std::string(513, 'a'),
				"oversized query was silently accepted or truncated");
			query = "query";
			require(
				DrawSearchInput("search", "Search...", query,
					static_cast<size_t>((std::numeric_limits<int>::max)())) ==
						DMUI_RESULT_INVALID_ARGUMENT &&
					query == "query",
				"unsafe ImGui buffer capacity was accepted");
		});

		runner.test("growable search accepts a large UTF8 paste across borrowed frames", [] {
			std::string clipboard(4094, 'a');
			clipboard += "\xC3\xA9";
			support::ImGuiTestContext imgui{
				{
					.disableInputTrickle = true,
					.disableErrorRecovery = true
				}
			};
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameDraw
			};
			RenderExecution::ClientGuard client{
				kResizeTestClient,
				true
			};
			SetTestClipboard(clipboard);
			std::string query;
			const auto drawFrame = [&] {
				imgui.BeginWindow(
					"##GrowableStringSearch",
					{ 0.0f, 0.0f },
					{ 640.0f, 180.0f },
					ImGuiWindowFlags_NoDecoration |
						ImGuiWindowFlags_NoSavedSettings,
					ImGuiCond_Always);
				ImGui::SetCursorScreenPos({ 20.0f, 20.0f });
				const auto result = DrawSearchInput(
					"GrowableStringSearch",
					"Search...",
					query);
				imgui.EndWindow(true);
				return result;
			};

			require(drawFrame() == DMUI_RESULT_OK,
				"growable search failed before activation");
			auto& io = ImGui::GetIO();
			io.AddMousePosEvent(80.0f, 32.0f);
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
			require(drawFrame() == DMUI_RESULT_OK,
				"growable search failed during activation");
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
			require(
				drawFrame() == DMUI_RESULT_OK && query.empty() &&
					ImGui::GetCurrentContext()->ActiveId != 0 &&
					ImGui::GetCurrentContext()->InputTextState.ID ==
						ImGui::GetCurrentContext()->ActiveId,
				"growable search did not retain focus after mouse release");
			io.AddKeyEvent(ImGuiMod_Ctrl, true);
			io.AddKeyEvent(ImGuiKey_V, true);
			require(drawFrame() == DMUI_RESULT_OK && query == clipboard,
				"single large UTF8 paste did not grow the search buffer");

			io.AddKeyEvent(ImGuiKey_V, false);
			io.AddKeyEvent(ImGuiMod_Ctrl, false);
			require(drawFrame() == DMUI_RESULT_OK && query == clipboard,
				"recreated borrowed search storage lost the active edit");

			RejectingTextBuffer deactivatedOwner{ "safe" };
			auto deactivatedBuffer = deactivatedOwner.Descriptor();
			bool changed{};
			imgui.BeginWindow(
				"##GrowableStringSearch",
				{ 0.0f, 0.0f },
				{ 640.0f, 180.0f },
				ImGuiWindowFlags_NoDecoration |
					ImGuiWindowFlags_NoSavedSettings,
				ImGuiCond_Always);
			ImGui::SetCursorScreenPos({ 20.0f, 20.0f });
			ImGui::SetActiveID(
				ImGui::GetID("##OtherInput"),
				ImGui::GetCurrentWindow());
			const auto deactivatedResult = DrawSearchInput(
				"GrowableStringSearch",
				"Search...",
				deactivatedBuffer,
				changed);
			imgui.EndWindow(true);
			require(
				deactivatedResult == DMUI_RESULT_BUFFER_TOO_SMALL &&
					!changed &&
					std::string_view{ deactivatedBuffer.data } == "safe" &&
					deactivatedOwner.resizeCalls == 1 &&
					deactivatedOwner.renderExecutionActive &&
					!deactivatedOwner.drawingClientVisible,
				"deactivated writeback hid failure or changed replacement storage");
		});

		runner.test("failed search growth preserves native edit state", [] {
			std::string clipboard(128, 'x');
			support::ImGuiTestContext imgui{
				{
					.disableInputTrickle = true,
					.disableErrorRecovery = true
				}
			};
			RenderExecution::Guard execution{
				RenderExecution::Phase::kFrameDraw
			};
			RenderExecution::ClientGuard client{
				kResizeTestClient,
				true
			};
			SetTestClipboard(clipboard);
			RejectingTextBuffer owner{ "abcdef" };
			auto descriptor = owner.Descriptor();
			bool changed{};
			require(
				DrawBufferSearchFrame(imgui, descriptor, changed) ==
						DMUI_RESULT_OK &&
					!changed,
				"rejecting search failed before activation");

			auto& io = ImGui::GetIO();
			io.AddMousePosEvent(80.0f, 32.0f);
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
			require(
				DrawBufferSearchFrame(imgui, descriptor, changed) ==
					DMUI_RESULT_OK,
				"rejecting search failed during activation");
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
			io.AddInputCharactersUTF8("g");
			ImGuiID inputId{};
			require(
				DrawBufferSearchFrame(
					imgui,
					descriptor,
					changed,
					&inputId) ==
						DMUI_RESULT_OK &&
					changed,
				"setup edit did not populate native undo state");

			auto* state = ImGui::GetInputTextState(inputId);
			require(state && state->Stb,
				"active search did not retain native edit state");
			state->SetSelection(1, 4);
			const auto cursor = state->Stb->cursor;
			const auto selectionStart = state->Stb->select_start;
			const auto selectionEnd = state->Stb->select_end;
			const auto undo = state->Stb->undostate;
			const std::string original{ descriptor.data };
			const auto* originalData = descriptor.data;
			const auto originalCapacity = descriptor.capacity;

			io.AddKeyEvent(ImGuiMod_Ctrl, true);
			io.AddKeyEvent(ImGuiKey_V, true);
			require(
				DrawBufferSearchFrame(imgui, descriptor, changed) ==
						DMUI_RESULT_BUFFER_TOO_SMALL &&
					!changed,
				"failed resize did not report its explicit result");
			state = ImGui::GetInputTextState(inputId);
			require(
				owner.resizeCalls == 1 &&
					owner.renderExecutionActive &&
					!owner.drawingClientVisible &&
					descriptor.data == originalData &&
					descriptor.capacity == originalCapacity &&
					std::string_view{ descriptor.data } == original &&
					state && state->Stb->cursor == cursor &&
					state->Stb->select_start == selectionStart &&
					state->Stb->select_end == selectionEnd &&
					std::memcmp(
						&state->Stb->undostate,
						&undo,
						sizeof(undo)) == 0,
				"failed resize truncated text or changed cursor, selection, or undo");

			io.AddKeyEvent(ImGuiKey_V, false);
			io.AddKeyEvent(ImGuiMod_Ctrl, false);
			require(
				DrawBufferSearchFrame(imgui, descriptor, changed) ==
						DMUI_RESULT_OK &&
					!changed &&
					std::string_view{ descriptor.data } == original &&
					owner.resizeCalls == 1,
				"rejected paste was replayed on the following frame");
		});

		runner.test("search input honors caller UTF8 capacity", [] {
			const auto at255 = EditSearch(std::string(255, 'a'), 512, "b");
			require(
				at255.second && at255.first.size() == 256 &&
					at255.first.find('b') != std::string::npos,
				"255-byte search did not grow through the shared control");

			const auto at256 = EditSearch(std::string(256, 'a'), 512, "b");
			require(
				at256.second && at256.first.size() == 257 &&
					at256.first.find('b') != std::string::npos,
				"256-byte search was truncated by a fixed host buffer");

			const auto at512 = EditSearch(std::string(511, 'a'), 512, "b");
			require(
				at512.second && at512.first.size() == 512 &&
					at512.first.find('b') != std::string::npos,
				"512-byte client capacity was not usable");

			const auto completeUtf8 =
				EditSearch(std::string(510, 'a'), 512, "\xC3\xA9");
			require(
				completeUtf8.second &&
					completeUtf8.first.size() == 512 &&
					completeUtf8.first.find("\xC3\xA9") != std::string::npos,
				"complete multibyte input was not preserved at the boundary");

			const auto rejectedUtf8 =
				EditSearch(std::string(511, 'a'), 512, "\xC3\xA9");
			require(
				!rejectedUtf8.second &&
					rejectedUtf8.first == std::string(511, 'a'),
				"partial multibyte input was accepted or truncated");

			const auto partialInput =
				EditSearch(std::string(511, 'a'), 512, "b\xC3\xA9");
			require(
				partialInput.second && partialInput.first.size() == 512 &&
					std::ranges::count(partialInput.first, 'a') == 511 &&
					partialInput.first.find('b') != std::string::npos,
				"bounded input lost existing text or split the final UTF8 sequence");
		});

		runner.test("host close and footer gear stay clear of adjacent content", [] {
			require(ShouldDrawHeaderClose(false, true),
				"undocked titleless host lost its close button");
			require(
					!ShouldDrawHeaderClose(true, true) &&
						!ShouldDrawHeaderClose(true, false) &&
						!ShouldDrawHeaderClose(false, false),
					"host close duplicated a native or docked close affordance");
			struct Case
			{
				float fontSize;
				float uiScale;
			};
			constexpr std::array cases{
				Case{ 16.0f, 1.0f },
				Case{ 18.0f, 1.25f },
				Case{ 21.0f, 1.5f },
				Case{ 28.0f, 2.0f }
			};
			for (const auto& test : cases)
			{
				const auto fontSize = test.fontSize * test.uiScale;
				const auto padding = 2.0f * test.uiScale;
				const auto spacing = 8.0f * test.uiScale;
				const auto iconSize = HostChromeIconSize(fontSize);
				const auto extent = HostChromeButtonExtent(fontSize, padding);
				const auto header = ResolveTrailingControlLayout(
					24.0f, 1896.0f, extent, spacing);
				const auto footer = ResolveTrailingControlLayout(
					36.0f, 1264.0f, extent, spacing);
				require(
						iconSize == fontSize * 1.5f &&
							extent > TitleBarButtonExtent(fontSize, padding),
						"host chrome did not use its larger icon scale");
				require(
						header.controlMaxX == 1896.0f &&
							header.controlMinX == 1896.0f - extent &&
							header.adjacentMaxX ==
								header.controlMinX - spacing,
						"close button geometry changed");
				require(
						footer.controlMaxX == 1264.0f &&
							footer.controlMinX == 1264.0f - extent &&
							footer.adjacentMaxX ==
								footer.controlMinX - spacing,
						"footer gear geometry changed");
				require(
						header.adjacentMaxX <= header.controlMinX &&
							footer.adjacentMaxX <= footer.controlMinX,
						"host chrome overlapped adjacent content");
			}
		});

		runner.test(
			"collapsible row hover remains owned by the full row",
			[] {
				constexpr std::array cases{
					RowHighlightStyle::kSelectable,
					RowHighlightStyle::kRoundedFill
				};
				for (const auto& test : cases)
				{
					InteractiveRow ui;
					bool expanded{};
					const RowOptions options{
						.id = "CollapsibleRow",
						.label = "Collapsible row",
						.leadingAffordance =
							RowLeadingAffordance::kArrow,
						.expanded = &expanded,
						.textColor =
							ImGui::GetColorU32(ImGuiCol_Text),
						.hoveredTextColor =
							ImGui::GetColorU32(ImGuiCol_Text),
						.highlightStyle = test
					};
					const auto initial = ui.Frame(
						options,
						{ -100.0f, -100.0f },
						false);
					const auto arrow = ArrowPoint(initial.row.rect);
					(void)ui.Frame(options, arrow, false);
					RequireFullHighlight(ui.Frame(options, arrow, false));

					const auto label = LabelPoint(initial.row.rect);
					RequireFullHighlight(ui.Frame(options, label, false));
					RequireFullHighlight(ui.Frame(options, arrow, false));
					RequireFullHighlight(ui.Frame(options, arrow, false));
				}
			});

		runner.test(
			"expandable rows share whole-row mouse keyboard and disabled behavior",
			[] {
				for (const auto highlight :
					{ RowHighlightStyle::kSelectable, RowHighlightStyle::kRoundedFill })
				{
					InteractiveRow ui;
					bool expanded{};
					const RowOptions options{
						.id = "ExpandableRow",
						.label = "Expandable row",
						.selected = true,
						.leadingAffordance = RowLeadingAffordance::kArrow,
						.expanded = &expanded,
						.textColor = ImGui::GetColorU32(ImGuiCol_Text),
						.hoveredTextColor = ImGui::GetColorU32(ImGuiCol_Text),
						.highlightStyle = highlight
					};
					const auto initial = ui.Frame(
						options, { -100.0f, -100.0f }, false);
					const auto arrow = ArrowPoint(initial.row.rect);
					const auto label = LabelPoint(initial.row.rect);
					(void)ui.Frame(options, arrow, false);
					const auto arrowDown = ui.Frame(options, arrow, true);
					require(arrowDown.active && !expanded,
						"row toggled before release or did not own the press");
					const auto arrowRelease = ui.Frame(options, arrow, false);
					require(expanded && arrowRelease.row.pressed,
						"arrow click did not activate and expand the row");

					for (const auto expected : { false, true })
					{
						(void)ui.Frame(options, label, false);
						(void)ui.Frame(options, label, true);
						const auto released = ui.Frame(options, label, false);
						require(released.row.pressed && expanded == expected,
							"label clicks did not collapse and reopen the row");
					}

					(void)ui.Frame(options, arrow, true);
					require(ui.Frame(options, label, false).row.pressed && !expanded,
						"arrow-to-label release did not stay within the same row");
					(void)ui.Frame(options, label, true);
					require(ui.Frame(options, arrow, false).row.pressed && expanded,
						"label-to-arrow release did not stay within the same row");

					(void)ui.Frame(options, arrow, true);
					require(
						!ui.Frame(options, { 600.0f, 300.0f }, false).row.pressed &&
							expanded,
						"release outside the row did not cancel the toggle");

					for (const auto key : { ImGuiKey_Enter, ImGuiKey_Space })
					{
						const auto before = expanded;
						(void)ui.Frame(options, arrow, false, true);
						ui.Key(key, true);
						const auto keyboard = ui.Frame(options, arrow, false);
						ui.Key(key, false);
						(void)ui.Frame(options, arrow, false);
						require(keyboard.row.pressed && expanded != before,
							"keyboard activation did not toggle the whole row");
					}

					const auto before = expanded;
					(void)ui.Frame(options, arrow, true, false, true);
					const auto disabledRelease =
						ui.Frame(options, arrow, false, false, true);
					require(!disabledRelease.row.pressed && expanded == before,
						"disabled row accepted a toggle");
					(void)ui.Frame(options, arrow, false, true, true);
					ui.Key(ImGuiKey_Enter, true);
					const auto disabledKeyboard = ui.Frame(options, arrow, false, false, true);
					ui.Key(ImGuiKey_Enter, false);
					(void)ui.Frame(options, arrow, false, false, true);
					require(!disabledKeyboard.row.pressed && expanded == before,
						"disabled row accepted keyboard activation");
				}
			});

		runner.test("leading and centered headings share expansion interaction", [] {
			for (const auto layout :
				{ RuledHeadingLayout::kLeadingRow, RuledHeadingLayout::kCentered })
			{
				InteractiveRow ui;
				bool expanded{};
				const RuledHeadingOptions options{
					.key = "SharedHeading",
					.text = "Shared heading",
					.expanded = &expanded,
					.layout = layout
				};
				(void)ui.HeadingFrame(options, { -100.0f, -100.0f }, false);
				const auto initial = ui.HeadingFrame(
					options, { -100.0f, -100.0f }, false);
				const auto point = initial.row.rect.GetCenter();
				for (const auto expected : { true, false, true })
				{
					(void)ui.HeadingFrame(options, point, false);
					(void)ui.HeadingFrame(options, point, true);
					(void)ui.HeadingFrame(options, point, false);
					require(expanded == expected,
						"heading layout changed whole-row toggle behavior");
				}
				(void)ui.HeadingFrame(options, point, false, true);
				ui.Key(ImGuiKey_Enter, true);
				(void)ui.HeadingFrame(options, point, false);
				ui.Key(ImGuiKey_Enter, false);
				(void)ui.HeadingFrame(options, point, false);
				require(!expanded, "heading ignored keyboard expansion");
			}
		});
	}
}
