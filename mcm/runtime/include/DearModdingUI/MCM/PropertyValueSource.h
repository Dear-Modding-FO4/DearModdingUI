#pragma once

#include <DearModdingUI/MCM/CachedAsyncValueSource.h>
#include <DearModdingUI/MCM/TaskScheduler.h>

#include <RE/B/BSScript_Object.h>
#include <RE/B/BSTSmartPointer.h>

#include <mutex>
#include <string>
#include <unordered_map>

namespace DearModdingUI::MCM
{
	class PropertyValueSource final : public CachedAsyncValueSource
	{
	public:
		PropertyValueSource(
			TaskScheduler& a_scheduler,
			DiagnosticReporter& a_diagnostics);

		[[nodiscard]] bool Supports(
			SourceFamily a_family) const noexcept override;
		[[nodiscard]] uint64_t Refresh(
			const MappedBinding& a_binding) override;
		[[nodiscard]] ValueSnapshot Write(
			const MappedBinding& a_binding,
			const dmui::SettingValue& a_value) override;
		[[nodiscard]] ValueSnapshot Write(
			const MappedBinding& a_binding,
			const dmui::SettingValue& a_value,
			ValueWriteCompletion a_completion) override;
	private:
		TaskScheduler& scheduler_;
		std::mutex objectMutex_;
		std::unordered_map<
			std::string,
			RE::BSTSmartPointer<RE::BSScript::Object>> objects_;
	};
}
