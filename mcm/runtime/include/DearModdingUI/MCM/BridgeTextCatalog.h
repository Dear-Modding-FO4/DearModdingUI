#pragma once

#include <Support/Message.h>
#include <Support/TextMap.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace DearModdingUI::MCM
{
	// Host-owned bridge text, cached for the process so returned views stay valid.
	class BridgeTextCatalog final : public Support::TextCatalog
	{
	public:
		[[nodiscard]] std::optional<std::string_view> Find(
			std::string_view a_key) const noexcept override;
		[[nodiscard]] Support::PluralCategory Plural(
			uint64_t a_count) const noexcept override;

	private:
		mutable std::mutex mutex_;
		// A cached miss keeps English without asking the host again.
		mutable Support::TextMap<std::optional<std::string>> entries_;
	};
}
