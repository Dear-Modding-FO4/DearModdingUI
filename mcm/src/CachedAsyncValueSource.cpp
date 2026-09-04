#include <DearModdingUI/MCM/CachedAsyncValueSource.h>

#include <utility>

namespace DearModdingUI::MCM
{
	ValueSnapshot CachedAsyncValueSource::Read(
		const MappedBinding& a_binding) const
	{
		return cache_.Read(a_binding.cacheKey);
	}

	void CachedAsyncValueSource::Pump() noexcept
	{
		try
		{
			std::vector<Completion> completions;
			{
				const std::scoped_lock lock{ completionMutex_ };
				completions.swap(completions_);
			}
			for (auto& completion : completions)
			{
				const auto accepted = completion.settlementToken ?
					cache_.CompleteWrite(
						completion.key,
						*completion.settlementToken,
						std::move(completion.snapshot)) :
					cache_.Complete(
						completion.key,
						std::move(completion.snapshot));
				if (!accepted)
					continue;
				if (completion.accept)
					completion.accept();
			}
		}
		catch (...)
		{}
	}

	ValueCache& CachedAsyncValueSource::Cache() noexcept
	{
		return cache_;
	}

	const ValueCache& CachedAsyncValueSource::Cache() const noexcept
	{
		return cache_;
	}

	void CachedAsyncValueSource::QueueCompletion(
		std::string a_key,
		ValueSnapshot a_snapshot,
		std::function<void()> a_accept) noexcept
	{
		try
		{
			const std::scoped_lock lock{ completionMutex_ };
			completions_.push_back({
				std::move(a_key),
				std::move(a_snapshot),
				std::nullopt,
				std::move(a_accept)
			});
		}
		catch (...)
		{}
	}

	void CachedAsyncValueSource::QueueWriteCompletion(
		std::string a_key,
		uint64_t a_settlementToken,
		ValueSnapshot a_snapshot,
		std::function<void()> a_accept) noexcept
	{
		try
		{
			const std::scoped_lock lock{ completionMutex_ };
			completions_.push_back({
				std::move(a_key),
				std::move(a_snapshot),
				a_settlementToken,
				std::move(a_accept)
			});
		}
		catch (...)
		{}
	}
}
