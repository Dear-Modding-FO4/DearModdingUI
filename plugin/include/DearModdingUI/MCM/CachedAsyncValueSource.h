#pragma once

#include <DearModdingUI/MCM/ValueSource.h>

#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace DearModdingUI::MCM
{
	class CachedAsyncValueSource : public ValueSource
	{
	public:
		[[nodiscard]] ValueSnapshot Read(
			const MappedBinding& a_binding) const final;
		void Pump() noexcept final;

	protected:
		[[nodiscard]] ValueCache& Cache() noexcept;
		[[nodiscard]] const ValueCache& Cache() const noexcept;
		void QueueCompletion(
			std::string a_key,
			ValueSnapshot a_snapshot,
			std::function<void()> a_accept = {}) noexcept;

	private:
		struct Completion
		{
			std::string key;
			ValueSnapshot snapshot;
			std::function<void()> accept;
		};

		ValueCache cache_;
		std::mutex completionMutex_;
		std::vector<Completion> completions_;
	};
}
