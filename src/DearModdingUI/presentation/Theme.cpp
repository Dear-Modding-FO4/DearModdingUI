#include <DearModdingUI/presentation/Theme.h>
#include <DearModdingUI/presentation/FontCatalog.h>
#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/presentation/TypographyHealth.h>
#include <DearModdingUI/localization/Localization.h>
#include <Platform/fonts/SystemFonts.h>
#include <Support/Runtime.h>
#include <Support/SubsystemHealth.h>

#include <REX/REX.h>

#include <imgui/backends/imgui_impl_dx11.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <filesystem>
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
		inline constexpr std::string_view kIconFontFile{
			"Phosphor\\Phosphor-Fill.ttf"
		};
		inline constexpr ImWchar kIconGlyphRanges[]{
			static_cast<ImWchar>(PhosphorGlyph::kFirstPrivateUse),
			static_cast<ImWchar>(PhosphorGlyph::kLastPrivateUse),
			0
		};
		inline constexpr size_t kRoleCount{ static_cast<size_t>(FontRole::kCount) };
		// Faces load once; roles size dynamically through PushFont.
		inline constexpr float kReferenceFontSize{ kBaselineFontSize };

		class TypographyHealthReporter final : public HealthReporter
		{
		public:
			void Report(
				HealthEvent a_event,
				const HealthSnapshot& a_snapshot) noexcept override
			{
				if (a_snapshot.state == HealthState::kFailed)
				{
					REX::ERROR("[{}] Typography Failed: {}"sv,
						a_snapshot.identity,
						a_snapshot.reason);
				}
				else if (a_snapshot.state == HealthState::kDegraded)
				{
					REX::WARN("[{}] Typography Degraded: {}"sv,
						a_snapshot.identity,
						a_snapshot.reason);
				}
				else if (a_snapshot.state == HealthState::kReady)
				{
					REX::INFO("[{}] Typography {}: {}"sv,
						a_snapshot.identity,
						a_event == HealthEvent::kRecovery ?
							"recovered" :
							"Ready",
						a_snapshot.reason);
				}
			}
		};

		TypographyHealthReporter s_typographyHealthReporter;
		SubsystemHealth s_typographyHealth{
			"dmui.typography",
			s_typographyHealthReporter,
			HostSubsystemHealthRegistry()
		};

		struct RoleFont
		{
			ImFont* font{ nullptr };
			float size{ 0.0f };
		};

		std::array<RoleFont, kRoleCount> g_roles;
		uint32_t g_roleBackBufferHeight{ 0 };
		float g_roleUserScale{ 0.0f };
		std::vector<FontCatalog::FontFamily> g_fontFamilies;
		std::vector<std::string> g_fontFamilyNames;
		std::string g_fontRequestFamily;
		std::string g_effectiveBodyFontFamily;

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
			config.GlyphOffset.y = size * kIconDefaults.baselineOffsetRatio;
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
				if (role.font && !std::ranges::contains(fonts, role.font))
					fonts.push_back(role.font);
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

		void ResolveRoleSizes(uint32_t a_backBufferHeight, float a_userScale) noexcept
		{
			for (size_t index = 0; index < kRoleCount; ++index)
			{
				g_roles[index].size = ResolveRoleFontSize(
					static_cast<FontRole>(index),
					a_backBufferHeight,
					a_userScale);
			}
			g_roleBackBufferHeight = a_backBufferHeight;
			g_roleUserScale = a_userScale;
		}

		[[nodiscard]] TypographyLoadOutcome LoadFonts(
			ImGuiIO& a_io,
			std::string_view a_requestedFamily,
			const FontCatalog::FontFamily* a_family,
			bool a_requestedFamilyFound)
		{
			TypographyLoadOutcome outcome;
			outcome.requestedFamily = a_requestedFamily;
			outcome.requestedFamilyFound = a_requestedFamilyFound;
			for (auto& role : g_roles)
				role.font = nullptr;
			auto& atlas = *a_io.Fonts;
			using LoadedFile = std::pair<std::string_view, ImFont*>;
			std::array<LoadedFile, kRoleCount + 1> loaded{};
			size_t loadedCount = 0;

			auto loadFile = [&](std::string_view a_file) {
				const auto end = loaded.begin() + static_cast<ptrdiff_t>(loadedCount);
				const auto cached = std::ranges::find(
					loaded.begin(), end, a_file, &LoadedFile::first);
				if (cached != end)
					return cached->second;
				auto* font = AddFont(atlas, a_file);
				loaded[loadedCount++] = { a_file, font };
				return font;
			};

			const auto& defaultBody =
				kFontRoleDefaults[static_cast<size_t>(FontRole::kBody)];
			const auto requestedFile = a_family ?
				std::string_view{ a_family->regularFile } :
				defaultBody.file;
			auto& body = Role(FontRole::kBody);
			body.font = loadFile(requestedFile);
			outcome.requestedBodyLoaded =
				a_requestedFamilyFound && body.font != nullptr;
			g_effectiveBodyFontFamily = a_family ?
				a_family->name :
				std::string{ kDefaultBodyFontFamily };
			if (!body.font && requestedFile != defaultBody.file)
			{
				body.font = loadFile(defaultBody.file);
				g_effectiveBodyFontFamily = kDefaultBodyFontFamily;
			}
			for (size_t index = 0; index < kRoleCount; ++index)
			{
				auto& role = g_roles[index];
				if (&role != &body)
					role.font = loadFile(kFontRoleDefaults[index].file);
				outcome.rolesLoaded[index] = role.font != nullptr;
			}
			if (!body.font)
			{
				const auto config = ReferenceConfig();
				body.font = atlas.AddFontDefault(&config);
				g_effectiveBodyFontFamily = "Built-in fallback";
				outcome.emergencyFontUsed = body.font != nullptr;
			}
			for (auto& role : g_roles)
			{
				if (!role.font)
					role.font = body.font;
			}
			a_io.FontDefault = body.font;
			MergeFallbacks(atlas, outcome);
			outcome.effectiveFamily = g_effectiveBodyFontFamily;
			outcome.usableAtlas = body.font != nullptr;
			return outcome;
		}

		[[nodiscard]] bool LoadEmergencyFont(ImGuiIO& a_io) noexcept
		{
			a_io.Fonts->Clear();
			const auto config = ReferenceConfig();
			auto* font = a_io.Fonts->AddFontDefault(&config);
			for (auto& role : g_roles)
				role.font = font;
			g_effectiveBodyFontFamily = "Built-in fallback";
			a_io.FontDefault = font;
			return font != nullptr;
		}

		[[nodiscard]] TypographyLoadOutcome RebuildFonts(
			ImGuiIO& a_io,
			std::string_view a_requestedFamily) noexcept
		{
			const auto* requestedFamily = FontCatalog::Find(
				a_requestedFamily,
				g_fontFamilies);
			TypographyLoadOutcome loaded;
			a_io.Fonts->Clear();
			try
			{
				loaded = LoadFonts(
					a_io,
					a_requestedFamily,
					ResolveFamily(a_requestedFamily),
					requestedFamily != nullptr);
			}
			catch (...)
			{
				loaded = {};
			}
			if (!loaded.usableAtlas)
			{
				loaded.requestedFamily = a_requestedFamily;
				loaded.requestedFamilyFound = requestedFamily != nullptr;
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

	namespace colors
	{
		ImVec4 Accent() noexcept
		{
			return HostAccentToImVec4(
				HostSettings::EffectivePreview().accentColor);
		}

		ImVec4 AccentMuted() noexcept
		{
			auto accent = Accent();
			accent.w = kMutedAccentOpacity;
			return accent;
		}
	}

	void ApplyStyle() noexcept
	{
		const auto baseStyle = MakeBaseStyle();
		auto style = baseStyle;
		const auto bodySize = FontSize(FontRole::kBody);
		const auto scaleFactor = ResolveStyleScale(bodySize);
		style.ScaleAllSizes(scaleFactor);
		const LayoutStyle layoutDefaults;
		Layout() = { ImTrunc(bodySize),
			{ ImTrunc(layoutDefaults.panelPadding.x * scaleFactor),
				ImTrunc(layoutDefaults.panelPadding.y * scaleFactor) } };

		const auto scaleBorder = [scaleFactor](float a_value) {
			if (a_value <= 0.0f)
				return 0.0f;
			return ImMax(1.0f, ImTrunc(a_value * scaleFactor));
		};
		style.WindowBorderSize = scaleBorder(kStyleDefaults.windowBorderSize);
		style.ChildBorderSize = scaleBorder(kStyleDefaults.childBorderSize);
		style.PopupBorderSize = scaleBorder(baseStyle.PopupBorderSize);
		style.FrameBorderSize = scaleBorder(kStyleDefaults.frameBorderSize);
		style.TabBorderSize = scaleBorder(baseStyle.TabBorderSize);
		style.TabBarBorderSize = scaleBorder(baseStyle.TabBarBorderSize);
		style.SeparatorTextBorderSize =
			scaleBorder(baseStyle.SeparatorTextBorderSize);
		style.DockingSeparatorSize = scaleBorder(baseStyle.DockingSeparatorSize);
		style.MouseCursorScale = ImMax(1.0f, baseStyle.MouseCursorScale);
		style.HoverDelayNormal = kTooltipHoverDelay;
		style.FontScaleMain = std::exp2(kDefaultGlobalScale);
		// Mid-frame the live value tracks the pushed font; rebase at frame edges.
		const auto* context = ImGui::GetCurrentContext();
		style.FontSizeBase = context && context->WithinFrameScope ?
			ImGui::GetStyle().FontSizeBase :
			bodySize;

		const auto settings = HostSettings::EffectivePreview();
		FieldFeedback::SetAppearance({
			settings.feedbackPlacement,
			ToFieldFeedbackColor(settings.feedbackInfoColor),
			ToFieldFeedbackColor(settings.feedbackWarningColor),
			ToFieldFeedbackColor(settings.feedbackErrorColor)
		});
		auto paletteBackground =
			HostAccentToImVec4(settings.paletteBackgroundColor);
		paletteBackground.w = settings.paletteBackgroundOpacity;
		const auto palette = MakeHostPalette(
			HostAccentToImVec4(settings.accentColor),
			settings.windowBackgroundOpacity,
			paletteBackground);
		for (size_t index = 0; index < palette.size(); ++index)
			style.Colors[index] = palette[index];
		ImGui::GetStyle() = style;
	}

	void Initialize([[maybe_unused]] void* a_window) noexcept
	{
		auto& io = ImGui::GetIO();
		io.ConfigDockingWithShift = true;
		io.ConfigInputTrickleEventQueue = false;
		RefreshFontFamilies();
		const auto settings = HostSettings::Current();
		ResolveRoleSizes(static_cast<uint32_t>(kDefaultScreenHeight), settings.uiScale);
		const auto loaded = RebuildFonts(io, settings.bodyFontFamily);
		g_fontRequestFamily = settings.bodyFontFamily;
		ApplyStyle();
		PublishTypographyOutcome(loaded);
	}

	bool PrepareFrame(uint32_t a_backBufferHeight) noexcept
	{
		const auto settings = HostSettings::Current();
		const auto familyChanged = settings.bodyFontFamily != g_fontRequestFamily;
		const auto sizeChanged = a_backBufferHeight != g_roleBackBufferHeight ||
			settings.uiScale != g_roleUserScale;
		if (!familyChanged && !sizeChanged)
		{
			ApplyStyle();
			return true;
		}

		auto* context = ImGui::GetCurrentContext();
		if (!context || context->WithinFrameScope)
			return false;

		if (familyChanged)
		{
			(void)s_typographyHealth.Observe(
				HealthState::kProgressing,
				"Rebuilding the font atlas for the requested family.");
			const auto loaded = RebuildFonts(ImGui::GetIO(), settings.bodyFontFamily);
			PublishTypographyOutcome(loaded);
			if (!loaded.usableAtlas)
				return false;
			g_fontRequestFamily = settings.bodyFontFamily;
		}
		if (sizeChanged)
		{
			ResolveRoleSizes(a_backBufferHeight, settings.uiScale);
			REX::INFO("DearModdingUI: typography resolved to {:.0f}px at {}p"sv,
				FontSize(FontRole::kBody), a_backBufferHeight);
		}
		ApplyStyle();
		return true;
	}

	bool FontsReady() noexcept
	{
		return Role(FontRole::kBody).font != nullptr;
	}

	float FontSize(FontRole a_role) noexcept
	{
		const auto size = Role(a_role).size;
		return size > 0.0f ? size : kBaselineFontSize;
	}

	bool PushFont(FontRole a_role, float a_scale) noexcept
	{
		const auto& role = Role(a_role);
		if (!role.font)
			return false;
		ImGui::PushFont(role.font, FontSize(a_role) * a_scale);
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

	ImVec4 IconTint() noexcept
	{
		const auto settings = HostSettings::EffectivePreview();
		return ResolveIconTint(
			settings.iconColorMode,
			HostAccentToImVec4(settings.accentColor),
			kFullPalette[ImGuiCol_Text]);
	}

	DMUI_ThemeColors ColorSnapshot() noexcept
	{
		return MakeColorSnapshot(colors::Accent());
	}

	const std::vector<std::string>& AvailableBodyFontFamilies() noexcept
	{
		return g_fontFamilyNames;
	}

	std::string_view ResolveBodyFontFamily(
		std::string_view a_requested) noexcept
	{
		if (const auto* family = ResolveFamily(a_requested))
			return family->name;
		return kDefaultBodyFontFamily;
	}

	std::string_view EffectiveBodyFontFamily() noexcept
	{
		return g_effectiveBodyFontFamily;
	}

	FontGuard::FontGuard(FontRole a_role, float a_scale) noexcept
	{
		m_pushed = PushFont(a_role, a_scale);
	}

	FontGuard::~FontGuard() noexcept
	{
		if (m_pushed)
			PopFont();
	}
}
