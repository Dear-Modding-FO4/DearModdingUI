#include "Harness.h"

#include <RE/msvc/LegacyFunction.h>

#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>

namespace vmm_tests
{
	namespace
	{
		using Arguments = std::function<bool(std::vector<int>&)>;

		template <class T>
		T ReadAbiValue(const void* a_object, size_t a_offset)
		{
			T value;
			std::memcpy(&value, static_cast<const std::byte*>(a_object) + a_offset, sizeof(value));
			return value;
		}

		template <class F>
		F NativeSlot(const void* a_proxy, size_t a_slot)
		{
			const auto* table = ReadAbiValue<const void*>(a_proxy, 0);
			return std::bit_cast<F>(ReadAbiValue<uintptr_t>(table, a_slot * sizeof(void*)));
		}

		bool InvokeArguments(void* a_proxy, std::vector<int>& a_output)
		{
			return NativeSlot<bool (*)(void*, std::vector<int>&)>(a_proxy, 2)(a_proxy, a_output);
		}

		struct NativeCopy
		{
			~NativeCopy()
			{
				if (proxy)
					NativeSlot<void (*)(void*, bool)>(proxy, 4)(proxy, true);
			}

			void* proxy{};
		};
	}

	void run_papyrus_callable_checks(Runner& runner)
	{
		using RE::msvc::with_native_function;

		runner.test("OG dispatch invokes captured arguments through the native callable layout", [] {
			const Arguments source = [values = std::vector<int>{ 3, 5, 8 }](
				std::vector<int>& a_output) {
				a_output = values;
				return false;
			};
			const auto accepted = with_native_function(true, source, [](const void* a_native) {
				auto* proxy = ReadAbiValue<void*>(a_native, 0x18);
				require(proxy != nullptr, "OG argument builder has no callable at offset 0x18");
				std::vector<int> output;
				const auto result = InvokeArguments(proxy, output);
				require(output == std::vector<int>{ 3, 5, 8 },
					"OG dispatch did not receive the captured arguments");
				return result;
			});
			require(!accepted, "OG dispatch lost the argument builder's rejection");
		});

		runner.test("native callable copies retain independent captures until released", [] {
			std::weak_ptr<int> lifetime;
			{
				NativeCopy copy;
				{
					auto owned = std::make_shared<int>(21);
					lifetime = owned;
					const Arguments source = [owned, values = std::vector<int>{ 4, 9 }](
						std::vector<int>& a_output) mutable {
						a_output = values;
						a_output.push_back(*owned);
						values.push_back(13);
						return true;
					};
					with_native_function(true, source, [&](const void* a_native) {
						auto* proxy = ReadAbiValue<void*>(a_native, 0x18);
						std::array<std::byte, 0x20> storage{};
						copy.proxy = NativeSlot<void* (*)(const void*, void*)>(proxy, 0)(
							proxy, storage.data());
						require(copy.proxy && copy.proxy != proxy && copy.proxy != storage.data(),
							"native copy does not own its callable");
						std::vector<int> original;
						require(InvokeArguments(proxy, original), "original callable failed");
					});
				}
				require(!lifetime.expired(), "native copy lost captures when dispatch returned");
				std::vector<int> output;
				require(InvokeArguments(copy.proxy, output) && output == std::vector<int>{ 4, 9, 21 },
					"native copy lost arguments or shared mutable value captures");
			}
			require(lifetime.expired(), "native release leaked its captures");
		});

		runner.test("modern dispatch stays unchanged and empty callbacks remain empty", [] {
			const Arguments source = [](std::vector<int>& a_output) {
				a_output.push_back(7);
				return true;
			};
			with_native_function(false, source, [&](const void* a_native) {
				require(a_native == &source, "modern dispatch changed the native function object");
				std::vector<int> output;
				require((*static_cast<const Arguments*>(a_native))(output) && output == std::vector<int>{ 7 },
					"modern dispatch did not receive the argument builder");
			});
			const Arguments empty;
			with_native_function(true, empty, [](const void* a_native) {
				require(ReadAbiValue<void*>(a_native, 0x18) == nullptr, "OG empty callback became callable");
			});
			with_native_function(false, empty, [&](const void* a_native) {
				require(a_native == &empty && !*static_cast<const Arguments*>(a_native),
					"modern empty callback became callable");
			});
		});
	}
}
