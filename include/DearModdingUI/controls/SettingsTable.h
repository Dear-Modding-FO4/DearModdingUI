#pragma once

#include <DearModdingUI/API.h>
#include <DearModdingUI/VisualDecisions.h>
#include <DearModdingUI/controls/FieldFeedback.h>

#include <cstdint>
#include <string_view>

namespace DearModdingUI::SettingsTable
{
	enum class Phase : uint32_t
	{
		kIdle,
		kTable,
		kRow
	};

	class BracketState
	{
	public:
		[[nodiscard]] constexpr DMUI_Result BeginTable(
			DMUI_ClientHandle a_owner) noexcept
		{
			if (m_phase != Phase::kIdle)
				return DMUI_RESULT_UNBALANCED_BRACKET;
			m_owner = a_owner;
			m_phase = Phase::kTable;
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] constexpr DMUI_Result BeginRow(
			DMUI_ClientHandle a_owner) noexcept
		{
			if (m_phase != Phase::kTable || m_owner != a_owner)
				return DMUI_RESULT_UNBALANCED_BRACKET;
			m_phase = Phase::kRow;
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] constexpr DMUI_Result EndRow(
			DMUI_ClientHandle a_owner) noexcept
		{
			if (m_phase != Phase::kRow || m_owner != a_owner)
				return DMUI_RESULT_UNBALANCED_BRACKET;
			m_phase = Phase::kTable;
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] constexpr DMUI_Result EndTable(
			DMUI_ClientHandle a_owner) noexcept
		{
			if (m_phase != Phase::kTable || m_owner != a_owner)
				return DMUI_RESULT_UNBALANCED_BRACKET;
			Reset();
			return DMUI_RESULT_OK;
		}

		constexpr void Reset() noexcept
		{
			m_owner = DMUI_INVALID_CLIENT_HANDLE;
			m_phase = Phase::kIdle;
		}

		[[nodiscard]] constexpr Phase CurrentPhase() const noexcept
		{
			return m_phase;
		}

		[[nodiscard]] constexpr DMUI_ClientHandle Owner() const noexcept
		{
			return m_owner;
		}

	private:
		DMUI_ClientHandle m_owner{ DMUI_INVALID_CLIENT_HANDLE };
		Phase m_phase{ Phase::kIdle };
	};

	[[nodiscard]] constexpr DMUI_Result ValidateFieldEndOptions(
		const DMUI_FieldEndOptions* a_options) noexcept
	{
		if (!a_options)
			return DMUI_RESULT_INVALID_ARGUMENT;
		return a_options->structSize < DMUI_FIELD_END_OPTIONS_0_1_SIZE ?
			DMUI_RESULT_STRUCT_TOO_SMALL :
			DMUI_RESULT_OK;
	}

	[[nodiscard]] constexpr DMUI_Result ValidateRowOptions(
		const DMUI_SettingsRowOptions* a_options) noexcept
	{
		if (!a_options)
			return DMUI_RESULT_INVALID_ARGUMENT;
		return a_options->structSize < DMUI_SETTINGS_ROW_OPTIONS_0_1_SIZE ?
			DMUI_RESULT_STRUCT_TOO_SMALL :
			DMUI_RESULT_OK;
	}

	[[nodiscard]] constexpr DMUI_Result ValidateRowBeginOptions(
		const DMUI_SettingsRowBeginOptions* a_options) noexcept
	{
		if (!a_options)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_options->structSize < DMUI_SETTINGS_ROW_BEGIN_OPTIONS_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		return a_options->layout == DMUI_SETTINGS_ROW_LAYOUT_LABEL_VALUE ||
				a_options->layout == DMUI_SETTINGS_ROW_LAYOUT_FULL_SPAN ?
			DMUI_RESULT_OK :
			DMUI_RESULT_INVALID_ARGUMENT;
	}

	[[nodiscard]] constexpr DMUI_Result ValidateFieldBeginOptions(
		const DMUI_FieldBeginOptions* a_options) noexcept
	{
		if (!a_options)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_options->structSize < DMUI_FIELD_BEGIN_OPTIONS_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		return a_options->layout == DMUI_FIELD_LAYOUT_LABEL_VALUE ||
				a_options->layout == DMUI_FIELD_LAYOUT_FULL_SPAN ?
			DMUI_RESULT_OK :
			DMUI_RESULT_INVALID_ARGUMENT;
	}

	[[nodiscard]] inline DMUI_Result ValidateFieldFeedback(
		const DMUI_FieldFeedback* a_feedback,
		size_t* a_messageLength = nullptr) noexcept
	{
		if (a_messageLength)
			*a_messageLength = 0;
		if (!a_feedback)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_feedback->structSize < DMUI_FIELD_FEEDBACK_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (!FieldFeedback::IsValidSeverity(a_feedback->severity))
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (!a_feedback->message)
			return DMUI_RESULT_OK;
		size_t length{};
		while (length <= FieldFeedback::kMaximumMessageBytes &&
			a_feedback->message[length] != '\0')
			++length;
		if (length > FieldFeedback::kMaximumMessageBytes)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_messageLength)
			*a_messageLength = length;
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] constexpr float ResolveResetColumnWidth(
		bool a_hasGlyph,
		float a_labelWidth,
		float a_buttonExtent,
		float a_framePaddingX) noexcept
	{
		return ActionButtonWidth(
			a_hasGlyph,
			a_labelWidth,
			a_buttonExtent,
			a_framePaddingX);
	}

	[[nodiscard]] constexpr float ResolveResetButtonOriginY(
		float a_controlMinY,
		float a_controlMaxY,
		float a_buttonExtent) noexcept
	{
		return a_controlMinY + RowContentOffsetY(
			a_controlMaxY - a_controlMinY,
			{ a_buttonExtent },
			RowContentMetric::kBox);
	}

	struct BeginResult
	{
		DMUI_Result result{ DMUI_RESULT_OK };
		bool visible{ false };
	};

	struct FieldEndOptions
	{
		bool resetVisible{ true };
		bool resetEnabled{ false };
	};

	enum class RowLayout : uint32_t
	{
		kLabelValue,
		kFullSpan
	};

	class ClientCallbackGuard
	{
	public:
		explicit ClientCallbackGuard(
			DMUI_ClientHandle a_client,
			bool a_settingsAllowed = true) noexcept;
		~ClientCallbackGuard() noexcept;

		ClientCallbackGuard(const ClientCallbackGuard&) = delete;
		ClientCallbackGuard(ClientCallbackGuard&&) = delete;
		ClientCallbackGuard& operator=(const ClientCallbackGuard&) = delete;
		ClientCallbackGuard& operator=(ClientCallbackGuard&&) = delete;
	};

	[[nodiscard]] bool AcceptsClient(DMUI_ClientHandle a_client) noexcept;
	[[nodiscard]] bool AcceptsFieldClient(
		DMUI_ClientHandle a_client) noexcept;
	[[nodiscard]] BeginResult Begin(
		DMUI_ClientHandle a_owner,
		const char* a_id) noexcept;
	[[nodiscard]] BeginResult BeginRow(
		DMUI_ClientHandle a_owner,
		const char* a_id,
		const char* a_label,
		const char* a_description,
		RowLayout a_layout = RowLayout::kLabelValue) noexcept;
	[[nodiscard]] DMUI_Result EndRow(
		DMUI_ClientHandle a_owner,
		const FieldEndOptions& a_options,
		bool& a_resetPressed) noexcept;
	[[nodiscard]] DMUI_Result End(
		DMUI_ClientHandle a_owner) noexcept;
	[[nodiscard]] BeginResult BeginField(
		DMUI_ClientHandle a_owner,
		const char* a_id,
		const char* a_label,
		const char* a_description,
		RowLayout a_layout) noexcept;
	[[nodiscard]] DMUI_Result SetFieldFeedback(
		DMUI_ClientHandle a_owner,
		DMUI_FieldFeedbackSeverity a_severity,
		std::string_view a_message) noexcept;
	[[nodiscard]] DMUI_Result EndField(
		DMUI_ClientHandle a_owner,
		const FieldEndOptions& a_options,
		bool& a_resetPressed) noexcept;
	void ResetBracketState() noexcept;
}
