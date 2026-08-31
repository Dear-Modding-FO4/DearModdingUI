#include <Support/Runtime.h>

#include <Windows.h>

#include <array>
#include <cassert>

namespace Addictol::Support
{
	std::string GetRuntimePath() noexcept
	{
		static const std::string path = []() {
			std::array<char, 4096> buffer{};
			const auto length = GetModuleFileNameA(
				GetModuleHandleA(nullptr),
				buffer.data(),
				static_cast<DWORD>(buffer.size()));
			assert(length != 0 && length < buffer.size());
			return length && length < buffer.size() ?
				std::string{ buffer.data(), length } :
				std::string{};
		}();
		return path;
	}

	std::string GetRuntimeDirectory() noexcept
	{
		static const std::string directory = []() {
			const auto path = GetRuntimePath();
			const auto lastSlash = path.rfind('\\');
			return lastSlash != std::string::npos ?
				path.substr(0, lastSlash + 1) :
				std::string{};
		}();
		return directory;
	}
}
