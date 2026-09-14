#include "GeneralTestFixtures.h"

#include <algorithm>
#include <exception>
#include <string>
#include <utility>

namespace DmuiTestFixtures
{
	void DrawExerciseIntro(
		dmui::Client& a_client,
		ExerciseKind a_kind,
		Outcome a_outcome,
		std::string_view a_observed) noexcept
	{
		const auto& page = Page(a_kind);
		(void)a_client.DrawSectionHeader(page.displayName);
		dmui::ui::TextWrapped("How to: %s", page.howTo);
		dmui::ui::TextWrapped("Expected: %s", page.expected);
		dmui::ui::Text(
			"Observed: %s%s%.*s",
			OutcomeName(a_outcome),
			a_observed.empty() ? "" : " - ",
			static_cast<int>(a_observed.size()),
			a_observed.data());
		dmui::ui::Separator();
	}

	namespace
	{
		inline constexpr std::array kFixtureCategories{
			dmui::CategoryDescriptor{
				"fixtures-navigation",
				"Fixture navigation",
				100
			},
			dmui::CategoryDescriptor{
				"fixtures-navigation-advanced",
				"Fixture navigation - Advanced",
				110
			},
			dmui::CategoryDescriptor{
				"fixtures-configuration",
				"Fixture configuration",
				120
			}
		};

		struct FixturePage
		{
			dmui::PageDescriptor descriptor;
			const char* detail;
		};

		inline constexpr std::array kFixturePages{
			FixturePage{
				{
					"fixtures-navigation-overview",
					"Navigation overview",
					"fixtures-navigation",
					"Basic category and page navigation.",
					0
				},
				"Expected: this page and Navigation tuning share one category."
			},
			FixturePage{
				{
					"fixtures-navigation-tuning",
					"Navigation tuning",
					"fixtures-navigation",
					"Sibling-page ordering within a category.",
					10
				},
				"Expected: this page follows Navigation overview."
			},
			FixturePage{
				{
					"fixtures-navigation-diagnostics",
					"Navigation diagnostics",
					"fixtures-navigation-advanced",
					"Navigation across fixture categories.",
					0
				},
				"Expected: switching here changes category without changing clients."
			}
		};

		template <dmui::SettingValueAlternative T>
		[[nodiscard]] dmui::SettingDescriptor MakeBoundSetting(
			SettingsFixtureState* a_state,
			T SettingsFixtureValues::* a_member,
			std::string a_id,
			std::string a_label,
			std::string a_description,
			dmui::SettingControl a_control,
			dmui::SettingApplyTiming a_timing =
				dmui::SettingApplyTiming::kNextLaunch)
		{
			dmui::SettingDescriptor setting;
			setting.id = std::move(a_id);
			setting.label = std::move(a_label);
			setting.description = std::move(a_description);
			setting.control = std::move(a_control);
			setting.defaultValue = a_state->defaults.*a_member;
			setting.binding = dmui::BindSetting(
				[a_state, a_member]() -> T {
					return a_state->draft.*a_member;
				},
				[a_state, a_member](T a_value) -> T {
					a_state->draft.*a_member = std::move(a_value);
					return a_state->draft.*a_member;
				});
			setting.applyTiming = a_timing;
			setting.isDirty = [a_state, a_member]() {
				return a_state->draft.*a_member !=
					a_state->committed.*a_member;
			};
			setting.isModified = [a_state, a_member]() {
				return a_state->draft.*a_member !=
					a_state->defaults.*a_member;
			};
			return setting;
		}

		[[nodiscard]] dmui::SettingsPage MakeSettingsFixturePage(
			SettingsFixtureState* a_state)
		{
			dmui::SettingsPage page;
			page.filterOptions.searchHint = "Search fixture configuration...";
			page.notes.push_back({
				"Fixture values exist only in memory; Apply never writes a file.",
				true
			});
			page.actions.showReset = true;
			page.actions.reset = [a_state]() {
				a_state->draft = a_state->defaults;
			};
			page.actions.revert = [a_state]() {
				a_state->draft = a_state->committed;
			};
			page.actions.apply = [a_state]() {
				a_state->committed = a_state->draft;
			};

			dmui::SettingGroup general{
				"general",
				"General",
				0,
				{},
				true
			};
			general.settings.push_back(MakeBoundSetting(
				a_state,
				&SettingsFixtureValues::enabled,
				"enabled",
				"Enable fixture feature",
				"Exercises an immediate in-memory Boolean setting.",
				dmui::CheckboxSettingControl{},
				dmui::SettingApplyTiming::kImmediate));
			general.settings.push_back(MakeBoundSetting(
				a_state,
				&SettingsFixtureValues::preset,
				"preset",
				"Preset",
				"Exercises an in-memory choice.",
				dmui::ChoiceSettingControl{
					{
						{ "Balanced", "Balanced" },
						{ "Quality", "Quality" },
						{ "Performance", "Performance" }
					}
				}));
			general.settings.push_back(MakeBoundSetting(
				a_state,
				&SettingsFixtureValues::profileName,
				"profile-name",
				"Profile name",
				"Exercises an in-memory text value.",
				dmui::TextSettingControl{ 96 }));

			dmui::SettingGroup performance{
				"performance",
				"Performance",
				10,
				{},
				true
			};
			dmui::SignedSettingControl workerControl;
			workerControl.range =
				dmui::NumericSettingRange<int64_t>{ int64_t{ 1 }, int64_t{ 16 } };
			workerControl.format = "%lld threads";
			performance.settings.push_back(MakeBoundSetting(
				a_state,
				&SettingsFixtureValues::workerThreads,
				"worker-threads",
				"Worker threads",
				"Exercises a bounded integer setting.",
				std::move(workerControl)));

			dmui::DoubleSettingControl animationControl;
			animationControl.range =
				dmui::NumericSettingRange<double>{ 0.5, 2.0 };
			animationControl.format = "%.2fx";
			performance.settings.push_back(MakeBoundSetting(
				a_state,
				&SettingsFixtureValues::animationSpeed,
				"animation-speed",
				"Animation speed",
				"Exercises an immediate floating-point setting.",
				std::move(animationControl),
				dmui::SettingApplyTiming::kImmediate));

			dmui::DoubleSettingControl pacingControl;
			pacingControl.range =
				dmui::NumericSettingRange<double>{ 0.25, std::nullopt };
			pacingControl.format = "%.2f ms";
			pacingControl.dragSpeed = 0.05f;
			performance.settings.push_back(MakeBoundSetting(
				a_state,
				&SettingsFixtureValues::framePacingWindow,
				"frame-pacing-window",
				"Frame pacing window",
				"Exercises an open-ended drag control.",
				std::move(pacingControl)));

			page.groups.push_back(std::move(general));
			page.groups.push_back(std::move(performance));
			return page;
		}

		[[nodiscard]] bool SetRegistrationError(
			const dmui::Client& a_client,
			std::string_view a_scope,
			std::string& a_error)
		{
			a_error = "Could not register ";
			a_error += a_scope;
			a_error += " (result ";
			a_error += DMUI_ResultToString(a_client.LastResult());
			a_error += ").";
			return false;
		}
	}

	#undef ImGui

	const ExercisePage& Page(ExerciseKind a_kind) noexcept
	{
		const auto found = std::ranges::find(
			kExercisePages,
			a_kind,
			&ExercisePage::kind);
		return found != kExercisePages.end() ? *found : kExercisePages.front();
	}

	const char* OutcomeName(Outcome a_outcome) noexcept
	{
		switch (a_outcome)
		{
		case Outcome::kObserved:
			return "observed";
		case Outcome::kFailed:
			return "failed";
		default:
			return "unexercised";
		}
	}

	std::optional<RegisteredExercises> RegisterExercises(
		dmui::Client& a_client,
		DrawExercise a_draw) noexcept
	{
		if (!a_draw ||
			!a_client.AddCategory({
				.id = kCategoryId.data(),
				.displayName = kCategoryDisplayName.data()
			}))
			return std::nullopt;

		RegisteredExercises result;
		for (size_t index = 0; index < kExercisePages.size(); ++index)
		{
			const auto& page = kExercisePages[index];
			const auto handle = a_client.AddPage(
				{
					.id = page.id,
					.displayName = page.displayName,
					.categoryId = kCategoryId.data(),
					.summary = page.summary,
					.sortKey = static_cast<int32_t>(index * 10),
					.kind = page.pageKind
				},
				[draw = a_draw, kind = page.kind] { draw(kind); });
			if (!handle)
				return std::nullopt;
			result.pages[index] = *handle;
		}
		return result;
	}

	bool RegisterFixturePages(
		dmui::Client& a_client,
		SettingsFixtureState& a_settings,
		std::string& a_error) noexcept
	{
		try
		{
			a_error.clear();
			for (const auto& category : kFixtureCategories)
			{
				if (!a_client.AddCategory(category))
					return SetRegistrationError(
						a_client,
						std::string{ "fixture category '" } +
							category.id + "'",
						a_error);
			}

			for (const auto& page : kFixturePages)
			{
				if (!a_client.AddPage(
					page.descriptor,
					[client = &a_client, page] {
						(void)client->DrawSectionHeader(page.descriptor.displayName);
						(void)client->DrawBulletText(page.descriptor.summary);
						(void)client->DrawBulletText(page.detail);
					}))
					return SetRegistrationError(
						a_client,
						std::string{ "fixture page '" } + page.descriptor.id + "'",
						a_error);
			}

			if (!a_client.AddSettingsPage(
					{
						.id = "fixtures-configuration-settings",
						.displayName = "In-memory configuration",
						.categoryId = "fixtures-configuration",
						.summary =
							"Declarative settings backed only by fixture memory."
					},
					MakeSettingsFixturePage(&a_settings)))
				return SetRegistrationError(
					a_client,
					"fixture page 'fixtures-configuration-settings'",
					a_error);

			return true;
		}
		catch (const std::exception& a_exception)
		{
			a_error = "Fixture page registration threw: ";
			a_error += a_exception.what();
			return false;
		}
	}
}
