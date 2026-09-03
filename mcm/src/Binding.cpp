#include <DearModdingUI/MCM/ValueSource.h>

#include <format>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace DearModdingUI::MCM
{
	namespace
	{
		[[nodiscard]] dmui::SettingDescriptor* FindDescriptor(
			dmui::SettingsPage& a_page,
			const std::string& a_id)
		{
			for (auto& group : a_page.groups)
			{
				for (auto& setting : group.settings)
				{
					if (setting.id == a_id)
						return &setting;
				}
			}
			return nullptr;
		}

		[[nodiscard]] const dmui::SettingDescriptor* FindDescriptor(
			const dmui::SettingsPage& a_page,
			const std::string& a_id)
		{
			for (const auto& group : a_page.groups)
			{
				for (const auto& setting : group.settings)
				if (setting.id == a_id)
					return &setting;
			}
			return nullptr;
		}

		[[nodiscard]] dmui::SettingsActionRow* FindActionRow(
			dmui::SettingsPage& a_page,
			const std::string& a_id)
		{
			for (auto& group : a_page.groups)
			{
				for (auto& action : group.actionRows)
				if (action.id == a_id)
					return &action;
			}
			return nullptr;
		}

		void BindUnsupported(dmui::SettingDescriptor& a_descriptor)
		{
			auto fallback = a_descriptor.defaultValue;
			a_descriptor.showReset = false;
			a_descriptor.isEnabled = [] { return false; };
			a_descriptor.binding.get = [fallback] { return fallback; };
			a_descriptor.binding.set =
				[fallback](dmui::SettingValue) { return fallback; };
		}

		struct LocalToggleState
		{
			std::unordered_map<int64_t, bool> values;
		};

		void BindLocalToggle(
			dmui::SettingDescriptor& a_descriptor,
			int64_t a_control,
			std::shared_ptr<LocalToggleState> a_state)
		{
			a_descriptor.showReset = false;
			a_descriptor.isEnabled = [] { return true; };
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
			ValueSource& a_source)
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

			auto priorEnabled = std::move(a_descriptor.isEnabled);
			a_descriptor.isEnabled =
				[&a_source, a_binding, fallback, matched,
				 priorEnabled = std::move(priorEnabled)] {
					const auto current = a_source.Read(a_binding);
					return matched(current, fallback) &&
						(!priorEnabled || priorEnabled());
				};
			a_descriptor.binding.get =
				[&a_source, a_binding, fallback, matched]() -> dmui::SettingValue {
					const auto current = a_source.Read(a_binding);
					const auto* value = matched(current, fallback);
					return value ? *value : fallback;
				};
			a_descriptor.binding.set =
				[&a_source, a_binding, fallback, matched](
					dmui::SettingValue a_value) -> dmui::SettingValue {
					const auto effective = a_source.Write(a_binding, a_value);
					const auto* value = matched(effective, fallback);
					return value ? *value : fallback;
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

		void CollectReferencedControls(
			const GroupCondition& a_condition,
			std::unordered_set<int64_t>& a_controls)
		{
			if (a_condition.type == ConditionType::kControl)
				a_controls.insert(a_condition.control);
			for (const auto& operand : a_condition.operands)
				CollectReferencedControls(operand, a_controls);
		}

		[[nodiscard]] std::unordered_set<int64_t> BuildReferencedControls(
			const std::vector<MappedRow>& a_rows)
		{
			std::unordered_set<int64_t> result;
			for (const auto& row : a_rows)
				if (row.groupCondition)
					CollectReferencedControls(*row.groupCondition, result);
			return result;
		}

		[[nodiscard]] bool IsLocallyOwnableToggle(
			const MappedRow& a_row,
			const std::unordered_set<int64_t>& a_referencedControls,
			const dmui::SettingsPage& a_page,
			const ValueSource& a_source)
		{
			if (!a_row.groupControl || !a_row.binding ||
				!a_referencedControls.contains(*a_row.groupControl) ||
				IsBindingOperable(*a_row.binding, a_source) ||
				!std::holds_alternative<ModSettingBinding>(
					a_row.binding->source) ||
				!std::holds_alternative<bool>(a_row.binding->target))
				return false;
			const auto* descriptor = FindDescriptor(a_page, a_row.id);
			return descriptor &&
				std::holds_alternative<dmui::CheckboxSettingControl>(
					descriptor->control);
		}

		[[nodiscard]] LocalToggleState BuildLocalToggleState(
			const std::vector<MappedRow>& a_rows,
			const dmui::SettingsPage& a_page,
			const ValueSource& a_source)
		{
			LocalToggleState result;
			const auto referencedControls = BuildReferencedControls(a_rows);
			for (const auto& row : a_rows)
			{
				if (!IsLocallyOwnableToggle(
						row,
						referencedControls,
						a_page,
						a_source))
					continue;
				result.values.insert_or_assign(
					*row.groupControl,
					std::get<bool>(row.binding->target));
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

		[[nodiscard]] PageCompatibilitySummary SummarizeConditionState(
			const std::vector<MappedRow>& a_rows,
			const Dependencies& a_dependencies,
			const ValueSource& a_source,
			const LocalToggleState& a_localToggles)
		{
			PageCompatibilitySummary result;
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
			const PageCompatibilitySummary& a_summary,
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
	}

	uint64_t Generation(const ValueSnapshot& a_snapshot) noexcept
	{
		return std::visit(
			[](const auto& a_state) { return a_state.generation; },
			a_snapshot);
	}

	void ValueSource::RefreshPage(const MappedPage& a_page)
	{
		for (const auto& row : a_page.rows)
		{
			if (row.binding && Supports(row.binding->Family()))
				(void)Refresh(*row.binding);
		}
	}

	PageCompatibilitySummary SummarizeCompatibility(
		const MappedPage& a_page,
		const ValueSource& a_source)
	{
		auto result = SummarizeCompatibility(a_page);
		const auto dependencies = BuildDependencies(a_page.rows);
		const auto localToggles =
			BuildLocalToggleState(a_page.rows, a_page.settings, a_source);
		const auto conditions = SummarizeConditionState(
			a_page.rows,
			dependencies,
			a_source,
			localToggles);
		result.pendingConditions = conditions.pendingConditions;
		result.unevaluableConditions = conditions.unevaluableConditions;
		result.visibleRows = conditions.visibleRows;
		return result;
	}

	void CompositeValueSource::Add(ValueSource& a_source)
	{
		sources_.push_back(a_source);
	}

	bool CompositeValueSource::Supports(SourceFamily a_family) const noexcept
	{
		return Find(a_family) != nullptr;
	}

	ValueSnapshot CompositeValueSource::Read(
		const MappedBinding& a_binding) const
	{
		const auto* source = Find(a_binding.Family());
		return source ? source->Read(a_binding) : ValueSnapshot{ MissingValue{} };
	}

	uint64_t CompositeValueSource::Refresh(const MappedBinding& a_binding)
	{
		auto* source = Find(a_binding.Family());
		return source ? source->Refresh(a_binding) : 0;
	}

	ValueSnapshot CompositeValueSource::Write(
		const MappedBinding& a_binding,
		const dmui::SettingValue& a_value)
	{
		auto* source = Find(a_binding.Family());
		return source ?
			source->Write(a_binding, a_value) :
			ValueSnapshot{ MissingValue{} };
	}

	void CompositeValueSource::RefreshPage(const MappedPage& a_page)
	{
		for (const auto source : sources_)
			source.get().RefreshPage(a_page);
	}

	void CompositeValueSource::Pump() noexcept
	{
		for (const auto source : sources_)
			source.get().Pump();
	}

	ValueSource* CompositeValueSource::Find(SourceFamily a_family) const noexcept
	{
		for (const auto source : sources_)
		{
			if (source.get().Supports(a_family))
				return &source.get();
		}
		return nullptr;
	}

	void BindPage(MappedPage& a_page, ValueSource& a_source)
	{
		const auto dependencies = std::make_shared<const Dependencies>(
			BuildDependencies(a_page.rows));
		const auto localToggles = std::make_shared<LocalToggleState>(
			BuildLocalToggleState(a_page.rows, a_page.settings, a_source));
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

		for (const auto& row : a_page.rows)
		{
			auto* descriptor = FindDescriptor(a_page.settings, row.id);
			if (row.groupCondition)
			{
				const auto bindVisibility = [&](
					std::function<bool()>& a_visible) {
					auto priorVisible = std::move(a_visible);
					a_visible =
						[&a_source,
						 condition = *row.groupCondition,
						 dependencies,
						 localToggles,
						 priorVisible = std::move(priorVisible)] {
							const auto result = EvaluateRowCondition(
								condition,
								*dependencies,
								a_source,
								*localToggles);
							return result != ConditionResult::kHidden &&
								result != ConditionResult::kPending &&
								(!priorVisible || priorVisible());
						};
				};
				if (descriptor)
					bindVisibility(descriptor->isVisible);
				else if (auto* action = FindActionRow(a_page.settings, row.id))
					bindVisibility(action->isVisible);
			}
			if (!row.binding)
				continue;
			const auto& binding = *row.binding;
			if (!descriptor)
				descriptor =
					FindDescriptor(a_page.settings, binding.descriptorId);
			if (!descriptor)
				continue;
			if (row.groupControl &&
				localToggles->values.contains(*row.groupControl))
			{
				BindLocalToggle(
					*descriptor,
					*row.groupControl,
					localToggles);
				continue;
			}
			if (!IsBindingOperable(binding, a_source))
			{
				const auto* setting =
					std::get_if<ModSettingBinding>(&binding.source);
				if (setting &&
					setting->declaration == DeclarationState::kUndeclared)
				{
					if (!descriptor->description.empty())
						descriptor->description.push_back('\n');
					descriptor->description +=
						"This setting is not declared in MCM settings.ini.";
				}
				BindUnsupported(*descriptor);
				continue;
			}
			BindSupported(*descriptor, binding, a_source);
		}
	}
}
