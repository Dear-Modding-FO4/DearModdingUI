includes("Depends/commonlibf4")

local plugin_name = "DearModdingUI"
local plugin_version = "0.1.0"

local function project_dir(relative)
    return path.join(os.projectdir(), relative)
end

local function canonical_path(value)
    return path.normalize(path.absolute(value)):lower()
end

local function path_is_within(value, root)
    local candidate = canonical_path(value)
    local parent = canonical_path(root)
    if candidate == parent then
        return true
    end
    if candidate:sub(1, #parent) ~= parent then
        return false
    end
    local separator = candidate:sub(#parent + 1, #parent + 1)
    return separator == "/" or separator == "\\"
end

local configured_release_variant

local function release_variant()
    return configured_release_variant or "release"
end

local function plugin_output_dir()
    return project_dir(path.join(
        ".Build",
        release_variant(),
        "F4SE",
        "Plugins"
    ))
end

local function plugin_object_dir(name)
    return path.join(".LinkConf/xmake", release_variant(), name)
end

local function disable_commonlib_auto_install(target)
    -- Package assembly owns deployment, so CommonLib must have no install map.
    target:set("installfiles")
    target:set(
        "installdir",
        project_dir(path.join(
            ".Build",
            "no-auto-install",
            release_variant(),
            target:name()
        ))
    )
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

option("test-release", function()
    set_default(false)
    set_showmenu(true)
    set_description("Build the diagnostic test distribution and DMUI test client")
end)

configured_release_variant = has_config("test-release") and "test" or "release"

target("imgui", function()
    set_kind("static")
    set_arch("x64")
    set_languages("c++latest")
    set_optimize("fastest")
    set_runtimes("MT")
    set_symbols("debug")
    set_exceptions("cxx")
    set_targetdir(project_dir(".Lib/xmake"))
    set_objectdir(".LinkConf/xmake/imgui")
    set_dependir(".LinkConf/xmake/imgui/deps")

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

target("dmui-mcm", function()
    set_kind("static")
    set_arch("x64")
    set_languages("c++23")
    set_optimize("fastest")
    set_runtimes("MT")
    set_exceptions("cxx")
    set_targetdir(project_dir(".Lib/xmake"))
    set_objectdir(".LinkConf/xmake/dmui-mcm")
    set_dependir(".LinkConf/xmake/dmui-mcm/deps")

    add_files("mcm/src/**.cpp")
    add_headerfiles("mcm/include/**.h")
    add_includedirs(
        "mcm/include",
        "Depends/commonlibf4/lib/dearmoddingui-api/include",
        { public = true }
    )
    add_includedirs("Depends/nlohmann-json/single_include")
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

target("dmui-tests", function()
    set_kind("binary")
    set_arch("x64")
    set_languages("c++23")
    set_optimize("fastest")
    set_runtimes("MT")
    set_targetdir(project_dir(".Build/Tests"))
    add_defines('DMUI_VERSION="' .. plugin_version .. '"')
    set_objectdir(".LinkConf/xmake/dmui-tests")
    set_dependir(".LinkConf/xmake/dmui-tests/deps")

    add_deps("imgui", "dmui-mcm")
    add_files(
        "Tests/**.cpp",
        "Fixtures/GeneralTestFixtures.cpp",
        "Depends/commonlibf4/lib/dearmoddingui-api/Tests/CompileForwardingNoHost.cpp",
        "Depends/commonlibf4/lib/dearmoddingui-api/Tests/CompileHostAPILayout.cpp",
        "Depends/commonlibf4/lib/dearmoddingui-api/Tests/CompileIconGlyphs.cpp",
        "Depends/commonlibf4/lib/dearmoddingui-api/Tests/CompileImGuiForward.cpp",
        "Depends/commonlibf4/lib/dearmoddingui-api/Tests/CompileNoWindowsMacros.cpp",
        "Depends/commonlibf4/lib/dearmoddingui-api/Tests/CompileSettingsActions.cpp",
        "Depends/commonlibf4/lib/dearmoddingui-api/Tests/CompileVisualDecisions.cpp",
        "src/DearModdingUI/Diagnostics.cpp",
        "src/DearModdingUI/ExternalOpen.cpp",
        "src/DearModdingUI/FontCatalog.cpp",
        "src/DearModdingUI/Health.cpp",
        "src/DearModdingUI/Home.cpp",
        "src/DearModdingUI/HostSettingsHealth.cpp",
        "src/DearModdingUI/Hotkeys.cpp",
        "src/DearModdingUI/MenuDismissal.cpp",
        "src/DearModdingUI/Navigation.cpp",
        "src/DearModdingUI/NavigationController.cpp",
        "src/DearModdingUI/NavigationPresentation.cpp",
        "src/DearModdingUI/PresentationServices.cpp",
        "src/DearModdingUI/RenderExecution.cpp",
        "src/DearModdingUI/Registry.cpp",
        "src/DearModdingUI/SettingsTable.cpp",
        "src/DearModdingUI/Status.cpp"
    )
    add_includedirs(
        "Preview/include",
        "Tests",
        "Fixtures",
        "src",
        "include",
        "Depends",
        "Depends/toml11/single_include",
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
    add_syslinks("bcrypt", "d3d11", "dxgi", "shell32")
    add_ldflags(
        "/EXPORT:DMUI_GetHostAPI",
        "/EXPORT:DMUI_GetImGuiVersionNum",
        { force = true }
    )
end)

target("dmui-preview", function()
    set_kind("binary")
    set_arch("x64")
    set_languages("c++23")
    set_optimize("fastest")
    set_symbols("debug")
    set_exceptions("cxx")
    set_runtimes("MT")
    set_targetdir(project_dir(".Build/Preview"))
    add_defines('DMUI_VERSION="' .. plugin_version .. '"')
    set_objectdir(".LinkConf/xmake/dmui-preview")
    set_dependir(".LinkConf/xmake/dmui-preview/deps")

    add_deps("imgui", "dmui-mcm")
    add_files(
        "Preview/Main.cpp",
        "Preview/FakeData.cpp",
        "Preview/PresentationDemo.cpp",
        "Preview/PlatformImguiStub.cpp",
        "Fixtures/GeneralTestFixtures.cpp",
        "Fixtures/GeneralTestSuite.cpp",
        "src/DearModdingUI/BackgroundBlur.cpp",
        "src/DearModdingUI/CursorLoader.cpp",
        "src/DearModdingUI/Diagnostics.cpp",
        "src/DearModdingUI/ExternalOpen.cpp",
        "src/DearModdingUI/FontCatalog.cpp",
        "src/DearModdingUI/Health.cpp",
        "src/DearModdingUI/Home.cpp",
        "src/DearModdingUI/Host.cpp",
        "src/DearModdingUI/HostSettings.cpp",
        "src/DearModdingUI/HostSettingsHealth.cpp",
        "src/DearModdingUI/HostSettingsView.cpp",
        "src/DearModdingUI/Hotkeys.cpp",
        "src/DearModdingUI/MenuDismissal.cpp",
        "src/DearModdingUI/Navigation.cpp",
        "src/DearModdingUI/NavigationController.cpp",
        "src/DearModdingUI/NavigationPresentation.cpp",
        "src/DearModdingUI/PresentationServices.cpp",
        "src/DearModdingUI/RenderExecution.cpp",
        "src/DearModdingUI/Registry.cpp",
        "src/DearModdingUI/SettingsTable.cpp",
        "src/DearModdingUI/Shell.cpp",
        "src/DearModdingUI/Status.cpp",
        "src/DearModdingUI/Theme.cpp",
        "src/Support/Runtime.cpp",
        "Depends/cimgui/cimgui.cpp"
    )
    add_includedirs(
        "Preview/include",
        "Preview",
        "Fixtures",
        "src",
        "include",
        "Depends",
        "Depends/toml11/single_include",
        "Depends/commonlibf4/lib/dearmoddingui-api/include"
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
    add_syslinks(
        "d3d11",
        "dxgi",
        "d3dcompiler",
        "windowscodecs",
        "ole32",
        "shell32",
        "user32",
        "gdi32",
        "imm32",
        "dwmapi"
    )

    after_build(function(target)
        local data_dir = path.join(target:targetdir(), "Data/F4SE/Plugins")
        os.mkdir(data_dir)
        os.cp(
            path.join(project_dir("data/F4SE/Plugins"), "*"),
            data_dir
        )
    end)
end)

target(plugin_name, function()
    add_options("test-release")
    set_kind("shared")
    set_optimize("fastest")
    set_symbols("debug")
    set_exceptions("cxx")
    set_targetdir(plugin_output_dir())
    add_defines('DMUI_VERSION="' .. plugin_version .. '"')
    set_objectdir(plugin_object_dir("DearModdingUI"))
    set_dependir(path.join(plugin_object_dir("DearModdingUI"), "deps"))

    add_rules("commonlibf4.plugin", {
        name = plugin_name,
        author = "Dear Modding FO4",
        description = "Shared Dear ImGui menu host for Fallout 4"
    })
    add_deps("commonlibf4")

    on_config(function(target)
        disable_commonlib_auto_install(target)
    end)

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
    add_syslinks("d3d11", "dxgi", "d3dcompiler", "shell32")
    set_pcxxheader("Depends/commonlibf4/include/F4SE/Impl/PCH.h")

end)

target("DearModdingUI-MCM", function()
    add_options("test-release")
    set_optimize("fastest")
    set_symbols("debug")
    set_exceptions("cxx")
    set_targetdir(plugin_output_dir())
    set_objectdir(plugin_object_dir("DearModdingUI-MCM"))
    set_dependir(path.join(plugin_object_dir("DearModdingUI-MCM"), "deps"))

    add_rules("commonlibf4.plugin", {
        name = "DearModdingUI-MCM",
        author = "Dear Modding FO4",
        description = "Mod Configuration Menu compatibility client for DearModdingUI"
    })

    on_config(function(target)
        disable_commonlib_auto_install(target)
    end)

    add_deps("dmui-mcm")
    add_files("plugin/src/**.cpp")
    if not has_config("test-release") then
        remove_files("plugin/src/ScaleformSpike.cpp")
    end
    add_headerfiles("plugin/include/**.h")
    add_extrafiles("mcm/README.md")
    add_includedirs("plugin/include")
    if has_config("test-release") then
        add_defines("DMUI_MCM_SCALEFORM_SPIKE")
    end
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
    set_pcxxheader("Depends/commonlibf4/include/F4SE/Impl/PCH.h")
end)

if has_config("test-release") then
target("dmui-test-client", function()
    add_options("test-release")
    set_kind("shared")
    set_version("0.1.0")
    set_optimize("fastest")
    set_symbols("debug")
    set_exceptions("cxx")
    set_targetdir(plugin_output_dir())
    set_objectdir(plugin_object_dir("dmui-test-client"))
    set_dependir(path.join(plugin_object_dir("dmui-test-client"), "deps"))

    add_rules("commonlibf4.plugin", {
        name = "dmui-test-client",
        author = "Dear Modding FO4",
        description = "General forwarding-only DearModdingUI test client"
    })
    add_deps("commonlibf4")

    on_config(function(target)
        disable_commonlib_auto_install(target)
        target:set(
            "configvar",
            "COMMONLIB_PROJECT_NAME",
            "Dear Modding UI Tests"
        )
        target:set("configvar", "COMMONLIB_PROJECT_VERSION", "0.1.0")
        target:set("configvar", "COMMONLIB_PROJECT_VERSION_MAJOR", 0)
        target:set("configvar", "COMMONLIB_PROJECT_VERSION_MINOR", 1)
        target:set("configvar", "COMMONLIB_PROJECT_VERSION_PATCH", 0)
    end)

    add_files(
        "Tools/dmui-test-client/Main.cpp",
        "Fixtures/GeneralTestFixtures.cpp",
        "Fixtures/GeneralTestSuite.cpp"
    )
    add_extrafiles(
        "Tools/dmui-test-client/README.md",
        "Fixtures/GeneralTestFixtures.h",
        "Fixtures/GeneralTestSuite.h",
        "Fixtures/TestHotkeyDescriptors.h"
    )
    add_includedirs("Fixtures")
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
    add_syslinks("d3d11", "dxgi")
    set_pcxxheader("Depends/commonlibf4/include/F4SE/Impl/PCH.h")

end)
end

task("verify-no-auto-install", function()
    set_menu {
        usage = "xmake verify-no-auto-install",
        description = "Verify DMUI plugin targets have no automatic install mappings",
        options = {
            { "P", "project-root", "kv", nil, "Absolute project root" }
        }
    }

    on_run(function()
        import("core.base.option")
        import("core.project.config")
        config.load()
        import("core.project.project")
        import("private.utils.target", { alias = "target_utils" })
        local requested = option.get("project")
        if not requested or canonical_path(requested) ~= canonical_path(os.projectdir()) or
            canonical_path(os.workingdir()) ~= canonical_path(os.projectdir()) then
            raise("run from the DearModdingUI project root and name that absolute path with -P")
        end
        target_utils.config_targets()
        local names = { plugin_name, "DearModdingUI-MCM" }
        local variant = config.read("test-release") and "test" or "release"
        if variant == "test" then
            table.insert(names, "dmui-test-client")
        end
        local expected_output = canonical_path(project_dir(path.join(
            ".Build",
            variant,
            "F4SE",
            "Plugins"
        )))
        local expected_install_root = project_dir(path.join(
            ".Build",
            "no-auto-install",
            variant
        ))
        for _, name in ipairs(names) do
            local target = project.target(name)
            if not target then
                raise("configured target not found: " .. name)
            end
            local source_files, destination_files = target:installfiles()
            if (source_files and #source_files > 0) or
                (destination_files and #destination_files > 0) then
                raise(name .. " still has automatic install files")
            end
            local install_dir = target:installdir()
            if not install_dir or
                not path_is_within(install_dir, expected_install_root) then
                raise(name .. " install directory is not inert and checkout-local")
            end
            if not path_is_within(target:targetfile(), expected_output) then
                raise(name .. " output is outside the configured release directory")
            end
        end
        cprint("${color.success}DMUI plugin targets have no automatic install mappings.")
    end)
end)

task("package-release", function()
    set_menu {
        usage = "xmake package-release",
        description = "Build and assemble the configured release or test package",
        options = {
            { "P", "project-root", "kv", nil, "Absolute project root" }
        }
    }

    on_run(function()
        import("core.base.option")
        import("core.project.config")
        config.load()
        import("utils.archive")

        local function remove_owned_path(value, root)
            if canonical_path(value) == canonical_path(root) or
                not path_is_within(value, root) then
                raise("refusing to remove path outside owned staging descendants: " .. value)
            end
            if os.exists(value) then
                os.rm(value)
                if os.exists(value) then
                    raise("could not remove owned staging path: " .. value)
                end
            end
        end

        local function tracked_package_assets()
            local output = os.iorunv(
                "git",
                { "-C", os.projectdir(), "-c", "core.quotePath=false",
                  "ls-files", "--", "data/F4SE/Plugins" }
            )
            local prefix = "data/F4SE/Plugins/"
            local assets = {}
            for source_relative in output:gmatch("[^\r\n]+") do
                if not source_relative:startswith(prefix) or
                    #source_relative <= #prefix then
                    raise("invalid tracked package asset path: " .. source_relative)
                end
                table.insert(assets, {
                    source = project_dir(source_relative),
                    relative = source_relative:sub(#prefix + 1)
                })
            end
            table.sort(assets, function(left, right)
                return left.relative < right.relative
            end)
            if #assets == 0 then
                raise("tracked package asset manifest is empty")
            end
            return assets
        end

        local requested = option.get("project")
        if not requested or canonical_path(requested) ~= canonical_path(os.projectdir()) or
            canonical_path(os.workingdir()) ~= canonical_path(os.projectdir()) then
            raise("run from the DearModdingUI project root and name that absolute path with -P")
        end
        local variant = config.read("test-release") and "test" or "release"
        local targets = { plugin_name, "DearModdingUI-MCM" }
        if variant == "test" then
            table.insert(targets, "dmui-test-client")
        end
        local package_owner_root = project_dir(".Build/packages")
        local package_root = project_dir(path.join(
            ".Build",
            "packages",
            variant,
            "DearModdingUI"
        ))
        local archive_dir = project_dir(".Build/packages")
        local archive_file = path.join(
            archive_dir,
            plugin_name .. "-" .. plugin_version .. "-" .. variant .. ".zip"
        )
        local archive_working = path.join(
            archive_dir,
            plugin_name .. "-" .. plugin_version .. "-" .. variant .. ".partial.zip"
        )
        remove_owned_path(package_root, package_owner_root)
        remove_owned_path(archive_file, package_owner_root)
        remove_owned_path(archive_working, package_owner_root)

        os.execv(
            os.programfile(),
            table.join({ "build", "-P", requested, "-y" }, targets)
        )

        local plugin_root = path.join(package_root, "F4SE", "Plugins")
        local output_root = project_dir(path.join(
            ".Build",
            variant,
            "F4SE",
            "Plugins"
        ))
        local binaries = {
            "DearModdingUI.dll",
            "DearModdingUI-MCM.dll"
        }
        if variant == "test" then
            table.insert(binaries, "dmui-test-client.dll")
        end
        local assets = tracked_package_assets()
        local documents = {
            "LICENSE",
            "README.md",
            "THIRD_PARTY_NOTICES.md"
        }
        for _, binary in ipairs(binaries) do
            local source = path.join(output_root, binary)
            if not os.isfile(source) then
                raise("missing package binary: " .. source)
            end
        end
        for _, asset in ipairs(assets) do
            if not os.isfile(asset.source) then
                raise("missing tracked package asset: " .. asset.source)
            end
        end
        for _, document in ipairs(documents) do
            local source = project_dir(document)
            if not os.isfile(source) then
                raise("missing package document: " .. source)
            end
        end

        os.mkdir(plugin_root)
        for _, binary in ipairs(binaries) do
            os.cp(
                path.join(output_root, binary),
                path.join(plugin_root, binary)
            )
        end
        for _, asset in ipairs(assets) do
            local destination = path.join(plugin_root, asset.relative)
            os.mkdir(path.directory(destination))
            os.cp(asset.source, destination)
        end
        for _, document in ipairs(documents) do
            local source = project_dir(document)
            os.cp(source, path.join(package_root, document))
        end

        local olddir = os.cd(package_root)
        local files = os.files("**")
        os.cd(olddir)
        archive.archive(archive_working, files, { curdir = package_root })
        if not os.isfile(archive_working) then
            raise("package archive was not produced: " .. archive_working)
        end
        os.mv(archive_working, archive_file)
        cprint("${color.success}Assembled %s package:", variant)
        cprint("  folder: %s", package_root)
        cprint("  archive: %s", archive_file)
    end)
end)
