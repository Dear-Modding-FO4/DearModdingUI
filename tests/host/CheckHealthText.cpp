#include "../Harness.h"
#include "../support/LanguageScope.h"

#include <DearModdingUI/host/Registry.h>
#include <DearModdingUI/localization/Localization.h>
#include <DearModdingUI/pages/Health.h>
#include <DearModdingUI/presentation/TypographyHealth.h>
#include <Platform/input/GameInput.h>

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;

	namespace
	{
		[[nodiscard]] bool Contains(std::string_view a_text, std::string_view a_part)
		{
			return a_text.find(a_part) != std::string_view::npos;
		}

		[[nodiscard]] Theme::TypographyLoadOutcome LoadedOutcome()
		{
			Theme::TypographyLoadOutcome outcome;
			outcome.requestedFamily = "Jost";
			outcome.effectiveFamily = "Jost";
			outcome.requestedFamilyFound = true;
			outcome.requestedBodyLoaded = true;
			outcome.rolesLoaded.fill(true);
			outcome.iconsLoaded = true;
			outcome.systemFallbacksLoaded = 1;
			outcome.usableAtlas = true;
			return outcome;
		}

		// The Typography subsystem name in Cyrillic.
		constexpr std::string_view kRussianTypography{
			"\xD0\xA2\xD0\xB8\xD0\xBF\xD0\xBE\xD0\xB3\xD1\x80\xD0\xB0\xD1\x84\xD0\xB8\xD0\xBA\xD0\xB0"
		};
	}

	void run_health_text_checks(Runner& runner)
	{
		runner.test("typography health lists the missing roles and counts the fallbacks", [] {
			auto outcome = LoadedOutcome();
			auto observation = Theme::ClassifyTypographyHealth(outcome);
			require(
				observation.state == HealthState::kReady &&
					observation.reason.English() ==
						"Loaded \"Jost\" with all bundled roles, Phosphor icons, and 1 system fallback font.",
				"a ready typography reason changed");

			outcome.systemFallbacksLoaded = 3;
			require(
				Theme::ClassifyTypographyHealth(outcome).reason.English().ends_with("and 3 system fallback fonts."),
				"the fallback font count did not follow its plural");

			outcome.rolesLoaded[static_cast<size_t>(Theme::FontRole::kTitle)] = false;
			observation = Theme::ClassifyTypographyHealth(outcome);
			require(
				observation.state == HealthState::kDegraded &&
					observation.reason.English() == "Bundled title role used fallback text.",
				"one missing role was not singular");

			outcome.rolesLoaded[static_cast<size_t>(Theme::FontRole::kMonospace)] = false;
			outcome.iconsLoaded = false;
			observation = Theme::ClassifyTypographyHealth(outcome);
			const auto english = observation.reason.English();
			require(
				english ==
					"Bundled title, monospace roles used fallback text. "
					"Phosphor icons are unavailable; labels remain text-only.",
				"missing roles or the sentence join changed");
			{
				const support::LanguageScope language{ "ru" };
				const auto russian = Localization::Render(observation.reason);
				require(
					russian != english && Contains(russian, "title, monospace"),
					"a translated typography reason lost its role names");
			}

			outcome.usableAtlas = false;
			require(Theme::ClassifyTypographyHealth(outcome).state == HealthState::kFailed,
				"an unusable atlas was not a failure");
		});

		runner.test("input hook failures name the receiver in one sentence per language", [] {
			using GameInput::InputHookFailure;
			using GameInput::InputReceiver;
			using GameInput::InputReceiverHookOutcome;
			std::array<InputReceiverHookOutcome, 3> outcomes{ {
				{ InputReceiver::kMenuControls, InputHookFailure::kNone },
				{ InputReceiver::kPlayerControls, InputHookFailure::kNone },
				{ InputReceiver::kPlayerCamera, InputHookFailure::kNone }
			} };
			const auto ready = GameInput::ClassifyInputHookHealth(outcomes);
			require(
				ready.state == HealthState::kReady &&
					ready.reason.English() ==
						"MenuControls, PlayerControls, and PlayerCamera interception installed.",
				"the ready input reason changed");

			outcomes[2].failure = InputHookFailure::kPatchFailed;
			const auto failed = GameInput::ClassifyInputHookHealth(outcomes);
			const auto english = failed.reason.English();
			require(
				failed.state == HealthState::kFailed &&
					english ==
						"PlayerCamera receiver patch failed; check for an incompatible input hook. "
						"Required game-input interception is unavailable.",
				"a failing receiver was not named in its sentence");
			{
				const support::LanguageScope language{ "fr" };
				const auto french = Localization::Render(failed.reason);
				require(
					french != english && Contains(french, "PlayerCamera"),
					"a translated input failure lost the receiver name");
			}
		});

		runner.test("health rows translate names and reasons while reports keep identities and English", [] {
			const auto now = HealthClock::now();
			auto outcome = LoadedOutcome();
			outcome.iconsLoaded = false;
			const auto degraded = Theme::ClassifyTypographyHealth(outcome);
			const std::array snapshots{
				HealthSnapshot{ "fixture.unlisted", HealthState::kReady, now, {}, Support::Message::Text("raw") },
				HealthSnapshot{
					"dmui.typography",
					degraded.state,
					now,
					{},
					degraded.reason,
					{ "$DMUI_Health_SubsystemTypography", "Typography" } }
			};
			const auto english = degraded.reason.English();

			{
				const support::LanguageScope language{ "ru" };
				const auto rows = BuildHealthSubsystemRows(snapshots, now);
				require(
					rows.size() == 2 &&
						rows[0].identity == "dmui.typography" &&
						rows[0].displayName == kRussianTypography &&
						rows[0].reason != english &&
						rows[1].identity == "fixture.unlisted" &&
						rows[1].displayName == "fixture.unlisted" &&
						rows[1].reason == "raw",
					"rows did not translate known subsystems or kept stale identities");

				const auto report = BuildHealthDiagnosticsReport(
					"Host",
					"1.0",
					snapshots,
					std::vector<RegisteredClient>{},
					std::span<const ClientStatus>{},
					std::span<const ClientDiagnosticSnapshot>{},
					now);
				require(
					Contains(report, "- dmui.typography: Degraded - " + english) &&
						Contains(report, "- fixture.unlisted: Ready - raw"),
					"the diagnostics report followed the display language");
			}

			const auto rows = BuildHealthSubsystemRows(snapshots, now);
			require(
				rows[0].displayName == "Typography" && rows[0].reason == english,
				"English rows changed their names or reasons");
		});
	}
}
