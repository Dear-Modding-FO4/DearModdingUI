includes("Depends/commonlibf4")

local plugin_name = "DearModdingUI"
local plugin_version = "1.0.0"

local function project_dir(relative)
    return path.join(os.projectdir(), relative)
end

set_project(plugin_name)
set_version(plugin_version)
set_license("GPL-3.0")
set_allowedplats("windows")
set_allowedarchs("x64")
set_allowedmodes("release")
set_defaultmode("release")
set_arch("x64")
set_languages("c++23")
set_toolchains("msvc")
set_warnings("all")
set_runtimes("MT")
set_config("builddir", ".LinkConf/xmake")
set_policy("build.fence", true)

option("msvc_package_toolchain", function()
    set_showmenu(false)

    on_check(function(option)
        os.setenv("CC", "cl")
        os.setenv("CXX", "cl")
        option:enable(true)
    end)
end)

target("imgui", function()
    set_kind("static")
    set_arch("x64")
    set_languages("c++latest")
    set_optimize("fastest")
    set_runtimes("MT")
    set_symbols("debug")
    set_exceptions("cxx")
    set_targetdir(project_dir(".Lib/xmake"))
    set_objectdir(project_dir(".LinkConf/xmake/imgui"))
    set_dependir(project_dir(".LinkConf/xmake/imgui/deps"))

    add_files(
        "Depends/imgui/imgui.cpp",
        "Depends/imgui/imgui_demo.cpp",
        "Depends/imgui/imgui_draw.cpp",
        "Depends/imgui/imgui_tables.cpp",
        "Depends/imgui/imgui_widgets.cpp",
        "Depends/imgui/backends/imgui_impl_win32.cpp",
        "Depends/imgui/backends/imgui_impl_dx11.cpp"
    )

    add_includedirs("Depends/imgui", { public = true })
    add_defines("NDEBUG", "_LIB")
    add_cxxflags(
        "/Ob2",
        "/Oi",
        "/Ot",
        "/Gy",
        "/GS",
        "/arch:AVX",
        "/fp:fast",
        "/permissive-",
        "/sdl",
        "/MP",
        { force = true }
    )
end)

target("dmui-tests", function()
    set_kind("binary")
    set_arch("x64")
    set_languages("c++23")
    set_optimize("fastest")
    set_runtimes("MT")
    set_targetdir(project_dir(".Build/Tests"))
    set_objectdir(project_dir(".LinkConf/xmake/dmui-tests"))
    set_dependir(project_dir(".LinkConf/xmake/dmui-tests/deps"))

    add_deps("imgui")
    add_files(
        "Tests/**.cpp",
        "src/DearModdingUI/FontCatalog.cpp",
        "src/DearModdingUI/Hotkeys.cpp",
        "src/DearModdingUI/Navigation.cpp",
        "src/DearModdingUI/Registry.cpp",
        "src/DearModdingUI/Status.cpp"
    )
    add_includedirs(
        "Tests",
        "include",
        "Depends",
        "Depends/commonlibf4/include",
        "Depends/commonlibf4/lib/dearmoddingui-api/include"
    )
    add_defines(
        "NDEBUG",
        "NOMINMAX",
        "WIN32_LEAN_AND_MEAN"
    )
    add_cxxflags(
        "/permissive-",
        "/Zc:preprocessor",
        { public = true }
    )
end)

target(plugin_name, function()
    set_optimize("fastest")
    set_symbols("debug")
    set_exceptions("cxx")
    set_targetdir(project_dir(".Build/F4SE/Plugins"))
    set_objectdir(project_dir(".LinkConf/xmake/DearModdingUI"))
    set_dependir(project_dir(".LinkConf/xmake/DearModdingUI/deps"))

    add_rules("commonlibf4.plugin", {
        name = plugin_name,
        author = "Dear Modding FO4",
        description = "Shared Dear ImGui menu host for Fallout 4"
    })

    add_deps("imgui")
    add_files(
        "src/**.cpp",
        "Depends/cimgui/cimgui.cpp"
    )
    add_headerfiles("include/**.h")
    add_extrafiles("data/**", "README.md", "THIRD_PARTY_NOTICES.md")
    add_includedirs(
        "include",
        "Depends",
        "Depends/toml11/single_include"
    )
    add_defines(
        "NDEBUG",
        "NOMINMAX",
        "WIN32_LEAN_AND_MEAN",
        "_CRT_SECURE_NO_WARNINGS"
    )
    add_cxxflags(
        "/permissive-",
        "/Zc:preprocessor",
        { public = true }
    )
    add_syslinks("d3d11", "dxgi", "d3dcompiler")
    set_pcxxheader("Depends/commonlibf4/include/F4SE/Impl/PCH.h")

    after_build(function(target)
        os.cp(
            path.join(project_dir("data/F4SE/Plugins"), "*"),
            target:targetdir()
        )
    end)
end)
