#include "../Harness.h"

#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/presentation/ThemePreset.h>

#include <imgui/imgui.h>

#include <toml.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;

	namespace
	{
		struct ScratchDirectory
		{
			explicit ScratchDirectory(std::string_view a_name) :
				path(std::filesystem::current_path() / ".Build" / "Tests" / a_name)
			{
				std::error_code error;
				std::filesystem::remove_all(path, error);
				std::filesystem::create_directories(path, error);
			}

			~ScratchDirectory()
			{
				std::error_code error;
				std::filesystem::remove_all(path, error);
			}

			std::filesystem::path path;
		};

		void Write(const std::filesystem::path& a_file, std::string_view a_text)
		{
			std::ofstream(a_file, std::ios::trunc | std::ios::binary) << a_text;
		}

		[[nodiscard]] std::string Joined(const std::vector<std::string>& a_items)
		{
			std::string text;
			for (const auto& item : a_items)
				text.append(item).append("; ");
			return text;
		}

		// Padding bytes are indeterminate, so the bool run is compared by field.
		[[nodiscard]] bool StyleEqual(const ImGuiStyle& a_left, const ImGuiStyle& a_right)
		{
			constexpr auto head = offsetof(ImGuiStyle, DockingNodeHasCloseButton);
			constexpr auto tail = offsetof(ImGuiStyle, CurveTessellationTol);
			return std::memcmp(&a_left, &a_right, head) == 0 &&
				a_left.DockingNodeHasCloseButton == a_right.DockingNodeHasCloseButton &&
				a_left.DockingSeparatorSize == a_right.DockingSeparatorSize &&
				a_left.MouseCursorScale == a_right.MouseCursorScale &&
				a_left.AntiAliasedLines == a_right.AntiAliasedLines &&
				a_left.AntiAliasedLinesUseTex == a_right.AntiAliasedLinesUseTex &&
				a_left.AntiAliasedFill == a_right.AntiAliasedFill &&
				std::memcmp(
					reinterpret_cast<const std::byte*>(&a_left) + tail,
					reinterpret_cast<const std::byte*>(&a_right) + tail,
					sizeof(ImGuiStyle) - tail) == 0;
		}

		[[nodiscard]] std::string FirstDifference(
			const Theme::ThemeSpec& a_left,
			const Theme::ThemeSpec& a_right)
		{
			if (!StyleEqual(a_left.style, a_right.style))
				return "style";
			if (std::memcmp(&a_left.tokens, &a_right.tokens, sizeof(Theme::Tokens)) != 0)
				return "tokens";
			if (std::memcmp(&a_left.chrome, &a_right.chrome, sizeof(Theme::Chrome)) != 0)
				return "chrome";
			const auto& left = a_left.effects;
			const auto& right = a_right.effects;
			if (left.backgroundBlur != right.backgroundBlur ||
				left.backgroundBlurStrength != right.backgroundBlurStrength ||
				left.iconColorMode != right.iconColorMode ||
				left.logoColorMode != right.logoColorMode)
				return "effects";
			if (!(a_left.typography == a_right.typography))
				return "typography";
			if (!(a_left.icons == a_right.icons))
				return "icons";
			return {};
		}

		[[nodiscard]] bool HasKey(const toml::value& a_root, std::string_view a_path)
		{
			const toml::value* node = &a_root;
			for (const auto part : std::views::split(a_path, '.'))
			{
				const std::string key{ part.begin(), part.end() };
				if (!node->is_table() || !node->contains(key))
					return false;
				node = &node->at(key);
			}
			return true;
		}

		[[nodiscard]] bool SameRgb(const ImVec4& a_left, const ImVec4& a_right)
		{
			return a_left.x == a_right.x && a_left.y == a_right.y && a_left.z == a_right.z;
		}

		[[nodiscard]] bool Reports(
			const Theme::ThemePreset& a_preset,
			std::string_view a_key)
		{
			return std::ranges::any_of(a_preset.problems, [&](const std::string& a_problem) {
				return a_problem.starts_with(std::string{ a_key } + ":");
			});
		}
	}

	void run_theme_preset_checks(Runner& runner)
	{
		runner.test("Default.toml reproduces the compiled default theme and lists every key", [] {
			const auto file = std::filesystem::current_path() / "data" / "F4SE" /
				"Plugins" / "DearModdingUI" / "Themes" / "Default.toml";
			const auto preset = Theme::LoadThemePreset(file);
			require(preset.failure.empty() && preset.problems.empty(),
				"Default.toml was rejected: " + preset.failure + Joined(preset.problems));
			const auto difference =
				FirstDifference(preset.spec, Theme::MakeDefaultTheme());
			require(difference.empty(),
				"Default.toml drifted from the compiled default in " + difference);

			const auto root = toml::parse(file.string());
			for (const auto& key : Theme::ThemePresetKeys())
				require(HasKey(root, key), "Default.toml does not document " + key);
		});

		runner.test("a partial preset applies its valid keys and reports each problem", [] {
			const ScratchDirectory scratch{ "theme-partial" };
			const auto file = scratch.path / "Partial.toml";
			Write(file, R"([theme]
name = "Partial"
[colors]
WindowBg = "#102030"
Button = "nothex"
Bogus = "#FFFFFF"
[tokens]
accent = "#FF0000"
[style]
WindowRounding = 0
FramePadding = [3, 5]
ScrollbarSize = -4
ItemSpacing = [1]
[chrome]
searchIconAlpha = 1.5
[effects]
iconColorMode = "sepia"
logoColorMode = "accent"
[typography.monospace]
family = "Fira"
weight = 700
size = 0
[icons.glyphs]
gear = 0xE001
no-such-icon = 0xE002
[mystery]
x = 1
)");
			const auto preset = Theme::LoadThemePreset(file);
			const auto base = Theme::MakeDefaultTheme();
			require(preset.failure.empty() && preset.name == "Partial",
				"a preset with problems was not loaded");
			const auto& colors = preset.spec.style.Colors;
			require(colors[ImGuiCol_WindowBg].x == 16.0f * (1.0f / 255.0f) &&
					colors[ImGuiCol_WindowBg].z == 48.0f * (1.0f / 255.0f) &&
					colors[ImGuiCol_WindowBg].w == 1.0f,
				"a hex color did not apply");
			require(preset.spec.style.WindowRounding == 0.0f &&
					preset.spec.style.FramePadding.x == 3.0f &&
					preset.spec.style.FramePadding.y == 5.0f,
				"valid style keys did not apply");
			require(preset.spec.style.ScrollbarSize == base.style.ScrollbarSize &&
					preset.spec.style.ItemSpacing.x == base.style.ItemSpacing.x &&
					preset.spec.chrome.searchIconAlpha == base.chrome.searchIconAlpha,
				"an invalid value replaced the compiled default");
			// An invalid Button keeps the base, retinted by the preset accent.
			require(colors[ImGuiCol_Button].x == 1.0f && colors[ImGuiCol_Button].y == 0.0f &&
					colors[ImGuiCol_Button].w == base.style.Colors[ImGuiCol_Button].w,
				"the preset accent did not retint the compiled accent slots");
			require(SameRgb(preset.spec.tokens.iconTint, preset.spec.tokens.accent) &&
					SameRgb(preset.spec.tokens.accentMuted, preset.spec.tokens.accent) &&
					preset.spec.tokens.accentMuted.w == base.chrome.mutedAccentOpacity,
				"derived tokens did not follow the preset accent");
			require(preset.spec.effects.logoColorMode == LogoColorMode::kAccent &&
					preset.spec.effects.iconColorMode == Theme::IconColorMode::kColored,
				"effects did not apply independently");
			const auto& monospace =
				preset.spec.typography.roles[static_cast<size_t>(Theme::FontRole::kMonospace)];
			require(monospace.family == "Fira" &&
					monospace.weight == FontCatalog::FontWeight::kBold &&
					monospace.sizeScale == 0.9f,
				"role typography did not apply key by key");
			require(preset.spec.icons.Find(dmui::PhosphorGlyph::kGear) == 0xE001,
				"an icon glyph did not map");

			for (const auto key : { "colors.Button", "colors.Bogus", "style.ScrollbarSize",
									"style.ItemSpacing", "chrome.searchIconAlpha",
									"effects.iconColorMode", "typography.monospace.size",
									"icons.glyphs.no-such-icon", "mystery" })
			{
				require(Reports(preset, key),
					std::string{ key } + " was not reported: " + Joined(preset.problems));
			}
			require(preset.problems.size() == 9,
				"unexpected problems: " + Joined(preset.problems));
			const auto health = Theme::ClassifyThemePresetHealth("Partial", preset);
			require(health.state == HealthState::kDegraded,
				"a preset with problems was not reported as degraded");
		});

		runner.test("an unparseable or missing preset resolves to the default with a failure", [] {
			const ScratchDirectory scratch{ "theme-broken" };
			Write(scratch.path / "Broken.toml", "[colors\n");
			const auto broken = Theme::LoadThemePreset(scratch.path / "Broken.toml");
			const auto missing = Theme::LoadThemePreset(scratch.path / "Missing.toml");
			for (const auto* preset : { &broken, &missing })
			{
				require(!preset->failure.empty() &&
						FirstDifference(preset->spec, Theme::MakeDefaultTheme()).empty(),
					"a bad preset did not fall back to the compiled default");
				require(Theme::ClassifyThemePresetHealth("X", *preset).state ==
						HealthState::kDegraded,
					"a fallback was reported as healthy");
			}
		});

		runner.test("user overrides retint accent slots by hue and keep each alpha", [] {
			auto theme = Theme::MakeDefaultTheme();
			theme.style.Colors[ImGuiCol_Button].w = 0.5f;
			const auto before = theme;
			Theme::ThemeOverrides overrides;
			overrides.accent = ImVec4{ 0.0f, 0.25f, 0.75f, 1.0f };
			const auto themed = Theme::ApplyThemeOverrides(theme, overrides);
			auto retinted = 0;
			for (int index = 0; index < ImGuiCol_COUNT; ++index)
			{
				const auto& was = before.style.Colors[index];
				const auto& now = themed.style.Colors[index];
				require(was.w == now.w, "an override changed a slot's alpha");
				if (std::memcmp(&was, &now, sizeof(ImVec4)) == 0)
					continue;
				++retinted;
				require(SameRgb(now, *overrides.accent), "a changed slot is not the accent");
			}
			require(retinted > 10, "the accent override left the accent slots alone");
			require(themed.style.Colors[ImGuiCol_Button].w == 0.5f,
				"the preset's own alpha was not kept");
			require(SameRgb(themed.tokens.accent, *overrides.accent) &&
					SameRgb(themed.tokens.iconTint, *overrides.accent) &&
					themed.tokens.accentMuted.w == before.chrome.mutedAccentOpacity,
				"tokens did not follow the accent override");

			Theme::ThemeOverrides mono;
			mono.iconColorMode = Theme::IconColorMode::kMonochrome;
			mono.windowOpacity = 0.4f;
			const auto plain = Theme::ApplyThemeOverrides(theme, mono);
			require(SameRgb(plain.tokens.iconTint, plain.tokens.text) &&
					plain.style.Colors[ImGuiCol_WindowBg].w == 0.4f &&
					SameRgb(plain.tokens.accent, theme.tokens.accent),
				"monochrome icons or window opacity ignored the override");
		});

		runner.test("the preset cache reloads a changed file and resolves late arrivals", [] {
			const ScratchDirectory scratch{ "theme-cache" };
			Theme::ThemePresetCache cache{ scratch.path, std::chrono::milliseconds{ 0 } };
			const auto file = scratch.path / "Live.toml";

			const auto absent = cache.Resolve("Live");
			require(!absent->failure.empty(), "an absent preset did not fail");
			const auto revision = cache.Revision();
			cache.Poll();
			require(cache.Revision() == revision && cache.Resolve("Live") == absent,
				"an unchanged preset reloaded");

			Write(file, "[theme]\nname = \"First\"\n");
			cache.Poll();
			const auto first = cache.Resolve("Live");
			require(cache.Revision() != revision && first->name == "First",
				"a preset created after the first lookup was not picked up");

			Write(file, "[theme]\nname = \"Second\"\n");
			std::filesystem::last_write_time(
				file,
				std::filesystem::last_write_time(file) + std::chrono::seconds{ 5 });
			const auto edited = cache.Revision();
			cache.Poll();
			require(cache.Revision() != edited && cache.Resolve("Live")->name == "Second",
				"an edited preset was not reloaded");
			require(first->name == "First", "a reload mutated a preset already handed out");

			const auto listed = cache.List();
			require(listed.size() == 1 && listed[0].id == "Live" && listed[0].name == "Second",
				"the preset list did not use the file's display name");
			require(!cache.Resolve("..\\escape")->failure.empty(),
				"a path-like preset id was resolved");
		});
	}
}
