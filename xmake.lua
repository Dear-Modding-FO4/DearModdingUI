includes("Depends/commonlibf4")

local plugin_name = "DearModdingUI"
local plugin_version = "0.2.0"

local function project_dir(relative)
    return path.join(os.projectdir(), relative)
end

local local_fixtures = os.isdir(project_dir("fixtures"))

-- Project-script OS access is read-only; build callbacks supply filesystem writes.
local function copy_mcm_fixture_data(destination, build_os)
    local source = project_dir("fixtures/mcm/data")
    for _, required in ipairs({
        "DMUITests.esp",
        "Scripts/DMUITestQuest.pex",
        "Scripts/DMUITestFunctions.pex",
        "MCM/Config/DMUITests/config.json",
        "MCM/Config/DMUITests/settings.ini",
        "MCM/Config/DMUITests/keybinds.json"
    }) do
        if not build_os.isfile(path.join(source, required)) then
            raise("missing local MCM test fixture: %s", path.join(source, required))
        end
    end
    build_os.mkdir(destination)
    build_os.cp(path.join(source, "*"), destination)
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

local function configure_dmui_target()
    set_optimize("fastest")
    add_defines("NDEBUG", "NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_cxxflags("/permissive-", "/Zc:preprocessor", { public = true })
end

local source_sets = {
    core = {
        "src/DearModdingUI/host/Diagnostics.cpp",
        "src/DearModdingUI/host/Hotkeys.cpp",
        "src/DearModdingUI/host/IconResolution.cpp",
        "src/DearModdingUI/host/HostAPISettingsRows.cpp",
        "src/DearModdingUI/host/MenuDismissal.cpp",
        "src/DearModdingUI/host/ModalCoordinator.cpp",
        "src/DearModdingUI/host/ControllerNavigation.cpp",
        "src/DearModdingUI/host/Registry*.cpp",
        "src/DearModdingUI/host/RenderExecution.cpp",
        "src/DearModdingUI/host/Status.cpp",
        "src/DearModdingUI/host/UIAdapter*.cpp",
        "src/DearModdingUI/controls/Controls.cpp",
        "src/DearModdingUI/controls/FieldFeedback.cpp",
        "src/DearModdingUI/controls/SettingsTable.cpp",
        "src/DearModdingUI/controls/TextInput.cpp",
        "src/DearModdingUI/controls/TextViewer.cpp",
        "src/DearModdingUI/navigation/Navigation*.cpp",
        "src/DearModdingUI/navigation/SidebarView.cpp",
        "src/DearModdingUI/pages/Health.cpp",
        "src/DearModdingUI/pages/Home.cpp",
        "src/DearModdingUI/settings/HostSettingsHealthState.cpp",
        "src/DearModdingUI/settings/HostSettingsColorControls.cpp",
        "src/DearModdingUI/settings/HostSettingsPersistence.cpp",
        "src/DearModdingUI/presentation/BlurPipelineState.cpp",
        "src/DearModdingUI/presentation/FontCatalog.cpp",
        "src/DearModdingUI/presentation/Logo.cpp",
        "src/DearModdingUI/presentation/Presentation*.cpp",
        "src/DearModdingUI/presentation/images/*.cpp",
        "src/Support/Runtime.cpp",
        "src/DearModdingUI/presentation/ThemeColors.cpp",
        "src/DearModdingUI/presentation/ThemeLayout.cpp",
        "src/Platform/files/ExternalOpen.cpp",
        "src/Platform/input/CursorLoader.cpp",
        "src/Platform/settings/GameColors.cpp"
    },
    ui = {
        "src/DearModdingUI/host/Host*.cpp",
        "src/DearModdingUI/host/Shell.cpp",
        "src/DearModdingUI/controls/Faq.cpp",
        "src/DearModdingUI/controls/LinkRow.cpp",
        "src/DearModdingUI/navigation/CommandPalette.cpp",
        "src/DearModdingUI/pages/HostPageViews.cpp",
        "src/DearModdingUI/settings/HostSettings.cpp",
        "src/DearModdingUI/settings/HostSettingsView.cpp",
        "src/DearModdingUI/presentation/BackgroundBlur.cpp",
        "src/DearModdingUI/presentation/Theme.cpp"
    },
    runtime = {
        "src/Main.cpp",
        "src/Platform/input/CarrierMenu.cpp",
        "src/Platform/input/GameInput.cpp",
        "src/Platform/rendering/**.cpp",
        "src/Support/Detours.cpp",
    },
    navigation_preview = {
        "tools/preview/navigation/NavigationPreview.cpp"
    },
    diagnostic_client = {
        "fixtures/tests/GeneralTestFixtures.cpp",
        "fixtures/shared/GeneralTestSuite*.cpp"
    }
}

local function add_source_sets(...)
    for _, name in ipairs({...}) do
        add_files(table.unpack(source_sets[name]))
    end
end

-- Each HLSL entry is compiled once at build time into an embedded bytecode header.
local shader_entries = {
    ["BackgroundBlurDownsample.hlsl"] = {
        { entry = "VS_Main", profile = "vs_5_0", name = "BackgroundBlurDownsampleVS" },
        { entry = "PS_Main", profile = "ps_5_0", name = "BackgroundBlurDownsamplePS" }
    },
    ["BackgroundBlurGaussian.hlsl"] = {
        { entry = "PS_Horizontal", profile = "ps_5_0", name = "BackgroundBlurHorizontalPS" },
        { entry = "PS_Vertical", profile = "ps_5_0", name = "BackgroundBlurVerticalPS" }
    },
    ["BackgroundBlurComposite.hlsl"] = {
        { entry = "PS_Main", profile = "ps_5_0", name = "BackgroundBlurCompositePS" }
    }
}

local function shader_header_root(target)
    return path.join(target:autogendir(), "shaders")
end

rule("dmui.shaders", function()
    set_extensions(".hlsl")

    on_load(function(target)
        target:add("includedirs", shader_header_root(target))
    end)

    before_buildcmd_file(function(target, batchcmds, sourcefile, opt)
        local entries = shader_entries[path.filename(sourcefile)]
        if not entries then
            raise("shader %s has no declared entry points", sourcefile)
        end
        local msvc = target:toolchain("msvc")
        local envs = msvc and msvc:runenvs() or {}
        local sdk_bin = envs.WindowsSdkVerBinPath
        local fxc = sdk_bin and path.join(sdk_bin, "x64", "fxc.exe")
        if not fxc or not os.isfile(fxc) then
            raise("fxc was not found in the Windows SDK")
        end
        local output_root = path.join(shader_header_root(target), "DearModdingUI", "shaders")
        batchcmds:mkdir(output_root)
        local outputs = {}
        for _, shader in ipairs(entries) do
            local header = path.join(output_root, shader.name .. ".h")
            batchcmds:show_progress(opt.progress, "${color.build.object}compiling.shader %s:%s",
                sourcefile, shader.entry)

			if not is_host("linux") then
				batchcmds:vrunv(fxc, {
					"/nologo", "/Ges", "/O3",
					"/E", shader.entry,
					"/T", shader.profile,
					"/Vn", "g_" .. shader.name,
					"/Fh", header,
					path(sourcefile)
				})
			else
				batchcmds:vrunv("wine", {
					fxc,
					"/nologo", "/Ges", "/O3",
					"/E", shader.entry,
					"/T", shader.profile,
					"/Vn", "g_" .. shader.name,
					"/Fh", header,
					path(sourcefile)
				})

				batchcmds:vrunv("sed", {
					"-i",
					"s/\x05#/#/g",
					header
				})
			end

            table.insert(outputs, header)
        end
        batchcmds:add_depfiles(sourcefile)
        local oldest
        for _, header in ipairs(outputs) do
            local mtime = os.mtime(header)
            oldest = oldest and math.min(oldest, mtime) or mtime
        end
        batchcmds:set_depmtime(oldest)
        batchcmds:set_depcache(target:dependfile(outputs[1]))
    end)
end)

local function add_shaders()
    add_rules("dmui.shaders")
    add_files("src/DearModdingUI/presentation/shaders/*.hlsl")
end

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
        "src/Platform/imgui/ImGuiWin32Integration.cpp",
        "Depends/imgui/backends/imgui_impl_dx11.cpp"
    )

    add_includedirs("Depends/imgui", { public = true })
    add_includedirs("include")
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
    configure_dmui_target()
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
    add_includedirs("include")
end)

target("dmui-tests", function()
    set_kind("binary")
    configure_dmui_target()
    set_targetdir(project_dir(".Build/Tests"))
    add_defines('DMUI_VERSION="' .. plugin_version .. '"')
    set_objectdir(".LinkConf/xmake/dmui-tests")
    set_dependir(".LinkConf/xmake/dmui-tests/deps")

    add_deps("imgui", "dmui-mcm")
    add_source_sets("core", "navigation_preview")
    add_shaders()
    add_files(
        "tests/**.cpp",
        "mcm/runtime/src/Win32FileListingAdapter.cpp",
        "Depends/commonlibf4/lib/dearmoddingui-api/Tests/CompileHostAPILayout.cpp",
        "Depends/commonlibf4/lib/dearmoddingui-api/Tests/CompileIconGlyphs.cpp",
        "Depends/commonlibf4/lib/dearmoddingui-api/Tests/CompileUI.cpp",
        "Depends/commonlibf4/lib/dearmoddingui-api/Tests/CompileNoWindowsMacros.cpp",
        "Depends/commonlibf4/lib/dearmoddingui-api/Tests/CompileSettingsActions.cpp",
        "Depends/commonlibf4/lib/dearmoddingui-api/Tests/CompileTextView.cpp",
        "Depends/commonlibf4/lib/dearmoddingui-api/Tests/CompileVisualDecisions.cpp"
    )
    if local_fixtures then
        add_files("fixtures/tests/*.cpp")
        add_includedirs("fixtures/tests")
        add_defines("DMUI_LOCAL_FIXTURES")
        add_ldflags("/EXPORT:DMUI_GetAPI", { force = true })
    end
    add_includedirs(
        "tools/preview/include",
        "tools/build-support/include",
        "mcm/runtime/include",
        "tests",
        "src",
        "include",
        "Depends",
        "Depends/toml11/single_include",
        "Depends/commonlibf4/include",
        "Depends/commonlibf4/lib/commonlib-shared/include",
        "Depends/commonlibf4/lib/dearmoddingui-api/include"
    )
    add_defines("DMUI_UI_TESTING", "DMUI_PREVIEW")
    add_syslinks("bcrypt", "d3d11", "dxgi", "d3dcompiler", "shell32", "windowscodecs", "ole32")
end)

target("dmui-preview", function()
    set_kind("binary")
    configure_dmui_target()
    set_symbols("debug")
    set_exceptions("cxx")
    set_targetdir(project_dir(".Build/Preview"))
    add_defines('DMUI_VERSION="' .. plugin_version .. '"')
    set_objectdir(".LinkConf/xmake/dmui-preview")
    set_dependir(".LinkConf/xmake/dmui-preview/deps")

    add_deps("imgui", "dmui-mcm")
    add_source_sets("core", "ui", "navigation_preview")
    add_shaders()
    add_files(
        "tools/preview/*.cpp",
        "mcm/runtime/src/Win32FileListingAdapter.cpp"
    )
    if local_fixtures then
        add_source_sets("diagnostic_client")
        add_files("fixtures/preview/*.cpp")
        add_includedirs("fixtures/preview", "fixtures/shared", "fixtures/tests")
        add_defines("DMUI_PREVIEW_FIXTURES")
        add_defines('DMUI_IMAGE_FIXTURES="' .. project_dir("fixtures/images"):gsub("\\", "/") .. '"')
        remove_files("tools/preview/FixtureRunner.cpp")
    end
    add_includedirs(
        "tools/preview/include",
        "tools/preview",
        "tools/preview/navigation",
        "tools/build-support/include",
        "mcm/adapters/include",
        "mcm/runtime/include",
        "src",
        "include",
        "Depends",
        "Depends/toml11/single_include",
        "Depends/commonlibf4/lib/dearmoddingui-api/include"
    )
    add_defines("_CRT_SECURE_NO_WARNINGS", "DMUI_PREVIEW")
    add_syslinks(
        "d3d11",
        "dxgi",
        "windowscodecs",
        "ole32",
        "shell32",
        "user32",
        "gdi32",
        "imm32",
        "dwmapi"
    )

    after_build(function(target)
        local data_root = path.join(target:targetdir(), "Data")
        local plugins = path.join(data_root, "F4SE/Plugins")
        os.mkdir(plugins)
        os.cp(
            path.join(project_dir("data/F4SE/Plugins"), "*"),
            plugins
        )
        if local_fixtures then
            copy_mcm_fixture_data(data_root, os)
        end
    end)
end)

target(plugin_name, function()
    add_options("test-release")
    set_kind("shared")
    configure_dmui_target()
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
        if release_variant() == "test" and not local_fixtures then
            raise("--test-release=y requires local fixtures in the gitignored fixtures/ folder")
        end
        disable_commonlib_auto_install(target)
    end)

    add_deps("imgui")
    add_source_sets("core", "ui", "runtime")
    add_shaders()
    add_headerfiles("include/**.h", "src/**.h")
    add_extrafiles("data/**", "README.md", "THIRD_PARTY_NOTICES.md")
    add_includedirs(
        "include",
        "Depends",
        "Depends/toml11/single_include"
    )
    add_defines("_CRT_SECURE_NO_WARNINGS")
    add_syslinks("d3d11", "dxgi", "shell32", "windowscodecs", "ole32")
    set_pcxxheader("Depends/commonlibf4/include/F4SE/Impl/PCH.h")

end)

target("DearModdingUI-MCM", function()
    add_options("test-release")
    configure_dmui_target()
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
    add_files("mcm/runtime/src/**.cpp")
    add_headerfiles("mcm/runtime/include/**.h", "mcm/adapters/include/**.h")
    add_extrafiles("mcm/README.md")
    add_includedirs("mcm/runtime/include", "mcm/adapters/include", "include")
    set_pcxxheader("Depends/commonlibf4/include/F4SE/Impl/PCH.h")
end)

if local_fixtures and has_config("test-release") then
target("dmui-test-client", function()
    add_options("test-release")
    set_kind("shared")
    set_version("0.1.0")
    configure_dmui_target()
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

    add_source_sets("diagnostic_client")
    add_files("fixtures/test-client/*.cpp")
    add_headerfiles("fixtures/test-client/*.h")
    add_extrafiles(
        "fixtures/test-client/README.md",
        "fixtures/mcm/data/**",
        "fixtures/tests/GeneralTestFixtures.h",
        "fixtures/shared/GeneralTestSuite.h",
        "fixtures/shared/TestHotkeyDescriptors.h"
    )
    add_includedirs("fixtures/tests", "fixtures/shared")
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
        description = "Build release component archives or one complete test bundle",
        options = {
            { "P", "project-root", "kv", nil, "Absolute project root" }
        }
    }

    on_run(function()
        import("core.base.option")
        import("core.project.config")
        config.load()
        import("utils.archive")

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
        os.execv(
            os.programfile(),
            table.join({ "build", "-P", requested, "-y" }, targets)
        )

        local output_root = project_dir(path.join(".Build", variant, "F4SE", "Plugins"))
        local package_owner = project_dir(".Build/packages")
        local data_root = project_dir("data")
        local runtime_assets = os.files(project_dir("data/F4SE/Plugins/**"))
        table.sort(runtime_assets)
        local components = { plugin_name, "DearModdingUI-MCM" }
        if variant == "test" then
            components = { plugin_name }
            for _, obsolete in ipairs({
                path.join(package_owner, "test", "DearModdingUI-MCM"),
                path.join(package_owner, "DearModdingUI-MCM-" .. plugin_version .. "-test.zip"),
                path.join(package_owner, "DearModdingUI-MCM-" .. plugin_version .. "-test.partial.zip")
            }) do
                if os.exists(obsolete) then
                    os.rm(obsolete)
                end
            end
        end
        for _, component in ipairs(components) do
            local folder = path.join(package_owner, variant, component)
            local zip = path.join(
                package_owner, component .. "-" .. plugin_version .. "-" .. variant .. ".zip")
            local partial = zip:gsub("%.zip$", ".partial.zip")
            if not path_is_within(folder, package_owner) or
                canonical_path(folder) == canonical_path(package_owner) then
                raise("package staging must remain inside the owned output directory")
            end
            if os.exists(folder) then
                os.rm(folder)
            end
            if os.exists(partial) then
                os.rm(partial)
            end
            local plugins = path.join(folder, "F4SE", "Plugins")
            os.mkdir(plugins)
            local packaged_targets = variant == "test" and targets or { component }
            for _, name in ipairs(packaged_targets) do
                os.cp(path.join(output_root, name .. ".dll"), plugins)
            end
            if component == plugin_name then
                if variant == "test" then
                    copy_mcm_fixture_data(folder, os)
                    local image_data = path.join(plugins, "dmui-test-client/images")
                    os.mkdir(image_data)
                    for _, name in ipairs({ "Tiles.png", "Tiles.dds" }) do
                        os.cp(project_dir(path.join("fixtures/images", name)), image_data)
                    end
                end
                for _, source in ipairs(runtime_assets) do
                    local extension = path.extension(source):lower()
                    if extension == ".toml" or extension == ".ttf" then
                        local relative = path.relative(source, data_root)
                        local destination = path.join(folder, relative)
                        os.mkdir(path.directory(destination))
                        os.cp(source, destination)
                    end
                end
            end
            local previous = os.cd(folder)
            local files = os.files("**")
            os.cd(previous)
            archive.archive(partial, files, { curdir = folder })
            if not os.isfile(partial) then
                raise("package archive was not produced: " .. partial)
            end
            os.mv(partial, zip)
            cprint("${color.success}%s", zip)
        end
    end)
end)
