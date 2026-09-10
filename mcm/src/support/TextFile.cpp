#include "TextFile.h"

#include <exception>
#include <fstream>
#include <sstream>

namespace DearModdingUI::MCM::detail
{
	TextFileResult ReadTextFile(
		const std::filesystem::path& a_path) noexcept
	{
		try
		{
			std::ifstream stream{ a_path, std::ios::binary };
			if (!stream)
				return {};
			std::ostringstream buffer;
			buffer << stream.rdbuf();
			if (stream.bad())
				return { TextFileStatus::kFailed };
			return {
				TextFileStatus::kLoaded,
				buffer.str()
			};
		}
		catch (const std::exception& a_error)
		{
			return {
				TextFileStatus::kFailed,
				{},
				a_error.what()
			};
		}
		catch (...)
		{
			return { TextFileStatus::kFailed };
		}
	}
}
