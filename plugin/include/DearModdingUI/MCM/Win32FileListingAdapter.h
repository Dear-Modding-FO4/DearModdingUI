#pragma once

#include <DearModdingUI/MCM/FileChoices.h>

namespace DearModdingUI::MCM
{
	class Win32FileListingAdapter final : public FileListingAdapter
	{
	public:
		[[nodiscard]] FileListingResult List(
			std::string_view a_path,
			std::string_view a_mask) override;
	};
}
