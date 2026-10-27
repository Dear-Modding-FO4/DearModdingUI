#pragma once

#include <DearModdingUI/presentation/ThemeSpec.h>
#include <Support/Revision.h>
#include <Support/SubsystemHealth.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace DearModdingUI::Theme
{
	// A preset file resolved over the compiled default theme.
	struct ThemePreset
	{
		// The file's [theme] name; empty when it sets none.
		std::string name;
		ThemeSpec spec;
		// Each rejected key; every valid key still applied.
		std::vector<std::string> problems;
		// Set when no file content applied and the compiled default stands in.
		std::string failure;
	};

	struct ThemePresetInfo
	{
		// The file stem.
		std::string id;
		// The display name: [theme] name, else the id.
		std::string name;
	};

	// Missing or unparseable files yield the default theme and a failure.
	[[nodiscard]] ThemePreset LoadThemePreset(
		const std::filesystem::path& a_file) noexcept;
	// Every *.toml in the folder, sorted by display name.
	[[nodiscard]] std::vector<ThemePresetInfo> ListThemePresets(
		const std::filesystem::path& a_directory) noexcept;
	// Each "section.key" a preset file may set; icon glyph names are open.
	[[nodiscard]] std::vector<std::string> ThemePresetKeys();
	[[nodiscard]] HealthObservation ClassifyThemePresetHealth(
		std::string_view a_id,
		const ThemePreset& a_preset);

	// Resolves presets by id and reloads one when its file's write time changes.
	class ThemePresetCache
	{
	public:
		ThemePresetCache(
			std::filesystem::path a_directory,
			std::chrono::milliseconds a_pollInterval) :
			m_directory(std::move(a_directory)),
			m_pollInterval(a_pollInterval)
		{}

		// Advances when a cached preset's file changes.
		[[nodiscard]] uint64_t Revision() const noexcept
		{
			return m_revision.Current();
		}

		// Checks cached files' write times at most once per poll interval.
		void Poll() noexcept;
		// Never null; a bad id or file resolves to the default with a failure.
		[[nodiscard]] std::shared_ptr<const ThemePreset> Resolve(
			std::string_view a_id) noexcept;
		[[nodiscard]] std::vector<ThemePresetInfo> List() const noexcept
		{
			return ListThemePresets(m_directory);
		}

	private:
		struct Entry
		{
			std::shared_ptr<const ThemePreset> preset;
			std::optional<std::filesystem::file_time_type> stamp;
		};

		[[nodiscard]] Entry Load(const std::string& a_id) const noexcept;

		std::filesystem::path m_directory;
		std::chrono::milliseconds m_pollInterval;
		std::atomic<std::chrono::steady_clock::rep> m_nextPoll{ 0 };
		std::mutex m_mutex;
		std::map<std::string, Entry, std::less<>> m_entries;
		Addictol::Support::Revision m_revision;
	};
}
