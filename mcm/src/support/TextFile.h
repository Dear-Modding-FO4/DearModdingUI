#pragma once

#include <filesystem>
#include <string>

namespace DearModdingUI::MCM::detail
{
	enum class TextFileStatus
	{
		kLoaded,
		kMissing,
		kFailed
	};

	struct TextFileResult
	{
		TextFileStatus status{ TextFileStatus::kMissing };
		std::string text;
		std::string error;
	};

	[[nodiscard]] TextFileResult ReadTextFile(
		const std::filesystem::path& a_path) noexcept;
}
