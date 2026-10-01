#include "GeneralTestSuiteInternal.h"

namespace DmuiTests::Detail
{
	void PresentationResources::EnableDrawingMarkers() noexcept
	{
		m_foregroundMarker = m_backgroundMarker = true;
	}

	bool PresentationResources::DrawingCaptureComplete() const noexcept
	{
		return m_customDrawingComplete;
	}

	void PresentationResources::DrawCustomDrawing() noexcept
	{
		namespace ui = dmui::ui;
		(void)ui::Checkbox("Foreground marker", &m_foregroundMarker);
		ui::SameLine();
		(void)ui::Checkbox("Background marker", &m_backgroundMarker);
		const auto origin = ui::GetCursorScreenPos();
		const auto scale = ui::GetFontSize() / 26.0f;
		const auto point = [origin, scale](float x, float y) -> ui::Vec2 {
			return { origin.x + x * scale, origin.y + y * scale };
		};
		const auto draw = ui::WindowDrawList();
		constexpr ui::Color32 cyan = 0x54D9EFFF, orange = 0xFFA85CFF, green = 0x83DD9AFF;
		draw.AddLine(point(10, 20), point(85, 65), cyan, 2 * scale);
		draw.AddRect(point(105, 10), point(175, 70), orange, 12 * scale, 2 * scale);
		draw.AddRectFilled(point(195, 10), point(265, 70), cyan, 12 * scale);
		draw.AddCircle(point(315, 40), 30 * scale, green, 0, 2 * scale);
		draw.AddCircleFilled(point(395, 40), 30 * scale, orange);
		draw.AddTriangle(point(455, 10), point(430, 70), point(485, 70), cyan, 2 * scale);
		draw.AddTriangleFilled(point(525, 10), point(500, 70), point(555, 70), green);
		draw.AddText(point(10, 85), 0xE8EDF5FF, "Line / rounded rectangles / circles / triangles");

		draw.AddBezierCubic(point(10, 170), point(40, 80), point(110, 230), point(150, 130), cyan, 3 * scale);
		const std::array polyline{ point(170, 170), point(190, 120), point(220, 160), point(245, 120) };
		draw.AddPolyline(polyline, orange, false, 3 * scale);
		const std::array concave{
			point(270, 120), point(340, 120), point(340, 180),
			point(305, 150), point(270, 180)
		};
		draw.AddPolygonFilled(concave, green);
		draw.AddText(point(365, 125), cyan, "Custom size", 32 * scale);
		draw.AddText(point(10, 195), 0xE8EDF5FF, "Bezier / polyline / concave polygon / text");

		if (m_image)
			draw.AddImage(m_image->Handle(), point(10, 235), point(85, 310));
		draw.AddText(point(10, 320), 0xE8EDF5FF, "Image");
		{
			ui::ClipRectScope clip{ draw, point(115, 235), point(340, 310) };
			draw.AddCircleFilled(point(120, 260), 55 * scale, cyan);
			draw.AddRectFilled(point(200, 210), point(365, 285), orange, 10 * scale);
			draw.AddText(point(160, 288), 0xFFFFFFFF, "Clipped text continues beyond the border");
		}
		draw.AddRect(point(115, 235), point(340, 310), 0xFFFFFFFF, 0, 2 * scale);
		draw.AddText(point(115, 320), 0xE8EDF5FF, "Clip rect");
		ui::Dummy({ 560 * scale, 350 * scale });

		if (m_backgroundMarker)
		{
			const auto background = ui::BackgroundDrawList();
			background.AddRectFilled({ 8, 130 }, { 150, 205 }, 0x276A7FFF, 10);
			background.AddText({ 18, 143 }, 0xFFFFFFFF, "BACKGROUND", 18);
			background.AddCircle({ 75, 180 }, 12, orange, 0, 2);
		}
		if (m_foregroundMarker)
		{
			const auto foreground = ui::ForegroundDrawList();
			foreground.AddRectFilled({ 18, 18 }, { 230, 62 }, 0xD48535EE, 8);
			foreground.AddText({ 30, 28 }, 0xFFFFFFFF, "FOREGROUND", 22);
		}
		m_customDrawingComplete = m_image && m_imageStatus == DMUI_IMAGE_STATUS_READY &&
			m_foregroundMarker && m_backgroundMarker && ui::detail::LastResult() == DMUI_RESULT_OK;
	}
}
