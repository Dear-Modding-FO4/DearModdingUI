#pragma once

#include <string>

namespace Addictol::Support
{
	[[nodiscard]] std::string GetRuntimePath() noexcept;
	[[nodiscard]] std::string GetRuntimeDirectory() noexcept;

	// Trim from the start (left trim)
	void LeftTrim(std::string& s) noexcept;
	// Trim from the end (right trim)
	void RightTrim(std::string& s) noexcept;
	// Trim from both ends
	void Trim(std::string& s) noexcept;
	// Trim from the start (left trim)
	void LeftTrim(std::wstring& s) noexcept;
	// Trim from the end (right trim)
	void RightTrim(std::wstring& s) noexcept;
	// Trim from both ends
	void Trim(std::wstring& s) noexcept;

	enum class Encoding : int8_t
	{
		Unknown = 0,
		UTF8_BOM,
		UTF16_LE,
		UTF16_BE,
		UTF32_LE,
		UTF32_BE
	};

	Encoding CheckBom(const std::string& filename) noexcept;

	std::string WideToSysChar(const std::wstring& s) noexcept;
	std::wstring SysCharToWide(const std::string& s) noexcept;
}
