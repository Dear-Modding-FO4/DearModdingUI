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
		template <dmui::SettingValueAlternative T>
		[[nodiscard]] dmui::SettingDescriptor MakeBoundSetting(
			SyntheticSettingsState* a_state,
			T SyntheticSettingsValues::* a_member,
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

	bool RegisterSyntheticClients(
		std::vector<std::unique_ptr<dmui::Client>>& a_clients,
		SyntheticSettingsState& a_settings,
		std::string& a_error) noexcept
	{
		try
		{
			a_error.clear();
			a_clients.reserve(a_clients.size() + kSyntheticClients.size());
			for (const auto& fixture : kSyntheticClients)
			{
				auto client = std::make_unique<dmui::Client>(
					fixture.id,
					fixture.displayName,
					dmui::Version{ 0, 1 },
					"test-tube",
					std::string_view{ fixture.id } ==
							"dearmodding.tests.synthetic.status" ?
						dmui::ClientOrigin{
							dmui::ClientOriginKind::kBridged,
							"Synthetic Test Bridge"
						} :
						dmui::ClientOrigin{});
				if (!client->Connect())
				{
					a_error = std::string{ "Could not connect " } +
						fixture.displayName + " (result " +
						DMUI_ResultToString(client->LastResult()) + ").";
					return false;
				}
				auto* registered = client.get();
				a_clients.push_back(std::move(client));

				if (std::string_view{ fixture.id } ==
					"dearmodding.tests.synthetic.navigation")
				{
					if (!registered->AddCategory({
							.id = "general",
							.displayName = "General"
						}) ||
						!registered->AddCategory({
							.id = "advanced",
							.displayName = "Advanced",
							.sortKey = 10
						}))
					{
						a_error =
							"Could not register synthetic navigation categories (result " +
							std::string{ DMUI_ResultToString(
								registered->LastResult()) } + ").";
						return false;
					}
					const std::array pages{
						dmui::PageDescriptor{
							.id = "overview",
							.displayName = "Overview",
							.categoryId = "general",
							.summary = "Synthetic navigation overview."
						},
						dmui::PageDescriptor{
							.id = "tuning",
							.displayName = "Tuning",
							.categoryId = "general",
							.summary = "Synthetic navigation tuning page.",
							.sortKey = 10
						},
						dmui::PageDescriptor{
							.id = "diagnostics",
							.displayName = "Diagnostics",
							.categoryId = "advanced",
							.summary = "Synthetic navigation diagnostic page.",
							.sortKey = 20
						}
					};
					for (const auto& page : pages)
					{
						if (!registered->AddPage(
								page,
								[registered, page] {
									(void)registered->DrawSectionHeader(
										page.displayName);
									(void)registered->DrawBulletText(page.summary);
									(void)registered->DrawBulletText(
										"Synthetic navigation data; no mod is installed.");
								}))
						{
							a_error =
								"Could not register synthetic navigation pages (result " +
								std::string{ DMUI_ResultToString(
									registered->LastResult()) } + ").";
							return false;
						}
					}
				}
				else if (std::string_view{ fixture.id } ==
					"dearmodding.tests.synthetic.configuration")
				{
					if (!registered->AddSettingsPage(
						{
							.id = fixture.pageId,
							.displayName = fixture.pageDisplayName,
							.summary = fixture.summary
						},
						MakeSyntheticSettingsPage(&a_settings)))
					{
						a_error =
							"Could not register synthetic configuration settings (result " +
							std::string{ DMUI_ResultToString(
								registered->LastResult()) } + ").";
						return false;
					}
				}
				else if (!registered->AddPage(
					{
						.id = fixture.pageId,
						.displayName = fixture.pageDisplayName,
						.summary = fixture.summary
					},
					[registered, fixture] {
						(void)registered->DrawSectionHeader(
							fixture.pageDisplayName);
						(void)registered->DrawBulletText(fixture.summary);
						(void)registered->DrawBulletText(
							"Expected: Health attributes this entry to Synthetic Test Bridge.");
					}))
				{
					a_error = std::string{ "Could not register " } +
						fixture.displayName + " (result " +
						DMUI_ResultToString(registered->LastResult()) + ").";
					return false;
				}
				if (std::string_view{ fixture.id } ==
					"dearmodding.tests.synthetic.status")
				{
					if (!registered->SetStatus(
						DMUI_STATUS_SEVERITY_INFO,
						"[Fixture] Synthetic status observation.") ||
						!registered->ReportDiagnostic({
							DMUI_STATUS_SEVERITY_WARNING,
							"synthetic-status.toml",
							"[Fixture] Deliberate diagnostic warning.",
							"Expected only in the DMUI test suite."
						}))
					{
						a_error =
							"Could not register synthetic status observations (result " +
							std::string{ DMUI_ResultToString(
								registered->LastResult()) } + ").";
						return false;
					}
				}
			}
			return true;
		}
		catch (const std::exception& a_exception)
		{
			a_error = "Synthetic fixture registration threw: ";
			a_error += a_exception.what();
			return false;
		}
	}

	dmui::SettingsPage MakeSyntheticSettingsPage(
		SyntheticSettingsState* a_state)
	{
		dmui::SettingsPage page;
		page.filterOptions.searchHint = "Search synthetic configuration...";
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
			&SyntheticSettingsValues::enabled,
			"enabled",
			"Enable synthetic feature",
			"Exercises an immediate in-memory Boolean setting.",
			dmui::CheckboxSettingControl{},
			dmui::SettingApplyTiming::kImmediate));
		general.settings.push_back(MakeBoundSetting(
			a_state,
			&SyntheticSettingsValues::preset,
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
			&SyntheticSettingsValues::profileName,
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
			&SyntheticSettingsValues::workerThreads,
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
			&SyntheticSettingsValues::animationSpeed,
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
			&SyntheticSettingsValues::framePacingWindow,
			"frame-pacing-window",
			"Frame pacing window",
			"Exercises an open-ended drag control.",
			std::move(pacingControl)));

		page.groups.push_back(std::move(general));
		page.groups.push_back(std::move(performance));
		return page;
	}
}
