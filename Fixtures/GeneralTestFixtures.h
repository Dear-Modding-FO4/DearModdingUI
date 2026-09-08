#pragma once

#if defined(IMGUI_VERSION) || defined(IMGUI_VERSION_NUM)
#error "general test fixtures require forwarding declarations, not real Dear ImGui"
#endif

// Isolate both inline API namespaces from the preview host's lockstep clients.
#define dmui DmuiFixtureClient
#define ImGui DmuiFixtureImGui
#include <DearModdingUI/Client.h>
#undef ImGui
#undef dmui

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace DmuiTestFixtures
{
	namespace dmui = DmuiFixtureClient;

	inline constexpr std::string_view kClientId{
		"dearmodding.tests.general"
	};
	inline constexpr std::string_view kClientDisplayName{ "DMUI Tests" };
	inline constexpr std::string_view kCategoryId{ "exercises" };
	inline constexpr std::string_view kCategoryDisplayName{ "Exercises" };

	struct SyntheticClient
	{
		const char* id;
		const char* displayName;
		const char* pageId;
		const char* pageDisplayName;
		const char* summary;
	};

	inline constexpr std::array kSyntheticClients{
		SyntheticClient{
			"dearmodding.tests.synthetic.navigation",
			"[Fixture] Navigation Client",
			"navigation",
			"Navigation fixture",
			"Synthetic categories and pages for navigation inspection."
		},
		SyntheticClient{
			"dearmodding.tests.synthetic.status",
			"[Fixture] Status Client",
			"status",
			"Status fixture",
			"Synthetic status and diagnostic presentation."
		},
		SyntheticClient{
			"dearmodding.tests.synthetic.configuration",
			"[Fixture] Configuration Client",
			"configuration",
			"Configuration fixture",
			"In-memory synthetic configuration with no persistence."
		}
	};

	struct SyntheticSettingsValues
	{
		bool enabled;
		std::string preset;
		std::string profileName;
		int64_t workerThreads;
		double animationSpeed;
		double framePacingWindow;

		bool operator==(const SyntheticSettingsValues&) const = default;
	};

	struct SyntheticSettingsState
	{
		SyntheticSettingsValues defaults{
			true,
			"Balanced",
			"Commonwealth",
			4,
			1.0,
			2.0
		};
		SyntheticSettingsValues committed{
			true,
			"Quality",
			"4K Preview",
			8,
			1.15,
			2.5
		};
		SyntheticSettingsValues draft{ committed };
	};

	enum class ExerciseKind : uint8_t
	{
		kResults,
		kImages,
		kOverlay,
		kInteractions,
		kNotificationsAndDialogs,
		kPlot
	};

	enum class Outcome : uint8_t
	{
		kUnexercised,
		kObserved,
		kFailed
	};

	struct ExercisePage
	{
		ExerciseKind kind;
		const char* id;
		const char* displayName;
		const char* summary;
		const char* howTo;
		const char* expected;
		DMUI_PageKind pageKind{ DMUI_PAGE_KIND_SETTINGS };
	};

	inline constexpr std::array kExercisePages{
		ExercisePage{
			ExerciseKind::kResults,
			"results",
			"Results and status",
			"Host, forwarding, service, and explicit exercise outcomes.",
			"Run each exercise page, then return here and log a result snapshot.",
			"Every outcome remains unexercised until an action or observation occurs."
		},
		ExercisePage{
			ExerciseKind::kImages,
			"images",
			"Images",
			"Host-owned CPU updates and imported D3D11 image lifetime.",
			"Update the CPU image, cycle the imported image, and release after a queued draw.",
			"The CPU handle changes pixels and size; queued imported draws survive release."
		},
		ExercisePage{
			ExerciseKind::kOverlay,
			"overlay-controls",
			"Overlay and arrangement",
			"Managed overlay visibility, placement, scaling, and frame demand.",
			"Toggle the overlay, try every anchor, then arrange the free overlay while the menu owns input.",
			"Requests and releases balance; free movement is blocked outside host-owned input."
		},
		ExercisePage{
			ExerciseKind::kInteractions,
			"interactions",
			"Hotkeys and edits",
			"Contextual hotkeys and native edit lifecycle counters.",
			"Exercise the namespaced hotkeys in safe and obstructed contexts, then edit and reset every field.",
			"Blocked contexts yield; saves count only completed changed edits and effective resets."
		},
		ExercisePage{
			ExerciseKind::kNotificationsAndDialogs,
			"notifications-dialogs",
			"Notifications and dialogs",
			"Page and worker notifications plus confirm and validated text dialogs.",
			"Post both notifications; accept, cancel, reject, and duplicate-submit the dialogs.",
			"Only one worker job runs; operations count on COMPLETED and rejected text is preserved."
		},
		ExercisePage{
			ExerciseKind::kPlot,
			"plot",
			"Plot and counters",
			"Annotated frame-time plot and explicit observed/failed/unexercised counters.",
			"Inspect the plot label and reference lines while reviewing the live counters.",
			"The label stays visible, plot contents clip, and absence of errors never implies pass."
		}
	};

	struct RegisteredExercises
	{
		std::array<DMUI_PageHandle, kExercisePages.size()> pages{};
	};

	using DrawExercise = std::function<void(ExerciseKind)>;

	[[nodiscard]] const ExercisePage& Page(ExerciseKind a_kind) noexcept;
	[[nodiscard]] const char* OutcomeName(Outcome a_outcome) noexcept;

	void DrawExerciseIntro(
		dmui::Client& a_client,
		ExerciseKind a_kind,
		Outcome a_outcome,
		std::string_view a_observed) noexcept;

	[[nodiscard]] std::optional<RegisteredExercises> RegisterExercises(
		dmui::Client& a_client,
		DrawExercise a_draw) noexcept;

	[[nodiscard]] bool RegisterSyntheticClients(
		std::vector<std::unique_ptr<dmui::Client>>& a_clients,
		SyntheticSettingsState& a_settings,
		std::string& a_error) noexcept;

	[[nodiscard]] dmui::SettingsPage MakeSyntheticSettingsPage(
		SyntheticSettingsState* a_state);
}
