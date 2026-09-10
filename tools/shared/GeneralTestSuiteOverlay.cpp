#include "GeneralTestSuiteInternal.h"

namespace DmuiTests::Detail
{
	OverlayExercise::OverlayExercise(
		DiagnosticContext& a_context,
		PresentationResources& a_resources) noexcept :
		m_context(a_context),
		m_resources(a_resources)
	{
		m_options.structSize = sizeof(m_options);
		m_options.anchor = DMUI_OVERLAY_ANCHOR_TOP_RIGHT;
		m_options.offset = { 28.0f, 28.0f };
		m_options.minimumSize = { 360.0f, 220.0f };
		m_options.maximumSize = { 640.0f, 520.0f };
		m_options.opacity = 0.88f;
		m_options.contentScale = 1.0f;
		m_options.backgroundVisible = 1;
		m_options.borderVisible = 1;
		m_options.allowArrangement = 1;
	}

	bool OverlayExercise::Register(std::string_view a_categoryId) noexcept
	{
		auto& client = m_context.Client();
		const auto overlay = client.AddPage(
			{
				.id = "managed-overlay",
				.displayName = "DMUI Test Overlay",
				.categoryId = a_categoryId.data(),
				.summary = "Managed non-interactive test overlay.",
				.sortKey = 100,
				.kind = DMUI_PAGE_KIND_OVERLAY
			},
			[this] { DrawOverlay(); });
		if (!overlay)
			return false;
		m_page = *overlay;
		m_context.Info(
			"dmui-test-client: registration overlay-page "
			"result={} handle={}"sv,
			DMUI_ResultToString(client.LastResult()),
			m_page);
		return true;
	}

	bool OverlayExercise::Configure() noexcept
	{
		if (m_page == DMUI_INVALID_PAGE_HANDLE)
			return false;
		auto& client = m_context.Client();
		const auto configured = client.ConfigureOverlay(m_page, m_options);
		m_result = client.LastResult();
		return configured;
	}

	bool OverlayExercise::SetEnabled(bool a_enabled) noexcept
	{
		if (a_enabled == m_enabled ||
			m_page == DMUI_INVALID_PAGE_HANDLE)
			return a_enabled == m_enabled;
		auto& client = m_context.Client();
		const auto succeeded = a_enabled ?
			client.RequestFrame(m_page) :
			client.ReleaseFrame(m_page);
		++m_demandAttempts;
		m_result = client.LastResult();
		if (succeeded)
		{
			m_enabled = a_enabled;
			if (a_enabled)
				++m_frameRequests;
			else
				++m_frameReleases;
		}
		m_context.Info(
			"dmui-test-client: overlay demand enabled={} result={} "
			"requests={} releases={}"sv,
			a_enabled,
			DMUI_ResultToString(m_result),
			m_frameRequests,
			m_frameReleases);
		return succeeded;
	}

	void OverlayExercise::Toggle() noexcept
	{
		(void)SetEnabled(!m_enabled);
	}

	void OverlayExercise::Observe() noexcept
	{
		if (m_page == DMUI_INVALID_PAGE_HANDLE)
			return;
		auto& client = m_context.Client();
		if (const auto placement = client.QueryOverlay(m_page))
		{
			m_placement = *placement;
			if (placement->arrangementCompleted)
				++m_arrangementCompletions;
		}
		else
		{
			m_result = client.LastResult();
		}
	}

	template <class DrawCallback>
	bool OverlayExercise::DrawSimpleRow(
		const char* a_id,
		const char* a_label,
		const char* a_description,
		DrawCallback&& a_draw)
	{
		auto& client = m_context.Client();
		const auto visible =
			client.BeginSettingsRow(a_id, a_label, a_description);
		if (!visible)
			return false;
		if (!*visible)
			return true;
		a_draw();
		return client.EndSettingsRow(false, false).has_value();
	}

	bool OverlayExercise::DrawFloatRow(
		const char* a_id,
		const char* a_label,
		float& a_value,
		float a_minimum,
		float a_maximum) noexcept
	{
		return DrawSimpleRow(
			a_id,
			a_label,
			"Changes are applied through ConfigureOverlay.",
			[this, &a_value, a_minimum, a_maximum] {
				if (dmui::ui::SliderScalar(
						"##Value",
						&a_value,
						&a_minimum,
						&a_maximum,
						"%.2f",
						dmui::ui::SliderFlags::kAlwaysClamp))
					(void)Configure();
			});
	}

	void OverlayExercise::DrawControls() noexcept
	{
		auto& client = m_context.Client();
		(void)client.DrawSectionHeader("Managed overlay");
		const auto table = client.BeginSettingsTable("overlay-controls");
		if (!table || !*table)
			return;

		if (!DrawSimpleRow(
				"overlay-enabled",
				"Enabled",
				"Balances RequestFrame and ReleaseFrame.",
				[this] {
					auto enabled = m_enabled;
					if (dmui::ui::Checkbox("##Value", &enabled))
						(void)SetEnabled(enabled);
				}))
		{
			(void)client.EndSettingsTable();
			return;
		}
		if (!DrawSimpleRow(
				"overlay-anchor",
				"Anchor",
				"Free position can move only while the menu owns input.",
				[this] {
					if (dmui::ui::BeginCombo(
							"##Value",
							AnchorName(m_options.anchor)))
					{
						for (uint32_t anchor =
								DMUI_OVERLAY_ANCHOR_TOP_LEFT;
							 anchor <= DMUI_OVERLAY_ANCHOR_FREE;
							 ++anchor)
						{
							const auto selected = m_options.anchor == anchor;
							if (dmui::ui::Selectable(
									AnchorName(anchor),
									selected))
							{
								m_options.anchor = anchor;
								(void)Configure();
							}
							if (selected)
								dmui::ui::SetItemDefaultFocus();
						}
						dmui::ui::EndCombo();
					}
				}))
		{
			(void)client.EndSettingsTable();
			return;
		}
		if (!DrawFloatRow(
				"overlay-x",
				"Free/anchor X offset",
				m_options.offset.x,
				0.0f,
				1200.0f) ||
			!DrawFloatRow(
				"overlay-y",
				"Free/anchor Y offset",
				m_options.offset.y,
				0.0f,
				800.0f) ||
			!DrawFloatRow(
				"overlay-scale",
				"Content scale",
				m_options.contentScale,
				0.5f,
				3.0f) ||
			!DrawFloatRow(
				"overlay-opacity",
				"Opacity",
				m_options.opacity,
				0.05f,
				1.0f))
		{
			(void)client.EndSettingsTable();
			return;
		}
		if (!DrawSimpleRow(
				"overlay-arrangement",
				"Allow arrangement",
				"Host permits free movement only while its menu owns input.",
				[this] {
					auto enabled = m_options.allowArrangement != 0;
					if (dmui::ui::Checkbox("##Value", &enabled))
					{
						m_options.allowArrangement = enabled ? 1u : 0u;
						(void)Configure();
					}
				}))
		{
			(void)client.EndSettingsTable();
			return;
		}
		const auto placement = client.BeginSettingsRow(
			"overlay-placement",
			"Observed placement",
			"Latest host-owned placement and completion edge.",
			dmui::RowPresentation::Layout::kFullSpan);
		if (!placement)
		{
			(void)client.EndSettingsTable();
			return;
		}
		if (*placement)
		{
			dmui::ui::Text(
				"visible=%u pos=(%.1f, %.1f) size=(%.1f, %.1f) "
				"generation=%llu completed=%llu requests/releases=%llu/%llu",
				m_placement.visible,
				m_placement.position.x,
				m_placement.position.y,
				m_placement.size.x,
				m_placement.size.y,
				m_placement.changeGeneration,
				m_arrangementCompletions,
				m_frameRequests,
				m_frameReleases);
			(void)client.EndSettingsRow(false, false);
		}
		(void)client.EndSettingsTable();
	}

	void OverlayExercise::DrawOverlay() noexcept
	{
		++m_draws;
		dmui::ui::TextUnformatted("DMUI TESTS / MANAGED OVERLAY");
		dmui::ui::Separator();
		dmui::ui::Text(
			"observer=%llu  overlay=%llu  hidden-menu=%llu",
			m_frameCount,
			m_draws,
			m_hiddenMenuObservations);
		dmui::ui::Text("timer=%.2f s", m_elapsedSeconds);
		m_resources.DrawOverlayImages();
		m_resources.DrawPlot("overlay-frame-times");
	}

	void OverlayExercise::ConfigurePresentation() noexcept
	{
		m_options.anchor = DMUI_OVERLAY_ANCHOR_TOP_RIGHT;
		m_options.offset = { 24.0f, 24.0f };
		m_options.minimumSize = { 640.0f, 420.0f };
		m_options.maximumSize = { 640.0f, 420.0f };
		m_options.opacity = 0.82f;
		m_options.contentScale = 1.0f;
		m_options.backgroundVisible = 1;
		m_options.borderVisible = 1;
		m_options.allowArrangement = 0;
	}

	DMUI_PageHandle OverlayExercise::Page() const noexcept
	{
		return m_page;
	}

	uint64_t OverlayExercise::EventCount() const noexcept
	{
		return m_demandAttempts;
	}

	uint64_t OverlayExercise::ObservedEventCount() const noexcept
	{
		return m_frameRequests + m_arrangementCompletions;
	}

	bool OverlayExercise::Failed() const noexcept
	{
		return m_demandAttempts > 0 && m_result != DMUI_RESULT_OK;
	}

	uint64_t OverlayExercise::DrawCount() const noexcept
	{
		return m_draws;
	}

	DMUI_Result OverlayExercise::Result() const noexcept
	{
		return m_result;
	}

	OverlayExercise::Snapshot OverlayExercise::CurrentSnapshot() const noexcept
	{
		return { m_frameRequests, m_frameReleases };
	}

	void OverlayExercise::SetFrameObservations(
		uint64_t a_frameCount,
		uint64_t a_hiddenMenuObservations,
		double a_elapsedSeconds) noexcept
	{
		m_frameCount = a_frameCount;
		m_hiddenMenuObservations = a_hiddenMenuObservations;
		m_elapsedSeconds = a_elapsedSeconds;
	}
}
