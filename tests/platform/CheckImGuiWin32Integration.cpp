#include "../Harness.h"
#include "../support/ImGuiTestContext.h"

#include <Platform/imgui/ImGuiWin32Integration.h>

#include <imgui/backends/imgui_impl_win32.h>

#include <Windows.h>

#include <array>
#include <string_view>

namespace vmm_tests
{
	namespace
	{
		constexpr char kForeignContextPropertyName[] = "IMGUI_CONTEXT";
		constexpr wchar_t kForeignPlatformWindowClassName[] =
			L"ImGui Platform";
		int g_foreignContextSentinel;

		LRESULT CALLBACK ForeignWindowProcedure(
			HWND a_window,
			UINT a_message,
			WPARAM a_wParam,
			LPARAM a_lParam)
		{
			return DefWindowProcW(a_window, a_message, a_wParam, a_lParam);
		}

		class HiddenWindow
		{
		public:
			HiddenWindow() :
				m_window(CreateWindowExW(
					0,
					L"STATIC",
					L"DMUI ImGui Win32 integration test",
					WS_POPUP,
					0,
					0,
					64,
					64,
					nullptr,
					nullptr,
					GetModuleHandleW(nullptr),
					nullptr))
			{
				require(m_window != nullptr, "hidden test window creation failed");
			}

			~HiddenWindow()
			{
				if (m_window != nullptr)
					DestroyWindow(m_window);
			}

			HiddenWindow(const HiddenWindow&) = delete;
			HiddenWindow& operator=(const HiddenWindow&) = delete;

			[[nodiscard]] HWND Get() const noexcept
			{
				return m_window;
			}

		private:
			HWND m_window{};
		};

		class ForeignContextProperty
		{
		public:
			explicit ForeignContextProperty(HWND a_window) :
				m_window(a_window)
			{
				require(
					SetPropA(
						m_window,
						kForeignContextPropertyName,
						&g_foreignContextSentinel) != FALSE,
					"foreign ImGui context property setup failed");
			}

			~ForeignContextProperty()
			{
				RemovePropA(m_window, kForeignContextPropertyName);
			}

			ForeignContextProperty(const ForeignContextProperty&) = delete;
			ForeignContextProperty& operator=(
				const ForeignContextProperty&) = delete;

			[[nodiscard]] HANDLE Value() const noexcept
			{
				return &g_foreignContextSentinel;
			}

		private:
			HWND m_window{};
		};

		class ForeignWindowClass
		{
		public:
			ForeignWindowClass() :
				m_instance(GetModuleHandleW(nullptr))
			{
				WNDCLASSEXW windowClass{};
				windowClass.cbSize = sizeof(windowClass);
				windowClass.lpfnWndProc = ForeignWindowProcedure;
				windowClass.hInstance = m_instance;
				windowClass.lpszClassName =
					kForeignPlatformWindowClassName;
				require(
					RegisterClassExW(&windowClass) != 0,
					"foreign ImGui platform class registration failed");
				m_registered = true;
			}

			~ForeignWindowClass()
			{
				if (m_registered)
					UnregisterClassW(
						kForeignPlatformWindowClassName,
						m_instance);
			}

			ForeignWindowClass(const ForeignWindowClass&) = delete;
			ForeignWindowClass& operator=(const ForeignWindowClass&) = delete;

			[[nodiscard]] HINSTANCE Instance() const noexcept
			{
				return m_instance;
			}

		private:
			HINSTANCE m_instance{};
			bool m_registered{};
		};

		class Win32BackendFixture
		{
		public:
			~Win32BackendFixture()
			{
				Shutdown();
			}

			Win32BackendFixture(const Win32BackendFixture&) = delete;
			Win32BackendFixture& operator=(const Win32BackendFixture&) =
				delete;

			Win32BackendFixture() = default;

			void Initialize()
			{
				require(
					ImGui_ImplWin32_Init(m_window.Get()),
					"DMUI Win32 backend initialization failed");
				m_initialized = true;
			}

			void Shutdown()
			{
				if (!m_initialized)
					return;
				ImGui::SetCurrentContext(m_context.Get());
				ImGui_ImplWin32_Shutdown();
				m_initialized = false;
			}

			[[nodiscard]] ImGuiContext* Context() const noexcept
			{
				return m_context.Get();
			}

			[[nodiscard]] HWND Window() const noexcept
			{
				return m_window.Get();
			}

		private:
			support::ImGuiTestContext m_context;
			HiddenWindow m_window;
			bool m_initialized{};
		};

		class SecondaryViewport
		{
		public:
			SecondaryViewport()
			{
				auto& platformIO = ImGui::GetPlatformIO();
				require(
					platformIO.Platform_CreateWindow != nullptr &&
						platformIO.Platform_DestroyWindow != nullptr,
					"Win32 platform callbacks were not installed");

				m_viewport.ID = 0xD4D55449;
				m_viewport.Pos = { 0.0f, 0.0f };
				m_viewport.Size = { 64.0f, 64.0f };
				m_viewport.DpiScale = 1.0f;
				platformIO.Platform_CreateWindow(&m_viewport);
				if (m_viewport.PlatformHandle == nullptr)
				{
					platformIO.Platform_DestroyWindow(&m_viewport);
					require(false, "hidden secondary viewport creation failed");
				}
				platformIO.Viewports.push_back(&m_viewport);
				m_listed = true;
			}

			~SecondaryViewport()
			{
				auto& platformIO = ImGui::GetPlatformIO();
				if (m_viewport.PlatformUserData != nullptr)
					platformIO.Platform_DestroyWindow(&m_viewport);
				if (!m_listed)
					return;
				for (int index = 0; index < platformIO.Viewports.Size; ++index)
				{
					if (platformIO.Viewports[index] == &m_viewport)
					{
						platformIO.Viewports.erase(
							platformIO.Viewports.Data + index);
						break;
					}
				}
			}

			SecondaryViewport(const SecondaryViewport&) = delete;
			SecondaryViewport& operator=(const SecondaryViewport&) = delete;

			[[nodiscard]] HWND Window() const noexcept
			{
				return static_cast<HWND>(m_viewport.PlatformHandle);
			}

			[[nodiscard]] ImGuiViewport& Viewport() noexcept
			{
				return m_viewport;
			}

		private:
			ImGuiViewport m_viewport;
			bool m_listed{};
		};

		[[nodiscard]] bool FindWindowClass(
			HINSTANCE a_instance,
			const wchar_t* a_name,
			WNDCLASSEXW& a_windowClass)
		{
			a_windowClass = {};
			a_windowClass.cbSize = sizeof(a_windowClass);
			return GetClassInfoExW(a_instance, a_name, &a_windowClass) !=
			       FALSE;
		}

		void RequireForeignState(
			const Win32BackendFixture& a_backend,
			const ForeignContextProperty& a_property,
			HINSTANCE a_instance)
		{
			require(
				GetPropA(
					a_backend.Window(),
					kForeignContextPropertyName) == a_property.Value(),
				"DMUI replaced the foreign ImGui context property");

			WNDCLASSEXW foreignWindowClass{};
			require(
				FindWindowClass(
					a_instance,
					kForeignPlatformWindowClassName,
					foreignWindowClass) &&
					foreignWindowClass.lpfnWndProc ==
						ForeignWindowProcedure,
				"DMUI replaced or unregistered the foreign ImGui platform class");
		}

		void RequirePrivateStateRemoved(
			const Win32BackendFixture& a_backend,
			HINSTANCE a_instance)
		{
			using namespace DearModdingUI::ImGuiWin32Integration;
			require(
				GetPropA(a_backend.Window(), kContextPropertyName) == nullptr,
				"DMUI retained its private ImGui context property after shutdown");

			WNDCLASSEXW privateWindowClass{};
			require(
				!FindWindowClass(
					a_instance,
					kPlatformWindowClassName,
					privateWindowClass),
				"DMUI retained its private ImGui platform class after shutdown");
		}

		void RequirePrivateSecondaryViewport(ImGuiContext* a_context)
		{
			using namespace DearModdingUI::ImGuiWin32Integration;
			SecondaryViewport viewport;
			const auto window = viewport.Window();

			std::array<wchar_t, 128> className{};
			const auto classNameLength = GetClassNameW(
				window,
				className.data(),
				static_cast<int>(className.size()));
			require(
				classNameLength > 0 &&
					std::wstring_view(
						className.data(),
						static_cast<std::size_t>(classNameLength)) ==
						kPlatformWindowClassName,
				"DMUI secondary viewport used the shared upstream class");
			require(
				GetPropA(window, kContextPropertyName) == a_context &&
					GetPropA(window, kForeignContextPropertyName) == nullptr,
				"DMUI secondary viewport used the shared upstream context property");

			viewport.Viewport().PlatformRequestClose = false;
			SendMessageW(window, WM_CLOSE, 0, 0);
			require(
				viewport.Viewport().PlatformRequestClose &&
					IsWindow(window) != FALSE,
				"DMUI private viewport procedure could not recover its context");
		}
	}

	void run_imgui_win32_integration_checks(Runner& runner)
	{
		for (const auto foreignFirst : { true, false })
		{
			runner.test(
				foreignFirst ?
					"Win32 backend preserves a foreign integration initialized first" :
					"Win32 backend preserves a foreign integration initialized later",
				[foreignFirst] {
					Win32BackendFixture backend;
					if (!foreignFirst)
						backend.Initialize();

					ForeignContextProperty foreignProperty(backend.Window());
					ForeignWindowClass foreignClass;
					if (foreignFirst)
						backend.Initialize();

					RequireForeignState(
						backend, foreignProperty, foreignClass.Instance());
					RequirePrivateSecondaryViewport(backend.Context());
					backend.Shutdown();
					RequireForeignState(
						backend, foreignProperty, foreignClass.Instance());
					RequirePrivateStateRemoved(backend, foreignClass.Instance());
				});
		}
	}
}
