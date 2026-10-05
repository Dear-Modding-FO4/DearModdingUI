#include "DearModdingUITestSupport.h"

#include <fstream>
#include <stdexcept>
#include <vector>

namespace vmm_tests::support::host
{
	using namespace DearModdingUI;

uint32_t s_externalOpenCalls{};
uint32_t s_externalNativeError{};
DMUI_Result s_externalResult{ DMUI_RESULT_OK };
ExternalOpenRequest s_externalRequest;
uint32_t s_externalResolveCalls{};
uint32_t s_externalResolveError{};
DMUI_Result s_externalResolveResult{ DMUI_RESULT_OK };
std::string s_externalVirtualFile;
std::string s_externalPhysicalFile;

bool ProbeSettingsMove(
	void* a_context,
	const std::filesystem::path& a_source,
	const std::filesystem::path& a_destination,
	uint32_t a_flags,
	uint32_t& a_nativeError) noexcept
{
	auto& probe = *static_cast<SettingsMoveProbe*>(a_context);
	const auto index = probe.calls++;
	if (index < probe.flags.size())
		probe.flags[index] = a_flags;
	if (index == 0)
	{
		a_nativeError = probe.firstError;
		return false;
	}
	if (probe.performSecondMove)
	{
		if (MoveFileExW(
				a_source.c_str(),
				a_destination.c_str(),
				a_flags))
		{
			a_nativeError = ERROR_SUCCESS;
			return true;
		}
		a_nativeError = GetLastError();
		return false;
	}
	a_nativeError = probe.secondError;
	return false;
}

DMUI_Result FakeExternalFileResolver(
	std::string_view a_virtualFile,
	std::string& a_physicalFile,
	uint32_t* a_nativeError) noexcept
{
	++s_externalResolveCalls;
	s_externalVirtualFile = a_virtualFile;
	if (a_nativeError)
		*a_nativeError = s_externalResolveError;
	if (s_externalResolveResult == DMUI_RESULT_OK)
		a_physicalFile = s_externalPhysicalFile;
	return s_externalResolveResult;
}

	ExternalFileFixture::ExternalFileFixture(std::string_view a_contents)
	{
		wchar_t directory[MAX_PATH]{};
		const auto length = GetTempPathW(MAX_PATH, directory);
		require(length != 0 && length < MAX_PATH,
			"temporary path was unavailable");
		wchar_t file[MAX_PATH]{};
		require(GetTempFileNameW(directory, L"dmu", 0, file) != 0,
			"temporary file creation failed");
		path = file;
		std::ofstream stream{ path, std::ios::binary | std::ios::trunc };
		stream.write(
			a_contents.data(),
			static_cast<std::streamsize>(a_contents.size()));
		require(stream.good(),
			"temporary file contents could not be written");
	}

	ExternalFileFixture::~ExternalFileFixture()
	{
		(void)SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
		(void)DeleteFileW(path.c_str());
	}

	std::string ExternalFileFixture::Utf8() const
	{
		const auto text = path.u8string();
		return { text.begin(), text.end() };
	}

DMUI_Result FakeExternalOpen(
	const ExternalOpenRequest& a_request,
	uint32_t* a_nativeError) noexcept
{
	++s_externalOpenCalls;
	s_externalRequest = a_request;
	if (a_nativeError)
		*a_nativeError = s_externalNativeError;
	return s_externalResult;
}

void DMUI_CALL Ready(const DMUI_HostReadyInfo* a_info, void* a_userData) noexcept
{
	auto& state = *static_cast<CallbackState*>(a_userData);
	++state.ready;
	(void)a_info;
}

void DMUI_CALL Unavailable(
	DMUI_UnavailableReason a_reason,
	void* a_userData) noexcept
{
	auto& state = *static_cast<CallbackState*>(a_userData);
	++state.unavailable;
	state.reason = a_reason;
}

void DMUI_CALL Draw(void* a_userData) noexcept
{
	++static_cast<CallbackState*>(a_userData)->draws;
}

DMUI_Result DMUI_CALL DrawPage(void* a_userData) noexcept
{
	Draw(a_userData);
	return DMUI_RESULT_OK;
}

void DMUI_CALL ObservePageActivity(
	const DMUI_PageActivityInfo* a_info,
	void* a_userData)
{
	static_cast<PageActivityState*>(a_userData)->events.push_back(*a_info);
}

void DMUI_CALL ThrowReady(const DMUI_HostReadyInfo*, void*)
{
	throw std::runtime_error("ready");
}

void DMUI_CALL ThrowUnavailable(DMUI_UnavailableReason, void*)
{
	throw std::runtime_error("unavailable");
}

void DMUI_CALL ThrowDraw(void*)
{
	throw std::runtime_error("draw");
}

DMUI_Result DMUI_CALL ThrowDrawPage(void*)
{
	throw std::runtime_error("draw");
}

DMUI_Result DMUI_CALL UnsupportedDrawPage(void*) noexcept
{
	return DMUI_RESULT_UNSUPPORTED_ABI;
}

[[nodiscard]] DMUI_ClientDescriptor Client(
	const char* a_id,
	const char* a_name,
	CallbackState& a_state) noexcept
{
	return {
		a_id,
		a_name,
		DMUI_MAKE_VERSION(1, 0),
		&Ready,
		&Unavailable,
		&a_state,
		DMUI_CLIENT_CAPABILITY_NONE,
		nullptr
	};
}

[[nodiscard]] DMUI_PageDescriptor Page(
	const char* a_id,
	const char* a_name,
	const char* a_category,
	int32_t a_sort,
	DMUI_PageKind a_kind,
	CallbackState& a_state,
	const char* a_iconName) noexcept
{
	return {
		a_id,
		a_name,
		a_category,
		nullptr,
		a_sort,
		a_kind,
		&DrawPage,
		&a_state,
		a_iconName
	};
}

[[nodiscard]] DMUI_ActionDescriptor Action(
	const char* a_id,
	const char* a_label,
	const char* a_icon,
	int32_t a_sort,
	CallbackState& a_state) noexcept
{
	return {
		a_id,
		a_label,
		a_icon,
		nullptr,
		a_sort,
		&DrawPage,
		&a_state
	};
}

[[nodiscard]] DMUI_FrameObserverDescriptor FrameObserver(
	CallbackState& a_state) noexcept
{
	return {
		&Draw,
		&a_state
	};
}

[[nodiscard]] DMUI_ClientHandle AddClient(
	Registry& a_registry,
	const char* a_id,
	const char* a_name,
	CallbackState& a_state,
	DMUI_ClientOrigin a_origin,
	const char* a_bridgeSourceLabel)
{
	auto descriptor = Client(a_id, a_name, a_state);
	descriptor.origin = a_origin;
	descriptor.bridgeSourceLabel = a_bridgeSourceLabel;
	DMUI_ClientHandle handle{};
	require(a_registry.RegisterClient(&descriptor, &handle) == DMUI_RESULT_OK,
		"client registration failed");
	return handle;
}

[[nodiscard]] DMUI_PageHandle AddPage(
	Registry& a_registry,
	DMUI_ClientHandle a_client,
	const char* a_id,
	const char* a_name,
	const char* a_category,
	int32_t a_sort,
	DMUI_PageKind a_kind,
	CallbackState& a_state,
	const char* a_iconName)
{
	auto descriptor = Page(
		a_id,
		a_name,
		a_category,
		a_sort,
		a_kind,
		a_state,
		a_iconName);
	DMUI_PageHandle handle{};
	require(a_registry.RegisterPage(a_client, &descriptor, &handle) == DMUI_RESULT_OK,
		"page registration failed");
	return handle;
}

void AddCategory(
	Registry& a_registry,
	DMUI_ClientHandle a_client,
	const char* a_id,
	const char* a_displayName,
	int32_t a_sortKey,
	const char* a_iconName)
{
	const DMUI_CategoryDescriptor descriptor{
		a_id,
		a_displayName,
		a_sortKey,
		0,
		a_iconName
	};
	require(
		a_registry.RegisterCategory(a_client, &descriptor) ==
			DMUI_RESULT_OK,
		"category registration failed");
}

[[nodiscard]] DMUI_ActionHandle AddAction(
	Registry& a_registry,
	DMUI_ClientHandle a_client,
	const char* a_id,
	const char* a_label,
	const char* a_icon,
	int32_t a_sort,
	CallbackState& a_state)
{
	auto descriptor = Action(a_id, a_label, a_icon, a_sort, a_state);
	DMUI_ActionHandle handle{};
	require(a_registry.RegisterAction(a_client, &descriptor, &handle) ==
			DMUI_RESULT_OK,
		"action registration failed");
	return handle;
}
}

namespace DearModdingUI
{
	bool PageFailed(DMUI_PageHandle) noexcept
	{
		// SidebarView links without Host.cpp, so it needs a host-free stand-in.
		return false;
	}
}
