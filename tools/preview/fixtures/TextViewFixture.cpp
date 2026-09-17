#include "TextViewFixture.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <span>
#include <string_view>
#include <utility>

namespace DmuiTestFixtures
{
	namespace
	{
		constexpr const char* kViewerId{ "##TextViewFixtureReader" };

		const std::string kTypedSuffix = std::string(300, 't') + "\xC3\xA9";
		const std::string kPastedQuery = std::string(4096, 'p') + "\xC3\xA9";

		[[nodiscard]] ImGuiWindow* FindTextViewWindow(ImGuiID a_id = 0)
		{
			auto* context = ImGui::GetCurrentContext();
			if (!context)
				return nullptr;
			if (a_id != 0)
				return ImGui::FindWindowByID(a_id);
			for (int index = context->Windows.Size - 1; index >= 0; --index)
			{
				auto* window = context->Windows[index];
				if ((window->Flags & ImGuiWindowFlags_ChildWindow) != 0 &&
					std::string_view{ window->Name }.contains(
						"TextViewFixtureReader"))
					return window;
			}
			return nullptr;
		}
	}

	bool TextViewFixture::Register(std::string& a_error)
	{
		a_error.clear();
		BuildContent();
		RebuildMatches();
		m_client = std::make_unique<dmui::Client>(
			"text-view",
			"Text viewer",
			dmui::Version{ 0, 1 },
			std::string_view{},
			dmui::ClientOrigin{},
			dmui::ClientOptions{
				.minimumHostAPISize = DMUI_HOST_API_DRAW_SEARCH_INPUT_BUFFER_SIZE
			});
		if (!m_client->Connect())
		{
			a_error = "Could not connect text-view fixture: ";
			a_error += DMUI_ResultToString(m_client->LastResult());
			return false;
		}
		if (!m_client->AddPage(
				{
					.id = "reader",
					.displayName = "Reader",
					.summary =
						"Public large-text rendering and navigation fixture."
				},
				[this] { Draw(); }))
		{
			a_error = "Could not register text-view reader page: ";
			a_error += DMUI_ResultToString(m_client->LastResult());
			return false;
		}
		return true;
	}

	bool TextViewFixture::ValidateCapture(std::string& a_error) const
	{
		a_error.clear();
		if (!m_callbackError.empty())
		{
			a_error = m_callbackError;
			return false;
		}
		if (!m_client)
		{
			a_error = "Text-view fixture was not registered.";
			return false;
		}
		if (!m_lastDrawSucceeded)
		{
			a_error =
				"Text-view capture did not complete a successful public draw.";
			return false;
		}
		if (!m_typedGrowthAccepted || !m_pasteGrowthAccepted || !m_searchRestored)
		{
			a_error = "Text-view capture did not complete growable search typing and paste.";
			return false;
		}
		if (m_state.activeMatch >= m_matchOffsets.size())
		{
			a_error = "Text-view capture has no active search match.";
			return false;
		}
		const auto activeOffset = m_matchOffsets[m_state.activeMatch];
		const auto hasOverlappingMatch = std::ranges::any_of(
			m_matchOffsets,
			[&](size_t a_offset) {
				return a_offset != activeOffset &&
					a_offset < activeOffset + m_query.size() &&
					activeOffset < a_offset + m_query.size();
			});
		if (!hasOverlappingMatch)
		{
			a_error =
				"Text-view capture active match has no overlapping peer.";
			return false;
		}
		if (m_state.revealByteOffset != dmui::kNoTextOffset)
		{
			a_error =
				"Text-view capture completed before the reveal was visible.";
			return false;
		}

		auto* window = FindTextViewWindow(m_childWindowId);
		if (!window || !window->Active)
		{
			a_error =
				"Text-view capture could not find the active renderer child.";
			return false;
		}
		if (window->Scroll.y <= 0.0f)
		{
			a_error =
				"Text-view capture remained at the beginning of the document.";
			return false;
		}
		if (window->ScrollMax.x <= 0.0f)
		{
			a_error =
				"Text-view capture did not retain horizontal content extent.";
			return false;
		}

		const auto activeColor =
			ImGui::GetColorU32(ImGuiCol_HeaderActive);
		const auto inactiveColor =
			ImGui::GetColorU32(ImGuiCol_TextSelectedBg);
		const auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
		int firstActive = -1;
		int lastInactive = -1;
		ImRect activeBounds;
		bool hasActiveBounds{};
		for (int index = 0; index < window->DrawList->VtxBuffer.Size; ++index)
		{
			const auto& vertex = window->DrawList->VtxBuffer[index];
			if (vertex.col == inactiveColor)
				lastInactive = index;
			if (vertex.col == activeColor)
			{
				if (firstActive < 0)
					firstActive = index;
				if (!hasActiveBounds)
				{
					activeBounds = { vertex.pos, vertex.pos };
					hasActiveBounds = true;
				}
				else
				{
					activeBounds.Add(vertex.pos);
				}
			}
			if (vertex.col == textColor &&
				(vertex.pos.x < window->ClipRect.Min.x - 0.01f ||
					vertex.pos.x > window->ClipRect.Max.x + 0.01f ||
					vertex.pos.y < window->ClipRect.Min.y - 0.01f ||
					vertex.pos.y > window->ClipRect.Max.y + 0.01f))
			{
				a_error =
					"Text-view capture emitted text outside the effective clip.";
				return false;
			}
		}
		if (firstActive < 0 || lastInactive < 0)
		{
			a_error =
				"Text-view capture did not draw both overlapping highlights.";
			return false;
		}
		if (firstActive <= lastInactive)
		{
			a_error =
				"Inactive overlap geometry was drawn over the active match.";
			return false;
		}
		if (!hasActiveBounds ||
			!window->ClipRect.Contains(activeBounds))
		{
			a_error =
				"Active match was not inside the effective reader clip.";
			return false;
		}
		if (window->DrawList->VtxBuffer.Size >= 8192)
		{
			a_error =
				"Text-view capture emitted unbounded offscreen geometry.";
			return false;
		}
		return true;
	}

	void TextViewFixture::BuildContent()
	{
		m_text.clear();
		m_lineOffsets = { 0 };
		m_sections.clear();
		m_text.reserve(7000);
		m_lineOffsets.reserve(151);
		m_sections.reserve(3);

		const auto appendLine = [this](std::string_view a_line) {
			m_text.append(a_line);
			m_text.push_back('\n');
			m_lineOffsets.push_back(m_text.size());
		};
		for (size_t line = 0; line < 150; ++line)
		{
			if (line == 0)
			{
				m_sections.push_back({
					"Initial % ## UTF-8",
					m_text.size()
				});
				appendLine("INITIAL reader content");
			}
			else if (line == 1)
			{
				appendLine("\r");
			}
			else if (line == 2)
			{
				appendLine("Literal 100% and ## markers; UTF-8 caf\xC3\xA9.");
			}
			else if (line == 106)
			{
				m_sections.push_back({
					"Long horizontal line",
					m_text.size()
				});
				std::string longLine{ "LONG horizontal content: " };
				longLine.append(1200, 'X');
				longLine.append(" :end");
				appendLine(longLine);
			}
			else if (line == 110)
			{
				m_sections.push_back({
					"Later overlapping match",
					m_text.size()
				});
				appendLine(
					"LATER content near line 110 contains ababa for overlap.");
			}
			else
			{
				appendLine("Reader line " + std::to_string(line));
			}
		}
	}

	void TextViewFixture::RebuildMatches()
	{
		m_matchOffsets.clear();
		if (!m_query.empty())
		{
			size_t offset{};
			while (offset <= m_text.size())
			{
				const auto match = m_text.find(m_query, offset);
				if (match == std::string::npos)
					break;
				m_matchOffsets.push_back(match);
				offset = match + 1;
			}
		}
		++m_matchRevision;
	}

	void TextViewFixture::PrepareCaptureFrame(uint32_t a_frame)
	{
		m_captureFrame = a_frame;
		auto& io = ImGui::GetIO();
		switch (a_frame)
		{
		case 1:
			io.AddMousePosEvent(m_searchPoint.x, m_searchPoint.y);
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
			break;
		case 2:
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
			io.AddKeyEvent(ImGuiKey_End, true);
			break;
		case 3:
			io.AddKeyEvent(ImGuiKey_End, false);
			io.AddInputCharactersUTF8(kTypedSuffix.c_str());
			break;
		case 4:
			io.AddKeyEvent(ImGuiMod_Ctrl, true);
			io.AddKeyEvent(ImGuiKey_A, true);
			break;
		case 5:
			io.AddKeyEvent(ImGuiKey_A, false);
			io.AddKeyEvent(ImGuiKey_V, true);
			break;
		case 6:
			io.AddKeyEvent(ImGuiKey_V, false);
			io.AddKeyEvent(ImGuiKey_A, true);
			break;
		case 7:
			io.AddKeyEvent(ImGuiKey_A, false);
			io.AddKeyEvent(ImGuiKey_V, true);
			break;
		case 8:
			io.AddKeyEvent(ImGuiKey_V, false);
			io.AddKeyEvent(ImGuiMod_Ctrl, false);
			io.AddMousePosEvent(-100.0f, -100.0f);
			break;
		default:
			break;
		}
	}

	void TextViewFixture::Draw()
	{
		m_lastDrawSucceeded = false;
		if (!m_client)
		{
			RecordFailure(
				"text-view page callback",
				DMUI_RESULT_CLIENT_NOT_FOUND);
			return;
		}

		auto& platform = ImGui::GetPlatformIO();
		const auto previousClipboard = platform.Platform_GetClipboardTextFn;
		const auto previousClipboardData = platform.Platform_ClipboardUserData;
		const char* pasteText = m_captureFrame == 5 ? kPastedQuery.c_str() : "aba";
		if (m_captureFrame == 5 || m_captureFrame == 7)
		{
			platform.Platform_ClipboardUserData = &pasteText;
			platform.Platform_GetClipboardTextFn = [](ImGuiContext* a_context) {
				return *static_cast<const char**>(
					a_context->PlatformIO.Platform_ClipboardUserData);
			};
		}
		const auto search = m_client->DrawSearchInput(
			"##TextViewFixtureSearch",
			"Search reader text",
			m_query);
		platform.Platform_GetClipboardTextFn = previousClipboard;
		platform.Platform_ClipboardUserData = previousClipboardData;
		const auto searchBounds = ImGui::GetItemRectMin();
		m_searchPoint = {
			searchBounds.x + 60.0f,
			searchBounds.y + ImGui::GetItemRectSize().y * 0.5f
		};
		if (!search)
		{
			RecordFailure("DrawSearchInput", m_client->LastResult());
			return;
		}
		if (m_captureFrame == 3)
			m_typedGrowthAccepted = *search && m_query == "aba" + kTypedSuffix;
		if (m_captureFrame == 5)
			m_pasteGrowthAccepted = *search && m_query == kPastedQuery;
		if (m_captureFrame == 7)
			m_searchRestored = *search && m_query == "aba";
		if (*search)
		{
			RebuildMatches();
			m_initialSelectionIssued = false;
		}

		const auto metrics = m_client->GetStyleMetrics();
		if (!metrics)
		{
			RecordFailure("GetStyleMetrics", m_client->LastResult());
			return;
		}
		const auto lineHeight =
			(std::max)(
				metrics->fontSizeBase + metrics->itemSpacing.y,
				1.0f);
		const dmui::TextViewRequest request{
			.text = m_text,
			.lineOffsets = m_lineOffsets,
			.matchByteOffsets = m_matchOffsets,
			.matchByteLength = m_query.size(),
			.contentRevision = 1,
			.matchRevision = m_matchRevision,
			.viewport = {
				0.0f,
				lineHeight * 13.0f +
					(std::max)(metrics->framePadding.y, 0.0f) * 2.0f
			}
		};
		if (!m_initialSelectionIssued)
		{
			m_initialSelectionIssued = true;
			(void)dmui::SelectNextTextMatch(request, m_state);
		}

		(void)dmui::DrawTextViewNavigation(
			"TextViewFixtureSections",
			std::span<const Section>{ m_sections },
			*metrics,
			request,
			m_state,
			[](const Section& a_section) {
				return std::pair{
					a_section.label,
					a_section.byteOffset
				};
			});
		if (dmui::ui::LastResult() != DMUI_RESULT_OK)
		{
			RecordFailure(
				"DrawTextViewNavigation",
				dmui::ui::LastResult());
			return;
		}

		const auto previous = dmui::ui::Button("Previous match");
		dmui::ui::SameLine();
		const auto next = dmui::ui::Button("Next match");
		if (dmui::ui::LastResult() != DMUI_RESULT_OK)
		{
			RecordFailure("match navigation controls", dmui::ui::LastResult());
			return;
		}
		if (previous)
			(void)dmui::SelectPreviousTextMatch(request, m_state);
		if (next)
			(void)dmui::SelectNextTextMatch(request, m_state);

		if (!m_client->DrawTextView(kViewerId, request, m_state))
		{
			RecordFailure("DrawTextView", m_client->LastResult());
			return;
		}
		m_lastDrawSucceeded = true;
		if (auto* window = FindTextViewWindow())
			m_childWindowId = window->ID;
		else
			RecordFailure(
				"locating rendered text-view child",
				DMUI_RESULT_CALLBACK_FAILED);
	}

	void TextViewFixture::RecordFailure(
		std::string_view a_operation,
		DMUI_Result a_result)
	{
		if (!m_callbackError.empty())
			return;
		m_callbackError.assign(a_operation);
		m_callbackError.append(" failed: ");
		m_callbackError.append(DMUI_ResultToString(a_result));
	}
}
