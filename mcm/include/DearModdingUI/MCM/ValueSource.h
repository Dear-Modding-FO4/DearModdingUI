#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

#include <optional>

namespace DearModdingUI::MCM
{
	class ValueSource
	{
	public:
		ValueSource() = default;
		virtual ~ValueSource() = default;

		ValueSource(const ValueSource&) = delete;
		ValueSource(ValueSource&&) = delete;
		ValueSource& operator=(const ValueSource&) = delete;
		ValueSource& operator=(ValueSource&&) = delete;

		[[nodiscard]] virtual bool Supports(
			SourceFamily a_family) const noexcept = 0;

		// Runs for every visible row every frame, so it must never dispatch or block.
		[[nodiscard]] virtual std::optional<dmui::SettingValue> Read(
			const MappedBinding& a_binding) const = 0;

		virtual void Refresh(const MappedBinding& a_binding) = 0;

		[[nodiscard]] virtual bool Write(
			const MappedBinding& a_binding,
			const dmui::SettingValue& a_value) = 0;
	};

	// The source must outlive the page, whose descriptors capture it by reference.
	void BindPage(MappedPage& a_page, ValueSource& a_source);
}
