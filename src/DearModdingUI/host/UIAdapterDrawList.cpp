#include "UIAdapterInternal.h"
#include <DearModdingUI/UIBindings.generated.h>
#include "../presentation/PresentationServiceOwners.h"
#include <Support/Utf8.h>

#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace DearModdingUI::UI
{
	namespace
	{
		using namespace AdapterInternal;
		constexpr uint32_t kMaxPoints = 65536;
		struct ClipEntry
		{
			DMUI_ClientHandle client;
			DMUI_DrawTarget target;
			ImDrawList* list;
			int depth;
			int windows;
			int tables;
		};
		thread_local std::vector<ClipEntry> s_clips;
		thread_local size_t s_clipFloor{};

		bool Finite(DMUI_Vec2 a_point) noexcept
		{
			return std::isfinite(a_point.x) && std::isfinite(a_point.y);
		}
		bool Size(float a_value) noexcept
		{
			return std::isfinite(a_value) && a_value >= 0.0f;
		}
		bool Rect(DMUI_Vec2 a_min, DMUI_Vec2 a_max) noexcept
		{
			return Finite(a_min) && Finite(a_max) &&
				a_max.x >= a_min.x && a_max.y >= a_min.y;
		}
		ImU32 Color(uint32_t a_color) noexcept
		{
			return ImGui::ColorConvertFloat4ToU32(StableRGBA(a_color));
		}
		ImDrawList* Target(DMUI_DrawTarget a_target) noexcept
		{
			switch (a_target)
			{
			case DMUI_DRAW_TARGET_WINDOW: return ImGui::GetWindowDrawList();
			case DMUI_DRAW_TARGET_FOREGROUND: return ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
			case DMUI_DRAW_TARGET_BACKGROUND: return ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());
			default: return nullptr;
			}
		}
		template <class Submit>
		DMUI_Result Draw(DMUI_ClientHandle a_client, DMUI_DrawTarget a_target,
			bool a_valid, Submit&& a_submit) noexcept
		{
			const auto result = Validate(a_client);
			if (result != DMUI_RESULT_OK)
				return result;
			if (!a_valid)
				return DMUI_RESULT_INVALID_ARGUMENT;
			auto* list = Target(a_target);
			if (!list)
				return DMUI_RESULT_INVALID_ARGUMENT;
			try
			{
				return a_submit(*list);
			}
			catch (const std::bad_alloc&)
			{
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			}
			catch (...)
			{
				return DMUI_RESULT_CALLBACK_FAILED;
			}
		}
		template <class Submit>
		DMUI_Result Points(DMUI_ClientHandle a_client, DMUI_DrawTarget a_target,
			const DMUI_Vec2* a_points, uint32_t a_count, bool a_valid, Submit&& a_submit) noexcept
		{
			return Draw(a_client, a_target,
				a_valid && a_points && a_count > 0 && a_count <= kMaxPoints,
				[&](ImDrawList& list) {
					std::vector<ImVec2> points;
					points.reserve(a_count);
					for (uint32_t index = 0; index < a_count; ++index)
					{
						if (!Finite(a_points[index]))
							return DMUI_RESULT_INVALID_ARGUMENT;
						points.push_back(Native(a_points[index]));
					}
					a_submit(list, points);
					return DMUI_RESULT_OK;
				});
		}
	}

	DrawListClipScope::DrawListClipScope() noexcept :
		m_depth(s_clips.size()), m_previousFloor(s_clipFloor)
	{
		s_clipFloor = m_depth;
	}
	DrawListClipScope::~DrawListClipScope() noexcept
	{
		auto& context = *ImGui::GetCurrentContext();
		const auto assertEnabled = context.IO.ConfigErrorRecoveryEnableAssert;
		context.IO.ConfigErrorRecoveryEnableAssert = false;
		while (s_clips.size() > m_depth)
		{
			const auto& entry = s_clips.back();
			if (entry.target == DMUI_DRAW_TARGET_WINDOW)
			{
				// End native scopes above this clip before touching their clip entries.
				while (context.CurrentWindowStack.Size > entry.windows ||
					context.TablesTempDataStacked > entry.tables ||
					entry.list->_ClipRectStack.Size > entry.depth + 1)
				{
					if (context.CurrentTable && context.CurrentTable->InnerWindow == context.CurrentWindow)
						ImGui::EndTable();
					else if (context.CurrentWindowStack.Size > entry.windows)
					{
						if (context.CurrentWindow->Flags & ImGuiWindowFlags_ChildWindow)
							ImGui::EndChild();
						else
							ImGui::End();
					}
					else
						break;
				}
			}
			if (entry.list->_ClipRectStack.Size == entry.depth + 1)
				entry.list->PopClipRect();
			s_clips.pop_back();
		}
		context.IO.ConfigErrorRecoveryEnableAssert = assertEnabled;
		s_clipFloor = m_previousFloor;
	}
	bool DrawListClipScope::Balanced() const noexcept
	{
		return s_clips.size() == m_depth;
	}

	namespace AdapterInternal
	{
		bool HasWindowDrawListClip(bool a_table) noexcept
		{
			const auto* list = ImGui::GetWindowDrawList();
			const auto& context = *ImGui::GetCurrentContext();
			return std::ranges::any_of(s_clips, [&](const ClipEntry& entry) {
				return entry.target == DMUI_DRAW_TARGET_WINDOW && entry.list == list &&
					entry.windows >= context.CurrentWindowStack.Size &&
					(!a_table || entry.tables >= context.TablesTempDataStacked);
			});
		}

		DMUI_Result EndDrawWindow(DMUI_ClientHandle a_client, void (*a_end)(), bool a_table) noexcept
		{
			const auto result = Validate(a_client);
			if (result != DMUI_RESULT_OK)
				return result;
			// Native End would consume a client clip instead of its own window clip.
			if (HasWindowDrawListClip(a_table))
				return DMUI_RESULT_INVALID_ARGUMENT;
			a_end();
			return DMUI_RESULT_OK;
		}
	}

	namespace Bindings
	{
		DMUI_Result DMUI_CALL DrawListAddLine(DMUI_ClientHandle client, DMUI_DrawTarget target,
			DMUI_Vec2 p1, DMUI_Vec2 p2, uint32_t color, float thickness) noexcept
		{
			return Draw(client, target, Finite(p1) && Finite(p2) && Size(thickness), [&](ImDrawList& list) {
				list.AddLine(Native(p1), Native(p2), Color(color), thickness);
				return DMUI_RESULT_OK;
			});
		}
		DMUI_Result DMUI_CALL DrawListAddRect(DMUI_ClientHandle client, DMUI_DrawTarget target,
			DMUI_Vec2 min, DMUI_Vec2 max, uint32_t color, float rounding, float thickness) noexcept
		{
			return Draw(client, target, Rect(min, max) && Size(rounding) && Size(thickness), [&](ImDrawList& list) {
				list.AddRect(Native(min), Native(max), Color(color), rounding, 0, thickness);
				return DMUI_RESULT_OK;
			});
		}
		DMUI_Result DMUI_CALL DrawListAddRectFilled(DMUI_ClientHandle client, DMUI_DrawTarget target,
			DMUI_Vec2 min, DMUI_Vec2 max, uint32_t color, float rounding) noexcept
		{
			return Draw(client, target, Rect(min, max) && Size(rounding), [&](ImDrawList& list) {
				list.AddRectFilled(Native(min), Native(max), Color(color), rounding);
				return DMUI_RESULT_OK;
			});
		}
		DMUI_Result DMUI_CALL DrawListAddCircle(DMUI_ClientHandle client, DMUI_DrawTarget target,
			DMUI_Vec2 center, float radius, uint32_t color, uint32_t segments, float thickness) noexcept
		{
			return Draw(client, target, Finite(center) && Size(radius) && Size(thickness) && segments <= kMaxPoints,
				[&](ImDrawList& list) {
					list.AddCircle(Native(center), radius, Color(color), static_cast<int>(segments), thickness);
					return DMUI_RESULT_OK;
				});
		}
		DMUI_Result DMUI_CALL DrawListAddCircleFilled(DMUI_ClientHandle client, DMUI_DrawTarget target,
			DMUI_Vec2 center, float radius, uint32_t color, uint32_t segments) noexcept
		{
			return Draw(client, target, Finite(center) && Size(radius) && segments <= kMaxPoints, [&](ImDrawList& list) {
				list.AddCircleFilled(Native(center), radius, Color(color), static_cast<int>(segments));
				return DMUI_RESULT_OK;
			});
		}
		DMUI_Result DMUI_CALL DrawListAddTriangle(DMUI_ClientHandle client, DMUI_DrawTarget target,
			DMUI_Vec2 p1, DMUI_Vec2 p2, DMUI_Vec2 p3, uint32_t color, float thickness) noexcept
		{
			return Draw(client, target, Finite(p1) && Finite(p2) && Finite(p3) && Size(thickness), [&](ImDrawList& list) {
				list.AddTriangle(Native(p1), Native(p2), Native(p3), Color(color), thickness);
				return DMUI_RESULT_OK;
			});
		}
		DMUI_Result DMUI_CALL DrawListAddTriangleFilled(DMUI_ClientHandle client, DMUI_DrawTarget target,
			DMUI_Vec2 p1, DMUI_Vec2 p2, DMUI_Vec2 p3, uint32_t color) noexcept
		{
			return Draw(client, target, Finite(p1) && Finite(p2) && Finite(p3), [&](ImDrawList& list) {
				list.AddTriangleFilled(Native(p1), Native(p2), Native(p3), Color(color));
				return DMUI_RESULT_OK;
			});
		}
		DMUI_Result DMUI_CALL DrawListAddBezierCubic(DMUI_ClientHandle client, DMUI_DrawTarget target,
			DMUI_Vec2 p1, DMUI_Vec2 p2, DMUI_Vec2 p3, DMUI_Vec2 p4, uint32_t color, float thickness, uint32_t segments) noexcept
		{
			return Draw(client, target, Finite(p1) && Finite(p2) && Finite(p3) && Finite(p4) &&
				Size(thickness) && segments <= kMaxPoints, [&](ImDrawList& list) {
					list.AddBezierCubic(Native(p1), Native(p2), Native(p3), Native(p4),
						Color(color), thickness, static_cast<int>(segments));
					return DMUI_RESULT_OK;
				});
		}
		DMUI_Result DMUI_CALL DrawListAddPolyline(DMUI_ClientHandle client, DMUI_DrawTarget target,
			const DMUI_Vec2* points, uint32_t count, uint32_t color, uint32_t closed, float thickness) noexcept
		{
			return Points(client, target, points, count, closed <= 1 && Size(thickness),
				[&](ImDrawList& list, const auto& native) {
					list.AddPolyline(native.data(), static_cast<int>(count), Color(color),
						closed ? ImDrawFlags_Closed : 0, thickness);
				});
		}
		DMUI_Result DMUI_CALL DrawListAddPolygonFilled(DMUI_ClientHandle client, DMUI_DrawTarget target,
			const DMUI_Vec2* points, uint32_t count, uint32_t color) noexcept
		{
			return Points(client, target, points, count, true, [&](ImDrawList& list, const auto& native) {
				list.AddConcavePolyFilled(native.data(), static_cast<int>(count), Color(color));
			});
		}
		DMUI_Result DMUI_CALL DrawListAddText(DMUI_ClientHandle client, DMUI_DrawTarget target,
			DMUI_Vec2 pos, uint32_t color, const char* text, size_t length, float fontSize) noexcept
		{
			return Draw(client, target, Finite(pos) && Size(fontSize) && text &&
				length <= static_cast<size_t>((std::numeric_limits<int>::max)()), [&](ImDrawList& list) {
					if (!Support::IsValidUtf8(text, length))
						return DMUI_RESULT_INVALID_ARGUMENT;
					list.AddText(ImGui::GetFont(), fontSize == 0 ? ImGui::GetFontSize() : fontSize,
						Native(pos), Color(color), text, text + length);
					return DMUI_RESULT_OK;
				});
		}
		DMUI_Result DMUI_CALL DrawListAddImage(DMUI_ClientHandle client, DMUI_DrawTarget target,
			DMUI_ImageHandle image, DMUI_Vec2 min, DMUI_Vec2 max, DMUI_Vec2 uv0, DMUI_Vec2 uv1, uint32_t tint) noexcept
		{
			return Draw(client, target, Rect(min, max) && Finite(uv0) && Finite(uv1), [&](ImDrawList& list) {
				PresentationServices::ImageResources::AcquiredImage acquired;
				const auto result = PresentationServices::ImageResources::Acquire(client, image, acquired);
				if (result == DMUI_RESULT_OK && acquired.view)
					list.AddImage(reinterpret_cast<ImTextureID>(acquired.view),
						Native(min), Native(max), Native(uv0), Native(uv1), Color(tint));
				return result;
			});
		}
		DMUI_Result DMUI_CALL DrawListPushClipRect(DMUI_ClientHandle client, DMUI_DrawTarget target,
			DMUI_Vec2 min, DMUI_Vec2 max, uint32_t intersectWithCurrent) noexcept
		{
			return Draw(client, target, Rect(min, max) && intersectWithCurrent <= 1, [&](ImDrawList& list) {
				const auto& context = *ImGui::GetCurrentContext();
				s_clips.push_back({ client, target, &list, list._ClipRectStack.Size,
					context.CurrentWindowStack.Size, context.TablesTempDataStacked });
				list.PushClipRect(Native(min), Native(max), intersectWithCurrent != 0);
				return DMUI_RESULT_OK;
			});
		}
		DMUI_Result DMUI_CALL DrawListPopClipRect(DMUI_ClientHandle client, DMUI_DrawTarget target) noexcept
		{
			return Draw(client, target, true, [&](ImDrawList& list) {
				for (size_t index = s_clips.size(); index > s_clipFloor; --index)
				{
					const auto& entry = s_clips[index - 1];
					if (entry.target != target)
						continue;
					if (entry.client != client || entry.list != &list ||
						list._ClipRectStack.Size != entry.depth + 1)
						return DMUI_RESULT_INVALID_ARGUMENT;
					list.PopClipRect();
					s_clips.erase(s_clips.begin() + static_cast<ptrdiff_t>(index - 1));
					return DMUI_RESULT_OK;
				}
				return DMUI_RESULT_INVALID_ARGUMENT;
			});
		}
	}
}
