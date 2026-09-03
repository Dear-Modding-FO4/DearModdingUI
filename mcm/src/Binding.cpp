#include <DearModdingUI/MCM/ValueSource.h>

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

		void BindUnsupported(dmui::SettingDescriptor& a_descriptor)
		{
			auto fallback = a_descriptor.defaultValue;
			a_descriptor.showReset = false;
			a_descriptor.isEnabled = [] { return false; };
			a_descriptor.binding.get = [fallback] { return fallback; };
			a_descriptor.binding.set =
				[fallback](dmui::SettingValue) { return fallback; };
		}

		void BindSupported(
			dmui::SettingDescriptor& a_descriptor,
			const MappedBinding& a_binding,
			ValueSource& a_source)
		{
			auto fallback = a_descriptor.defaultValue;

			// A mismatched alternative throws out of the host's draw.
			const auto matched =
				[](const std::optional<dmui::SettingValue>& a_value,
					const dmui::SettingValue& a_fallback) {
					return a_value && a_value->index() == a_fallback.index();
				};

			a_descriptor.binding.get =
				[&a_source, a_binding, fallback, matched]() -> dmui::SettingValue {
					auto value = a_source.Read(a_binding);
					return matched(value, fallback) ?
						std::move(*value) :
						fallback;
				};
			a_descriptor.binding.set =
				[&a_source, a_binding, fallback, matched](
					dmui::SettingValue a_value) -> dmui::SettingValue {
					if (a_source.Write(a_binding, a_value))
						return a_value;
					auto current = a_source.Read(a_binding);
					return matched(current, fallback) ?
						std::move(*current) :
						fallback;
				};
		}
	}

	void BindPage(MappedPage& a_page, ValueSource& a_source)
	{
		for (const auto& binding : a_page.bindings)
		{
			auto* descriptor =
				FindDescriptor(a_page.settings, binding.descriptorId);
			if (!descriptor)
				continue;
			if (a_source.Supports(binding.source.family))
				BindSupported(*descriptor, binding, a_source);
			else
				BindUnsupported(*descriptor);
		}
	}
}
