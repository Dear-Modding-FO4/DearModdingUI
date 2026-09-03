#include <DearModdingUI/MCM/Availability.h>

#include <algorithm>
#include <optional>
#include <utility>

namespace DearModdingUI::MCM
{
	namespace
	{
		constexpr auto kAvailabilityNoteId =
			"dearmodding.mcm.availability";

		[[nodiscard]] dmui::SettingDescriptor* FindDescriptor(
			dmui::SettingsPage& a_page,
			std::string_view a_id)
		{
			for (auto& group : a_page.groups)
			{
				for (auto& setting : group.settings)
				if (setting.id == a_id)
					return &setting;
			}
			return nullptr;
		}

		void UpdateAvailabilityNote(
			dmui::SettingsPage& a_page,
			AvailabilityState a_availability)
		{
			std::erase_if(
				a_page.notes,
				[](const dmui::SettingsPageNote& a_note) {
					return a_note.noteId == kAvailabilityNoteId;
				});
			if (a_availability == AvailabilityState::kPresent)
				return;
			a_page.notes.push_back({
				a_availability == AvailabilityState::kAbsent ?
					"Mod Configuration Menu is not installed, so these values cannot be changed." :
					"Checking whether Mod Configuration Menu is installed. Mod-setting controls are disabled until the check completes.",
				a_availability == AvailabilityState::kUnknown,
				kAvailabilityNoteId
			});
		}
	}

	void ComposeMcmAvailability(
		MappedPage& a_page,
		AvailabilityResolver a_resolve)
	{
		auto requiresMcm = false;
		for (const auto& row : a_page.rows)
		{
			if (!row.binding)
				continue;
			const auto family = row.binding->Family();
			requiresMcm = requiresMcm ||
				(family == SourceFamily::kModSetting &&
					row.valueRoute == ValueRoute::kSource);
			auto* descriptor =
				FindDescriptor(a_page.settings, row.binding->descriptorId);
			if (!descriptor)
				continue;

			auto priorEnabled = std::move(descriptor->isEnabled);
			descriptor->isEnabled =
				[family,
				 route = row.valueRoute,
				 priorEnabled = std::move(priorEnabled),
				 resolve = a_resolve] {
					return IsControlOperable(resolve(), family, route) &&
						(!priorEnabled || priorEnabled());
				};

			auto priorDescription = std::move(descriptor->resolveDescription);
			descriptor->resolveDescription =
				[family,
				 route = row.valueRoute,
				 description = descriptor->description,
				 priorDescription = std::move(priorDescription),
				 resolve = a_resolve] {
					auto result = priorDescription ?
						priorDescription() :
						description;
					const auto reason =
						ControlUnavailableReason(resolve(), family, route);
					if (reason.empty())
						return result;
					if (!result.empty())
						result.push_back('\n');
					result.append(reason);
					return result;
				};
		}

		if (!requiresMcm)
			return;
		auto priorPrepare = std::move(a_page.settings.prepareView);
		a_page.settings.prepareView =
			[priorPrepare = std::move(priorPrepare),
			 resolve = std::move(a_resolve),
			 lastAvailability = std::optional<AvailabilityState>{}](
				dmui::SettingsPage& a_settings) mutable {
				if (priorPrepare)
					priorPrepare(a_settings);
				const auto availability = resolve();
				if (lastAvailability == availability)
					return;
				UpdateAvailabilityNote(a_settings, availability);
				lastAvailability = availability;
			};
	}
}
