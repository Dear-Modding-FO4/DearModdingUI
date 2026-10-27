#include <DearModdingUI/controls/ColorEdit.h>

#include <DearModdingUI/controls/ControlRenderer.h>
#include <DearModdingUI/controls/Tooltip.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace DearModdingUI
{
	ImVec4 UnpackRgba(uint32_t a_rgba) noexcept
	{
		constexpr float scale{ 1.0f / 255.0f };
		return {
			static_cast<float>((a_rgba >> 24u) & 0xFFu) * scale,
			static_cast<float>((a_rgba >> 16u) & 0xFFu) * scale,
			static_cast<float>((a_rgba >> 8u) & 0xFFu) * scale,
			static_cast<float>(a_rgba & 0xFFu) * scale
		};
	}

	uint32_t PackRgba(const ImVec4& a_color) noexcept
	{
		const auto byte = [](float a_value) noexcept {
			const auto normalized = std::isfinite(a_value) ?
				(std::clamp)(a_value, 0.0f, 1.0f) :
				0.0f;
			return static_cast<uint32_t>(std::lround(normalized * 255.0f));
		};
		return byte(a_color.x) << 24u | byte(a_color.y) << 16u |
			byte(a_color.z) << 8u | byte(a_color.w);
	}

	DMUI_Result ValidateColorEdit(const DMUI_ColorEditDescriptor* a_color) noexcept
	{
		if (!a_color || !a_color->id || a_color->id[0] == '\0' ||
			a_color->reserved != 0 ||
			(a_color->flags & ~DMUI_COLOR_EDIT_ALPHA) != 0 ||
			!std::isfinite(a_color->width) || a_color->width < 0.0f ||
			a_color->presetCount > kMaxColorPresets ||
			(a_color->presetCount != 0 && !a_color->presets))
			return DMUI_RESULT_INVALID_ARGUMENT;
		for (uint32_t index = 0; index < a_color->presetCount; ++index)
		{
			const auto& preset = a_color->presets[index];
			if (!preset.name || preset.reserved != 0)
				return DMUI_RESULT_INVALID_ARGUMENT;
		}
		return DMUI_RESULT_OK;
	}

	DMUI_ColorEditResult DrawColorEdit(const DMUI_ColorEditDescriptor& a_color) noexcept
	{
		DMUI_ColorEditResult result{ a_color.rgba, 0u, 0u };
		const auto alpha = (a_color.flags & DMUI_COLOR_EDIT_ALPHA) != 0;
		const auto& renderer = CurrentControlRenderer();
		const auto& style = ImGui::GetStyle();
		const auto width = (std::max)(
			a_color.width > 0.0f ? a_color.width : ImGui::GetContentRegionAvail().x,
			1.0f);
		const auto targetRight = ImGui::GetCursorScreenPos().x + width;
		// Without alpha editing the alpha byte of the input is never disturbed.
		const auto settle = [&](uint32_t a_rgba) noexcept {
			return alpha ? a_rgba : (a_rgba & 0xFFFFFF00u) | (a_color.rgba & 0xFFu);
		};

		ImGui::PushID(a_color.id);
		auto color = UnpackRgba(a_color.rgba);
		ImGui::SetNextItemWidth(width);
		if (renderer.DrawColorEdit(color, alpha))
		{
			result.rgba = settle(PackRgba(color));
			result.changed = result.rgba != a_color.rgba ? 1u : 0u;
		}
		result.completed = ImGui::IsItemDeactivatedAfterEdit() ? 1u : 0u;

		const auto count = static_cast<size_t>(a_color.presetCount);
		if (count != 0)
		{
			const auto swatchHeight = ImGui::GetFrameHeight();
			const auto* label = a_color.presetsLabel ? a_color.presetsLabel : "";
			const auto hasLabel = label[0] != '\0';
			const auto labelWidth = hasLabel ? ImGui::CalcTextSize(label).x : 0.0f;
			const auto minimumSwatchesWidth =
				swatchHeight * static_cast<float>(count) +
				style.ItemSpacing.x * static_cast<float>(count - 1);
			const auto presetsInline = hasLabel &&
				labelWidth + style.ItemSpacing.x + minimumSwatchesWidth <= width;

			if (hasLabel)
			{
				ImGui::AlignTextToFramePadding();
				ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
				ImGui::TextUnformatted(label);
				ImGui::PopTextWrapPos();
				if (presetsInline)
					ImGui::SameLine();
			}

			const auto swatchSpan = (std::max)(
				targetRight - ImGui::GetCursorScreenPos().x,
				1.0f);
			auto gap = 0.0f;
			if (count > 1)
			{
				gap = (std::min)(
					style.ItemSpacing.x,
					(std::max)(
						(swatchSpan - static_cast<float>(count)) /
							static_cast<float>(count - 1),
						0.0f));
			}

			for (size_t index = 0; index < count; ++index)
			{
				if (index > 0)
					ImGui::SameLine(0.0f, gap);
				const auto remainingCount = static_cast<float>(count - index);
				const auto remainingWidth = (std::max)(
					targetRight - ImGui::GetCursorScreenPos().x,
					1.0f);
				// The last swatch absorbs rounding so the row ends flush.
				const auto swatchWidth = index + 1 == count ?
					remainingWidth :
					(std::max)(
						(remainingWidth - gap * (remainingCount - 1.0f)) /
							remainingCount,
						1.0f);

				const auto& preset = a_color.presets[index];
				ImGui::PushID(static_cast<int>(index));
				const auto id = std::string{ preset.name } + "###Preset";
				if (renderer.DrawColorSwatch(
						id.c_str(),
						UnpackRgba(preset.rgba),
						alpha,
						{ swatchWidth, swatchHeight }))
				{
					result.rgba = settle(preset.rgba);
					result.changed = result.rgba != a_color.rgba ? 1u : 0u;
					result.completed = 1u;
				}
				if (preset.description)
					Tooltip::Item(preset.description);
				ImGui::PopID();
			}
		}
		ImGui::PopID();
		return result;
	}
}
