#include <Support/Runtime.h>

#include <Windows.h>

#include <array>
#include <fstream>
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

	constexpr static std::string	WHITESPACEA = " \n\r\t\f\v";
	constexpr static std::wstring	WHITESPACEW = L" \n\r\t\f\v";

	// Trim from the start (left trim)
	void LeftTrim(std::string& s) noexcept
	{
		size_t start = s.find_first_not_of(WHITESPACEA);
		s.erase(0, start);
	}

	// Trim from the end (right trim)
	void RightTrim(std::string& s) noexcept
	{
		size_t end = s.find_last_not_of(WHITESPACEA);
		if (end != std::string::npos)
			s.erase(end + 1);
	}

	// Trim from both ends
	void Trim(std::string& s) noexcept
	{
		RightTrim(s);
		LeftTrim(s);
	}

	// Trim from the start (left trim)
	void LeftTrim(std::wstring& s) noexcept
	{
		size_t start = s.find_first_not_of(WHITESPACEW);
		s.erase(0, start);
	}

	// Trim from the end (right trim)
	void RightTrim(std::wstring& s) noexcept
	{
		size_t end = s.find_last_not_of(WHITESPACEW);
		if (end != std::wstring::npos)
			s.erase(end + 1);
	}

	// Trim from both ends
	void Trim(std::wstring& s) noexcept
	{
		RightTrim(s);
		LeftTrim(s);
	}

	Encoding CheckBom(const std::string& filename) noexcept
	{
		std::ifstream file(filename, std::ios::binary);
		if (!file) return Encoding::Unknown;

		// Read the first 4 bytes
		unsigned char bytes[4] = { 0 };
		file.read(reinterpret_cast<char*>(bytes), 4);
		std::streamsize bytes_read = file.gcount();

		if (bytes_read >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
			return Encoding::UTF8_BOM;

		if (bytes_read >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE)
		{
			// Could be UTF-32 LE if followed by 00 00, but usually UTF-16 LE
			if (bytes_read == 4 && bytes[2] == 0x00 && bytes[3] == 0x00) return Encoding::UTF32_LE;
			return Encoding::UTF16_LE;
		}

		if (bytes_read >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF)
			return Encoding::UTF16_BE;

		if (bytes_read == 4 && bytes[0] == 0x00 && bytes[1] == 0x00 && bytes[2] == 0xFE && bytes[3] == 0xFF)
			return Encoding::UTF32_BE;

		return Encoding::Unknown; // No BOM found
	}

	std::string WideToSysChar(const std::wstring& s) noexcept
	{
		if (s.empty() || !s.length())
			return "";

		auto w2mb = [](const wchar_t* a_src, int32_t a_srcLen, char* a_dst = nullptr, int32_t a_dstLen = 0)
			{
				return REX::W32::WideCharToMultiByte(CP_ACP, 0, a_src, a_srcLen, a_dst, a_dstLen, nullptr, nullptr);
			};

		auto len = w2mb(s.c_str(), static_cast<int32_t>(s.length()));
		if (len > 0)
		{
			auto buf = std::make_unique<char[]>((size_t)len + 1);
			std::fill_n(buf.get(), (size_t)len + 1, 0);
			w2mb(s.c_str(), static_cast<int32_t>(s.length()), buf.get(), len);
			return buf.get();
		}

		return "";
	}

	std::wstring SysCharToWide(const std::string& s) noexcept
	{
		if (s.empty() || !s.length())
			return L"";

		auto mb2w = [](const char* a_src, std::int32_t a_srcLen, wchar_t* a_dst = nullptr, std::int32_t a_dstLen = 0)
			{
				return REX::W32::MultiByteToWideChar(CP_ACP, 0, a_src, a_srcLen, a_dst, a_dstLen);
			};

		int len = mb2w(s.c_str(), static_cast<int32_t>(s.length()));
		if (len > 0)
		{
			auto buf = std::make_unique<wchar_t[]>((size_t)len + 1);
			std::fill_n(buf.get(), (size_t)len + 1, 0);
			mb2w(s.c_str(), static_cast<int32_t>(s.length()), buf.get(), len);
			return buf.get();
		}

		return L"";
	}
}
