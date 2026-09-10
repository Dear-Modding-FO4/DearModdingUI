#pragma once

#include "PreviewOptions.h"

#include <memory>

namespace DearModdingUIPreview
{
	class PreviewApplication final
	{
	public:
		explicit PreviewApplication(PreviewOptions a_options);
		~PreviewApplication();

		PreviewApplication(const PreviewApplication&) = delete;
		PreviewApplication(PreviewApplication&&) = delete;
		PreviewApplication& operator=(const PreviewApplication&) = delete;
		PreviewApplication& operator=(PreviewApplication&&) = delete;

		[[nodiscard]] int Run();

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
