#pragma once

#include <DearModdingUI/MCM/ValueSource.h>

#include <RE/T/TESGlobal.h>

#include <mutex>
#include <string>
#include <unordered_map>

namespace DearModdingUI::MCM
{
	class GlobalValueSource final : public ValueSource
	{
	public:
		[[nodiscard]] bool Supports(
			SourceFamily a_family) const noexcept override;

		[[nodiscard]] ValueSnapshot Read(
			const MappedBinding& a_binding) const noexcept override;

		[[nodiscard]] uint64_t Refresh(
			const MappedBinding& a_binding) noexcept override;

		[[nodiscard]] ValueSnapshot Write(
			const MappedBinding& a_binding,
			const dmui::SettingValue& a_value) noexcept override;

	private:
		[[nodiscard]] RE::TESGlobal* Find(
			const MappedBinding& a_binding) const noexcept;

		struct Entry
		{
			RE::TESGlobal* global{};
			uint64_t generation{};
		};

		[[nodiscard]] const GlobalBinding* Global(
			const MappedBinding& a_binding) const noexcept;

		mutable std::mutex mutex_;
		std::unordered_map<std::string, Entry> globals_;
	};
}
