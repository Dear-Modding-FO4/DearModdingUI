#include "GeneralTestSuiteInternal.h"

namespace DmuiTests::Detail
{
	const char* HostStateName(DMUI_HostState a_state) noexcept
	{
		switch (a_state)
		{
		case DMUI_HOST_STATE_NOT_INITIALIZED:
			return "not initialized";
		case DMUI_HOST_STATE_WAITING_FOR_PRESENT:
			return "waiting for Present";
		case DMUI_HOST_STATE_INITIALIZING:
			return "initializing";
		case DMUI_HOST_STATE_READY:
			return "ready";
		case DMUI_HOST_STATE_UNAVAILABLE:
			return "unavailable";
		default:
			return "unknown";
		}
	}

	const char* BindingStateName(DMUI_HotkeyBindingState a_state) noexcept
	{
		switch (a_state)
		{
		case DMUI_HOTKEY_BINDING_BOUND:
			return "bound";
		case DMUI_HOTKEY_BINDING_UNBOUND_USER:
			return "unbound by user";
		case DMUI_HOTKEY_BINDING_UNBOUND_DEFAULT_CONFLICT:
			return "default conflict";
		case DMUI_HOTKEY_BINDING_UNBOUND_NEVER_SET:
			return "none";
		case DMUI_HOTKEY_BINDING_UNBOUND_OVERRIDE_CONFLICT:
			return "override conflict";
		case DMUI_HOTKEY_BINDING_UNBOUND_INVALID_OVERRIDE:
			return "invalid override";
		default:
			return "unknown";
		}
	}

	const char* DialogEventName(DMUI_DialogEventKind a_kind) noexcept
	{
		switch (a_kind)
		{
		case DMUI_DIALOG_EVENT_PENDING:
			return "PENDING";
		case DMUI_DIALOG_EVENT_SUBMITTED:
			return "SUBMITTED";
		case DMUI_DIALOG_EVENT_CANCELLED:
			return "CANCELLED";
		case DMUI_DIALOG_EVENT_COMPLETED:
			return "COMPLETED";
		default:
			return "NONE";
		}
	}

	const char* InitializationStatusName(
		InitializationStatus a_status) noexcept
	{
		switch (a_status)
		{
		case InitializationStatus::kPending:
			return "pending";
		case InitializationStatus::kComplete:
			return "complete";
		case InitializationStatus::kIncomplete:
			return "incomplete";
		case InitializationStatus::kUnavailable:
			return "unavailable";
		default:
			return "unknown";
		}
	}

	const char* AnchorName(DMUI_OverlayAnchor a_anchor) noexcept
	{
		switch (a_anchor)
		{
		case DMUI_OVERLAY_ANCHOR_TOP_LEFT:
			return "Top left";
		case DMUI_OVERLAY_ANCHOR_TOP_RIGHT:
			return "Top right";
		case DMUI_OVERLAY_ANCHOR_BOTTOM_LEFT:
			return "Bottom left";
		case DMUI_OVERLAY_ANCHOR_BOTTOM_RIGHT:
			return "Bottom right";
		case DMUI_OVERLAY_ANCHOR_FREE:
			return "Free";
		default:
			return "Unknown";
		}
	}

	DiagnosticContext::DiagnosticContext(Environment& a_environment) :
		m_environment(a_environment),
		m_client(
			DmuiTestFixtures::kClientId,
			DmuiTestFixtures::kClientDisplayName,
			dmui::Version{ 0, 1 },
			"test-tube",
			{},
			{
				.requiredServices = kRequiredServices,
				.minimumUIRevision = DMUI_UI_REVISION_CURRENT,
				.minimumUIAPISize = DMUI_UI_API_REQUIRED_SIZE
			})
	{}

	dmui::Client& DiagnosticContext::Client() noexcept
	{
		return m_client;
	}

	const dmui::Client& DiagnosticContext::Client() const noexcept
	{
		return m_client;
	}

	Environment& DiagnosticContext::HostEnvironment() noexcept
	{
		return m_environment;
	}

	const Environment& DiagnosticContext::HostEnvironment() const noexcept
	{
		return m_environment;
	}
}
