#pragma once

#include <wrl/client.h>

#include <memory>
#include <string_view>

struct ID3D11Device;

namespace DmuiTests
{
	enum class LogLevel
	{
		kInfo,
		kWarning,
		kError
	};

	class Environment
	{
	public:
		virtual ~Environment() = default;

		[[nodiscard]] virtual Microsoft::WRL::ComPtr<ID3D11Device>
			AcquireRendererDevice() noexcept = 0;
		[[nodiscard]] virtual bool SupportsGameInputContexts() const noexcept = 0;
		virtual void Log(LogLevel a_level, std::string_view a_message) noexcept = 0;
	};

	class GeneralTestSuite final
	{
	public:
		explicit GeneralTestSuite(Environment& a_environment);
		~GeneralTestSuite();

		GeneralTestSuite(const GeneralTestSuite&) = delete;
		GeneralTestSuite(GeneralTestSuite&&) = delete;
		GeneralTestSuite& operator=(const GeneralTestSuite&) = delete;
		GeneralTestSuite& operator=(GeneralTestSuite&&) = delete;

		[[nodiscard]] bool Initialize() noexcept;
		void Stop() noexcept;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
