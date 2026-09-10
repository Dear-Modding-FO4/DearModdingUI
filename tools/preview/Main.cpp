#include "PreviewApplication.h"
#include "PreviewOptions.h"

#include <Windows.h>
#include <shellapi.h>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <utility>

namespace
{
	void PrintHRESULTError(const wchar_t* a_operation, HRESULT a_result)
	{
		std::wcerr
			<< L"dmui-preview: " << a_operation << L" failed (0x"
			<< std::hex << static_cast<uint32_t>(a_result) << L").\n";
	}
}

int main()
{
	using namespace DearModdingUIPreview;

	int argumentCount{};
	auto** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
	if (!arguments)
	{
		std::wcerr << L"dmui-preview: could not read the command line.\n";
		return 1;
	}

	PreviewOptions options;
	std::wstring error;
	const auto parsed = ParsePreviewOptions(
		argumentCount,
		arguments,
		options,
		error);
	LocalFree(arguments);
	if (!parsed)
	{
		std::wcerr << L"dmui-preview: " << error << L'\n';
		PrintPreviewUsage();
		return 2;
	}
	if (options.help)
	{
		PrintPreviewUsage();
		return 0;
	}

	const auto com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(com))
	{
		PrintHRESULTError(L"CoInitializeEx", com);
		return 1;
	}
	const auto result = PreviewApplication{ std::move(options) }.Run();
	CoUninitialize();
	return result;
}
