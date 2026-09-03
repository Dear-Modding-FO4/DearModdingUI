#pragma once

#include <DearModdingUI/MCM/ValueSource.h>

#include <RE/T/TESGlobal.h>

#include <string>
#include <unordered_map>

namespace DearModdingUI::MCM
{
	class GlobalValueSource final : public ValueSource
	{
	public:
		[[nodiscard]] bool Supports(
			SourceFamily a_family) const noexcept override;

		[[nodiscard]] std::optional<dmui::SettingValue> Read(
			const MappedBinding& a_binding) const noexcept override;

		void Refresh(const MappedBinding& a_binding) noexcept override;

		[[nodiscard]] bool Write(
			const MappedBinding& a_binding,
			const dmui::SettingValue& a_value) noexcept override;

	private:
		[[nodiscard]] RE::TESGlobal* Find(
			const MappedBinding& a_binding) const noexcept;

		std::unordered_map<std::string, RE::TESGlobal*> globals_;
	};
}
