#include <DearModdingUI/presentation/Logo.h>

#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace DearModdingUI
{
	namespace
	{
		constexpr size_t kCircleSegments = 128;
		using Polygon = std::vector<ImVec2>;
		using LogoGeometry = std::array<std::array<Polygon, 4>, 4>;

		[[nodiscard]] float EdgeDistance(
			ImVec2 a_start, ImVec2 a_end, ImVec2 a_point) noexcept
		{
			return (a_end.x - a_start.x) * (a_point.y - a_start.y) -
				(a_end.y - a_start.y) * (a_point.x - a_start.x);
		}

		[[nodiscard]] LogoGeometry BuildLogoGeometry()
		{
			std::array<ImVec2, kCircleSegments> circle{};
			for (size_t index = 0; index < circle.size(); ++index)
			{
				const auto angle = 2.0f * std::numbers::pi_v<float> *
					static_cast<float>(index) / static_cast<float>(circle.size());
				circle[index] = { std::cos(angle), std::sin(angle) };
			}
			LogoGeometry geometry;
			constexpr std::array<ImVec2, 4> axes{
				ImVec2{ 0.0f, -1.0f }, ImVec2{ -1.0f, 0.0f },
				ImVec2{ 1.0f, 0.0f }, ImVec2{ 0.0f, 1.0f }
			};
			for (size_t band = 0; band < 4; ++band)
			{
				const auto low = 0.060f + static_cast<float>(band) * 0.2274f;
				const auto high = low + 0.113f;
				Polygon polygon{
					{ 0.0f, low }, { 2.0f, low + 2.0f },
					{ 2.0f, high + 2.0f }, { 0.0f, high },
					{ -2.0f, high + 2.0f }, { -2.0f, low + 2.0f }
				};
				// Clip the whole V so the concave vertex has no shared-edge AA seam.
				for (size_t edge = 0; edge < circle.size(); ++edge)
				{
					Polygon clipped;
					auto previous = polygon.back();
					auto previousDistance = EdgeDistance(
						circle[edge], circle[(edge + 1) % circle.size()], previous);
					for (const auto point : polygon)
					{
						const auto distance = EdgeDistance(
							circle[edge], circle[(edge + 1) % circle.size()], point);
						if ((distance >= 0.0f) != (previousDistance >= 0.0f))
						{
							const auto fraction =
								previousDistance / (previousDistance - distance);
							clipped.push_back({
								previous.x + fraction * (point.x - previous.x),
								previous.y + fraction * (point.y - previous.y)
							});
						}
						if (distance >= 0.0f)
							clipped.push_back(point);
						previous = point;
						previousDistance = distance;
					}
					polygon = std::move(clipped);
				}
				for (size_t wedge = 0; wedge < axes.size(); ++wedge)
				{
					const auto axis = axes[wedge];
					for (const auto point : polygon)
						geometry[wedge][band].push_back({
							point.y * axis.x + point.x * axis.y,
							point.y * axis.y - point.x * axis.x
						});
				}
			}
			return geometry;
		}
	}

	void DrawLogo(
		ImDrawList* a_drawList,
		ImVec2 a_center,
		float a_radius,
		const LogoColors& a_colors) noexcept
	{
		if (!a_drawList || !(a_radius > 0.0f))
			return;
		static const auto geometry = BuildLogoGeometry();
		const std::array colors{
			a_colors.top, a_colors.left, a_colors.right, a_colors.bottom
		};
		std::array<ImVec2, kCircleSegments + 6> vertices{};
		for (size_t wedge = 0; wedge < geometry.size(); ++wedge)
		{
			for (const auto& polygon : geometry[wedge])
			{
				for (size_t index = 0; index < polygon.size(); ++index)
					vertices[index] = {
						a_center.x + polygon[index].x * a_radius,
						a_center.y + polygon[index].y * a_radius
					};
				a_drawList->AddConcavePolyFilled(
					vertices.data(), static_cast<int>(polygon.size()), colors[wedge]);
			}
		}
	}
}
