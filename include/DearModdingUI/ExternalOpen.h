#pragma once

#include <DearModdingUI/API.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace DearModdingUI
{
	struct ExternalOpenRequest
	{
		DMUI_ExternalTargetKind targetKind{ DMUI_EXTERNAL_TARGET_NONE };
		std::string target;
		std::string application;
		std::vector<std::string> arguments;
		std::string workingDirectory;
	};

	using ExternalOpenDispatch = DMUI_Result (*)(
		const ExternalOpenRequest&,
		uint32_t*) noexcept;

	class ExternalOpener
	{
	public:
		explicit ExternalOpener(ExternalOpenDispatch a_dispatch = nullptr) noexcept;

		[[nodiscard]] DMUI_Result Open(
			const DMUI_ExternalOpenDescriptor* a_descriptor,
			uint32_t* a_nativeError = nullptr) const noexcept;

	private:
		ExternalOpenDispatch m_dispatch;
	};

	[[nodiscard]] DMUI_Result ValidateExternalOpenDescriptor(
		const DMUI_ExternalOpenDescriptor* a_descriptor,
		ExternalOpenRequest& a_request) noexcept;
	[[nodiscard]] std::wstring QuoteWindowsArgument(
		std::wstring_view a_argument);
	[[nodiscard]] DMUI_Result DispatchExternalOpen(
		const ExternalOpenRequest& a_request,
		uint32_t* a_nativeError) noexcept;
}
