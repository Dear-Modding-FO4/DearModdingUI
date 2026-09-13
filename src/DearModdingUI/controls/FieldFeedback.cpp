#include <DearModdingUI/controls/FieldFeedback.h>

#include <DearModdingUI/presentation/Theme.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>

namespace DearModdingUI::FieldFeedback
{
	namespace
	{
		std::atomic<FieldFeedbackPlacement> s_placement{
			DEFAULT_FIELD_FEEDBACK_LAYOUT
		};
		std::atomic<uint32_t> s_infoColor{ kDefaultInfoColor };
		std::atomic<uint32_t> s_warningColor{ kDefaultWarningColor };
		std::atomic<uint32_t> s_errorColor{ kDefaultErrorColor };

#if defined(DMUI_PREVIEW)
		thread_local std::optional<FieldFeedbackPlacement> s_previewLayout;
#endif

		[[nodiscard]] ImVec4 DecodeColor(uint32_t a_color) noexcept
		{
			constexpr float inverseByte = 1.0f / 255.0f;
			return {
				static_cast<float>(a_color & 0xFFu) * inverseByte,
				static_cast<float>((a_color >> 8u) & 0xFFu) * inverseByte,
				static_cast<float>((a_color >> 16u) & 0xFFu) * inverseByte,
				static_cast<float>((a_color >> 24u) & 0xFFu) * inverseByte
			};
		}

		struct FormattedFeedbackText
		{
			std::array<char, kMaximumMessageBytes + 16> bytes;
			size_t size{};
		};

		[[nodiscard]] FormattedFeedbackText FormatText(
			std::string_view a_message,
			DMUI_FieldFeedbackSeverity a_severity) noexcept
		{
			FormattedFeedbackText text;
			const std::string_view label{ SeverityLabel(a_severity) };
			std::memcpy(text.bytes.data(), label.data(), label.size());
			text.size = label.size();
			text.bytes[text.size++] = ':';
			text.bytes[text.size++] = ' ';
			const auto available = text.bytes.size() - text.size - 1;
			const auto messageSize = (std::min)(a_message.size(), available);
			std::memcpy(
				text.bytes.data() + text.size,
				a_message.data(),
				messageSize);
			text.size += messageSize;
			text.bytes[text.size] = '\0';
			return text;
		}

		[[nodiscard]] float ContentHeight(
			std::string_view a_message,
			DMUI_FieldFeedbackSeverity a_severity,
			float a_width) noexcept
		{
			const Theme::FontGuard font{ Theme::FontRole::kSubtext };
			const auto text = FormatText(a_message, a_severity);
			return ImGui::CalcTextSize(
				text.bytes.data(),
				text.bytes.data() + text.size,
				false,
				(std::max)(a_width, ImGui::GetFontSize())).y;
		}

		void DrawTextBlock(
			std::string_view a_message,
			DMUI_FieldFeedbackSeverity a_severity,
			float a_width) noexcept
		{
			const Theme::FontGuard font{ Theme::FontRole::kSubtext };
			const auto text = FormatText(a_message, a_severity);
			// Keep subsequent lines anchored to the padded feedback region.
			ImGui::BeginGroup();
			ImGui::PushStyleColor(ImGuiCol_Text, SeverityColor(a_severity));
			ImGui::PushTextWrapPos(
				ImGui::GetCursorPosX() +
					(std::max)(a_width, ImGui::GetFontSize()));
			ImGui::TextUnformatted(
				text.bytes.data(),
				text.bytes.data() + text.size);
			ImGui::PopTextWrapPos();
			ImGui::PopStyleColor();
			ImGui::EndGroup();
		}

		[[nodiscard]] float RegionWidth(
			const LayoutInput& a_input,
			Region a_region) noexcept
		{
			switch (a_region)
			{
			case Region::kLabel:
				return a_input.labelWidth;
			case Region::kControl:
				return a_input.controlWidth;
			case Region::kContainer:
				return a_input.containerWidth;
			default:
				return 0.0f;
			}
		}
	}

	bool IsValidSeverity(
		DMUI_FieldFeedbackSeverity a_severity) noexcept
	{
		return a_severity == DMUI_FIELD_FEEDBACK_SEVERITY_INFO ||
			a_severity == DMUI_FIELD_FEEDBACK_SEVERITY_WARNING ||
			a_severity == DMUI_FIELD_FEEDBACK_SEVERITY_ERROR;
	}

	void SetAppearance(const Appearance& a_appearance) noexcept
	{
		s_placement.store(
			NormalizeFieldFeedbackLayout(a_appearance.placement),
			std::memory_order_release);
		s_infoColor.store(a_appearance.infoColor, std::memory_order_release);
		s_warningColor.store(
			a_appearance.warningColor,
			std::memory_order_release);
		s_errorColor.store(a_appearance.errorColor, std::memory_order_release);
	}

	Appearance CurrentAppearance() noexcept
	{
		return {
			s_placement.load(std::memory_order_acquire),
			s_infoColor.load(std::memory_order_acquire),
			s_warningColor.load(std::memory_order_acquire),
			s_errorColor.load(std::memory_order_acquire)
		};
	}

	FieldFeedbackPlacement EffectivePlacement() noexcept
	{
#if defined(DMUI_PREVIEW)
		if (s_previewLayout)
			return *s_previewLayout;
#endif
		return s_placement.load(std::memory_order_acquire);
	}

	ImVec4 SeverityColor(
		DMUI_FieldFeedbackSeverity a_severity) noexcept
	{
		switch (a_severity)
		{
		case DMUI_FIELD_FEEDBACK_SEVERITY_WARNING:
			return DecodeColor(
				s_warningColor.load(std::memory_order_acquire));
		case DMUI_FIELD_FEEDBACK_SEVERITY_ERROR:
			return DecodeColor(
				s_errorColor.load(std::memory_order_acquire));
		default:
			return DecodeColor(
				s_infoColor.load(std::memory_order_acquire));
		}
	}

	const char* SeverityLabel(
		DMUI_FieldFeedbackSeverity a_severity) noexcept
	{
		switch (a_severity)
		{
		case DMUI_FIELD_FEEDBACK_SEVERITY_WARNING:
			return "Warning";
		case DMUI_FIELD_FEEDBACK_SEVERITY_ERROR:
			return "Error";
		default:
			return "Info";
		}
	}

	float Measure(
		std::string_view a_message,
		DMUI_FieldFeedbackSeverity a_severity,
		float a_width,
		bool a_strip) noexcept
	{
		if (a_message.empty())
			return 0.0f;
		const auto padding = a_strip ? 8.0f * Theme::Scale() : 0.0f;
		return ContentHeight(
			a_message,
			a_severity,
			(std::max)(a_width - padding * 2.0f, 1.0f)) +
			padding * 2.0f;
	}

	static LayoutPlan BuildLayoutPlan(
		const LayoutInput& a_input,
		Region a_region,
		bool a_strip) noexcept
	{
		if (a_input.message.empty())
			return {};
		const auto width = (std::max)(
			RegionWidth(a_input, a_region),
			1.0f);
		return {
			a_region,
			a_input.message,
			a_input.severity,
			width,
			ImGui::GetStyle().ItemSpacing.y,
			Measure(
				a_input.message,
				a_input.severity,
				width,
				a_strip),
			a_strip
		};
	}

	static void DrawFeedbackBlock(
		std::string_view a_message,
		DMUI_FieldFeedbackSeverity a_severity,
		float a_width,
		float a_contentExtent,
		bool a_strip) noexcept
	{
		if (a_message.empty())
			return;
		a_width = (std::max)(a_width, 1.0f);
		if (!a_strip)
		{
			DrawTextBlock(a_message, a_severity, a_width);
			return;
		}

		const auto origin = ImGui::GetCursorScreenPos();
		const auto padding = 8.0f * Theme::Scale();
		const ImVec2 end{
			origin.x + a_width,
			origin.y + a_contentExtent
		};
		auto tint = SeverityColor(a_severity);
		tint.w = 0.10f;
		auto* draw = ImGui::GetWindowDrawList();
		draw->AddRectFilled(origin, end, ImGui::GetColorU32(tint));
		draw->AddRectFilled(
			origin,
			{ origin.x + 2.0f * Theme::Scale(), end.y },
			ImGui::GetColorU32(SeverityColor(a_severity)));
		ImGui::SetCursorScreenPos({
			origin.x + padding,
			origin.y + padding
		});
		DrawTextBlock(
			a_message,
			a_severity,
			(std::max)(a_width - padding * 2.0f, 1.0f));
		ImGui::SetCursorScreenPos({ origin.x, end.y });
		ImGui::Dummy({ a_width, 0.0f });
	}

	LayoutPlan ResolveLayout(const LayoutInput& a_input) noexcept
	{
		const auto* layout =
			FindFieldFeedbackLayout(EffectivePlacement());
		return layout ?
			layout->resolve(a_input) :
			ResolveFullWidthStrip(a_input);
	}

	float RequiredExtent(
		const LayoutPlan& a_plan,
		Region a_region) noexcept
	{
		return a_plan.region == a_region ?
			a_plan.leadingExtent + a_plan.contentExtent :
			0.0f;
	}

	void Draw(const LayoutPlan& a_plan, Region a_region) noexcept
	{
		if (a_plan.region != a_region || a_plan.message.empty())
			return;
		ImGui::SetCursorPosY(
			ImGui::GetCursorPosY() + a_plan.leadingExtent);
		DrawFeedbackBlock(
			a_plan.message,
			a_plan.severity,
			a_plan.width,
			a_plan.contentExtent,
			a_plan.strip);
	}

	LayoutPlan ResolveUnderLabel(
		const LayoutInput& a_input) noexcept
	{
		return BuildLayoutPlan(
			a_input,
			a_input.hasLabelRegion ? Region::kLabel : Region::kControl,
			false);
	}

	LayoutPlan ResolveUnderControl(
		const LayoutInput& a_input) noexcept
	{
		return BuildLayoutPlan(a_input, Region::kControl, false);
	}

	LayoutPlan ResolveFullWidthStrip(
		const LayoutInput& a_input) noexcept
	{
		return BuildLayoutPlan(a_input, Region::kContainer, true);
	}

#if defined(DMUI_PREVIEW)
	PreviewLayoutOverride::PreviewLayoutOverride(
		FieldFeedbackPlacement a_layout) noexcept :
		m_previous(s_previewLayout)
	{
		s_previewLayout = NormalizeFieldFeedbackLayout(a_layout);
	}

	PreviewLayoutOverride::~PreviewLayoutOverride() noexcept
	{
		s_previewLayout = m_previous;
	}
#endif
}
