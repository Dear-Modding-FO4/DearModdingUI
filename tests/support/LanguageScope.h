#pragma once

#include <DearModdingUI/localization/Localization.h>

#include <string_view>

namespace vmm_tests::support
{
	// The catalog is process-wide; later suites expect the default language.
	class LanguageScope
	{
	public:
		explicit LanguageScope(std::string_view a_language)
		{
			DearModdingUI::Localization::SetLanguage(a_language);
		}

		~LanguageScope()
		{
			DearModdingUI::Localization::SetLanguage({});
		}

		LanguageScope(const LanguageScope&) = delete;
		LanguageScope& operator=(const LanguageScope&) = delete;
	};
}
