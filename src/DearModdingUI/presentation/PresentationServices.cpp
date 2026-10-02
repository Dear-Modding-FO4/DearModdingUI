#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/presentation/Theme.h>
#include <DearModdingUI/host/RenderExecution.h>
#include "PresentationServiceOwners.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cmath>

namespace DearModdingUI::PresentationServices
{
	namespace
	{
		inline constexpr size_t kPlotSampleLimit{ 1'000'000 };
		inline constexpr size_t kPlotReferenceLimit{ 64 };
	}

	void SetDevice(ID3D11Device* a_device) noexcept
	{
		ImageResources::SetDevice(a_device);
	}

	void InvalidateDevice() noexcept
	{
		SetDevice(nullptr);
		Notifications::Clear();
	}

	void BeginFrame() noexcept
	{
		DiscardFrame();
		ImageResources::PublishCompletions();
	}

	void CompleteRenderSubmission() noexcept
	{
		ImageResources::ReleaseFrameLeases();
	}

	void DiscardFrame() noexcept
	{
		ImageResources::ReleaseFrameLeases();
	}

	uint64_t DeviceGeneration() noexcept
	{
		return ImageResources::DeviceGeneration();
	}

	DMUI_Result DrawAnnotatedPlot(
		DMUI_ClientHandle a_client,
		const char* a_id,
		const DMUI_AnnotatedPlotDescriptor* a_descriptor) noexcept
	{
		if (!a_id || !*a_id || !a_descriptor ||
			a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (!RenderExecution::IsActiveClient(a_client, true))
			return DMUI_RESULT_WRONG_THREAD;
		if (a_descriptor->sampleCount > kPlotSampleLimit ||
			a_descriptor->referenceLineCount > kPlotReferenceLimit ||
			(a_descriptor->sampleCount && !a_descriptor->samples) ||
			(a_descriptor->referenceLineCount && !a_descriptor->referenceLines) ||
			!std::isfinite(a_descriptor->scaleMinimum) ||
			!std::isfinite(a_descriptor->scaleMaximum) ||
			a_descriptor->scaleMaximum <= a_descriptor->scaleMinimum ||
			!std::isfinite(a_descriptor->size.x) ||
			!std::isfinite(a_descriptor->size.y))
			return DMUI_RESULT_INVALID_ARGUMENT;
		for (size_t index = 0; index < a_descriptor->sampleCount; ++index)
			if (!std::isfinite(a_descriptor->samples[index]))
				return DMUI_RESULT_INVALID_ARGUMENT;
		for (size_t index = 0; index < a_descriptor->referenceLineCount; ++index)
			if (!std::isfinite(a_descriptor->referenceLines[index].value))
				return DMUI_RESULT_INVALID_ARGUMENT;

		const auto sampleCount = static_cast<int>(a_descriptor->sampleCount);
		const auto sampleOffset = sampleCount ?
			static_cast<int>(a_descriptor->sampleOffset %
				a_descriptor->sampleCount) :
			0;
		const auto labelSize = ImGui::CalcTextSize(a_id, nullptr, true);
		const auto frameSize = ImGui::CalcItemSize(
			{ a_descriptor->size.x, a_descriptor->size.y },
			ImGui::CalcItemWidth(),
			labelSize.y + ImGui::GetStyle().FramePadding.y * 2.0f);
		const auto itemMinimum = ImGui::GetCursorScreenPos();
		const auto padding = ImGui::GetStyle().FramePadding;
		const ImVec2 plotMinimum{
			itemMinimum.x + padding.x,
			itemMinimum.y + padding.y
		};
		const ImVec2 plotMaximum{
			itemMinimum.x + frameSize.x - padding.x,
			itemMinimum.y + frameSize.y - padding.y
		};
		auto* drawList = ImGui::GetWindowDrawList();
		const auto visible = !ImGui::GetCurrentWindow()->SkipItems && ImGui::IsRectVisible(frameSize);
		if (visible)
		{
			// Plots are display surfaces, not input fields, so they share the panel surface.
			const auto surface = Theme::ColorSnapshot().panel;
			ImGui::RenderFrame(itemMinimum,
				{ itemMinimum.x + frameSize.x, itemMinimum.y + frameSize.y },
				ImGui::GetColorU32(ImVec4{ surface.x, surface.y, surface.z, surface.w }), true,
				ImGui::GetStyle().FrameRounding);
			drawList->PushClipRect(plotMinimum, plotMaximum, true);
			for (size_t index = 0; index < a_descriptor->referenceLineCount; ++index)
			{
				const auto& line = a_descriptor->referenceLines[index];
				if (line.value < a_descriptor->scaleMinimum ||
					line.value > a_descriptor->scaleMaximum)
					continue;
				const auto ratio =
					(line.value - a_descriptor->scaleMinimum) /
					(a_descriptor->scaleMaximum - a_descriptor->scaleMinimum);
				const auto y =
					plotMaximum.y -
					ratio * (plotMaximum.y - plotMinimum.y);
				drawList->AddLine(
					{ plotMinimum.x, y },
					{ plotMaximum.x, y },
					ImGui::GetColorU32(ImVec4{
						line.color.x,
						line.color.y,
						line.color.z,
						line.color.w
					}));
			}
			drawList->PopClipRect();
		}

		// Keep ImGui's sampling, hover, and item behavior without repainting the frame.
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{});
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		ImGui::PlotLines(a_id, a_descriptor->samples, sampleCount, sampleOffset,
			nullptr, a_descriptor->scaleMinimum, a_descriptor->scaleMaximum,
			{ a_descriptor->size.x, a_descriptor->size.y });
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
		if (visible && a_descriptor->overlayText && *a_descriptor->overlayText)
		{
			const auto wrapWidth = (std::max)(1.0f, plotMaximum.x - plotMinimum.x - padding.x * 2.0f);
			const auto textSize = ImGui::CalcTextSize(a_descriptor->overlayText, nullptr, false, wrapWidth);
			const auto width = (std::min)(textSize.x, wrapWidth);
			const ImVec2 textMinimum{
				plotMinimum.x + (plotMaximum.x - plotMinimum.x - width) * 0.5f,
				plotMinimum.y + padding.y
			};
			const ImVec2 textMaximum{ textMinimum.x + width, textMinimum.y + textSize.y };
			drawList->PushClipRect(plotMinimum, plotMaximum, true);
			drawList->AddRectFilled(
				{ textMinimum.x - padding.x, textMinimum.y - padding.y },
				{ textMaximum.x + padding.x, textMaximum.y + padding.y },
				ImGui::GetColorU32(ImGuiCol_WindowBg), ImGui::GetStyle().FrameRounding);
			drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textMinimum,
				ImGui::GetColorU32(ImGuiCol_Text), a_descriptor->overlayText, nullptr, wrapWidth);
			drawList->PopClipRect();
		}
		return DMUI_RESULT_OK;
	}

	bool HasFrameDemand() noexcept
	{
		if (!ImageResources::HasDevice())
			return false;
		return ImageResources::HasFrameDemand() || Notifications::HasFrameDemand() ||
			Dialogs::HasFrameDemand();
	}

}
