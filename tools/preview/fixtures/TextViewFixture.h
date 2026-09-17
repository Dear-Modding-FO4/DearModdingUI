#pragma once

#include <DearModdingUI/Client.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace DmuiTestFixtures
{
	class TextViewFixture
	{
	public:
		[[nodiscard]] bool Register(std::string& a_error);
		[[nodiscard]] bool ValidateCapture(std::string& a_error) const;

	private:
		struct Section
		{
			std::string_view label;
			size_t byteOffset{};
		};

		void BuildContent();
		void RebuildMatches();
		void Draw();
		void RecordFailure(
			std::string_view a_operation,
			DMUI_Result a_result);

		std::unique_ptr<dmui::Client> m_client;
		std::string m_text;
		std::vector<size_t> m_lineOffsets;
		std::vector<size_t> m_matchOffsets;
		std::vector<Section> m_sections;
		std::string m_query{ "aba" };
		dmui::TextViewState m_state;
		uint64_t m_matchRevision{};
		uint32_t m_childWindowId{};
		bool m_initialSelectionIssued{};
		bool m_lastDrawSucceeded{};
		std::string m_callbackError;
	};
}
