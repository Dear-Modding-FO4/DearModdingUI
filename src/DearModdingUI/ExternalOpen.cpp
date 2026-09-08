#include <DearModdingUI/ExternalOpen.h>

#include <Windows.h>
#include <psapi.h>
#include <shellapi.h>

#include <cctype>
#include <memory>
#include <new>
#include <string_view>

namespace DearModdingUI
{
	namespace
	{
		inline constexpr size_t kExternalValueCapacity{ 32767 };
		inline constexpr uint32_t kExternalArgumentCapacity{ 128 };
		inline constexpr DWORD kWindowsPathCapacity{ 32768 };

		[[nodiscard]] DMUI_Result ResolutionError(
			DWORD a_error,
			uint32_t* a_nativeError,
			DMUI_Result a_result = DMUI_RESULT_EXTERNAL_RESOLUTION_FAILED) noexcept
		{
			if (a_nativeError)
				*a_nativeError = a_error;
			return a_result;
		}

		[[nodiscard]] bool ReadUtf8(
			const char* a_value,
			bool a_optional,
			std::string& a_output)
		{
			if (!a_value)
				return a_optional;
			size_t length{};
			while (length <= kExternalValueCapacity && a_value[length])
				++length;
			if (length > kExternalValueCapacity ||
				(!a_optional && length == 0))
				return false;
			if (length != 0 &&
				MultiByteToWideChar(
					CP_UTF8,
					MB_ERR_INVALID_CHARS,
					a_value,
					static_cast<int>(length),
					nullptr,
					0) == 0)
				return false;
			a_output.assign(a_value, length);
			return true;
		}

		[[nodiscard]] bool IsAbsoluteWindowsPath(
			std::string_view a_value) noexcept
		{
			return (a_value.size() >= 3 &&
					std::isalpha(static_cast<unsigned char>(a_value[0])) &&
					a_value[1] == ':' &&
					(a_value[2] == '\\' || a_value[2] == '/')) ||
				(a_value.size() >= 3 &&
					(a_value[0] == '\\' || a_value[0] == '/') &&
					(a_value[1] == '\\' || a_value[1] == '/') &&
					a_value[2] != '\\' && a_value[2] != '/');
		}

		[[nodiscard]] bool IsUri(std::string_view a_value) noexcept
		{
			if (a_value.empty() ||
				!std::isalpha(static_cast<unsigned char>(a_value.front())))
				return false;
			for (size_t index = 1; index < a_value.size(); ++index)
			{
				const auto character =
					static_cast<unsigned char>(a_value[index]);
				if (character == ':')
					return true;
				if (!std::isalnum(character) &&
					character != '+' && character != '-' && character != '.')
					return false;
			}
			return false;
		}

		[[nodiscard]] bool IsFilesystemPath(std::wstring_view a_path) noexcept
		{
			const auto isDrivePath = [](std::wstring_view path) {
				return path.size() >= 3 &&
					((path[0] >= L'A' && path[0] <= L'Z') ||
						(path[0] >= L'a' && path[0] <= L'z')) &&
					path[1] == L':' && path[2] == L'\\';
			};
			if (a_path.starts_with(L"\\\\?\\"))
			{
				a_path.remove_prefix(4);
				if (a_path.starts_with(L"UNC\\"))
					a_path.remove_prefix(4);
				else
					return isDrivePath(a_path);
			}
			else if (isDrivePath(a_path))
				return true;
			else if (a_path.starts_with(L"\\\\"))
				a_path.remove_prefix(2);
			else
				return false;
			if (a_path.empty() || a_path[0] == L'.' || a_path[0] == L'?')
				return false;
			const auto share = a_path.find(L'\\');
			return share != std::wstring_view::npos && share > 0 &&
				share + 1 < a_path.size() && a_path[share + 1] != L'\\';
		}

		[[nodiscard]] std::wstring Utf16(std::string_view a_value)
		{
			if (a_value.empty())
				return {};
			const auto size = MultiByteToWideChar(
				CP_UTF8,
				MB_ERR_INVALID_CHARS,
				a_value.data(),
				static_cast<int>(a_value.size()),
				nullptr,
				0);
			std::wstring result(static_cast<size_t>(size), L'\0');
			(void)MultiByteToWideChar(
				CP_UTF8,
				MB_ERR_INVALID_CHARS,
				a_value.data(),
				static_cast<int>(a_value.size()),
				result.data(),
				size);
			return result;
		}

		[[nodiscard]] bool SameWindowsText(
			std::wstring_view a_left,
			std::wstring_view a_right) noexcept
		{
			return a_left.size() == a_right.size() &&
				CompareStringOrdinal(
					a_left.data(), static_cast<int>(a_left.size()),
					a_right.data(), static_cast<int>(a_right.size()), TRUE) == CSTR_EQUAL;
		}

		[[nodiscard]] bool HasPathPrefix(
			std::wstring_view a_path,
			std::wstring_view a_prefix) noexcept
		{
			return a_path.size() > a_prefix.size() &&
				a_path[a_prefix.size()] == L'\\' &&
				SameWindowsText(a_path.substr(0, a_prefix.size()), a_prefix);
		}

		[[nodiscard]] DMUI_Result ExternalPathFromMappedName(
			std::wstring_view a_mappedName,
			std::wstring& a_path,
			uint32_t* a_nativeError)
		{
			constexpr std::wstring_view mup{ L"\\Device\\Mup" };
			if (HasPathPrefix(a_mappedName, mup))
			{
				const auto networkPath = a_mappedName.substr(mup.size() + 1);
				const auto share = networkPath.find(L'\\');
				const auto file = share == std::wstring_view::npos ?
					std::wstring_view::npos : networkPath.find(L'\\', share + 1);
				if (share == 0 || file == std::wstring_view::npos ||
					file == share + 1 || file + 1 == networkPath.size() ||
					networkPath.front() == L';')
					return ResolutionError(ERROR_NOT_SUPPORTED, a_nativeError,
						DMUI_RESULT_EXTERNAL_RESOLUTION_UNSUPPORTED);
				a_path = L"\\\\";
				a_path.append(networkPath);
				return DMUI_RESULT_OK;
			}

			wchar_t volume[MAX_PATH]{};
			const auto rawSearch = FindFirstVolumeW(volume, MAX_PATH);
			if (rawSearch == INVALID_HANDLE_VALUE)
				return ResolutionError(GetLastError(), a_nativeError);
			const std::unique_ptr<void, decltype(&FindVolumeClose)> search{
				rawSearch, &FindVolumeClose
			};
			DWORD queryError{};
			do
			{
				std::wstring deviceName{ volume };
				deviceName.erase(0, 4);
				deviceName.pop_back();
				std::vector<wchar_t> device(256);
				DWORD length{};
				for (;;)
				{
					length = QueryDosDeviceW(
						deviceName.c_str(), device.data(), static_cast<DWORD>(device.size()));
					if (length != 0)
						break;
					const auto error = GetLastError();
					if (error != ERROR_INSUFFICIENT_BUFFER ||
						device.size() >= kWindowsPathCapacity)
					{
						queryError = error;
						break;
					}
					device.resize(device.size() * 2);
				}
				if (length == 0 ||
					!HasPathPrefix(a_mappedName, std::wstring_view{ device.data() }))
					continue;

				DWORD capacity{};
				if (!GetVolumePathNamesForVolumeNameW(volume, nullptr, 0, &capacity) &&
					GetLastError() != ERROR_MORE_DATA)
					return ResolutionError(GetLastError(), a_nativeError);
				std::vector<wchar_t> paths;
				for (;;)
				{
					if (capacity == 0 || capacity > kWindowsPathCapacity)
						return ResolutionError(ERROR_FILENAME_EXCED_RANGE, a_nativeError,
							DMUI_RESULT_EXTERNAL_RESOLUTION_UNSUPPORTED);
					paths.assign(capacity, L'\0');
					if (GetVolumePathNamesForVolumeNameW(
							volume, paths.data(), static_cast<DWORD>(paths.size()), &capacity))
						break;
					const auto error = GetLastError();
					if (error != ERROR_MORE_DATA)
						return ResolutionError(error, a_nativeError);
				}
				std::wstring_view mount;
				for (size_t offset = 0; offset < paths.size() && paths[offset] != L'\0';)
				{
					const std::wstring_view candidate{ paths.data() + offset };
					if (mount.empty() || candidate.size() < mount.size() ||
						(candidate.size() == mount.size() && candidate < mount))
						mount = candidate;
					offset += candidate.size() + 1;
				}
				if (mount.empty())
					return ResolutionError(ERROR_PATH_NOT_FOUND, a_nativeError,
						DMUI_RESULT_EXTERNAL_RESOLUTION_UNSUPPORTED);
				a_path = mount;
				a_path.append(a_mappedName.substr(
					std::wstring_view{ device.data() }.size() + 1));
				return DMUI_RESULT_OK;
			}
			while (FindNextVolumeW(search.get(), volume, MAX_PATH));
			const auto error = GetLastError();
			if (error != ERROR_NO_MORE_FILES)
				return ResolutionError(error, a_nativeError);
			if (queryError != 0)
				return ResolutionError(queryError, a_nativeError);
			return ResolutionError(ERROR_NOT_SUPPORTED, a_nativeError,
				DMUI_RESULT_EXTERNAL_RESOLUTION_UNSUPPORTED);
		}
	}

	DMUI_Result ResolveExternalFile(
		std::string_view a_virtualFile,
		std::string& a_physicalFile,
		uint32_t* a_nativeError) noexcept
	{
		if (a_nativeError)
			*a_nativeError = 0;
		if (a_virtualFile.size() > kExternalValueCapacity ||
			a_virtualFile.find('\0') != std::string_view::npos ||
			!IsAbsoluteWindowsPath(a_virtualFile))
			return DMUI_RESULT_INVALID_DESCRIPTOR;
		try
		{
			auto virtualFile = Utf16(a_virtualFile);
			if (virtualFile.empty())
				return ResolutionError(ERROR_NO_UNICODE_TRANSLATION, a_nativeError);
			for (auto& character : virtualFile)
				if (character == L'/')
					character = L'\\';
			if (!IsFilesystemPath(virtualFile))
				return DMUI_RESULT_INVALID_DESCRIPTOR;
			const auto rawFile = CreateFileW(
				virtualFile.c_str(), GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
			if (rawFile == INVALID_HANDLE_VALUE)
				return ResolutionError(GetLastError(), a_nativeError);
			const std::unique_ptr<void, decltype(&CloseHandle)> file{ rawFile, &CloseHandle };
			SetLastError(ERROR_SUCCESS);
			const auto fileType = GetFileType(file.get());
			if (fileType != FILE_TYPE_DISK)
			{
				const auto error = fileType == FILE_TYPE_UNKNOWN ? GetLastError() : ERROR_SUCCESS;
				if (error != ERROR_SUCCESS)
					return ResolutionError(error, a_nativeError);
				return ResolutionError(ERROR_NOT_SUPPORTED, a_nativeError,
					DMUI_RESULT_EXTERNAL_RESOLUTION_UNSUPPORTED);
			}
			FILE_STANDARD_INFO info{};
			if (!GetFileInformationByHandleEx(file.get(), FileStandardInfo, &info, sizeof(info)))
				return ResolutionError(GetLastError(), a_nativeError);
			if (info.Directory || info.EndOfFile.QuadPart == 0)
				return ResolutionError(info.Directory ? ERROR_DIRECTORY : ERROR_FILE_INVALID,
					a_nativeError, DMUI_RESULT_EXTERNAL_RESOLUTION_UNSUPPORTED);

			const std::unique_ptr<void, decltype(&CloseHandle)> mapping{
				CreateFileMappingW(file.get(), nullptr, PAGE_READONLY, 0, 0, nullptr),
				&CloseHandle
			};
			if (!mapping)
				return ResolutionError(GetLastError(), a_nativeError);
			const std::unique_ptr<void, decltype(&UnmapViewOfFile)> view{
				MapViewOfFile(mapping.get(), FILE_MAP_READ, 0, 0, 1), &UnmapViewOfFile
			};
			if (!view)
				return ResolutionError(GetLastError(), a_nativeError);
			std::wstring mappedName(256, L'\0');
			for (;;)
			{
				// USVFS rewrites handle names, but not the backing section name.
				const auto length = K32GetMappedFileNameW(
					GetCurrentProcess(), view.get(), mappedName.data(),
					static_cast<DWORD>(mappedName.size()));
				if (length == 0)
					return ResolutionError(GetLastError(), a_nativeError);
				if (length < mappedName.size())
				{
					mappedName.resize(length);
					break;
				}
				if (mappedName.size() >= kWindowsPathCapacity)
					return ResolutionError(ERROR_FILENAME_EXCED_RANGE, a_nativeError,
						DMUI_RESULT_EXTERNAL_RESOLUTION_UNSUPPORTED);
				mappedName.resize(mappedName.size() * 2);
			}
			std::wstring physicalFile;
			const auto translated =
				ExternalPathFromMappedName(mappedName, physicalFile, a_nativeError);
			if (translated != DMUI_RESULT_OK)
				return translated;
			const auto size = WideCharToMultiByte(
				CP_UTF8, WC_ERR_INVALID_CHARS, physicalFile.data(),
				static_cast<int>(physicalFile.size()), nullptr, 0, nullptr, nullptr);
			if (size == 0)
				return ResolutionError(GetLastError(), a_nativeError);
			std::string result(static_cast<size_t>(size), '\0');
			if (WideCharToMultiByte(
					CP_UTF8, WC_ERR_INVALID_CHARS, physicalFile.data(),
					static_cast<int>(physicalFile.size()), result.data(), size,
					nullptr, nullptr) == 0)
				return ResolutionError(GetLastError(), a_nativeError);
			a_physicalFile = std::move(result);
			return DMUI_RESULT_OK;
		}
		catch (const std::bad_alloc&)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	ExternalOpener::ExternalOpener(
		ExternalOpenDispatch a_dispatch,
		ExternalFileResolver a_resolveFile) noexcept :
		m_dispatch(a_dispatch ? a_dispatch : &DispatchExternalOpen),
		m_resolveFile(a_resolveFile ? a_resolveFile : &ResolveExternalFile)
	{}

	DMUI_Result ExternalOpener::Open(
		const DMUI_ExternalOpenDescriptor* a_descriptor,
		uint32_t* a_nativeError) const noexcept
	{
		if (a_nativeError)
			*a_nativeError = 0;
		ExternalOpenRequest request;
		const auto result =
			ValidateExternalOpenDescriptor(a_descriptor, request);
		if (result != DMUI_RESULT_OK)
			return result;
		if (request.targetKind == DMUI_EXTERNAL_TARGET_VIRTUAL_FILE ||
			request.targetKind == DMUI_EXTERNAL_TARGET_VIRTUAL_FILE_PARENT)
		{
			std::string physicalFile;
			const auto resolved =
				m_resolveFile(request.target, physicalFile, a_nativeError);
			if (resolved != DMUI_RESULT_OK)
				return resolved;
			if (!IsAbsoluteWindowsPath(physicalFile))
				return ResolutionError(ERROR_BAD_PATHNAME, a_nativeError);
			if (request.targetKind == DMUI_EXTERNAL_TARGET_VIRTUAL_FILE_PARENT)
			{
				const auto separator = physicalFile.find_last_of("\\/");
				if (separator == std::string::npos)
				{
					if (a_nativeError)
						*a_nativeError = ERROR_BAD_PATHNAME;
					return DMUI_RESULT_EXTERNAL_RESOLUTION_FAILED;
				}
				physicalFile.resize(separator + 1);
				request.targetKind = DMUI_EXTERNAL_TARGET_DIRECTORY;
			}
			else
				request.targetKind = DMUI_EXTERNAL_TARGET_FILE;
			request.target = std::move(physicalFile);
		}
		return m_dispatch(request, a_nativeError);
	}

	DMUI_Result ValidateExternalOpenDescriptor(
		const DMUI_ExternalOpenDescriptor* a_descriptor,
		ExternalOpenRequest& a_request) noexcept
	{
		if (!a_descriptor)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_descriptor->structSize < DMUI_EXTERNAL_OPEN_DESCRIPTOR_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (a_descriptor->reserved != 0 ||
			a_descriptor->argumentCount > kExternalArgumentCapacity ||
			(a_descriptor->argumentCount != 0 && !a_descriptor->arguments))
			return DMUI_RESULT_INVALID_DESCRIPTOR;
		if (a_descriptor->targetKind > DMUI_EXTERNAL_TARGET_VIRTUAL_FILE_PARENT)
			return DMUI_RESULT_INVALID_DESCRIPTOR;

		try
		{
			ExternalOpenRequest request{};
			request.targetKind = a_descriptor->targetKind;
			if (!ReadUtf8(a_descriptor->target, true, request.target) ||
				!ReadUtf8(a_descriptor->application, true, request.application) ||
				!ReadUtf8(
					a_descriptor->workingDirectory,
					true,
					request.workingDirectory))
				return DMUI_RESULT_INVALID_DESCRIPTOR;
			for (uint32_t index = 0; index < a_descriptor->argumentCount; ++index)
			{
				std::string argument;
				if (!a_descriptor->arguments[index] ||
					!ReadUtf8(
						a_descriptor->arguments[index],
						true,
						argument))
					return DMUI_RESULT_INVALID_DESCRIPTOR;
				request.arguments.push_back(std::move(argument));
			}

			const auto hasTarget = request.targetKind != DMUI_EXTERNAL_TARGET_NONE;
			if (hasTarget != !request.target.empty())
				return DMUI_RESULT_INVALID_DESCRIPTOR;
			if (request.targetKind == DMUI_EXTERNAL_TARGET_URI &&
				!IsUri(request.target))
				return DMUI_RESULT_INVALID_DESCRIPTOR;
			if ((request.targetKind == DMUI_EXTERNAL_TARGET_FILE ||
					request.targetKind == DMUI_EXTERNAL_TARGET_DIRECTORY ||
					request.targetKind == DMUI_EXTERNAL_TARGET_VIRTUAL_FILE ||
					request.targetKind == DMUI_EXTERNAL_TARGET_VIRTUAL_FILE_PARENT) &&
				!IsAbsoluteWindowsPath(request.target))
				return DMUI_RESULT_INVALID_DESCRIPTOR;
			if (request.application.empty())
			{
				if (!hasTarget || !request.arguments.empty() ||
					!request.workingDirectory.empty())
					return DMUI_RESULT_INVALID_DESCRIPTOR;
			}
			else
			{
				if (!IsAbsoluteWindowsPath(request.application) ||
					(!request.workingDirectory.empty() &&
						!IsAbsoluteWindowsPath(request.workingDirectory)))
					return DMUI_RESULT_INVALID_DESCRIPTOR;
			}
			a_request = std::move(request);
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	std::wstring QuoteWindowsArgument(std::wstring_view a_argument)
	{
		std::wstring result{ L'"' };
		size_t backslashes{};
		for (const auto character : a_argument)
		{
			if (character == L'\\')
			{
				++backslashes;
				continue;
			}
			if (character == L'"')
			{
				result.append(backslashes * 2 + 1, L'\\');
				result.push_back(L'"');
				backslashes = 0;
				continue;
			}
			result.append(backslashes, L'\\');
			backslashes = 0;
			result.push_back(character);
		}
		result.append(backslashes * 2, L'\\');
		result.push_back(L'"');
		return result;
	}

	DMUI_Result DispatchExternalOpen(
		const ExternalOpenRequest& a_request,
		uint32_t* a_nativeError) noexcept
	{
		if (a_nativeError)
			*a_nativeError = 0;
		try
		{
			if (a_request.application.empty())
			{
				const auto target = Utf16(a_request.target);
				SHELLEXECUTEINFOW execute{};
				execute.cbSize = sizeof(execute);
				execute.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
				execute.lpVerb = L"open";
				execute.lpFile = target.c_str();
				execute.nShow = SW_SHOWNORMAL;
				if (!ShellExecuteExW(&execute))
				{
					if (a_nativeError)
						*a_nativeError = GetLastError();
					return DMUI_RESULT_EXTERNAL_OPEN_FAILED;
				}
				if (execute.hProcess)
					CloseHandle(execute.hProcess);
				return DMUI_RESULT_OK;
			}

			const auto application = Utf16(a_request.application);
			std::wstring commandLine = QuoteWindowsArgument(application);
			for (const auto& argument : a_request.arguments)
			{
				commandLine.push_back(L' ');
				commandLine.append(QuoteWindowsArgument(Utf16(argument)));
			}
			if (a_request.targetKind != DMUI_EXTERNAL_TARGET_NONE)
			{
				commandLine.push_back(L' ');
				commandLine.append(QuoteWindowsArgument(Utf16(a_request.target)));
			}
			auto workingDirectory = Utf16(a_request.workingDirectory);
			STARTUPINFOW startup{};
			startup.cb = sizeof(startup);
			PROCESS_INFORMATION process{};
			if (!CreateProcessW(
					application.c_str(),
					commandLine.data(),
					nullptr,
					nullptr,
					FALSE,
					0,
					nullptr,
					workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
					&startup,
					&process))
			{
				if (a_nativeError)
					*a_nativeError = GetLastError();
				return DMUI_RESULT_EXTERNAL_OPEN_FAILED;
			}
			CloseHandle(process.hThread);
			CloseHandle(process.hProcess);
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}
}
