#pragma once

#include <DearModdingUI/API.h>
#include <DearModdingUI/presentation/VisualDecisions.h>
#include <DearModdingUI/controls/FieldFeedback.h>
#include <DearModdingUI/host/Status.h>

#include <cstdint>
#include <string_view>

namespace DearModdingUI::SettingsTable
{
	enum class Phase : uint32_t
	{
		kIdle,
		kTable,
		kField
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

		[[nodiscard]] constexpr DMUI_Result BeginField(
			DMUI_ClientHandle a_owner) noexcept
		{
			if (m_phase != Phase::kTable || m_owner != a_owner)
				return DMUI_RESULT_UNBALANCED_BRACKET;
			m_phase = Phase::kField;
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] constexpr DMUI_Result EndField(
			DMUI_ClientHandle a_owner) noexcept
		{
			if (m_phase != Phase::kField || m_owner != a_owner)
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
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] constexpr DMUI_Result ValidateFieldBeginOptions(
		const DMUI_FieldBeginOptions* a_options) noexcept
	{
		if (!a_options)
			return DMUI_RESULT_INVALID_ARGUMENT;
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
		if (!IsValidSeverity(a_feedback->severity))
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

	enum class FieldLayout : uint32_t
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
	[[nodiscard]] DMUI_Result End(
		DMUI_ClientHandle a_owner) noexcept;
	[[nodiscard]] BeginResult BeginField(
		DMUI_ClientHandle a_owner,
		const char* a_id,
		const char* a_label,
		const char* a_description,
		FieldLayout a_layout) noexcept;
	[[nodiscard]] DMUI_Result SetFieldFeedback(
		DMUI_ClientHandle a_owner,
		DMUI_Severity a_severity,
		std::string_view a_message) noexcept;
	[[nodiscard]] DMUI_Result EndField(
		DMUI_ClientHandle a_owner,
		const FieldEndOptions& a_options,
		bool& a_resetPressed) noexcept;
	void ResetBracketState() noexcept;
}
