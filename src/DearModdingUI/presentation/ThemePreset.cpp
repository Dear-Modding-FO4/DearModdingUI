#include <DearModdingUI/presentation/ThemePreset.h>
#include <DearModdingUI/settings/HostSettings.h>

#include <imgui/imgui.h>

#include <toml.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <exception>
#include <format>
#include <set>
#include <utility>

namespace DearModdingUI::Theme
{
	namespace
	{
		enum class Range
		{
			kFinite,
			kNonNegative,
			kPositive,
			kUnit,
			kSignedUnit
		};

		template <class Owner>
		struct FloatKey
		{
			std::string_view key;
			float Owner::* member;
			Range range;
		};

		template <class Owner>
		struct Vec2Key
		{
			std::string_view key;
			ImVec2 Owner::* member;
			Range range;
		};

		template <class Owner>
		struct ColorKey
		{
			std::string_view key;
			ImVec4 Owner::* member;
		};

		using StyleFloat = FloatKey<ImGuiStyle>;
		using StyleVec2 = Vec2Key<ImGuiStyle>;
		using TokenColor = ColorKey<Tokens>;
		using ChromeFloat = FloatKey<Chrome>;

		inline constexpr std::array kStyleFloats{
			StyleFloat{ "Alpha", &ImGuiStyle::Alpha, Range::kUnit },
			StyleFloat{ "DisabledAlpha", &ImGuiStyle::DisabledAlpha, Range::kUnit },
			StyleFloat{ "WindowRounding", &ImGuiStyle::WindowRounding, Range::kNonNegative },
			StyleFloat{ "WindowBorderSize", &ImGuiStyle::WindowBorderSize, Range::kNonNegative },
			StyleFloat{ "WindowBorderHoverPadding", &ImGuiStyle::WindowBorderHoverPadding, Range::kNonNegative },
			StyleFloat{ "ChildRounding", &ImGuiStyle::ChildRounding, Range::kNonNegative },
			StyleFloat{ "ChildBorderSize", &ImGuiStyle::ChildBorderSize, Range::kNonNegative },
			StyleFloat{ "PopupRounding", &ImGuiStyle::PopupRounding, Range::kNonNegative },
			StyleFloat{ "PopupBorderSize", &ImGuiStyle::PopupBorderSize, Range::kNonNegative },
			StyleFloat{ "FrameRounding", &ImGuiStyle::FrameRounding, Range::kNonNegative },
			StyleFloat{ "FrameBorderSize", &ImGuiStyle::FrameBorderSize, Range::kNonNegative },
			StyleFloat{ "IndentSpacing", &ImGuiStyle::IndentSpacing, Range::kNonNegative },
			StyleFloat{ "ColumnsMinSpacing", &ImGuiStyle::ColumnsMinSpacing, Range::kNonNegative },
			StyleFloat{ "ScrollbarSize", &ImGuiStyle::ScrollbarSize, Range::kPositive },
			StyleFloat{ "ScrollbarRounding", &ImGuiStyle::ScrollbarRounding, Range::kNonNegative },
			StyleFloat{ "ScrollbarPadding", &ImGuiStyle::ScrollbarPadding, Range::kNonNegative },
			StyleFloat{ "GrabMinSize", &ImGuiStyle::GrabMinSize, Range::kPositive },
			StyleFloat{ "GrabRounding", &ImGuiStyle::GrabRounding, Range::kNonNegative },
			StyleFloat{ "LogSliderDeadzone", &ImGuiStyle::LogSliderDeadzone, Range::kNonNegative },
			StyleFloat{ "ImageRounding", &ImGuiStyle::ImageRounding, Range::kNonNegative },
			StyleFloat{ "ImageBorderSize", &ImGuiStyle::ImageBorderSize, Range::kNonNegative },
			StyleFloat{ "TabRounding", &ImGuiStyle::TabRounding, Range::kNonNegative },
			StyleFloat{ "TabBorderSize", &ImGuiStyle::TabBorderSize, Range::kNonNegative },
			StyleFloat{ "TabMinWidthBase", &ImGuiStyle::TabMinWidthBase, Range::kNonNegative },
			StyleFloat{ "TabMinWidthShrink", &ImGuiStyle::TabMinWidthShrink, Range::kNonNegative },
			StyleFloat{ "TabCloseButtonMinWidthSelected", &ImGuiStyle::TabCloseButtonMinWidthSelected, Range::kFinite },
			StyleFloat{ "TabCloseButtonMinWidthUnselected", &ImGuiStyle::TabCloseButtonMinWidthUnselected, Range::kFinite },
			StyleFloat{ "TabBarBorderSize", &ImGuiStyle::TabBarBorderSize, Range::kNonNegative },
			StyleFloat{ "TabBarOverlineSize", &ImGuiStyle::TabBarOverlineSize, Range::kNonNegative },
			StyleFloat{ "TreeLinesSize", &ImGuiStyle::TreeLinesSize, Range::kNonNegative },
			StyleFloat{ "TreeLinesRounding", &ImGuiStyle::TreeLinesRounding, Range::kNonNegative },
			StyleFloat{ "MenuItemRounding", &ImGuiStyle::MenuItemRounding, Range::kNonNegative },
			StyleFloat{ "SelectableRounding", &ImGuiStyle::SelectableRounding, Range::kNonNegative },
			StyleFloat{ "DragDropTargetRounding", &ImGuiStyle::DragDropTargetRounding, Range::kFinite },
			StyleFloat{ "DragDropTargetBorderSize", &ImGuiStyle::DragDropTargetBorderSize, Range::kNonNegative },
			StyleFloat{ "DragDropTargetPadding", &ImGuiStyle::DragDropTargetPadding, Range::kNonNegative },
			StyleFloat{ "ColorMarkerSize", &ImGuiStyle::ColorMarkerSize, Range::kNonNegative },
			StyleFloat{ "InputTextCursorSize", &ImGuiStyle::InputTextCursorSize, Range::kPositive },
			StyleFloat{ "SeparatorSize", &ImGuiStyle::SeparatorSize, Range::kPositive },
			StyleFloat{ "SeparatorTextBorderSize", &ImGuiStyle::SeparatorTextBorderSize, Range::kNonNegative },
			StyleFloat{ "DockingSeparatorSize", &ImGuiStyle::DockingSeparatorSize, Range::kNonNegative },
			StyleFloat{ "MouseCursorScale", &ImGuiStyle::MouseCursorScale, Range::kPositive },
			StyleFloat{ "HoverStationaryDelay", &ImGuiStyle::HoverStationaryDelay, Range::kNonNegative },
			StyleFloat{ "HoverDelayShort", &ImGuiStyle::HoverDelayShort, Range::kNonNegative },
			StyleFloat{ "HoverDelayNormal", &ImGuiStyle::HoverDelayNormal, Range::kNonNegative }
		};

		inline constexpr std::array kStyleVec2s{
			StyleVec2{ "WindowPadding", &ImGuiStyle::WindowPadding, Range::kNonNegative },
			StyleVec2{ "WindowMinSize", &ImGuiStyle::WindowMinSize, Range::kNonNegative },
			StyleVec2{ "WindowTitleAlign", &ImGuiStyle::WindowTitleAlign, Range::kUnit },
			StyleVec2{ "FramePadding", &ImGuiStyle::FramePadding, Range::kNonNegative },
			StyleVec2{ "ItemSpacing", &ImGuiStyle::ItemSpacing, Range::kNonNegative },
			StyleVec2{ "ItemInnerSpacing", &ImGuiStyle::ItemInnerSpacing, Range::kNonNegative },
			StyleVec2{ "CellPadding", &ImGuiStyle::CellPadding, Range::kNonNegative },
			StyleVec2{ "TouchExtraPadding", &ImGuiStyle::TouchExtraPadding, Range::kNonNegative },
			StyleVec2{ "TableAngledHeadersTextAlign", &ImGuiStyle::TableAngledHeadersTextAlign, Range::kUnit },
			StyleVec2{ "ButtonTextAlign", &ImGuiStyle::ButtonTextAlign, Range::kUnit },
			StyleVec2{ "SelectableTextAlign", &ImGuiStyle::SelectableTextAlign, Range::kUnit },
			StyleVec2{ "SeparatorTextAlign", &ImGuiStyle::SeparatorTextAlign, Range::kUnit },
			StyleVec2{ "SeparatorTextPadding", &ImGuiStyle::SeparatorTextPadding, Range::kNonNegative },
			StyleVec2{ "DisplayWindowPadding", &ImGuiStyle::DisplayWindowPadding, Range::kNonNegative },
			StyleVec2{ "DisplaySafeAreaPadding", &ImGuiStyle::DisplaySafeAreaPadding, Range::kNonNegative }
		};

		inline constexpr std::array kTokenColors{
			TokenColor{ "text", &Tokens::text },
			TokenColor{ "muted", &Tokens::muted },
			TokenColor{ "accent", &Tokens::accent },
			TokenColor{ "accentMuted", &Tokens::accentMuted },
			TokenColor{ "info", &Tokens::info },
			TokenColor{ "success", &Tokens::success },
			TokenColor{ "warning", &Tokens::warning },
			TokenColor{ "error", &Tokens::error },
			TokenColor{ "disabled", &Tokens::disabled },
			TokenColor{ "restartNeeded", &Tokens::restartNeeded },
			TokenColor{ "currentHotkey", &Tokens::currentHotkey },
			TokenColor{ "heading", &Tokens::heading },
			TokenColor{ "headingHovered", &Tokens::headingHovered },
			TokenColor{ "searchFrameActive", &Tokens::searchFrameActive },
			TokenColor{ "rowAlt", &Tokens::rowAlt },
			TokenColor{ "iconTint", &Tokens::iconTint },
			TokenColor{ "logoTop", &Tokens::logoTop },
			TokenColor{ "logoLeft", &Tokens::logoLeft },
			TokenColor{ "logoRight", &Tokens::logoRight },
			TokenColor{ "logoBottom", &Tokens::logoBottom }
		};

		inline constexpr std::array kChromeFloats{
			ChromeFloat{ "headerFallbackTextScale", &Chrome::headerFallbackTextScale, Range::kPositive },
			ChromeFloat{ "separatorThickness", &Chrome::separatorThickness, Range::kNonNegative },
			ChromeFloat{ "featureTitleScale", &Chrome::featureTitleScale, Range::kPositive },
			ChromeFloat{ "versionTextOpacity", &Chrome::versionTextOpacity, Range::kUnit },
			ChromeFloat{ "headingMinimizedFactor", &Chrome::headingMinimizedFactor, Range::kPositive },
			ChromeFloat{ "searchIconSize", &Chrome::searchIconSize, Range::kPositive },
			ChromeFloat{ "searchIconAlpha", &Chrome::searchIconAlpha, Range::kUnit },
			ChromeFloat{ "searchIconOffsetX", &Chrome::searchIconOffsetX, Range::kNonNegative },
			ChromeFloat{ "searchInputPaddingExtra", &Chrome::searchInputPaddingExtra, Range::kNonNegative },
			ChromeFloat{ "searchInputFramePaddingY", &Chrome::searchInputFramePaddingY, Range::kNonNegative },
			ChromeFloat{ "searchIconStrokeRatio", &Chrome::searchIconStrokeRatio, Range::kPositive },
			ChromeFloat{ "mutedAccentOpacity", &Chrome::mutedAccentOpacity, Range::kUnit },
			ChromeFloat{ "sectionGapScale", &Chrome::sectionGapScale, Range::kPositive }
		};

		inline constexpr Vec2Key<Chrome> kChromePanelPadding{
			"panelPadding", &Chrome::panelPadding, Range::kNonNegative
		};

		inline constexpr std::string_view kBlurKey{ "backgroundBlur" };
		inline constexpr std::string_view kBlurStrengthKey{ "backgroundBlurStrength" };
		inline constexpr std::string_view kIconColorModeKey{ "iconColorMode" };
		inline constexpr std::string_view kLogoColorModeKey{ "logoColorMode" };
		inline constexpr std::array<std::string_view, 4> kRoleKeys{
			"family", "weight", "italic", "size"
		};

		struct WeightName
		{
			std::string_view name;
			FontCatalog::FontWeight weight;
		};

		inline constexpr std::array kWeightNames{
			WeightName{ "thin", FontCatalog::FontWeight::kThin },
			WeightName{ "extralight", FontCatalog::FontWeight::kExtraLight },
			WeightName{ "light", FontCatalog::FontWeight::kLight },
			WeightName{ "regular", FontCatalog::FontWeight::kRegular },
			WeightName{ "medium", FontCatalog::FontWeight::kMedium },
			WeightName{ "semibold", FontCatalog::FontWeight::kSemiBold },
			WeightName{ "bold", FontCatalog::FontWeight::kBold },
			WeightName{ "extrabold", FontCatalog::FontWeight::kExtraBold },
			WeightName{ "black", FontCatalog::FontWeight::kBlack }
		};

		[[nodiscard]] std::string Join(
			std::string_view a_path,
			std::string_view a_key)
		{
			return a_path.empty() ?
				std::string{ a_key } :
				std::format("{}.{}", a_path, a_key);
		}

		[[nodiscard]] bool InRange(double a_value, Range a_range) noexcept
		{
			switch (a_range)
			{
			case Range::kNonNegative:
				return a_value >= 0.0;
			case Range::kPositive:
				return a_value > 0.0;
			case Range::kUnit:
				return a_value >= 0.0 && a_value <= 1.0;
			case Range::kSignedUnit:
				return a_value >= -1.0 && a_value <= 1.0;
			default:
				return true;
			}
		}

		[[nodiscard]] std::string_view RangeText(Range a_range) noexcept
		{
			switch (a_range)
			{
			case Range::kNonNegative:
				return "must be 0 or more";
			case Range::kPositive:
				return "must be greater than 0";
			case Range::kUnit:
				return "must be between 0 and 1";
			case Range::kSignedUnit:
				return "must be between -1 and 1";
			default:
				return "must be a finite number";
			}
		}

		[[nodiscard]] std::string Lowercase(std::string_view a_text)
		{
			std::string result{ a_text };
			std::ranges::transform(result, result.begin(), [](unsigned char a_char) {
				return static_cast<char>(std::tolower(a_char));
			});
			return result;
		}

		class Problems
		{
		public:
			explicit Problems(std::vector<std::string>& a_list) noexcept :
				m_list(a_list)
			{}

			void Add(std::string_view a_path, std::string_view a_text)
			{
				m_list.push_back(std::format("{}: {}", a_path, a_text));
			}

		private:
			std::vector<std::string>& m_list;
		};

		// One table of a preset file; keys read through Take are known.
		class Section
		{
		public:
			Section(
				std::string a_path,
				const toml::value& a_table,
				Problems& a_problems) :
				m_path(std::move(a_path)),
				m_table(a_table),
				m_problems(a_problems)
			{}

			[[nodiscard]] const toml::value* Take(std::string_view a_key)
			{
				m_known.emplace(a_key);
				const auto& table = m_table.as_table();
				const auto found = table.find(std::string{ a_key });
				return found == table.end() ? nullptr : &found->second;
			}

			[[nodiscard]] std::string Path(std::string_view a_key) const
			{
				return Join(m_path, a_key);
			}

			[[nodiscard]] std::optional<Section> Child(std::string_view a_key)
			{
				const auto* value = Take(a_key);
				if (!value)
					return std::nullopt;
				if (!value->is_table())
				{
					m_problems.Add(Path(a_key), "expected a table");
					return std::nullopt;
				}
				return Section{ Path(a_key), *value, m_problems };
			}

			// Every key not taken is unknown, reported in sorted order.
			void Finish()
			{
				std::set<std::string> unknown;
				for (const auto& [key, value] : m_table.as_table())
				{
					if (!m_known.contains(key))
						unknown.insert(key);
				}
				for (const auto& key : unknown)
					m_problems.Add(Path(key), "unknown key");
			}

			[[nodiscard]] Problems& Report() noexcept
			{
				return m_problems;
			}

			// Keys of this table in sorted order.
			[[nodiscard]] std::set<std::string> Keys() const
			{
				std::set<std::string> keys;
				for (const auto& [key, value] : m_table.as_table())
					keys.insert(key);
				return keys;
			}

		private:
			std::string m_path;
			const toml::value& m_table;
			Problems& m_problems;
			std::set<std::string, std::less<>> m_known;
		};

		[[nodiscard]] std::optional<double> ReadNumber(
			const toml::value& a_value,
			std::string_view a_path,
			Problems& a_problems)
		{
			double number{};
			if (a_value.is_floating())
				number = a_value.as_floating();
			else if (a_value.is_integer())
				number = static_cast<double>(a_value.as_integer());
			else
			{
				a_problems.Add(a_path, "expected a number");
				return std::nullopt;
			}
			if (!std::isfinite(number))
			{
				a_problems.Add(a_path, "must be a finite number");
				return std::nullopt;
			}
			return number;
		}

		[[nodiscard]] std::optional<float> ReadFloat(
			const toml::value& a_value,
			std::string_view a_path,
			Range a_range,
			Problems& a_problems)
		{
			const auto number = ReadNumber(a_value, a_path, a_problems);
			if (!number)
				return std::nullopt;
			const auto narrowed = static_cast<float>(*number);
			if (!std::isfinite(narrowed) || !InRange(*number, a_range))
			{
				a_problems.Add(a_path, RangeText(a_range));
				return std::nullopt;
			}
			return narrowed;
		}

		[[nodiscard]] std::optional<ImVec2> ReadVec2(
			const toml::value& a_value,
			std::string_view a_path,
			Range a_range,
			Problems& a_problems)
		{
			if (!a_value.is_array() || a_value.as_array().size() != 2)
			{
				a_problems.Add(a_path, "expected [x, y]");
				return std::nullopt;
			}
			const auto& items = a_value.as_array();
			const auto x = ReadFloat(items[0], a_path, a_range, a_problems);
			const auto y = ReadFloat(items[1], a_path, a_range, a_problems);
			if (!x || !y)
				return std::nullopt;
			return ImVec2{ *x, *y };
		}

		[[nodiscard]] std::optional<ImVec4> ReadColor(
			const toml::value& a_value,
			std::string_view a_path,
			Problems& a_problems)
		{
			constexpr std::string_view expected{
				"expected \"#RRGGBB\", \"#RRGGBBAA\", or [r, g, b, a] in 0 to 1"
			};
			if (a_value.is_string())
			{
				const auto& text = a_value.as_string();
				if ((text.size() == 7 || text.size() == 9) && text.front() == '#')
				{
					constexpr auto inverseByte = 1.0f / 255.0f;
					std::array<float, 4> channels{ 0.0f, 0.0f, 0.0f, 1.0f };
					auto valid = true;
					for (size_t index = 0; index < (text.size() - 1) / 2; ++index)
					{
						const auto high = HexDigitValue(text[1 + index * 2]);
						const auto low = HexDigitValue(text[2 + index * 2]);
						valid = valid && high >= 0 && low >= 0;
						if (valid)
						{
							channels[index] =
								static_cast<float>(high * 16 + low) * inverseByte;
						}
					}
					if (valid)
						return ImVec4{ channels[0], channels[1], channels[2], channels[3] };
				}
				a_problems.Add(a_path, expected);
				return std::nullopt;
			}
			if (a_value.is_array() && (a_value.as_array().size() == 3 ||
										  a_value.as_array().size() == 4))
			{
				std::array<float, 4> channels{ 0.0f, 0.0f, 0.0f, 1.0f };
				const auto& items = a_value.as_array();
				for (size_t index = 0; index < items.size(); ++index)
				{
					const auto channel =
						ReadFloat(items[index], a_path, Range::kUnit, a_problems);
					if (!channel)
						return std::nullopt;
					channels[index] = *channel;
				}
				return ImVec4{ channels[0], channels[1], channels[2], channels[3] };
			}
			a_problems.Add(a_path, expected);
			return std::nullopt;
		}

		[[nodiscard]] std::optional<std::string> ReadText(
			const toml::value& a_value,
			std::string_view a_path,
			Problems& a_problems)
		{
			if (!a_value.is_string())
			{
				a_problems.Add(a_path, "expected text");
				return std::nullopt;
			}
			return a_value.as_string();
		}

		[[nodiscard]] std::optional<bool> ReadBool(
			const toml::value& a_value,
			std::string_view a_path,
			Problems& a_problems)
		{
			if (!a_value.is_boolean())
			{
				a_problems.Add(a_path, "expected true or false");
				return std::nullopt;
			}
			return a_value.as_boolean();
		}

		template <class Enum>
		[[nodiscard]] std::optional<Enum> ReadChoice(
			const toml::value& a_value,
			std::string_view a_path,
			Problems& a_problems,
			std::string_view a_firstName,
			Enum a_first,
			std::string_view a_secondName,
			Enum a_second)
		{
			const auto text = ReadText(a_value, a_path, a_problems);
			if (!text)
				return std::nullopt;
			if (*text == a_firstName)
				return a_first;
			if (*text == a_secondName)
				return a_second;
			a_problems.Add(a_path, std::format(
				"expected \"{}\" or \"{}\"", a_firstName, a_secondName));
			return std::nullopt;
		}

		[[nodiscard]] std::optional<FontCatalog::FontWeight> ReadWeight(
			const toml::value& a_value,
			std::string_view a_path,
			Problems& a_problems)
		{
			if (a_value.is_integer())
			{
				const auto number = a_value.as_integer();
				const auto found = std::ranges::find(
					kWeightNames,
					static_cast<FontCatalog::FontWeight>(number),
					&WeightName::weight);
				if (number >= 100 && number <= 900 && found != kWeightNames.end())
					return found->weight;
			}
			else if (a_value.is_string())
			{
				const auto found = std::ranges::find(
					kWeightNames,
					Lowercase(a_value.as_string()),
					&WeightName::name);
				if (found != kWeightNames.end())
					return found->weight;
			}
			a_problems.Add(
				a_path,
				"expected thin, extralight, light, regular, medium, semibold, bold, "
				"extrabold, black, or 100 to 900 in steps of 100");
			return std::nullopt;
		}

		[[nodiscard]] bool IsRelativeFontPath(const std::string& a_file)
		{
			const std::filesystem::path path{ a_file };
			return !path.has_root_name() && !path.has_root_directory() &&
				std::ranges::none_of(path, [](const std::filesystem::path& a_part) {
					return a_part == "..";
				});
		}

		template <class Owner, size_t Count>
		void ApplyFloats(
			Section& a_section,
			Owner& a_owner,
			const std::array<FloatKey<Owner>, Count>& a_keys)
		{
			for (const auto& key : a_keys)
			{
				if (const auto* value = a_section.Take(key.key))
				{
					if (const auto number = ReadFloat(
							*value,
							a_section.Path(key.key),
							key.range,
							a_section.Report()))
						a_owner.*key.member = *number;
				}
			}
		}

		template <class Owner>
		void ApplyVec2(
			Section& a_section,
			Owner& a_owner,
			const Vec2Key<Owner>& a_key)
		{
			if (const auto* value = a_section.Take(a_key.key))
			{
				if (const auto vector = ReadVec2(
						*value,
						a_section.Path(a_key.key),
						a_key.range,
						a_section.Report()))
					a_owner.*a_key.member = *vector;
			}
		}

		template <class Owner, size_t Count>
		void ApplyVec2s(
			Section& a_section,
			Owner& a_owner,
			const std::array<Vec2Key<Owner>, Count>& a_keys)
		{
			for (const auto& key : a_keys)
				ApplyVec2(a_section, a_owner, key);
		}

		void ApplyRole(Section& a_section, RoleTypography& a_role)
		{
			if (const auto* value = a_section.Take("family"))
			{
				const auto path = a_section.Path("family");
				const auto family = ReadText(*value, path, a_section.Report());
				if (family && IsPlainFileName(*family))
					a_role.family = *family;
				else if (family)
					a_section.Report().Add(path, "expected a font family folder name");
			}
			if (const auto* value = a_section.Take("weight"))
			{
				if (const auto weight = ReadWeight(
						*value, a_section.Path("weight"), a_section.Report()))
					a_role.weight = *weight;
			}
			if (const auto* value = a_section.Take("italic"))
			{
				if (const auto italic = ReadBool(
						*value, a_section.Path("italic"), a_section.Report()))
					a_role.italic = *italic;
			}
			if (const auto* value = a_section.Take("size"))
			{
				if (const auto size = ReadFloat(
						*value, a_section.Path("size"), Range::kPositive, a_section.Report()))
					a_role.sizeScale = *size;
			}
			a_section.Finish();
		}

		void ApplyTypography(Section& a_section, Typography& a_typography)
		{
			if (const auto* value = a_section.Take("baseFontRatio"))
			{
				if (const auto ratio = ReadFloat(
						*value,
						a_section.Path("baseFontRatio"),
						Range::kPositive,
						a_section.Report()))
					a_typography.baseFontRatio = *ratio;
			}
			for (size_t index = 0; index < kFontRoleCount; ++index)
			{
				if (auto role = a_section.Child(kFontRoleKeys[index]))
					ApplyRole(*role, a_typography.roles[index]);
			}
			a_section.Finish();
		}

		void ApplyGlyphs(Section& a_section, IconSet& a_icons)
		{
			std::vector<std::pair<std::string, char32_t>> entries;
			for (const auto& name : a_section.Keys())
			{
				const auto* value = a_section.Take(name);
				const auto path = a_section.Path(name);
				if (!value->is_integer())
				{
					a_section.Report().Add(path, "expected an integer codepoint");
					continue;
				}
				const auto codepoint = value->as_integer();
				if (codepoint <= 0 ||
					!dmui::IsRepresentableIconGlyph<ImWchar>(
						static_cast<char32_t>(codepoint)))
				{
					a_section.Report().Add(path, "not a drawable codepoint");
					continue;
				}
				entries.emplace_back(name, static_cast<char32_t>(codepoint));
			}
			std::vector<NamedIconGlyph> named;
			named.reserve(entries.size());
			for (const auto& [name, glyph] : entries)
				named.push_back({ name, glyph });
			auto mapped = MapIconNames(named);
			for (const auto& unknown : mapped.unknownNames)
				a_section.Report().Add(a_section.Path(unknown), "unknown icon name");
			a_icons.glyphs = std::move(mapped.glyphs);
		}

		void ApplyIcons(Section& a_section, IconSet& a_icons)
		{
			if (const auto* value = a_section.Take("font"))
			{
				const auto path = a_section.Path("font");
				const auto font = ReadText(*value, path, a_section.Report());
				if (font && IsRelativeFontPath(*font))
					a_icons.font = *font;
				else if (font)
					a_section.Report().Add(path, "expected a path under the Fonts folder");
			}
			if (const auto* value = a_section.Take("baselineOffsetRatio"))
			{
				if (const auto ratio = ReadFloat(
						*value,
						a_section.Path("baselineOffsetRatio"),
						Range::kSignedUnit,
						a_section.Report()))
					a_icons.baselineOffsetRatio = *ratio;
			}
			if (auto glyphs = a_section.Child("glyphs"))
				ApplyGlyphs(*glyphs, a_icons);
			a_section.Finish();
		}

		void ApplyEffects(Section& a_section, Effects& a_effects)
		{
			if (const auto* value = a_section.Take(kBlurKey))
			{
				if (const auto blur = ReadBool(
						*value, a_section.Path(kBlurKey), a_section.Report()))
					a_effects.backgroundBlur = *blur;
			}
			if (const auto* value = a_section.Take(kBlurStrengthKey))
			{
				if (const auto strength = ReadFloat(
						*value,
						a_section.Path(kBlurStrengthKey),
						Range::kUnit,
						a_section.Report()))
					a_effects.backgroundBlurStrength = *strength;
			}
			if (const auto* value = a_section.Take(kIconColorModeKey))
			{
				if (const auto mode = ReadChoice(
						*value,
						a_section.Path(kIconColorModeKey),
						a_section.Report(),
						"colored",
						IconColorMode::kColored,
						"monochrome",
						IconColorMode::kMonochrome))
					a_effects.iconColorMode = *mode;
			}
			if (const auto* value = a_section.Take(kLogoColorModeKey))
			{
				if (const auto mode = ReadChoice(
						*value,
						a_section.Path(kLogoColorModeKey),
						a_section.Report(),
						"original",
						LogoColorMode::kOriginal,
						"accent",
						LogoColorMode::kAccent))
					a_effects.logoColorMode = *mode;
			}
			a_section.Finish();
		}

		[[nodiscard]] std::optional<ImVec4> PeekAccent(const toml::value& a_root)
		{
			if (!a_root.contains("tokens") || !a_root.at("tokens").is_table() ||
				!a_root.at("tokens").contains("accent"))
				return std::nullopt;
			std::vector<std::string> discarded;
			Problems quiet{ discarded };
			return ReadColor(a_root.at("tokens").at("accent"), {}, quiet);
		}

		void ApplyPreset(const toml::value& a_root, ThemePreset& a_preset)
		{
			Problems problems{ a_preset.problems };
			const auto accent = PeekAccent(a_root);
			a_preset.spec = accent ? MakeDefaultTheme(*accent) : MakeDefaultTheme();
			auto& spec = a_preset.spec;
			Section top{ {}, a_root, problems };

			if (auto theme = top.Child("theme"))
			{
				if (const auto* value = theme->Take("name"))
				{
					const auto path = theme->Path("name");
					const auto name = ReadText(*value, path, problems);
					if (name && !name->empty())
						a_preset.name = *name;
					else if (name)
						problems.Add(path, "must not be empty");
				}
				theme->Finish();
			}
			if (auto colors = top.Child("colors"))
			{
				for (int index = 0; index < ImGuiCol_COUNT; ++index)
				{
					const std::string_view name{ ImGui::GetStyleColorName(index) };
					if (const auto* value = colors->Take(name))
					{
						if (const auto color = ReadColor(
								*value, colors->Path(name), problems))
							spec.style.Colors[index] = *color;
					}
				}
				colors->Finish();
			}
			auto pinnedAccentMuted = false;
			auto pinnedIconTint = false;
			if (auto tokens = top.Child("tokens"))
			{
				for (const auto& key : kTokenColors)
				{
					const auto* value = tokens->Take(key.key);
					if (!value)
						continue;
					if (const auto color = ReadColor(
							*value, tokens->Path(key.key), problems))
					{
						spec.tokens.*key.member = *color;
						pinnedAccentMuted |= key.member == &Tokens::accentMuted;
						pinnedIconTint |= key.member == &Tokens::iconTint;
					}
				}
				tokens->Finish();
			}
			if (auto style = top.Child("style"))
			{
				ApplyFloats(*style, spec.style, kStyleFloats);
				ApplyVec2s(*style, spec.style, kStyleVec2s);
				style->Finish();
			}
			if (auto chrome = top.Child("chrome"))
			{
				ApplyFloats(*chrome, spec.chrome, kChromeFloats);
				ApplyVec2(*chrome, spec.chrome, kChromePanelPadding);
				chrome->Finish();
			}
			if (auto effects = top.Child("effects"))
				ApplyEffects(*effects, spec.effects);
			if (auto typography = top.Child("typography"))
				ApplyTypography(*typography, spec.typography);
			if (auto icons = top.Child("icons"))
				ApplyIcons(*icons, spec.icons);
			top.Finish();
			DeriveTokens(spec, pinnedAccentMuted, pinnedIconTint);
		}

		[[nodiscard]] std::string FirstLine(std::string_view a_text)
		{
			return std::string{ a_text.substr(0, a_text.find('\n')) };
		}

		[[nodiscard]] std::filesystem::path PresetFile(
			const std::filesystem::path& a_directory,
			std::string_view a_id)
		{
			return a_directory /
				std::filesystem::path{ std::u8string{ a_id.begin(), a_id.end() } }
					.concat(".toml");
		}

		[[nodiscard]] std::optional<std::filesystem::file_time_type> StampOf(
			const std::filesystem::path& a_file) noexcept
		{
			std::error_code error;
			const auto stamp = std::filesystem::last_write_time(a_file, error);
			return error ? std::nullopt : std::optional{ stamp };
		}

		[[nodiscard]] std::string ReadDisplayName(
			const std::filesystem::path& a_file,
			const std::string& a_fallback) noexcept
		{
			try
			{
				const auto root = toml::parse(a_file.string());
				if (root.contains("theme") && root.at("theme").is_table() &&
					root.at("theme").contains("name") &&
					root.at("theme").at("name").is_string() &&
					!root.at("theme").at("name").as_string().empty())
					return root.at("theme").at("name").as_string();
			}
			catch (...)
			{}
			return a_fallback;
		}

		[[nodiscard]] std::shared_ptr<const ThemePreset> Fallback(
			std::string a_failure)
		{
			auto preset = std::make_shared<ThemePreset>();
			preset->spec = MakeDefaultTheme();
			preset->failure = std::move(a_failure);
			return preset;
		}
	}

	ThemePreset LoadThemePreset(const std::filesystem::path& a_file) noexcept
	{
		ThemePreset preset;
		try
		{
			std::error_code error;
			if (!std::filesystem::is_regular_file(a_file, error))
			{
				preset.spec = MakeDefaultTheme();
				preset.failure = std::format(
					"{} was not found", a_file.filename().string());
				return preset;
			}
			ApplyPreset(toml::parse(a_file.string()), preset);
		}
		catch (const std::exception& error)
		{
			preset = {};
			preset.spec = MakeDefaultTheme();
			preset.failure = FirstLine(error.what());
		}
		catch (...)
		{
			preset = {};
			preset.spec = MakeDefaultTheme();
			preset.failure = "the file could not be read";
		}
		return preset;
	}

	std::vector<ThemePresetInfo> ListThemePresets(
		const std::filesystem::path& a_directory) noexcept
	{
		std::vector<ThemePresetInfo> presets;
		try
		{
			std::error_code error;
			for (std::filesystem::directory_iterator it{ a_directory, error };
				 !error && it != std::filesystem::directory_iterator{};
				 it.increment(error))
			{
				std::error_code entryError;
				if (!it->is_regular_file(entryError) ||
					Lowercase(it->path().extension().string()) != ".toml")
					continue;
				const auto stem = it->path().stem().u8string();
				std::string id{ stem.begin(), stem.end() };
				if (!IsPlainFileName(id))
					continue;
				auto name = ReadDisplayName(it->path(), id);
				presets.push_back({ std::move(id), std::move(name) });
			}
			std::ranges::sort(presets, [](const auto& a_left, const auto& a_right) {
				const auto left = Lowercase(a_left.name);
				const auto right = Lowercase(a_right.name);
				return left != right ? left < right : a_left.id < a_right.id;
			});
		}
		catch (...)
		{}
		return presets;
	}

	std::vector<std::string> ThemePresetKeys()
	{
		std::vector<std::string> keys{ "theme.name" };
		for (int index = 0; index < ImGuiCol_COUNT; ++index)
			keys.push_back(Join("colors", ImGui::GetStyleColorName(index)));
		for (const auto& key : kTokenColors)
			keys.push_back(Join("tokens", key.key));
		for (const auto& key : kStyleFloats)
			keys.push_back(Join("style", key.key));
		for (const auto& key : kStyleVec2s)
			keys.push_back(Join("style", key.key));
		for (const auto& key : kChromeFloats)
			keys.push_back(Join("chrome", key.key));
		keys.push_back(Join("chrome", kChromePanelPadding.key));
		for (const auto key : { kBlurKey, kBlurStrengthKey, kIconColorModeKey, kLogoColorModeKey })
			keys.push_back(Join("effects", key));
		keys.push_back("typography.baseFontRatio");
		for (const auto role : kFontRoleKeys)
		{
			for (const auto key : kRoleKeys)
				keys.push_back(Join(Join("typography", role), key));
		}
		keys.push_back("icons.font");
		keys.push_back("icons.baselineOffsetRatio");
		return keys;
	}

	HealthObservation ClassifyThemePresetHealth(
		std::string_view a_id,
		const ThemePreset& a_preset)
	{
		if (!a_preset.failure.empty())
		{
			return {
				HealthState::kDegraded,
				std::format(
					"Theme preset \"{}\" could not be loaded ({}); using the built-in default theme.",
					a_id,
					a_preset.failure)
			};
		}
		if (a_preset.problems.empty())
			return { HealthState::kReady, std::format("Loaded theme preset \"{}\".", a_id) };

		constexpr size_t kVisibleLimit = 5;
		const auto visible = (std::min)(a_preset.problems.size(), kVisibleLimit);
		std::string reason = std::format(
			"Theme preset \"{}\" ignored {} invalid entr{}: ",
			a_id,
			a_preset.problems.size(),
			a_preset.problems.size() == 1 ? "y" : "ies");
		for (size_t index = 0; index < visible; ++index)
		{
			if (index != 0)
				reason.append("; ");
			reason.append(a_preset.problems[index]);
		}
		if (a_preset.problems.size() > visible)
		{
			reason.append(std::format(
				"; and {} more", a_preset.problems.size() - visible));
		}
		reason.push_back('.');
		return { HealthState::kDegraded, std::move(reason) };
	}

	void ThemePresetCache::Poll() noexcept
	{
		using Clock = std::chrono::steady_clock;
		const auto now = Clock::now().time_since_epoch().count();
		auto next = m_nextPoll.load(std::memory_order_relaxed);
		if (now < next)
			return;
		const auto interval = std::chrono::duration_cast<Clock::duration>(
			m_pollInterval).count();
		if (!m_nextPoll.compare_exchange_strong(next, now + interval))
			return;

		auto changed = false;
		const std::scoped_lock lock{ m_mutex };
		for (auto& [id, entry] : m_entries)
		{
			if (StampOf(PresetFile(m_directory, id)) == entry.stamp)
				continue;
			entry = Load(id);
			changed = true;
		}
		if (changed)
			m_revision.Advance();
	}

	std::shared_ptr<const ThemePreset> ThemePresetCache::Resolve(
		std::string_view a_id) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		auto found = m_entries.find(a_id);
		if (found == m_entries.end())
			found = m_entries.emplace(std::string{ a_id }, Load(std::string{ a_id })).first;
		return found->second.preset;
	}

	ThemePresetCache::Entry ThemePresetCache::Load(
		const std::string& a_id) const noexcept
	{
		if (!IsPlainFileName(a_id))
			return { Fallback("the preset name is not a valid file name"), std::nullopt };
		const auto file = PresetFile(m_directory, a_id);
		// Stamped first so an edit during the parse reloads on the next poll.
		const auto stamp = StampOf(file);
		return { std::make_shared<const ThemePreset>(LoadThemePreset(file)), stamp };
	}
}
