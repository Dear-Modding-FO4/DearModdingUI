#include <DearModdingUI/presentation/Theme.h>
#include <DearModdingUI/presentation/FontCatalog.h>
#include <DearModdingUI/presentation/ThemePreset.h>
#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/presentation/TypographyHealth.h>
#include <DearModdingUI/localization/Localization.h>
#include <Platform/fonts/SystemFonts.h>
#include <Support/Revision.h>
#include <Support/Runtime.h>
#include <Support/SubsystemHealth.h>

#include <REX/REX.h>

#include <imgui/backends/imgui_impl_dx11.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace DearModdingUI::Theme
{
	using namespace std::literals;

	namespace
	{
		inline constexpr std::string_view kFontRoot{
			"Data\\F4SE\\Plugins\\DearModdingUI\\Fonts"
		};
		inline constexpr std::string_view kThemeRoot{
			"Data\\F4SE\\Plugins\\DearModdingUI\\Themes"
		};
		inline constexpr std::string_view kIconFontFile{
			"Phosphor\\Phosphor-Fill.ttf"
		};
		// Phosphor's own metric; a theme icon set carries its own ratio.
		inline constexpr float kPhosphorBaselineOffsetRatio{ 4.0f / 21.0f };
		inline constexpr ImWchar kIconGlyphRanges[]{
			static_cast<ImWchar>(dmui::PhosphorGlyph::kFirstPrivateUse),
			static_cast<ImWchar>(dmui::PhosphorGlyph::kLastPrivateUse),
			0
		};
		inline constexpr size_t kStyleCount{ 4 };
		// Faces load once; roles size dynamically through PushFont.
		inline constexpr float kReferenceFontSize{ kBaselineFontSize };

		class LoggingHealthReporter final : public HealthReporter
		{
		public:
			explicit LoggingHealthReporter(std::string_view a_subject) noexcept :
				m_subject(a_subject)
			{}

			void Report(
				HealthEvent a_event,
				const HealthSnapshot& a_snapshot) noexcept override
			{
				if (a_snapshot.state == HealthState::kFailed)
				{
					REX::ERROR("[{}] {} Failed: {}"sv,
						a_snapshot.identity,
						m_subject,
						a_snapshot.reason);
				}
				else if (a_snapshot.state == HealthState::kDegraded)
				{
					REX::WARN("[{}] {} Degraded: {}"sv,
						a_snapshot.identity,
						m_subject,
						a_snapshot.reason);
				}
				else if (a_snapshot.state == HealthState::kReady)
				{
					REX::INFO("[{}] {} {}: {}"sv,
						a_snapshot.identity,
						m_subject,
						a_event == HealthEvent::kRecovery ?
							"recovered" :
							"Ready",
						a_snapshot.reason);
				}
			}

		private:
			std::string_view m_subject;
		};

		LoggingHealthReporter s_typographyHealthReporter{ "Typography" };
		SubsystemHealth s_typographyHealth{
			"dmui.typography",
			s_typographyHealthReporter,
			HostSubsystemHealthRegistry()
		};
		LoggingHealthReporter s_themeHealthReporter{ "Theme" };
		SubsystemHealth s_themeHealth{
			"dmui.theme",
			s_themeHealthReporter,
			HostSubsystemHealthRegistry()
		};

		// Authors edit presets in place, so changes show within about a second.
		inline constexpr auto kPresetPollInterval = 1s;
		std::vector<ThemePresetInfo> g_presetList;

		[[nodiscard]] ThemePresetCache& PresetCache()
		{
			static ThemePresetCache cache{
				std::filesystem::path{ Addictol::Support::GetRuntimeDirectory() } /
					kThemeRoot,
				kPresetPollInterval
			};
			return cache;
		}

		struct RoleFont
		{
			std::array<ImFont*, kStyleCount> fonts{};
			float size{ 0.0f };

			[[nodiscard]] ImFont* Regular() const noexcept
			{
				return fonts[static_cast<size_t>(FontStyle::kRegular)];
			}
		};

		std::array<RoleFont, kFontRoleCount> g_roles;
		uint32_t g_roleBackBufferHeight{ 0 };
		float g_roleUserScale{ 0.0f };
		// The typography the atlas faces and role sizes were last built from.
		Typography g_loadedTypography{};
		Typography g_sizedTypography{};
		IconSet g_loadedIcons{};
		// The theme icon font; null when absent, rebuilt with the atlas.
		ImFont* g_iconSetFont{ nullptr };
		// Atlas glyph ranges are read at bake time, so they outlive the load.
		std::vector<ImWchar> g_iconSetRanges;
		std::vector<FontCatalog::FontFamily> g_fontFamilies;
		std::vector<std::string> g_fontFamilyNames;
		std::string g_effectiveBodyFontFamily;
		std::string g_effectiveMonospaceFontFamily;

		struct AppliedStyle
		{
			uint64_t settingsRevision{};
			float bodySize{};
			// Mid-frame pops restore stale values, so the next frame edge reapplies.
			bool atFrameEdge{};
		};
		std::optional<AppliedStyle> g_appliedStyle;

		[[nodiscard]] RoleFont& Role(FontRole a_role) noexcept
		{
			return g_roles[static_cast<size_t>(a_role)];
		}

		[[nodiscard]] std::filesystem::path AssetPath(std::string_view a_relative)
		{
			auto path = std::filesystem::path{ Addictol::Support::GetRuntimeDirectory() };
			path /= kFontRoot;
			path /= a_relative;
			return path;
		}

		[[nodiscard]] ImFontConfig ReferenceConfig() noexcept
		{
			ImFontConfig config{};
			config.SizePixels = kReferenceFontSize;
			return config;
		}

		[[nodiscard]] ImFont* AddFont(
			ImFontAtlas& a_atlas,
			std::string_view a_relative) noexcept
		{
			if (a_relative.empty())
			{
				const auto config = ReferenceConfig();
				return a_atlas.AddFontDefaultVector(&config);
			}
			const auto path = AssetPath(a_relative);
			std::error_code error;
			if (!std::filesystem::exists(path, error))
				return nullptr;

			auto config = ReferenceConfig();
			config.OversampleH = 3;
			config.OversampleV = 2;
			config.PixelSnapH = true;
			config.RasterizerMultiply = 1.1f;
			const auto file = path.string();
			return a_atlas.AddFontFromFileTTF(file.c_str(), 0.0f, &config);
		}

		[[nodiscard]] bool MergeIconFont(
			ImFontAtlas& a_atlas,
			ImFont* a_destination) noexcept
		{
			const auto path = AssetPath(kIconFontFile);
			std::error_code error;
			if (!std::filesystem::exists(path, error))
				return false;

			const auto size = a_destination->LegacySize;
			ImFontConfig config{};
			config.MergeMode = true;
			config.PixelSnapH = true;
			config.GlyphOffset.y = size * kPhosphorBaselineOffsetRatio;
			config.GlyphMinAdvanceX = size;
			config.GlyphMaxAdvanceX = size;
			config.DstFont = a_destination;
			const auto file = path.string();
			return a_atlas.AddFontFromFileTTF(
				file.c_str(),
				size,
				&config,
				kIconGlyphRanges) != nullptr;
		}

		[[nodiscard]] ImFont* AddIconSetFont(
			ImFontAtlas& a_atlas,
			const IconSet& a_icons)
		{
			std::vector<ImWchar> glyphs;
			for (const auto& mapping : a_icons.glyphs)
			{
				if (mapping.glyph != 0 &&
					dmui::IsRepresentableIconGlyph<ImWchar>(mapping.glyph))
					glyphs.push_back(static_cast<ImWchar>(mapping.glyph));
			}
			std::ranges::sort(glyphs);
			const auto duplicates = std::ranges::unique(glyphs);
			glyphs.erase(duplicates.begin(), duplicates.end());
			if (glyphs.empty())
				return nullptr;
			g_iconSetRanges.clear();
			for (const auto glyph : glyphs)
			{
				g_iconSetRanges.push_back(glyph);
				g_iconSetRanges.push_back(glyph);
			}
			g_iconSetRanges.push_back(0);

			const auto path = AssetPath(a_icons.font);
			std::error_code error;
			if (!std::filesystem::exists(path, error))
				return nullptr;
			auto config = ReferenceConfig();
			config.PixelSnapH = true;
			config.GlyphOffset.y =
				kReferenceFontSize * a_icons.baselineOffsetRatio;
			config.GlyphMinAdvanceX = kReferenceFontSize;
			config.GlyphMaxAdvanceX = kReferenceFontSize;
			const auto file = path.string();
			return a_atlas.AddFontFromFileTTF(
				file.c_str(),
				0.0f,
				&config,
				g_iconSetRanges.data());
		}

		[[nodiscard]] bool MergeSystemFont(
			ImFontAtlas& a_atlas,
			ImFont* a_destination,
			const SystemFontFace& a_face) noexcept
		{
			ImFontConfig config{};
			config.MergeMode = true;
			config.DstFont = a_destination;
			// The face is a shared process-lifetime mapping, never freed by the atlas.
			config.FontDataOwnedByAtlas = false;
			return a_atlas.AddFontFromMemoryTTF(
				const_cast<std::byte*>(a_face.data.data()),
				static_cast<int>(a_face.data.size()),
				a_destination->LegacySize,
				&config) != nullptr;
		}

		// Icons merge first so the Phosphor private-use range wins over system faces.
		void MergeFallbacks(
			ImFontAtlas& a_atlas,
			TypographyLoadOutcome& a_outcome)
		{
			std::vector<ImFont*> fonts;
			for (const auto& role : g_roles)
			{
				for (auto* font : role.fonts)
				{
					if (font && !std::ranges::contains(fonts, font))
						fonts.push_back(font);
				}
			}
			a_outcome.iconsLoaded = !fonts.empty() &&
				std::ranges::all_of(fonts, [&](ImFont* a_font) {
					return MergeIconFont(a_atlas, a_font);
				});
			for (const auto& face : SystemFallbackFonts(Localization::Language()))
			{
				const auto merged = std::ranges::all_of(fonts, [&](ImFont* a_font) {
					return MergeSystemFont(a_atlas, a_font, face);
				});
				if (merged)
					++a_outcome.systemFallbacksLoaded;
			}
		}

		void RefreshFontFamilies()
		{
			g_fontFamilies = FontCatalog::Enumerate(AssetPath({}));
			g_fontFamilyNames.clear();
			g_fontFamilyNames.reserve(g_fontFamilies.size());
			for (const auto& family : g_fontFamilies)
				g_fontFamilyNames.push_back(family.name);
		}

		[[nodiscard]] const FontCatalog::FontFamily* ResolveFamily(
			std::string_view a_requested) noexcept
		{
			return FontCatalog::Resolve(
				a_requested,
				g_fontFamilies,
				kDefaultBodyFontFamily);
		}

		void ResolveRoleSizes(
			const Typography& a_typography,
			uint32_t a_backBufferHeight,
			float a_userScale) noexcept
		{
			for (size_t index = 0; index < kFontRoleCount; ++index)
			{
				g_roles[index].size = ResolveRoleFontSize(
					a_typography.roles[index].sizeScale,
					a_typography.baseFontRatio,
					a_backBufferHeight,
					a_userScale);
			}
			g_roleBackBufferHeight = a_backBufferHeight;
			g_roleUserScale = a_userScale;
			g_sizedTypography = a_typography;
		}

		// Atlas fonts depend on faces only; sizes resolve at push time.
		[[nodiscard]] bool FacesDiffer(
			const Typography& a_left,
			const Typography& a_right) noexcept
		{
			return !std::ranges::equal(
				a_left.roles,
				a_right.roles,
				[](const RoleTypography& a_first, const RoleTypography& a_second) {
					return a_first.family == a_second.family &&
						a_first.weight == a_second.weight &&
						a_first.italic == a_second.italic;
				});
		}

		[[nodiscard]] bool SizesDiffer(
			const Typography& a_left,
			const Typography& a_right) noexcept
		{
			return a_left.baseFontRatio != a_right.baseFontRatio ||
				!std::ranges::equal(
					a_left.roles,
					a_right.roles,
					[](const RoleTypography& a_first, const RoleTypography& a_second) {
						return a_first.sizeScale == a_second.sizeScale;
					});
		}

		[[nodiscard]] FontCatalog::FontWeight StyleWeight(
			const RoleTypography& a_role,
			FontStyle a_style) noexcept
		{
			const auto bold = a_style == FontStyle::kBold ||
				a_style == FontStyle::kBoldItalic;
			return bold ?
				std::max(a_role.weight, FontCatalog::FontWeight::kBold) :
				a_role.weight;
		}

		[[nodiscard]] bool StyleItalic(
			const RoleTypography& a_role,
			FontStyle a_style) noexcept
		{
			return a_role.italic ||
				a_style == FontStyle::kItalic ||
				a_style == FontStyle::kBoldItalic;
		}

		class FontLoader
		{
		public:
			explicit FontLoader(ImFontAtlas& a_atlas) noexcept :
				m_atlas(a_atlas)
			{}

			// A null family selects the built-in vector font.
			[[nodiscard]] std::array<ImFont*, kStyleCount> Load(
				const FontCatalog::FontFamily* a_family,
				const RoleTypography& a_role)
			{
				std::array<ImFont*, kStyleCount> fonts{};
				for (size_t index = 0; index < kStyleCount; ++index)
				{
					const auto style = static_cast<FontStyle>(index);
					std::string_view file;
					if (a_family)
					{
						const auto face = FontCatalog::ResolveFace(
							*a_family,
							StyleWeight(a_role, style),
							StyleItalic(a_role, style));
						if (!face.face)
							continue;
						file = face.face->file;
					}
					fonts[index] = LoadFile(file);
				}
				// A variant that cannot load keeps the role's regular look.
				for (auto*& font : fonts)
				{
					if (!font)
						font = fonts[static_cast<size_t>(FontStyle::kRegular)];
				}
				return fonts;
			}

		private:
			using Loaded = std::pair<std::string, ImFont*>;

			ImFont* LoadFile(std::string_view a_file)
			{
				const auto cached = std::ranges::find(
					m_loaded,
					a_file,
					&Loaded::first);
				if (cached != m_loaded.end())
					return cached->second;
				auto* font = AddFont(m_atlas, a_file);
				m_loaded.emplace_back(std::string{ a_file }, font);
				return font;
			}

			ImFontAtlas& m_atlas;
			std::vector<Loaded> m_loaded;
		};

		[[nodiscard]] TypographyLoadOutcome LoadFonts(
			ImGuiIO& a_io,
			const Typography& a_typography,
			const IconSet& a_icons)
		{
			constexpr auto bodyIndex = static_cast<size_t>(FontRole::kBody);
			const auto& bodyTypography = a_typography.roles[bodyIndex];
			const auto builtinBody = FontCatalog::IsBuiltin(bodyTypography.family);
			TypographyLoadOutcome outcome;
			outcome.requestedFamily = bodyTypography.family;
			outcome.requestedFamilyFound = builtinBody ||
				FontCatalog::Find(bodyTypography.family, g_fontFamilies) != nullptr;
			for (auto& role : g_roles)
				role.fonts = {};
			auto& atlas = *a_io.Fonts;
			FontLoader loader{ atlas };

			auto& body = g_roles[bodyIndex];
			const auto* bodyFamily = builtinBody ?
				nullptr :
				ResolveFamily(bodyTypography.family);
			body.fonts = loader.Load(bodyFamily, bodyTypography);
			outcome.requestedBodyLoaded =
				outcome.requestedFamilyFound && body.Regular() != nullptr;
			g_effectiveBodyFontFamily = builtinBody ?
				std::string{ FontCatalog::kBuiltinFamily } :
				bodyFamily ?
				bodyFamily->name :
				std::string{ kDefaultBodyFontFamily };
			if (!body.Regular() && !builtinBody)
			{
				const auto* defaultFamily = FontCatalog::Find(
					kDefaultBodyFontFamily,
					g_fontFamilies);
				if (defaultFamily && defaultFamily != bodyFamily)
				{
					body.fonts = loader.Load(defaultFamily, bodyTypography);
					g_effectiveBodyFontFamily = kDefaultBodyFontFamily;
				}
			}
			outcome.rolesLoaded[bodyIndex] = body.Regular() != nullptr;
			for (size_t index = 0; index < kFontRoleCount; ++index)
			{
				if (index == bodyIndex)
					continue;
				const auto& typography = a_typography.roles[index];
				const auto builtin = FontCatalog::IsBuiltin(typography.family);
				const auto* family = builtin ?
					nullptr :
					FontCatalog::Find(typography.family, g_fontFamilies);
				if (builtin || family)
					g_roles[index].fonts = loader.Load(family, typography);
				else if (index == static_cast<size_t>(FontRole::kMonospace))
				{
					// Code never reads in a proportional face.
					g_roles[index].fonts = loader.Load(nullptr, typography);
				}
				if (index == static_cast<size_t>(FontRole::kMonospace))
				{
					g_effectiveMonospaceFontFamily = family && !builtin ?
						family->name :
						std::string{ FontCatalog::kBuiltinFamily };
				}
				outcome.rolesLoaded[index] = g_roles[index].Regular() != nullptr;
			}
			if (!body.Regular())
			{
				const auto config = ReferenceConfig();
				auto* font = atlas.AddFontDefault(&config);
				body.fonts.fill(font);
				g_effectiveBodyFontFamily = "Built-in fallback";
				outcome.emergencyFontUsed = font != nullptr;
			}
			for (auto& role : g_roles)
			{
				for (size_t style = 0; style < kStyleCount; ++style)
				{
					if (!role.fonts[style])
						role.fonts[style] = body.fonts[style];
				}
			}
			a_io.FontDefault = body.Regular();
			MergeFallbacks(atlas, outcome);
			if (!a_icons.font.empty())
			{
				outcome.iconSetFont = a_icons.font;
				outcome.iconSetMapped = !a_icons.glyphs.empty();
				g_iconSetFont = outcome.iconSetMapped ?
					AddIconSetFont(atlas, a_icons) :
					nullptr;
				outcome.iconSetLoaded = g_iconSetFont != nullptr;
			}
			outcome.effectiveFamily = g_effectiveBodyFontFamily;
			outcome.usableAtlas = body.Regular() != nullptr;
			return outcome;
		}

		[[nodiscard]] bool LoadEmergencyFont(ImGuiIO& a_io) noexcept
		{
			a_io.Fonts->Clear();
			const auto config = ReferenceConfig();
			auto* font = a_io.Fonts->AddFontDefault(&config);
			for (auto& role : g_roles)
				role.fonts.fill(font);
			g_effectiveBodyFontFamily = "Built-in fallback";
			a_io.FontDefault = font;
			return font != nullptr;
		}

		[[nodiscard]] TypographyLoadOutcome RebuildFonts(
			ImGuiIO& a_io,
			const Typography& a_typography,
			const IconSet& a_icons) noexcept
		{
			const auto& requested =
				a_typography.roles[static_cast<size_t>(FontRole::kBody)].family;
			TypographyLoadOutcome loaded;
			a_io.Fonts->Clear();
			g_iconSetFont = nullptr;
			try
			{
				loaded = LoadFonts(a_io, a_typography, a_icons);
			}
			catch (...)
			{
				loaded = {};
			}
			if (!loaded.usableAtlas)
			{
				loaded.requestedFamily = requested;
				loaded.requestedFamilyFound = FontCatalog::IsBuiltin(requested) ||
					FontCatalog::Find(requested, g_fontFamilies) != nullptr;
				loaded.emergencyFontUsed = LoadEmergencyFont(a_io);
				loaded.usableAtlas = loaded.emergencyFontUsed;
				loaded.effectiveFamily = g_effectiveBodyFontFamily;
			}
			return loaded;
		}

		void PublishTypographyOutcome(
			const TypographyLoadOutcome& a_outcome) noexcept
		{
			try
			{
				const auto observation =
					ClassifyTypographyHealth(a_outcome);
				(void)s_typographyHealth.Observe(
					observation.state,
					observation.reason);
			}
			catch (const std::exception& error)
			{
				REX::WARN(
					"DearModdingUI: typography health could not be published: {}"sv,
					error.what());
			}
			catch (...)
			{
				REX::WARN(
					"DearModdingUI: typography health could not be published"sv);
			}
		}
	}

	const ThemeSpec& Current() noexcept
	{
		using Key = std::pair<uint64_t, uint64_t>;
		thread_local Addictol::Support::Derived<Key, ThemeSpec> cache;
		auto& presets = PresetCache();
		return cache.Get(
			Key{ HostSettings::Revision(), presets.Revision() },
			[&]() noexcept {
				const auto preview = HostSettings::EffectivePreview();
				const auto preset = presets.Resolve(preview.themePreset);
				const auto health =
					ClassifyThemePresetHealth(preview.themePreset, *preset);
				(void)s_themeHealth.Observe(health.state, health.reason);
				auto appearance = preview.appearance;
				// The family is committed, not previewed: changing it rebuilds the atlas.
				const auto& committed = HostSettings::Current().appearance;
				appearance.bodyFontFamily = committed.bodyFontFamily;
				appearance.monospaceFontFamily = committed.monospaceFontFamily;
				return ApplyThemeOverrides(
					preset->spec,
					MakeThemeOverrides(appearance));
			});
	}

	void RefreshThemePresets() noexcept
	{
		g_presetList = PresetCache().List();
	}

	const std::vector<ThemePresetInfo>& AvailableThemePresets() noexcept
	{
		return g_presetList;
	}

	std::shared_ptr<const ThemePreset> ResolveThemePreset(
		std::string_view a_id) noexcept
	{
		return PresetCache().Resolve(a_id);
	}

	IconGlyphRef ResolveIcon(char32_t a_phosphor) noexcept
	{
		return ResolveIconIn(
			Current().icons,
			g_iconSetFont,
			ImGui::GetFont(),
			a_phosphor);
	}

	void ApplyStyle() noexcept
	{
		const auto settingsRevision = HostSettings::Revision();
		const auto bodySize = FontSize(FontRole::kBody);
		const auto* context = ImGui::GetCurrentContext();
		const auto withinFrame = context && context->WithinFrameScope;
		if (g_appliedStyle &&
			g_appliedStyle->settingsRevision == settingsRevision &&
			g_appliedStyle->bodySize == bodySize &&
			(withinFrame || g_appliedStyle->atFrameEdge))
			return;
		g_appliedStyle = { settingsRevision, bodySize, !withinFrame };

		const auto& theme = Current();
		auto style = theme.style;
		const auto scaleFactor = ResolveStyleScale(bodySize);
		style.ScaleAllSizes(scaleFactor);
		const auto& panelPadding = theme.chrome.panelPadding;
		Layout() = { ImTrunc(bodySize * theme.chrome.sectionGapScale),
			{ ImTrunc(panelPadding.x * scaleFactor),
				ImTrunc(panelPadding.y * scaleFactor) } };

		const auto scaleBorder = [scaleFactor](float a_value) {
			if (a_value <= 0.0f)
				return 0.0f;
			return ImMax(1.0f, ImTrunc(a_value * scaleFactor));
		};
		style.WindowBorderSize = scaleBorder(theme.style.WindowBorderSize);
		style.ChildBorderSize = scaleBorder(theme.style.ChildBorderSize);
		style.PopupBorderSize = scaleBorder(theme.style.PopupBorderSize);
		style.FrameBorderSize = scaleBorder(theme.style.FrameBorderSize);
		style.TabBorderSize = scaleBorder(theme.style.TabBorderSize);
		style.TabBarBorderSize = scaleBorder(theme.style.TabBarBorderSize);
		style.SeparatorTextBorderSize =
			scaleBorder(theme.style.SeparatorTextBorderSize);
		style.DockingSeparatorSize = scaleBorder(theme.style.DockingSeparatorSize);
		style.MouseCursorScale = ImMax(1.0f, theme.style.MouseCursorScale);
		style.HoverDelayNormal = theme.style.HoverDelayNormal;
		style.FontScaleMain = std::exp2(kDefaultGlobalScale);
		// Mid-frame the live value tracks the pushed font; rebase at frame edges.
		style.FontSizeBase = withinFrame ?
			ImGui::GetStyle().FontSizeBase :
			bodySize;

		FieldFeedback::SetAppearance({
			HostSettings::EffectivePreview().feedbackPlacement
		});
		ImGui::GetStyle() = style;
	}

	void Initialize([[maybe_unused]] void* a_window) noexcept
	{
		auto& io = ImGui::GetIO();
		io.ConfigDockingWithShift = true;
		io.ConfigInputTrickleEventQueue = false;
		RefreshFontFamilies();
		RefreshThemePresets();
		const auto& typography = Current().typography;
		const auto& icons = Current().icons;
		ResolveRoleSizes(
			typography,
			static_cast<uint32_t>(kDefaultScreenHeight),
			HostSettings::Current().uiScale);
		const auto loaded = RebuildFonts(io, typography, icons);
		g_loadedTypography = typography;
		g_loadedIcons = icons;
		g_appliedStyle.reset();
		ApplyStyle();
		PublishTypographyOutcome(loaded);
	}

	bool PrepareFrame(uint32_t a_backBufferHeight) noexcept
	{
		// Preset files reload at the frame edge, never mid-draw.
		PresetCache().Poll();
		const auto& settings = HostSettings::Current();
		const auto& typography = Current().typography;
		const auto& icons = Current().icons;
		const auto facesChanged = FacesDiffer(typography, g_loadedTypography) ||
			icons != g_loadedIcons;
		const auto sizeChanged = a_backBufferHeight != g_roleBackBufferHeight ||
			settings.uiScale != g_roleUserScale ||
			SizesDiffer(typography, g_sizedTypography);
		if (!facesChanged && !sizeChanged)
		{
			ApplyStyle();
			return true;
		}

		auto* context = ImGui::GetCurrentContext();
		if (!context || context->WithinFrameScope)
			return false;

		if (facesChanged)
		{
			(void)s_typographyHealth.Observe(
				HealthState::kProgressing,
				"Rebuilding the font atlas for the theme typography.");
			const auto loaded = RebuildFonts(ImGui::GetIO(), typography, icons);
			PublishTypographyOutcome(loaded);
			if (!loaded.usableAtlas)
				return false;
			g_loadedTypography = typography;
			g_loadedIcons = icons;
		}
		if (sizeChanged)
		{
			ResolveRoleSizes(typography, a_backBufferHeight, settings.uiScale);
			REX::INFO("DearModdingUI: typography resolved to {:.0f}px at {}p"sv,
				FontSize(FontRole::kBody), a_backBufferHeight);
		}
		ApplyStyle();
		return true;
	}

	bool FontsReady() noexcept
	{
		return Role(FontRole::kBody).Regular() != nullptr;
	}

	float FontSize(FontRole a_role) noexcept
	{
		const auto size = Role(a_role).size;
		return size > 0.0f ? size : kBaselineFontSize;
	}

	bool PushFont(FontRole a_role, FontStyle a_style, float a_scale) noexcept
	{
		auto* font = Role(a_role).fonts[static_cast<size_t>(a_style)];
		if (!font)
			return false;
		ImGui::PushFont(font, FontSize(a_role) * a_scale);
		return true;
	}

	void PopFont() noexcept
	{
		ImGui::PopFont();
	}

	float Scale() noexcept
	{
		return FontSize(FontRole::kBody) / kBaselineFontSize;
	}

	float SearchScale() noexcept
	{
		return FontSize(FontRole::kBody) / kSearchBaselineFontSize;
	}

	std::vector<std::string> AvailableFontFamilies(bool a_includeBuiltin)
	{
		auto families = g_fontFamilyNames;
		if (a_includeBuiltin)
			families.insert(families.begin(), std::string{ FontCatalog::kBuiltinFamily });
		return families;
	}

	std::string_view ResolveFontFamily(
		std::string_view a_requested,
		std::string_view a_fallback) noexcept
	{
		if (FontCatalog::IsBuiltin(a_requested))
			return FontCatalog::kBuiltinFamily;
		if (const auto* family = FontCatalog::Find(a_requested, g_fontFamilies))
			return family->name;
		return a_fallback;
	}

	std::string_view EffectiveFontFamily(FontRole a_role) noexcept
	{
		return a_role == FontRole::kMonospace ?
			g_effectiveMonospaceFontFamily : g_effectiveBodyFontFamily;
	}

	FontGuard::FontGuard(
		FontRole a_role,
		FontStyle a_style,
		float a_scale) noexcept
	{
		m_pushed = PushFont(a_role, a_style, a_scale);
	}

	FontGuard::~FontGuard() noexcept
	{
		if (m_pushed)
			PopFont();
	}
}
