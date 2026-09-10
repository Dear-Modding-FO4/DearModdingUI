#include "../support/DearModdingUITestSupport.h"
#include <Platform/files/ExternalOpen.h>
#include <DearModdingUI/host/Host.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;
	using namespace support::host;

	void run_external_open_checks(Runner& runner)
	{
		runner.test("external opening validates and dispatches typed targets", [] {
			s_externalOpenCalls = 0;
			s_externalResult = DMUI_RESULT_OK;
			s_externalNativeError = 0;
			const ExternalOpener opener{ &FakeExternalOpen };

			DMUI_ExternalOpenDescriptor descriptor{
				sizeof(DMUI_ExternalOpenDescriptor),
				DMUI_EXTERNAL_TARGET_URI,
				"https://example.invalid/docs"
			};
			require(opener.Open(&descriptor) == DMUI_RESULT_OK,
				"default URI dispatch failed");
			require(
				s_externalOpenCalls == 1 &&
					s_externalRequest.targetKind == DMUI_EXTERNAL_TARGET_URI &&
					s_externalRequest.target == "https://example.invalid/docs" &&
					s_externalRequest.application.empty(),
				"default URI did not use the associated-handler path");

			descriptor.targetKind = DMUI_EXTERNAL_TARGET_FILE;
			descriptor.target = "C:\\mods\\readme.txt";
			require(opener.Open(&descriptor) == DMUI_RESULT_OK,
				"absolute file dispatch failed");
			descriptor.targetKind = DMUI_EXTERNAL_TARGET_DIRECTORY;
			descriptor.target = "\\\\server\\mods";
			require(opener.Open(&descriptor) == DMUI_RESULT_OK,
				"absolute directory dispatch failed");

			const char* arguments[]{ "--line", "42", "" };
			descriptor.targetKind = DMUI_EXTERNAL_TARGET_FILE;
			descriptor.target = "C:\\mods\\settings.ini";
			descriptor.application = "C:\\Windows\\notepad.exe";
			descriptor.arguments = arguments;
			descriptor.argumentCount = static_cast<uint32_t>(std::size(arguments));
			descriptor.workingDirectory = "C:\\mods";
			require(opener.Open(&descriptor) == DMUI_RESULT_OK,
				"explicit application dispatch failed");
			require(
				s_externalRequest.application == "C:\\Windows\\notepad.exe" &&
					s_externalRequest.arguments.size() == 3 &&
					s_externalRequest.arguments.back().empty() &&
					s_externalRequest.target == "C:\\mods\\settings.ini" &&
					s_externalRequest.workingDirectory == "C:\\mods",
				"explicit application arguments were not preserved");

			descriptor.targetKind = DMUI_EXTERNAL_TARGET_NONE;
			descriptor.target = nullptr;
			descriptor.arguments = nullptr;
			descriptor.argumentCount = 0;
			descriptor.workingDirectory = nullptr;
			require(opener.Open(&descriptor) == DMUI_RESULT_OK,
				"application-only dispatch failed");

			s_externalResult = DMUI_RESULT_EXTERNAL_OPEN_FAILED;
			s_externalNativeError = 5;
			uint32_t nativeError{};
			require(
				opener.Open(&descriptor, &nativeError) ==
						DMUI_RESULT_EXTERNAL_OPEN_FAILED &&
					nativeError == 5 &&
					s_externalOpenCalls == 6,
				"platform dispatch failure was hidden or retried");
		});

		runner.test("external opening rejects ambiguous or unsafe descriptors", [] {
			ExternalOpenRequest request;
			DMUI_ExternalOpenDescriptor descriptor{
				sizeof(DMUI_ExternalOpenDescriptor),
				DMUI_EXTERNAL_TARGET_FILE,
				"relative.txt"
			};
			require(
				ValidateExternalOpenDescriptor(&descriptor, request) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"a relative file target was accepted");
			descriptor.targetKind = DMUI_EXTERNAL_TARGET_URI;
			descriptor.target = "not a URI";
			require(
				ValidateExternalOpenDescriptor(&descriptor, request) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"a URI without a scheme was accepted");
			descriptor.targetKind = DMUI_EXTERNAL_TARGET_NONE;
			descriptor.target = nullptr;
			require(
				ValidateExternalOpenDescriptor(&descriptor, request) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"an empty default-handler request was accepted");
			descriptor.application = "notepad.exe";
			require(
				ValidateExternalOpenDescriptor(&descriptor, request) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"a relative explicit application was accepted");
			descriptor.application = "C:\\Windows\\notepad.exe";
			descriptor.argumentCount = 129;
			require(
				ValidateExternalOpenDescriptor(&descriptor, request) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"an overflowing argument count was accepted");
			descriptor.argumentCount = 0;
			std::string oversized(32768, 'x');
			descriptor.targetKind = DMUI_EXTERNAL_TARGET_FILE;
			descriptor.target = oversized.c_str();
			require(
				ValidateExternalOpenDescriptor(&descriptor, request) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"an oversized target was accepted");
			const char invalidUtf8[]{ static_cast<char>(0xC3), '(', '\0' };
			descriptor.targetKind = DMUI_EXTERNAL_TARGET_URI;
			descriptor.target = invalidUtf8;
			require(
				ValidateExternalOpenDescriptor(&descriptor, request) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"malformed UTF-8 was accepted");
		});

		runner.test("virtual-file preflight requires advertised targets and opening entry", [] {
			auto api = PreflightHostAPI();
			api.queryServices = &MockQueryServices;
			api.openExternal = &MockOpenExternal;
			const dmui::ClientOptions options{
				.requiredServices = DMUI_HOST_SERVICE_VIRTUAL_FILE_TARGETS
			};
			s_mockServices = DMUI_HOST_SERVICE_EXTERNAL_OPEN;
			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_SERVICE_UNAVAILABLE,
				"ordinary external opening promised virtual-file resolution");
			s_mockServices |= DMUI_HOST_SERVICE_VIRTUAL_FILE_TARGETS;
			require(dmui::PreflightHostAPI(&api, options) == DMUI_RESULT_OK,
				"virtual-file service was not recognized");
			api.openExternal = nullptr;
			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_SERVICE_UNAVAILABLE,
				"virtual-file preflight omitted its dispatch entry");
			api.openExternal = &MockOpenExternal;
			api.structSize = DMUI_HOST_API_OPEN_EXTERNAL_SIZE - 1;
			require(dmui::PreflightHostAPI(&api, options) ==
					DMUI_RESULT_UNSUPPORTED_ABI,
				"truncated host table exposed the appended UI query");
		});

		runner.test("virtual targets resolve the file before dispatching either action", [] {
			s_externalOpenCalls = 0;
			s_externalResolveCalls = 0;
			s_externalResult = DMUI_RESULT_OK;
			s_externalNativeError = 0;
			s_externalResolveResult = DMUI_RESULT_OK;
			s_externalResolveError = 0;
			s_externalPhysicalFile = "D:\\mod library\\winner\\settings.ini";
			const ExternalOpener opener{ &FakeExternalOpen, &FakeExternalFileResolver };
			const char* arguments[]{ "--reuse-window" };
			DMUI_ExternalOpenDescriptor descriptor{
				sizeof(DMUI_ExternalOpenDescriptor),
				DMUI_EXTERNAL_TARGET_VIRTUAL_FILE,
				"C:\\game\\Data\\settings.ini",
				"C:\\Tools\\viewer.exe",
				arguments,
				1,
				0,
				"C:\\Tools"
			};
			uint32_t nativeError{ 999 };
			require(opener.Open(&descriptor, &nativeError) == DMUI_RESULT_OK &&
					nativeError == 0 && s_externalResolveCalls == 1 &&
					s_externalVirtualFile == descriptor.target &&
					s_externalRequest.target == s_externalPhysicalFile &&
					s_externalRequest.targetKind == DMUI_EXTERNAL_TARGET_FILE,
				"virtual file was not resolved before opening");
			require(s_externalRequest.application == descriptor.application &&
					s_externalRequest.workingDirectory == descriptor.workingDirectory &&
					s_externalRequest.arguments == std::vector<std::string>{ "--reuse-window" },
				"resolution rewrote application selection or arguments");
			descriptor.targetKind = DMUI_EXTERNAL_TARGET_VIRTUAL_FILE_PARENT;
			descriptor.application = nullptr;
			descriptor.arguments = nullptr;
			descriptor.argumentCount = 0;
			descriptor.workingDirectory = nullptr;
			require(opener.Open(&descriptor) == DMUI_RESULT_OK &&
					s_externalResolveCalls == 2 &&
					s_externalVirtualFile == descriptor.target &&
					s_externalRequest.target == "D:\\mod library\\winner\\" &&
					s_externalRequest.targetKind == DMUI_EXTERNAL_TARGET_DIRECTORY &&
					s_externalRequest.application.empty(),
				"containing-folder action resolved the virtual directory or changed the handler");
			s_externalPhysicalFile = "D:\\settings.ini";
			require(opener.Open(&descriptor) == DMUI_RESULT_OK &&
					s_externalRequest.target == "D:\\",
				"root-level parent became drive-relative");
			s_externalPhysicalFile = "\\\\server\\share\\settings.ini";
			require(opener.Open(&descriptor) == DMUI_RESULT_OK &&
					s_externalRequest.target == "\\\\server\\share\\",
				"UNC root-level parent lost its share");
			for (const auto kind : { DMUI_EXTERNAL_TARGET_FILE, DMUI_EXTERNAL_TARGET_DIRECTORY })
			{
				descriptor.targetKind = kind;
				require(opener.Open(&descriptor) == DMUI_RESULT_OK &&
						s_externalRequest.target == descriptor.target &&
						s_externalResolveCalls == 4,
					"ordinary path was implicitly resolved");
			}
		});

		runner.test("virtual resolution failure never dispatches an unresolved fallback", [] {
			s_externalOpenCalls = 0;
			s_externalResolveCalls = 0;
			s_externalResolveError = ERROR_ACCESS_DENIED;
			const ExternalOpener opener{ &FakeExternalOpen, &FakeExternalFileResolver };
			DMUI_ExternalOpenDescriptor descriptor{
				sizeof(DMUI_ExternalOpenDescriptor),
				DMUI_EXTERNAL_TARGET_VIRTUAL_FILE,
				"C:\\game\\Data\\settings.ini"
			};
			for (const auto kind :
				{ DMUI_EXTERNAL_TARGET_VIRTUAL_FILE, DMUI_EXTERNAL_TARGET_VIRTUAL_FILE_PARENT })
			{
				descriptor.targetKind = kind;
				for (const auto failure : { DMUI_RESULT_EXTERNAL_RESOLUTION_FAILED,
						 DMUI_RESULT_EXTERNAL_RESOLUTION_UNSUPPORTED })
				{
					s_externalResolveResult = failure;
					uint32_t nativeError{};
					require(opener.Open(&descriptor, &nativeError) == failure &&
							nativeError == ERROR_ACCESS_DENIED && s_externalOpenCalls == 0,
						"resolution failure was hidden or fell back to a virtual path");
				}
				descriptor.target = "relative.ini";
				require(opener.Open(&descriptor) == DMUI_RESULT_INVALID_DESCRIPTOR,
					"relative virtual file reached the resolver");
				descriptor.target = "C:\\game\\Data\\settings.ini";
			}
			require(s_externalResolveCalls == 4,
				"invalid virtual descriptor reached the resolver");
			s_externalResolveResult = DMUI_RESULT_OK;
			s_externalResolveError = 0;
			s_externalPhysicalFile = "D:\\winner\\settings.ini";
			s_externalResult = DMUI_RESULT_EXTERNAL_OPEN_FAILED;
			s_externalNativeError = ERROR_FILE_NOT_FOUND;
			uint32_t nativeError{};
			require(opener.Open(&descriptor, &nativeError) == DMUI_RESULT_EXTERNAL_OPEN_FAILED &&
					nativeError == ERROR_FILE_NOT_FOUND && s_externalOpenCalls == 1,
				"launch failure was mislabeled as resolution failure or retried");
		});

		runner.test("backing-file resolution preserves readable file contents and releases handles", [] {
			const ExternalFileFixture fixture{ "resolution must not rewrite this file" };
			require(SetFileAttributesW(fixture.path.c_str(), FILE_ATTRIBUTE_READONLY) != 0,
				"read-only fixture attribute could not be set");
			std::string physical;
			uint32_t nativeError{ 999 };
			const auto requested = fixture.Utf8();
			require(ResolveExternalFile(requested, physical, &nativeError) == DMUI_RESULT_OK &&
					nativeError == 0,
				"mapped-file resolution failed for a readable read-only file");
			require(std::filesystem::equivalent(
						fixture.path, std::filesystem::path{ std::u8string(physical.begin(), physical.end()) }),
				"resolved physical file identifies a different file");
			DWORD before{};
			DWORD after{};
			require(GetProcessHandleCount(GetCurrentProcess(), &before) != 0,
				"process handle count was unavailable");
			for (int iteration = 0; iteration < 16; ++iteration)
				require(ResolveExternalFile(requested, physical, &nativeError) == DMUI_RESULT_OK,
					"repeated resolution failed");
			require(GetProcessHandleCount(GetCurrentProcess(), &after) != 0 && after == before,
				"resolution leaked file, mapping, or volume handles");
			const auto exclusive = CreateFileW(
				fixture.path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
			require(exclusive != INVALID_HANDLE_VALUE, "resolution retained a file lease");
			CloseHandle(exclusive);
			std::ifstream stream{ fixture.path, std::ios::binary };
			const std::string contents{ std::istreambuf_iterator<char>{ stream }, {} };
			require(contents == "resolution must not rewrite this file" &&
					(GetFileAttributesW(fixture.path.c_str()) & FILE_ATTRIBUTE_READONLY) != 0,
				"resolution modified file contents or attributes");

			ExternalFileFixture unicodeFixture{ "unicode" };
			auto renamed = unicodeFixture.path;
			renamed += L"-\u00e9-\u6d4b\u8bd5";
			require(MoveFileW(unicodeFixture.path.c_str(), renamed.c_str()) != 0,
				"Unicode fixture rename failed");
			unicodeFixture.path = renamed;
			const auto unicodeRequested = "\\\\?\\" + unicodeFixture.Utf8();
			require(ResolveExternalFile(unicodeRequested, physical, nullptr) == DMUI_RESULT_OK,
				"Unicode extended path resolution failed");
			require(std::filesystem::equivalent(unicodeFixture.path,
						std::filesystem::path{ std::u8string(physical.begin(), physical.end()) }),
				"Unicode backing filename was corrupted");
			s_externalResult = DMUI_RESULT_OK;
			s_externalNativeError = 0;
			const ExternalOpener opener{ &FakeExternalOpen };
			const DMUI_ExternalOpenDescriptor descriptor{
				sizeof(DMUI_ExternalOpenDescriptor),
				DMUI_EXTERNAL_TARGET_VIRTUAL_FILE_PARENT,
				unicodeRequested.c_str()
			};
			require(opener.Open(&descriptor) == DMUI_RESULT_OK &&
					s_externalRequest.targetKind == DMUI_EXTERNAL_TARGET_DIRECTORY &&
					std::filesystem::equivalent(unicodeFixture.path.parent_path(),
						std::filesystem::path{ std::u8string(
							s_externalRequest.target.begin(), s_externalRequest.target.end()) }),
				"real resolution did not reach fake dispatch with the physical parent");
		});

		runner.test("backing-file failures preserve output and never create or extend a file", [] {
			const ExternalFileFixture fixture{ "" };
			const auto requested = fixture.Utf8();
			std::string physical{ "unchanged" };
			uint32_t nativeError{};
			require(ResolveExternalFile(requested, physical, &nativeError) ==
						DMUI_RESULT_EXTERNAL_RESOLUTION_UNSUPPORTED &&
					nativeError == ERROR_FILE_INVALID && physical == "unchanged" &&
					std::filesystem::file_size(fixture.path) == 0,
				"empty file was extended or reported as successfully resolved");
			const auto missing = requested + ".missing";
			require(ResolveExternalFile(missing, physical, &nativeError) ==
						DMUI_RESULT_EXTERNAL_RESOLUTION_FAILED &&
					nativeError == ERROR_FILE_NOT_FOUND && physical == "unchanged" &&
					!std::filesystem::exists(fixture.path.wstring() + L".missing"),
				"missing file was created or resolution failure lost its native error");
			const auto parentText = fixture.path.parent_path().u8string();
			const std::string parent{ parentText.begin(), parentText.end() };
			require(ResolveExternalFile(parent, physical, &nativeError) ==
						DMUI_RESULT_EXTERNAL_RESOLUTION_UNSUPPORTED &&
					nativeError == ERROR_DIRECTORY && physical == "unchanged",
				"a directory was accepted as a backing file");
			for (const auto* device : { "\\\\.\\pipe\\dmui-never-open",
					 "\\\\?\\GLOBALROOT\\Device\\HarddiskVolume1\\anything",
					 "\\\\?\\pipe\\dmui-never-open" })
				require(ResolveExternalFile(device, physical, &nativeError) ==
						DMUI_RESULT_INVALID_DESCRIPTOR && nativeError == 0,
					"a device namespace reached file opening");

			const ExternalFileFixture unreadable{ "sharing violation" };
			const auto handle = CreateFileW(
				unreadable.path.c_str(),
				GENERIC_READ,
				0,
				nullptr,
				OPEN_EXISTING,
				0,
				nullptr);
			require(handle != INVALID_HANDLE_VALUE, "exclusive file fixture could not be opened");
			const std::unique_ptr<void, decltype(&CloseHandle)> exclusive{ handle, &CloseHandle };
			const auto unreadablePath = unreadable.Utf8();
			s_externalOpenCalls = 0;
			const ExternalOpener opener{ &FakeExternalOpen };
			DMUI_ExternalOpenDescriptor descriptor{
				sizeof(DMUI_ExternalOpenDescriptor),
				DMUI_EXTERNAL_TARGET_VIRTUAL_FILE,
				unreadablePath.c_str()
			};
			for (const auto kind :
				{ DMUI_EXTERNAL_TARGET_VIRTUAL_FILE, DMUI_EXTERNAL_TARGET_VIRTUAL_FILE_PARENT })
			{
				descriptor.targetKind = kind;
				uint32_t nativeError{};
				require(opener.Open(&descriptor, &nativeError) ==
							DMUI_RESULT_EXTERNAL_RESOLUTION_FAILED &&
						nativeError == ERROR_SHARING_VIOLATION && s_externalOpenCalls == 0,
					"unreadable backing file was dispatched or lost its Windows error");
			}
		});

		runner.test("Windows argv quoting preserves empty spaces quotes and slashes", [] {
			require(QuoteWindowsArgument(L"") == L"\"\"",
				"empty argument quoting changed");
			require(QuoteWindowsArgument(L"two words") == L"\"two words\"",
				"space-containing argument quoting changed");
			require(QuoteWindowsArgument(L"a\"b") == L"\"a\\\"b\"",
				"embedded quote escaping changed");
			require(
				QuoteWindowsArgument(L"C:\\path\\") == L"\"C:\\path\\\\\"",
				"trailing backslash escaping changed");
		});

	}
}
