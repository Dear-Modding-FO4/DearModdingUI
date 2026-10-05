#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <DearModdingUI/host/UIAdapter.h>
#include "../support/ImGuiTestContext.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

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

			RejectingTextBuffer rejectingOwner{ "abcdef" };
			auto rejecting = rejectingOwner.Descriptor();
			require(
				DrawBufferSearchFrame(imgui, rejecting, changed) == DMUI_RESULT_OK,
				"rejecting search failed before activation");
			io.AddMousePosEvent(80.0f, 32.0f);
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
			require(
				DrawBufferSearchFrame(imgui, rejecting, changed) == DMUI_RESULT_OK,
				"rejecting search failed during activation");
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
			require(
				DrawBufferSearchFrame(imgui, rejecting, changed) == DMUI_RESULT_OK,
				"rejecting search lost focus after mouse release");
			io.AddKeyEvent(ImGuiMod_Ctrl, true);
			io.AddKeyEvent(ImGuiKey_V, true);
			require(
				DrawBufferSearchFrame(imgui, rejecting, changed) ==
						DMUI_RESULT_BUFFER_TOO_SMALL &&
					!changed &&
					std::string_view{ rejecting.data } == "abcdef" &&
					rejectingOwner.resizeCalls == 1 &&
					rejectingOwner.renderExecutionActive &&
					!rejectingOwner.drawingClientVisible,
				"failed resize truncated text or hid its explicit result");
			io.AddKeyEvent(ImGuiKey_V, false);
			io.AddKeyEvent(ImGuiMod_Ctrl, false);
			require(
				DrawBufferSearchFrame(imgui, rejecting, changed) == DMUI_RESULT_OK &&
					!changed && rejectingOwner.resizeCalls == 1,
				"rejected paste was replayed on the following frame");
		});

		runner.test("search input honors caller UTF8 capacity", [] {
			const auto at256 = EditSearch(std::string(256, 'a'), 512, "b");
			require(
				at256.second && at256.first.size() == 257 &&
					at256.first.find('b') != std::string::npos,
				"256-byte search was truncated by a fixed host buffer");

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
	}
}
