#include <DearModdingUI/ExternalOpen.h>

#include <Windows.h>
#include <shellapi.h>

#include <cctype>
#include <string_view>

namespace DearModdingUI
{
	namespace
	{
		inline constexpr size_t kExternalValueCapacity{ 32767 };
		inline constexpr uint32_t kExternalArgumentCapacity{ 128 };

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
	}

	ExternalOpener::ExternalOpener(ExternalOpenDispatch a_dispatch) noexcept :
		m_dispatch(a_dispatch ? a_dispatch : &DispatchExternalOpen)
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
		if (a_descriptor->targetKind > DMUI_EXTERNAL_TARGET_DIRECTORY)
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
					request.targetKind == DMUI_EXTERNAL_TARGET_DIRECTORY) &&
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
