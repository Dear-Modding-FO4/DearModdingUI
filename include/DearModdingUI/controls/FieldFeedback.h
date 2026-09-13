#pragma once

#include <DearModdingUI/API.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

struct ImVec4;

namespace DearModdingUI
{
	enum class FieldFeedbackPlacement : uint32_t
	{
		kUnderLabel,
		kUnderControl,
		kFullWidthStrip
	};

	namespace FieldFeedback
	{
		[[nodiscard]] constexpr uint32_t PackColor(
			uint8_t a_red,
			uint8_t a_green,
			uint8_t a_blue) noexcept
		{
			return UINT32_C(0xFF000000) |
				(static_cast<uint32_t>(a_blue) << 16u) |
				(static_cast<uint32_t>(a_green) << 8u) |
				static_cast<uint32_t>(a_red);
		}

		inline constexpr uint32_t kDefaultInfoColor =
			PackColor(0x56, 0xB4, 0xE9);
		inline constexpr uint32_t kDefaultWarningColor =
			PackColor(0xE6, 0x9F, 0x00);
		inline constexpr uint32_t kDefaultErrorColor =
			PackColor(0xCC, 0x79, 0xA7);

		enum class Region : uint32_t
		{
			kNone,
			kLabel,
			kControl,
			kContainer
		};

		struct LayoutInput
		{
			std::string_view message;
			DMUI_FieldFeedbackSeverity severity{
				DMUI_FIELD_FEEDBACK_SEVERITY_INFO
			};
			bool hasLabelRegion{ true };
			float labelWidth{};
			float controlWidth{};
			float containerWidth{};
		};

		struct LayoutPlan
		{
			Region region{ Region::kNone };
			std::string_view message;
			DMUI_FieldFeedbackSeverity severity{
				DMUI_FIELD_FEEDBACK_SEVERITY_INFO
			};
			float width{};
			float leadingExtent{};
			float contentExtent{};
			bool strip{};
		};

		using LayoutResolver = LayoutPlan (*)(
			const LayoutInput&) noexcept;

		[[nodiscard]] LayoutPlan ResolveUnderLabel(
			const LayoutInput& a_input) noexcept;
		[[nodiscard]] LayoutPlan ResolveUnderControl(
			const LayoutInput& a_input) noexcept;
		[[nodiscard]] LayoutPlan ResolveFullWidthStrip(
			const LayoutInput& a_input) noexcept;
	}

	struct FieldFeedbackLayoutDescriptor
	{
		FieldFeedbackPlacement kind;
		std::string_view id;
		std::string_view label;
		std::string_view description;
		FieldFeedback::LayoutResolver resolve;
	};

	inline constexpr std::array FIELD_FEEDBACK_LAYOUTS{
		FieldFeedbackLayoutDescriptor{
			FieldFeedbackPlacement::kUnderLabel,
			"label",
			"Under label",
			"Show feedback below the field label and description.",
			&FieldFeedback::ResolveUnderLabel
		},
		FieldFeedbackLayoutDescriptor{
			FieldFeedbackPlacement::kUnderControl,
			"control",
			"Under control",
			"Show feedback directly below the field control.",
			&FieldFeedback::ResolveUnderControl
		},
		FieldFeedbackLayoutDescriptor{
			FieldFeedbackPlacement::kFullWidthStrip,
			"strip",
			"Full-width strip",
			"Show feedback in a highlighted strip beneath the field.",
			&FieldFeedback::ResolveFullWidthStrip
		}
	};
	inline constexpr auto DEFAULT_FIELD_FEEDBACK_LAYOUT =
		FieldFeedbackPlacement::kFullWidthStrip;

	[[nodiscard]] constexpr const FieldFeedbackLayoutDescriptor*
		FindFieldFeedbackLayout(FieldFeedbackPlacement a_kind) noexcept
	{
		for (const auto& layout : FIELD_FEEDBACK_LAYOUTS)
		{
			if (layout.kind == a_kind)
				return &layout;
		}
		return nullptr;
	}

	[[nodiscard]] constexpr std::string_view FieldFeedbackLayoutName(
		FieldFeedbackPlacement a_kind) noexcept
	{
		const auto* layout = FindFieldFeedbackLayout(a_kind);
		return layout ? layout->id : "unknown";
	}

	[[nodiscard]] constexpr std::optional<FieldFeedbackPlacement>
		ParseFieldFeedbackLayout(std::string_view a_name) noexcept
	{
		for (const auto& layout : FIELD_FEEDBACK_LAYOUTS)
		{
			if (layout.id == a_name)
				return layout.kind;
		}
		return std::nullopt;
	}

	[[nodiscard]] constexpr FieldFeedbackPlacement
		NormalizeFieldFeedbackLayout(FieldFeedbackPlacement a_kind) noexcept
	{
		return FindFieldFeedbackLayout(a_kind) ?
			a_kind :
			DEFAULT_FIELD_FEEDBACK_LAYOUT;
	}

	namespace FieldFeedback
	{
		inline constexpr size_t kMaximumMessageBytes =
			DMUI_FIELD_FEEDBACK_MAX_MESSAGE_BYTES;

		struct Appearance
		{
			FieldFeedbackPlacement placement{
				DEFAULT_FIELD_FEEDBACK_LAYOUT
			};
			uint32_t infoColor{ kDefaultInfoColor };
			uint32_t warningColor{ kDefaultWarningColor };
			uint32_t errorColor{ kDefaultErrorColor };
		};

		[[nodiscard]] bool IsValidSeverity(
			DMUI_FieldFeedbackSeverity a_severity) noexcept;
		void SetAppearance(const Appearance& a_appearance) noexcept;
		[[nodiscard]] Appearance CurrentAppearance() noexcept;
		[[nodiscard]] FieldFeedbackPlacement EffectivePlacement() noexcept;
		[[nodiscard]] ImVec4 SeverityColor(
			DMUI_FieldFeedbackSeverity a_severity) noexcept;
		[[nodiscard]] const char* SeverityLabel(
			DMUI_FieldFeedbackSeverity a_severity) noexcept;
		[[nodiscard]] float Measure(
			std::string_view a_message,
			DMUI_FieldFeedbackSeverity a_severity,
			float a_width,
			bool a_strip) noexcept;
		[[nodiscard]] LayoutPlan ResolveLayout(
			const LayoutInput& a_input) noexcept;
		[[nodiscard]] float RequiredExtent(
			const LayoutPlan& a_plan,
			Region a_region) noexcept;
		void Draw(
			const LayoutPlan& a_plan,
			Region a_region) noexcept;

#if defined(DMUI_PREVIEW)
		class PreviewLayoutOverride
		{
		public:
			explicit PreviewLayoutOverride(
				FieldFeedbackPlacement a_layout) noexcept;
			~PreviewLayoutOverride() noexcept;

			PreviewLayoutOverride(const PreviewLayoutOverride&) = delete;
			PreviewLayoutOverride(PreviewLayoutOverride&&) = delete;
			PreviewLayoutOverride& operator=(
				const PreviewLayoutOverride&) = delete;
			PreviewLayoutOverride& operator=(
				PreviewLayoutOverride&&) = delete;

		private:
			std::optional<FieldFeedbackPlacement> m_previous;
		};
#endif
	}
}
