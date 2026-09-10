#include <DearModdingUI/MCM/ValueSource.h>

#include "../support/Conditions.h"
#include "../support/SliderNormalization.h"

#include <DearModdingUI/MCM/FileChoices.h>
#include <DearModdingUI/MCM/PageLookup.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace DearModdingUI::MCM
{
	namespace
	{
		void BindUnsupported(dmui::SettingDescriptor& a_descriptor)
		{
			auto fallback = a_descriptor.defaultValue;
			a_descriptor.showReset = false;
			a_descriptor.binding.get = [fallback] { return fallback; };
			a_descriptor.binding.set =
				[fallback](dmui::SettingValue) { return fallback; };
		}

		struct LocalToggleState
		{
			std::unordered_map<int64_t, bool> values;
			std::unordered_set<std::string> ownedRows;
		};

		void BindLocalToggle(
			dmui::SettingDescriptor& a_descriptor,
			int64_t a_control,
			std::shared_ptr<LocalToggleState> a_state)
		{
			a_descriptor.showReset = false;
			a_descriptor.binding.get =
				[a_control, a_state]() -> dmui::SettingValue {
					return a_state->values.at(a_control);
				};
			a_descriptor.binding.set =
				[a_control, a_state](dmui::SettingValue a_value)
					-> dmui::SettingValue {
					auto& current = a_state->values.at(a_control);
					if (const auto* value = std::get_if<bool>(&a_value))
						current = *value;
					return current;
				};
		}

		void BindSupported(
			dmui::SettingDescriptor& a_descriptor,
			const MappedBinding& a_binding,
			ValueSource& a_source,
			MappedRow& a_row,
			const std::optional<SliderNormalization>& a_sliderNormalization)
		{
			auto fallback = a_descriptor.defaultValue;

			// A mismatched alternative throws out of the host's draw.
			const auto matched =
				[](const ValueSnapshot& a_snapshot,
					const dmui::SettingValue& a_fallback)
					-> const dmui::SettingValue* {
					const auto* ready = std::get_if<ReadyValue>(&a_snapshot);
					return ready &&
							ready->value.index() == a_fallback.index() ?
						&ready->value :
						nullptr;
				};

			a_descriptor.binding.get =
				[&a_source,
				 a_binding,
				 fallback,
				 matched]() -> dmui::SettingValue {
					const auto current = a_source.Read(a_binding);
					const auto* value = matched(current, fallback);
					return value ? *value : fallback;
				};
			a_row.writeValue =
				[&a_source,
				 a_binding,
				 fallback,
				 matched,
				 a_sliderNormalization](
					dmui::SettingValue a_value,
					ValueWriteCompletion a_completion) -> dmui::SettingValue {
					if (a_sliderNormalization)
						a_value = detail::NormalizeSliderValue(
							*a_sliderNormalization,
							std::move(a_value));
					auto complete =
						[fallback,
						 completion = std::move(a_completion)](
							ValueWriteResult a_result) mutable {
							if (!completion)
								return;
							if (!a_result)
							{
								completion(std::unexpected(
									std::move(a_result.error())));
								return;
							}
							if (a_result->index() != fallback.index())
							{
								completion(std::unexpected(
									"written value has the wrong type"));
								return;
							}
							completion(std::move(*a_result));
						};
					const auto effective = a_source.Write(
						a_binding,
						a_value,
						std::move(complete));
					const auto* value = matched(effective, fallback);
					return value ? *value : fallback;
				};
			a_descriptor.binding.set =
				[writeValue = a_row.writeValue](
					dmui::SettingValue a_value) mutable -> dmui::SettingValue {
					return writeValue(std::move(a_value), {});
				};
		}

		using Dependencies = std::unordered_map<int64_t, MappedBinding>;

		[[nodiscard]] Dependencies BuildDependencies(
			const std::vector<MappedRow>& a_rows)
		{
			Dependencies result;
			for (const auto& row : a_rows)
			{
				if (row.groupControl && row.binding)
					result.insert_or_assign(*row.groupControl, *row.binding);
			}
			return result;
		}

		[[nodiscard]] bool IsBindingOperable(
			const MappedBinding& a_binding,
			const ValueSource& a_source) noexcept
		{
			if (!a_source.Supports(a_binding.Family()))
				return false;
			const auto* setting =
				std::get_if<ModSettingBinding>(&a_binding.source);
			return !setting ||
				setting->declaration != DeclarationState::kUndeclared;
		}

		[[nodiscard]] InertReason SnapshotReason(
			const ValueSnapshot& a_snapshot,
			const dmui::SettingValue& a_target) noexcept
		{
			if (const auto* ready = std::get_if<ReadyValue>(&a_snapshot))
				return ready->value.index() == a_target.index() ?
					InertReason::kNone :
					InertReason::kValueFailed;
			if (std::holds_alternative<PendingValue>(a_snapshot))
				return InertReason::kValuePending;
			if (std::holds_alternative<MissingValue>(a_snapshot))
				return InertReason::kValueMissing;
			return InertReason::kValueFailed;
		}

		[[nodiscard]] std::optional<bool> LocalToggleDefault(
			const MappedRow& a_row,
			const std::unordered_set<int64_t>& a_referencedControls,
			const dmui::SettingsPage& a_page,
			const ValueSource& a_source)
		{
			if (!a_row.groupControl)
				return std::nullopt;
			const auto* descriptor =
				FindSettingDescriptor(a_page, a_row.id);
			if (!descriptor ||
				!std::holds_alternative<dmui::CheckboxSettingControl>(
					descriptor->control))
				return std::nullopt;
			if (a_row.valueRoute == ValueRoute::kLocalUiState &&
				!a_row.binding)
			{
				// The mapper sees references that presentation-only controls do not retain.
				if (const auto* value =
						std::get_if<bool>(&descriptor->defaultValue))
					return *value;
				return std::nullopt;
			}
			if (!a_referencedControls.contains(*a_row.groupControl) ||
				!a_row.binding ||
				IsBindingOperable(*a_row.binding, a_source) ||
				!std::holds_alternative<ModSettingBinding>(
					a_row.binding->source))
				return std::nullopt;
			const auto* value = std::get_if<bool>(&a_row.binding->target);
			return value ? std::optional<bool>{ *value } : std::nullopt;
		}

		[[nodiscard]] LocalToggleState BuildLocalToggleState(
			const std::vector<MappedRow>& a_rows,
			const dmui::SettingsPage& a_page,
			const ValueSource& a_source)
		{
			LocalToggleState result;
			const auto referencedControls =
				detail::BuildReferencedControls(a_rows);
			for (const auto& row : a_rows)
			{
				const auto initial = LocalToggleDefault(
					row,
					referencedControls,
					a_page,
					a_source);
				if (!initial)
					continue;
				result.values.insert_or_assign(
					*row.groupControl,
					*initial);
				result.ownedRows.insert(row.id);
			}
			return result;
		}

		[[nodiscard]] ConditionResult EvaluateRowCondition(
			const GroupCondition& a_condition,
			const Dependencies& a_dependencies,
			const ValueSource& a_source,
			const LocalToggleState& a_localToggles)
		{
			return EvaluateCondition(
				a_condition,
				[&](int64_t a_control) -> ValueSnapshot {
					if (const auto local = a_localToggles.values.find(a_control);
						local != a_localToggles.values.end())
						return ReadyValue{ local->second };
					const auto found = a_dependencies.find(a_control);
					return found == a_dependencies.end() ||
							!IsBindingOperable(found->second, a_source) ?
						ValueSnapshot{ MissingValue{} } :
						a_source.Read(found->second);
				});
		}

		struct ConditionStateSummary
		{
			size_t pendingConditions{};
			size_t unevaluableConditions{};
			size_t visibleRows{};
		};

		[[nodiscard]] ConditionStateSummary SummarizeConditionState(
			const std::vector<MappedRow>& a_rows,
			const Dependencies& a_dependencies,
			const ValueSource& a_source,
			const LocalToggleState& a_localToggles)
		{
			ConditionStateSummary result;
			for (const auto& row : a_rows)
			{
				if (!row.emitted)
					continue;
				if (!row.groupCondition)
				{
					++result.visibleRows;
					continue;
				}
				const auto condition = EvaluateRowCondition(
					*row.groupCondition,
					a_dependencies,
					a_source,
					a_localToggles);
				if (condition == ConditionResult::kPending)
					++result.pendingConditions;
				else if (condition == ConditionResult::kUnavailable)
				{
					++result.unevaluableConditions;
					++result.visibleRows;
				}
				else if (condition == ConditionResult::kVisible)
					++result.visibleRows;
			}
			return result;
		}

		struct ConditionNotes
		{
			bool pending{};
			bool unavailable{};
		};

		void RemoveNote(
			dmui::SettingsPage& a_page,
			std::string_view a_id)
		{
			std::erase_if(
				a_page.notes,
				[a_id](const dmui::SettingsPageNote& a_note) {
					return a_note.noteId == a_id;
				});
		}

		void UpdateConditionNotes(
			dmui::SettingsPage& a_page,
			const ConditionStateSummary& a_summary,
			ConditionNotes& a_notes)
		{
			constexpr auto kPendingNote = "dearmodding.mcm.condition.pending";
			constexpr auto kUnavailableNote =
				"dearmodding.mcm.condition.unavailable";
			const auto loading =
				a_summary.visibleRows == 0 && a_summary.pendingConditions != 0;
			if (!loading)
			{
				RemoveNote(a_page, kPendingNote);
				a_notes.pending = false;
			}
			if (!a_summary.unevaluableConditions)
			{
				RemoveNote(a_page, kUnavailableNote);
				a_notes.unavailable = false;
			}
			if (loading && !a_notes.pending)
			{
				a_page.notes.push_back({
					"Loading setting visibility…",
					true,
					kPendingNote
				});
				a_notes.pending = true;
			}
			if (a_summary.unevaluableConditions && !a_notes.unavailable)
			{
				a_page.notes.push_back({
					std::format(
						"Compatibility: {} condition{} could not be applied "
						"because a controller is inoperable or its value is "
						"missing or failed. "
						"Dependent content is shown.",
						a_summary.unevaluableConditions,
						a_summary.unevaluableConditions == 1 ? "" : "s"),
					false,
					kUnavailableNote
				});
				a_notes.unavailable = true;
			}
		}

		void UpdateInertNote(
			dmui::SettingsPage& a_page,
			const std::vector<std::function<ResolvedInertState()>>& a_resolvers)
		{
			constexpr auto kNoteId = "dearmodding.mcm.availability";
			RemoveNote(a_page, kNoteId);
			std::vector<InertReason> reasons;
			for (const auto& resolve : a_resolvers)
			{
				const auto reason = resolve().governingReason;
				if (Describe(reason).scope == InertReasonScope::kEnvironment &&
					!std::ranges::contains(reasons, reason))
					reasons.push_back(reason);
			}
			std::string text;
			for (const auto reason : reasons)
			{
				if (!text.empty())
					text.push_back('\n');
				text.append(Describe(reason).pageText);
			}
			if (!text.empty())
				a_page.notes.push_back({ std::move(text), false, kNoteId });
		}
	}

	void BindPage(
		MappedPage& a_page,
		ValueSource& a_source)
	{
		BindPage(
			a_page,
			a_source,
			[] { return McmState{ true, true }; });
	}

	void BindPage(
		MappedPage& a_page,
		ValueSource& a_source,
		McmStateResolver a_resolveState)
	{
		auto inertResolvers =
			std::make_shared<
				std::vector<std::function<ResolvedInertState()>>>();
		const auto dependencies = std::make_shared<const Dependencies>(
			BuildDependencies(a_page.rows));
		const auto localToggles = std::make_shared<LocalToggleState>(
			BuildLocalToggleState(a_page.rows, a_page.settings, a_source));
		a_page.localUiStateRows = localToggles->ownedRows.size();
		for (auto& row : a_page.rows)
		{
			if (localToggles->ownedRows.contains(row.id))
				row.valueRoute = ValueRoute::kLocalUiState;
		}
		auto priorPrepare = std::move(a_page.settings.prepareView);
		a_page.settings.prepareView =
			[&a_source,
			 rows = a_page.rows,
			 dependencies,
			 localToggles,
			 priorPrepare = std::move(priorPrepare),
			 notes = ConditionNotes{}](dmui::SettingsPage& a_settings) mutable {
				if (priorPrepare)
					priorPrepare(a_settings);
				UpdateConditionNotes(
					a_settings,
					SummarizeConditionState(
						rows,
						*dependencies,
						a_source,
						*localToggles),
					notes);
			};

		for (auto& row : a_page.rows)
		{
			auto* descriptor =
				FindSettingDescriptor(a_page.settings, row.id);
			std::function<ConditionResult()> resolveCondition;
			if (row.groupCondition)
			{
				resolveCondition =
					[&a_source,
					 condition = *row.groupCondition,
					 dependencies,
					 localToggles] {
						return EvaluateRowCondition(
							condition,
							*dependencies,
							a_source,
							*localToggles);
					};
				const auto bindVisibility = [&resolveCondition](
					std::function<bool()>& a_visible) {
					a_visible = [resolve = resolveCondition] {
						const auto result = resolve();
						return result != ConditionResult::kHidden &&
							result != ConditionResult::kPending;
					};
				};
				if (descriptor)
					bindVisibility(descriptor->isVisible);
				else if (auto* action = FindActionRow(a_page.settings, row.id))
					bindVisibility(action->isVisible);
			}
			const auto sourceSupported =
				row.binding && a_source.Supports(row.binding->Family());
			const auto undeclared = row.binding &&
				std::get_if<ModSettingBinding>(&row.binding->source) &&
				std::get<ModSettingBinding>(row.binding->source).declaration ==
					DeclarationState::kUndeclared;
			row.resolveInertState =
				[&a_source,
				 binding = row.binding,
				 route = row.valueRoute,
				 unsupported = row.unsupported || row.unmappedSource.has_value(),
				 sourceSupported,
				 undeclared,
				 keybindInertState = row.keybindInertState,
				 actionInertReason = row.actionInertReason,
				 fileChoices = row.fileChoiceState,
				 resolveCondition,
				 resolveState = a_resolveState]() -> ResolvedInertState {
					if (resolveCondition)
					{
						const auto condition = resolveCondition();
						if (condition == ConditionResult::kHidden)
							return ResolvedInertState{
								InertReason::kConditionFalse,
								InertReason::kConditionFalse
							};
						if (condition == ConditionResult::kPending)
							return ResolvedInertState{
								InertReason::kConditionPending,
								InertReason::kConditionPending
							};
					}
					if (route == ValueRoute::kLocalUiState)
						return ResolvedInertState{
							InertReason::kNone,
							actionInertReason.value_or(InertReason::kNone)
						};
					if (keybindInertState)
						return *keybindInertState;
					const auto bindingReason =
						unsupported || (binding && !sourceSupported) ?
							InertReason::kUnsupported :
							(undeclared ?
								InertReason::kUndeclaredModSetting :
								InertReason::kNone);
					const auto rowReason =
						bindingReason != InertReason::kNone ?
							bindingReason :
							actionInertReason.value_or(InertReason::kNone);
					if (!binding)
						return {
							rowReason,
							rowReason
						};
					const auto state = resolveState();
					const auto environmentReason = ResolveControlInertReason(
						state,
						binding->Family(),
						route);
					if (environmentReason != InertReason::kNone)
						return ResolvedInertState{
							environmentReason,
							rowReason
						};
					if (bindingReason != InertReason::kNone)
						return ResolvedInertState{
							bindingReason,
							bindingReason
						};
					const auto snapshotReason = SnapshotReason(
						a_source.Read(*binding),
						binding->target);
					if (snapshotReason != InertReason::kNone)
						return ResolvedInertState{
							snapshotReason,
							snapshotReason
						};
					if (fileChoices)
					{
						const auto fileReason = fileChoices->Reason();
						if (fileReason != InertReason::kNone)
							return ResolvedInertState{
								fileReason,
								fileReason
							};
					}
					return ResolvedInertState{
						InertReason::kNone,
						rowReason
					};
				};
			inertResolvers->push_back(row.resolveInertState);
			if (descriptor)
			{
				auto resolve = row.resolveInertState;
				descriptor->isEnabled = [resolve] {
					return resolve().governingReason == InertReason::kNone;
				};
				auto priorDescription =
					std::move(descriptor->resolveDescription);
				const auto description = descriptor->description;
				descriptor->resolveDescription =
					[resolve,
					 fileChoices = row.fileChoiceState,
					 priorDescription = std::move(priorDescription),
					 description] {
						auto result = priorDescription ?
							priorDescription() :
							description;
						const auto reason = resolve().rowReason;
						const auto detailed =
							fileChoices &&
								reason == InertReason::kFileChoicesFailed ?
							fileChoices->Description() :
							std::string{};
						const auto explanation = detailed.empty() ?
							std::string{ Describe(reason).text } :
							detailed;
						if (!explanation.empty())
						{
							if (!result.empty())
								result.push_back('\n');
							result.append(explanation);
						}
						return result;
					};
			}
			else if (auto* action = FindActionRow(a_page.settings, row.id))
			{
				auto resolve = row.resolveInertState;
				action->isEnabled = [resolve] {
					return resolve().governingReason == InertReason::kNone;
				};
				const auto explanation =
					Describe(resolve().rowReason).text;
				if (!explanation.empty())
				{
					if (!action->description.empty())
						action->description.push_back('\n');
					action->description.append(explanation);
				}
			}
			if (row.groupControl &&
				localToggles->ownedRows.contains(row.id))
			{
				if (descriptor)
					BindLocalToggle(
						*descriptor,
						*row.groupControl,
						localToggles);
				continue;
			}
			if (!row.binding)
				continue;
			const auto& binding = *row.binding;
			if (!descriptor)
				descriptor =
					FindSettingDescriptor(
						a_page.settings,
						binding.descriptorId);
			if (!descriptor)
				continue;
			if (!IsBindingOperable(binding, a_source))
			{
				BindUnsupported(*descriptor);
				continue;
			}
			BindSupported(
				*descriptor,
				binding,
				a_source,
				row,
				row.sliderNormalization);
		}

		auto priorPrepareView = std::move(a_page.settings.prepareView);
		a_page.settings.prepareView =
			[priorPrepareView = std::move(priorPrepareView),
			 inertResolvers](dmui::SettingsPage& a_settings) {
				if (priorPrepareView)
					priorPrepareView(a_settings);
				UpdateInertNote(a_settings, *inertResolvers);
			};

	}
}
