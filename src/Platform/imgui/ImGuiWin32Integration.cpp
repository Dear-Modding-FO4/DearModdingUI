#ifndef WIN32_LEAN_AND_MEAN
#	define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "ImGuiWin32Integration.h"

#include <cstring>
#include <cwchar>

namespace
{
	constexpr char kUpstreamContextPropertyName[] = "IMGUI_CONTEXT";
	constexpr wchar_t kUpstreamPlatformWindowClassName[] =
		L"ImGui Platform";

	bool IsNamedResource(const char* a_name) noexcept
	{
		return a_name != nullptr && !IS_INTRESOURCE(a_name);
	}

	bool IsNamedResource(const wchar_t* a_name) noexcept
	{
		return a_name != nullptr && !IS_INTRESOURCE(a_name);
	}

	bool IsUpstreamContextProperty(const char* a_name) noexcept
	{
		return IsNamedResource(a_name) &&
		       std::strcmp(a_name, kUpstreamContextPropertyName) == 0;
	}

	bool IsUpstreamPlatformWindowClass(const wchar_t* a_name) noexcept
	{
		return IsNamedResource(a_name) &&
		       std::wcscmp(a_name, kUpstreamPlatformWindowClassName) == 0;
	}
}

static BOOL WINAPI DmuiImGuiSetPropA(
	HWND a_window,
	LPCSTR a_name,
	HANDLE a_data)
{
	if (IsUpstreamContextProperty(a_name))
		a_name = DearModdingUI::ImGuiWin32Integration::kContextPropertyName;
	return ::SetPropA(a_window, a_name, a_data);
}

static HANDLE WINAPI DmuiImGuiGetPropA(HWND a_window, LPCSTR a_name)
{
	if (IsUpstreamContextProperty(a_name))
		a_name = DearModdingUI::ImGuiWin32Integration::kContextPropertyName;
	return ::GetPropA(a_window, a_name);
}

static ATOM WINAPI DmuiImGuiRegisterClassExW(
	const WNDCLASSEXW* a_windowClass)
{
	if (a_windowClass == nullptr ||
	    !IsUpstreamPlatformWindowClass(a_windowClass->lpszClassName))
		return ::RegisterClassExW(a_windowClass);

	auto privateWindowClass = *a_windowClass;
	privateWindowClass.lpszClassName =
		DearModdingUI::ImGuiWin32Integration::kPlatformWindowClassName;
	return ::RegisterClassExW(&privateWindowClass);
}

static BOOL WINAPI DmuiImGuiUnregisterClassW(
	LPCWSTR a_className,
	HINSTANCE a_instance)
{
	if (IsUpstreamPlatformWindowClass(a_className))
		a_className =
			DearModdingUI::ImGuiWin32Integration::kPlatformWindowClassName;
	return ::UnregisterClassW(a_className, a_instance);
}

static HWND WINAPI DmuiImGuiCreateWindowExW(
	DWORD a_extendedStyle,
	LPCWSTR a_className,
	LPCWSTR a_windowName,
	DWORD a_style,
	int a_x,
	int a_y,
	int a_width,
	int a_height,
	HWND a_parent,
	HMENU a_menu,
	HINSTANCE a_instance,
	LPVOID a_parameter)
{
	if (IsUpstreamPlatformWindowClass(a_className))
		a_className =
			DearModdingUI::ImGuiWin32Integration::kPlatformWindowClassName;
	return ::CreateWindowExW(
		a_extendedStyle,
		a_className,
		a_windowName,
		a_style,
		a_x,
		a_y,
		a_width,
		a_height,
		a_parent,
		a_menu,
		a_instance,
		a_parameter);
}

// Keep upstream code intact while making its process-global Win32 names product-private.
#define SetPropA DmuiImGuiSetPropA
#define GetPropA DmuiImGuiGetPropA
#define RegisterClassExW DmuiImGuiRegisterClassExW
#define UnregisterClassW DmuiImGuiUnregisterClassW
#define CreateWindowExW DmuiImGuiCreateWindowExW

#include "../../../Depends/imgui/backends/imgui_impl_win32.cpp"

#undef CreateWindowExW
#undef UnregisterClassW
#undef RegisterClassExW
#undef GetPropA
#undef SetPropA
