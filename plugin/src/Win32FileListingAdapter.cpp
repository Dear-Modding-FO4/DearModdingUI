#include <DearModdingUI/MCM/Win32FileListingAdapter.h>

#include <Windows.h>

#include <limits>
#include <system_error>
#include <utility>

namespace DearModdingUI::MCM
{
	namespace
	{
		[[nodiscard]] std::expected<std::wstring, std::string> ToWide(
			std::string_view a_value,
			std::string_view a_name)
		{
			if (a_value.size() >
				static_cast<size_t>((std::numeric_limits<int>::max)()))
			{
				return std::unexpected(
					std::string{ a_name } + " is too long");
			}
			if (a_value.empty())
				return std::unexpected(
					std::string{ a_name } + " is empty");
			if (a_value.find('\0') != std::string_view::npos)
			{
				return std::unexpected(
					std::string{ a_name } + " contains an embedded NUL");
			}
			const auto size = ::MultiByteToWideChar(
				CP_UTF8,
				MB_ERR_INVALID_CHARS,
				a_value.data(),
				static_cast<int>(a_value.size()),
				nullptr,
				0);
			if (size <= 0)
			{
				return std::unexpected(
					std::string{ a_name } + " is not valid UTF-8");
			}
			std::wstring result(static_cast<size_t>(size), L'\0');
			if (::MultiByteToWideChar(
					CP_UTF8,
					MB_ERR_INVALID_CHARS,
					a_value.data(),
					static_cast<int>(a_value.size()),
					result.data(),
					size) != size)
			{
				return std::unexpected(
					std::string{ a_name } + " could not be converted");
			}
			return result;
		}

		[[nodiscard]] std::expected<std::string, std::string> ToUtf8(
			std::wstring_view a_value)
		{
			if (a_value.empty())
				return std::string{};
			if (a_value.size() >
				static_cast<size_t>((std::numeric_limits<int>::max)()))
				return std::unexpected("a filename is too long");
			const auto size = ::WideCharToMultiByte(
				CP_UTF8,
				WC_ERR_INVALID_CHARS,
				a_value.data(),
				static_cast<int>(a_value.size()),
				nullptr,
				0,
				nullptr,
				nullptr);
			if (size <= 0)
				return std::unexpected("a filename is not valid Unicode");
			std::string result(static_cast<size_t>(size), '\0');
			if (::WideCharToMultiByte(
					CP_UTF8,
					WC_ERR_INVALID_CHARS,
					a_value.data(),
					static_cast<int>(a_value.size()),
					result.data(),
					size,
					nullptr,
					nullptr) != size)
				return std::unexpected("a filename could not be converted");
			return result;
		}

		[[nodiscard]] bool IsEmptyListingError(DWORD a_error) noexcept
		{
			return a_error == ERROR_FILE_NOT_FOUND ||
				a_error == ERROR_PATH_NOT_FOUND ||
				a_error == ERROR_NO_MORE_FILES;
		}

		[[nodiscard]] std::string WindowsError(DWORD a_error)
		{
			return std::system_category().message(
				static_cast<int>(a_error)) +
				" (Windows error " + std::to_string(a_error) + ")";
		}

		class FindHandle
		{
		public:
			explicit FindHandle(HANDLE a_handle) noexcept :
				handle_(a_handle)
			{}

			~FindHandle()
			{
				if (handle_ != INVALID_HANDLE_VALUE)
					::FindClose(handle_);
			}

			FindHandle(const FindHandle&) = delete;
			FindHandle& operator=(const FindHandle&) = delete;

		private:
			HANDLE handle_;
		};
	}

	FileListingResult Win32FileListingAdapter::List(
		std::string_view a_path,
		std::string_view a_mask)
	{
		const auto path = ToWide(a_path, "path");
		if (!path)
			return std::unexpected(path.error());
		const auto mask = ToWide(a_mask, "mask");
		if (!mask)
			return std::unexpected(mask.error());

		auto wildcard = *path;
		wildcard.push_back(L'\\');
		wildcard.append(*mask);
		WIN32_FIND_DATAW data{};
		const auto raw = ::FindFirstFileW(wildcard.c_str(), &data);
		if (raw == INVALID_HANDLE_VALUE)
		{
			const auto error = ::GetLastError();
			return IsEmptyListingError(error) ?
				FileListingResult{ std::vector<std::string>{} } :
				FileListingResult{ std::unexpected(WindowsError(error)) };
		}

		FindHandle handle{ raw };
		std::vector<std::string> result;
		for (;;)
		{
			auto name = ToUtf8(data.cFileName);
			if (!name)
				return std::unexpected(name.error());
			result.push_back(std::move(*name));
			if (::FindNextFileW(raw, &data))
				continue;
			const auto error = ::GetLastError();
			if (error == ERROR_NO_MORE_FILES)
				return result;
			return std::unexpected(WindowsError(error));
		}
	}
}
