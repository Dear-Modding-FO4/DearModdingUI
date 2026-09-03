#pragma once

#include <DearModdingUI/MCM/CachedAsyncValueSource.h>
#include <DearModdingUI/MCM/PapyrusDispatcher.h>
#include <DearModdingUI/MCM/TaskScheduler.h>

#include <string>

namespace DearModdingUI::MCM
{
	class ModSettingValueSource final : public CachedAsyncValueSource
	{
	public:
		ModSettingValueSource(
			std::string a_modName,
			McmEventDispatcher& a_events,
			TaskScheduler& a_scheduler,
			PapyrusDispatcher& a_dispatcher);

		[[nodiscard]] bool Supports(
			SourceFamily a_family) const noexcept override;
		[[nodiscard]] uint64_t Refresh(
			const MappedBinding& a_binding) override;
		[[nodiscard]] ValueSnapshot Write(
			const MappedBinding& a_binding,
			const dmui::SettingValue& a_value) override;

	private:
		std::string modName_;
		McmEventDispatcher& events_;
		TaskScheduler& scheduler_;
		PapyrusDispatcher& dispatcher_;
	};
}
