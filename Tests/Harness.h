#pragma once

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace vmm_tests
{
	class Failure final : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};

	inline void require(bool condition, std::string message)
	{
		if (!condition)
			throw Failure(std::move(message));
	}

	class Runner
	{
	public:
		template <class Function>
		void test(std::string_view name, Function&& function)
		{
			++_tests;
			try
			{
				function();
				std::cout << "[PASS] " << name << '\n';
			}
			catch (const std::exception& error)
			{
				++_failures;
				std::cout << "[FAIL] " << name << ": " << error.what() << '\n';
			}
			catch (...)
			{
				++_failures;
				std::cout << "[FAIL] " << name << ": unknown exception\n";
			}
		}

		void info(std::string_view message) const
		{
			std::cout << "[INFO] " << message << '\n';
		}

		[[nodiscard]] int failures() const
		{
			return _failures;
		}

		[[nodiscard]] int tests() const
		{
			return _tests;
		}

	private:
		int _failures{};
		int _tests{};
	};

	void run_imgui_platform_checks(Runner& runner);
	void run_dear_modding_ui_checks(Runner& runner);
}
