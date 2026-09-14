#include "../Harness.h"

#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/host/HostAPIEntries.h>

#include <array>
#include <string>
#include <utility>

namespace vmm_tests
{
	using DearModdingUI::HostAPIInternal::ApiResolveIconGlyph;

	namespace
	{
		char32_t QueryIconGlyph(
			const char* a_explicit,
			const char* a_primary,
			const char* a_secondary = nullptr)
		{
			const DMUI_IconResolutionRequest request{
				sizeof(request),
				a_explicit,
				a_primary,
				a_secondary
			};
			uint32_t glyph{ 0xFFFFFFFFu };
			require(
				ApiResolveIconGlyph(&request, &glyph) == DMUI_RESULT_OK,
				"valid icon query failed");
			return static_cast<char32_t>(glyph);
		}
	}

	void run_icon_resolution_checks(Runner& runner)
	{
		runner.test("host icon query delegates selection precedence to shared resolver", [] {
			const auto explicitGlyph =
				DearModdingUI::ResolveNamedIconGlyphOrZero("wrench");
			require(
				explicitGlyph != 0 &&
					QueryIconGlyph("wrench", "speaker", "display") == explicitGlyph,
				"canonical explicit name did not win");

			const auto fallbackExpected =
				DearModdingUI::ResolveIconSelection(
					"not-a-real-icon-name",
					"wrench",
					"speaker")
					.GlyphOr({});
			require(
				fallbackExpected != 0 &&
					QueryIconGlyph("not-a-real-icon-name", "wrench", "speaker") ==
						fallbackExpected,
				"unknown explicit name did not fall through to metadata");

			const auto precedenceExpected =
				DearModdingUI::ResolveIconSelection(
					{},
					"speaker",
					"wrench")
					.GlyphOr({});
			require(
				precedenceExpected != 0 &&
					QueryIconGlyph(nullptr, "speaker", "wrench") ==
						precedenceExpected,
				"primary metadata did not retain precedence");
			require(
				QueryIconGlyph(nullptr, "host-only-unmatched-label") == 0,
				"genuine no-match did not return a successful zero glyph");
		});

		runner.test("host icon inference covers common interface roles and word forms", [] {
			const std::array cases{
				std::pair{ "Overview", "list-bullets" },
				std::pair{ "Summary", "list-bullets" },
				std::pair{ "At a glance", "list-bullets" },
				std::pair{ "Status", "info" },
				std::pair{ "Statuses", "info" },
				std::pair{ "Asset", "files" },
				std::pair{ "Assets", "files" },
				std::pair{ "Health", "heart" },
				std::pair{ "Character", "user" },
				std::pair{ "Character & Progression", "user" },
				std::pair{ "World", "globe" }
			};
			for (const auto& [label, name] : cases)
			{
				const auto expected = DearModdingUI::FindPhosphorIconGlyphOrZero(name);
				require(
					expected != 0 && QueryIconGlyph(nullptr, label) == expected,
					std::string{ "unexpected interface icon for " } + label);
			}
		});

		runner.test("authored section headings retain their subject over generic fragments", [] {
			const std::array cases{
				std::pair{ "General", "gear" },
				std::pair{ "Diagnostics", "stethoscope" },
				std::pair{ "Menus & Saves", "floppy-disk" },
				std::pair{ "Travel & Exploration", "path" },
				std::pair{ "Weight & Difficulty", "gauge" },
				std::pair{ "Hardcore Rules", "campfire" },
				std::pair{ "Sustenance Tuning", "fork-knife" },
				std::pair{ "Food Stage Thresholds", "fork-knife" },
				std::pair{ "Drink Stage Thresholds", "drop" },
				std::pair{ "Sleep Tuning", "moon" },
				std::pair{ "Disease Tuning", "virus" },
				std::pair{ "Adrenaline", "heartbeat" },
				std::pair{ "Encumbrance", "backpack" },
				std::pair{ "Consumables", "pill" },
				std::pair{ "Survival Stage Penalties", "campfire" },
				std::pair{ "Damage Taken", "shield" },
				std::pair{ "Damage Done", "sword" },
				std::pair{ "Progression", "trend-up" },
				std::pair{ "Legendary Chance", "dice-five" },
				std::pair{ "Legendary Rarity", "diamond" },
				std::pair{ "Effect Duration", "timer" },
				std::pair{ "Effect Magnitude", "gauge" },
				std::pair{ "Action Points", "lightning" },
				std::pair{ "Sprint", "person-simple-run" },
				std::pair{ "Carry Weight", "backpack" },
				std::pair{ "Radiation", "radioactive" },
				std::pair{ "Physical", "hand-fist" },
				std::pair{ "Energy", "lightning" },
				std::pair{ "Jetpack", "rocket-launch" },
				std::pair{ "Fusion Core Drain", "battery-low" },
				std::pair{ "Durability", "shield-check" },
				std::pair{ "Buy / Sell Pricing", "tag" },
				std::pair{ "Persuasion", "chat-circle-dots" },
				std::pair{ "Crafting XP - Cooking", "cooking-pot" },
				std::pair{ "Crafting XP - Weapon/Armor Workbench", "hammer" },
				std::pair{ "Crafting XP - Settlement Workshop", "house" },
				std::pair{ "Lockpick XP", "lock-key" },
				std::pair{ "Other", "dots-three-circle" },
				std::pair{ "Kill & Discovery XP", "trend-up" },
				std::pair{ "XP Formula", "function" },
				std::pair{ "Targeting", "crosshair" },
				std::pair{ "Timing", "timer" },
				std::pair{ "Pickpocket", "hand-coins" },
				std::pair{ "Hacking", "terminal-window" },
				std::pair{ "Lockpicking", "lock-key" },
				std::pair{ "Lockpick Durability", "lock-key" },
				std::pair{ "Sweet Spot by Lock Level", "lock" },
				std::pair{ "Sneak Attack Damage", "knife" },
				std::pair{ "Detection", "eye" },
				std::pair{ "Affinity Per Reaction", "hand-heart" },
				std::pair{ "Reaction Cooldowns", "timer" },
				std::pair{ "Combat Chances", "sword" },
				std::pair{ "Light Armor Perk", "shield" },
				std::pair{ "Heavy Armor Perk", "shield" },
				std::pair{ "Settler & Building", "building" },
				std::pair{ "Placement Constraints", "ruler" }
			};
			for (const auto& [label, name] : cases)
			{
				const auto expected = DearModdingUI::FindPhosphorIconGlyphOrZero(name);
				require(
					expected != 0 && QueryIconGlyph(nullptr, label) == expected,
					std::string{ "unexpected gameplay icon for " } + label);
			}

			const auto wrench = DearModdingUI::FindPhosphorIconGlyphOrZero("wrench");
			const auto moon = DearModdingUI::FindPhosphorIconGlyphOrZero("moon");
			const auto files = DearModdingUI::FindPhosphorIconGlyphOrZero("files");
			require(
				QueryIconGlyph("wrench", "Sleep Tuning") == wrench &&
					QueryIconGlyph(nullptr, "Sleep Tuning", "Wrench") == moon &&
					QueryIconGlyph(nullptr, "Files Assets") == files &&
					QueryIconGlyph(nullptr, "Assets Wrench") == wrench &&
					QueryIconGlyph(nullptr, "Tuning") ==
						DearModdingUI::FindPhosphorIconGlyphOrZero("sliders-horizontal"),
				"authored phrases changed explicit, canonical, or primary precedence");
		});

		runner.test("host icon query validates its versioned bounded request", [] {
			DMUI_IconResolutionRequest request{
				DMUI_ICON_RESOLUTION_REQUEST_0_1_SIZE,
				nullptr,
				"wrench\tsettings",
				nullptr
			};
			uint32_t glyph{ 0xFFFFFFFFu };
			require(
				ApiResolveIconGlyph(&request, &glyph) == DMUI_RESULT_OK &&
					glyph == static_cast<uint32_t>(
						DearModdingUI::ResolveIconSelection(
							{},
							"wrench\tsettings")
							.GlyphOr({})),
				"exact request size or permitted tab was rejected");

			request.structSize += sizeof(uint64_t);
			glyph = 0xFFFFFFFFu;
			require(
				ApiResolveIconGlyph(&request, &glyph) == DMUI_RESULT_OK &&
					glyph != 0,
				"extended request was rejected");

			request.structSize =
				DMUI_ICON_RESOLUTION_REQUEST_0_1_SIZE - 1;
			glyph = 0xFFFFFFFFu;
			require(
				ApiResolveIconGlyph(&request, &glyph) ==
						DMUI_RESULT_STRUCT_TOO_SMALL &&
					glyph == 0,
				"short request did not fail with zeroed output");
			glyph = 0xFFFFFFFFu;
			require(
				ApiResolveIconGlyph(nullptr, &glyph) ==
						DMUI_RESULT_INVALID_ARGUMENT &&
					glyph == 0 &&
					ApiResolveIconGlyph(&request, nullptr) ==
						DMUI_RESULT_INVALID_ARGUMENT,
				"null request or output validation changed");

			std::array<char, 129> overlongName{};
			overlongName.fill('a');
			std::array<char, 257> overlongMetadata{};
			overlongMetadata.fill('b');
			request.structSize = DMUI_ICON_RESOLUTION_REQUEST_0_1_SIZE;
			request.explicitName = overlongName.data();
			request.primaryMetadata = nullptr;
			glyph = 0xFFFFFFFFu;
			require(
				ApiResolveIconGlyph(&request, &glyph) ==
						DMUI_RESULT_INVALID_ARGUMENT &&
					glyph == 0,
				"nonterminated explicit name was accepted");
			request.explicitName = nullptr;
			request.primaryMetadata = overlongMetadata.data();
			glyph = 0xFFFFFFFFu;
			require(
				ApiResolveIconGlyph(&request, &glyph) ==
						DMUI_RESULT_INVALID_ARGUMENT &&
					glyph == 0,
				"nonterminated metadata was accepted");

			const std::string controlText{ "wrench\nsettings" };
			request.primaryMetadata = controlText.c_str();
			glyph = 0xFFFFFFFFu;
			require(
				ApiResolveIconGlyph(&request, &glyph) ==
						DMUI_RESULT_INVALID_ARGUMENT &&
					glyph == 0,
				"disallowed control text was accepted");
		});
	}
}
