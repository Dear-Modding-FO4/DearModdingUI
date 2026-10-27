#include <DearModdingUI/MCM/BridgeTextCatalog.h>

#include <DearModdingUI/Client.h>

namespace DearModdingUI::MCM
{
	namespace
	{
		constexpr std::string_view kHostOwner{ "DearModdingUI" };

		// Other results may succeed on a later frame, so they are not cached.
		[[nodiscard]] bool IsFinalMiss(DMUI_Result a_result) noexcept
		{
			return a_result == DMUI_RESULT_TEXT_NOT_FOUND ||
				a_result == DMUI_RESULT_UNSUPPORTED_ABI ||
				a_result == DMUI_RESULT_HOST_NOT_INITIALIZED;
		}
	}

	std::optional<std::string_view> BridgeTextCatalog::Find(
		std::string_view a_key) const noexcept
	{
		try
		{
			const std::scoped_lock lock{ mutex_ };
			auto found = entries_.find(a_key);
			if (found == entries_.end())
			{
				DMUI_Result result{ DMUI_RESULT_OK };
				auto text = dmui::Client::ResolveText(kHostOwner, a_key, &result);
				if (result != DMUI_RESULT_OK && !IsFinalMiss(result))
					return std::nullopt;
				found = entries_.emplace(std::string{ a_key }, std::move(text)).first;
			}
			if (!found->second)
				return std::nullopt;
			return std::string_view{ *found->second };
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	// Bridge text has no plurals; resolveText may answer with English.
	Support::PluralCategory BridgeTextCatalog::Plural(
		uint64_t a_count) const noexcept
	{
		return Support::EnglishCatalog().Plural(a_count);
	}
}
