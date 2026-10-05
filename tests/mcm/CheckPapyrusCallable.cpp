#include "../Harness.h"

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
						require(InvokeArguments(proxy, original) &&
							original == std::vector<int>{ 4, 9, 21 },
							"OG dispatch did not receive the captured arguments");
					});
				}
				require(!lifetime.expired(), "native copy lost captures when dispatch returned");
				std::vector<int> output;
				require(InvokeArguments(copy.proxy, output) && output == std::vector<int>{ 4, 9, 21 },
					"native copy lost arguments or shared mutable value captures");
			}
			require(lifetime.expired(), "native release leaked its captures");
		});


	}
}
