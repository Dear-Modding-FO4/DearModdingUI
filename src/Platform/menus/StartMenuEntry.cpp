#include <Platform/menus/StartMenuEntry.h>

#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/localization/Localization.h>
#include <DearModdingUI/settings/HostSettings.h>

#include <RE/B/BSTEvent.h>
#include <RE/M/MainMenu.h>
#include <RE/M/MenuOpenCloseEvent.h>
#include <RE/P/PauseMenu.h>
#include <RE/U/UI.h>
#include <REX/REX.h>
#include <Scaleform/G/GFx_FunctionHandler.h>
#include <Scaleform/G/GFx_Movie.h>
#include <Scaleform/G/GFx_Value.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <string_view>

namespace DearModdingUI::StartMenuEntry
{
	namespace
	{
		using namespace std::literals;
		using Scaleform::GFx::FunctionHandler;
		using Scaleform::GFx::Movie;
		using Scaleform::GFx::Value;

		// Rows dispatch on entry.index; MCM owns 100 and vanilla ignores others.
		constexpr double kEntryIndex = 0x444D;
		constexpr double kModConfigIndex = 100;
		constexpr double kSettingsIndex = 8;
		constexpr auto kMainListPath = "root.Menu_mc.MainPanel_mc.List_mc";
		// Its rows open categories by position, so ours can only follow them.
		constexpr auto kSettingsListPath = "root.Menu_mc.SettingsPanel_mc.SettingsList_mc";
		constexpr auto kRemovedModConfig = "DearModdingUI_RemovedModConfigEntry";
		constexpr auto kRemovedModConfigSlot = "DearModdingUI_RemovedModConfigSlot";

		// Vanilla greys these out by position, so it must learn their new slots.
		struct TrackedRow
		{
			double index;
			const char* update;
		};
		constexpr std::array kTrackedRows{
			TrackedRow{ 6, "UpdateModsIndex" },
			TrackedRow{ 12, "UpdateCClubIndex" },
			TrackedRow{ 14, "UpdateInstalledContentIndex" }
		};

		struct Placement
		{
			bool listShown{ false };
			uint32_t listSlot{ 0 };
			bool replaceModConfig{ false };
			bool settingsShown{ false };
		};

		void (*s_open)() noexcept{ nullptr };

		enum class Failure : uint32_t
		{
			kRowRejected,
			kSettingsListGrow,
			kMainListMissing,
			kSettingsListMissing,
			kMenuMissing,
			kListenersRejected,
			kCount
		};

		constexpr std::array<std::string_view, static_cast<uint32_t>(Failure::kCount)> kFailureDetails{
			"the list rejected the new row"sv,
			"the Settings list could not grow"sv,
			"MainPanel_mc.List_mc.entryList is missing"sv,
			"SettingsPanel_mc.SettingsList_mc.entryList is missing"sv,
			"Menu_mc is missing"sv,
			"its listeners could not be attached"sv
		};
		std::array<std::atomic<bool>, static_cast<uint32_t>(Failure::kCount)> s_failuresLogged{};

		void LogLayoutFailure(Failure a_failure) noexcept
		{
			const auto index = static_cast<uint32_t>(a_failure);
			if (!s_failuresLogged[index].exchange(true, std::memory_order_acq_rel))
				REX::ERROR("DearModdingUI: menu entry skipped, {}"sv, kFailureDetails[index]);
		}

		[[nodiscard]] Placement PlacementFor(StartMenu a_menu, const HostInterfaceSettings& a_settings) noexcept
		{
			const auto slot = [](int32_t a_position) {
				return static_cast<uint32_t>((std::max)(a_position, int32_t{ 1 }) - 1);
			};
			if (a_menu == StartMenu::kPause)
				return {
					a_settings.pauseMenuEntry != PauseMenuEntryMode::kOff,
					slot(a_settings.pauseMenuEntryPosition),
					a_settings.pauseMenuEntry == PauseMenuEntryMode::kReplace,
					a_settings.settingsMenuEntry
				};
			return {
				a_settings.mainMenuEntry,
				slot(a_settings.mainMenuEntryPosition),
				false,
				a_settings.settingsMenuEntry
			};
		}

		[[nodiscard]] std::optional<double> Number(const Value& a_value) noexcept
		{
			if (a_value.IsNumber())
				return a_value.GetNumber();
			if (a_value.IsInt())
				return a_value.GetInt();
			if (a_value.IsUInt())
				return a_value.GetUInt();
			return std::nullopt;
		}

		[[nodiscard]] bool HasIndex(const Value& a_entry, double a_index)
		{
			Value index;
			return a_entry.IsObject() && a_entry.GetMember("index", &index) && Number(index) == a_index;
		}

		[[nodiscard]] std::optional<uint32_t> FindIndex(const Value& a_entries, double a_index)
		{
			for (uint32_t i = 0, size = a_entries.GetArraySize(); i < size; ++i)
			{
				Value entry;
				if (a_entries.GetElement(i, &entry) && HasIndex(entry, a_index))
					return i;
			}
			return std::nullopt;
		}

		struct List
		{
			Value clip;
			Value entries;

			[[nodiscard]] bool Load(const Movie& a_movie, const char* a_path)
			{
				return a_movie.GetVariable(&clip, a_path) && clip.IsObject() &&
					clip.GetMember("entryList", &entries) && entries.IsArray();
			}

			[[nodiscard]] bool Insert(uint32_t a_slot, const Value& a_entry)
			{
				const Value args[]{ Value{ static_cast<double>(a_slot) }, Value{ 0.0 }, a_entry };
				return entries.Invoke("splice", nullptr, args, std::size(args));
			}
		};

		// Puts MCM's own row object back where replace mode took it from.
		[[nodiscard]] bool SyncModConfig(Value& a_root, List& a_main, bool a_replace, bool& a_pending)
		{
			const auto modConfig = FindIndex(a_main.entries, kModConfigIndex);
			Value removed;
			const auto hasRemoved = a_root.GetMember(kRemovedModConfig, &removed) && removed.IsObject();
			if (a_replace && modConfig)
			{
				Value entry;
				return a_main.entries.GetElement(*modConfig, &entry) &&
					a_root.SetMember(kRemovedModConfig, entry) &&
					a_root.SetMember(kRemovedModConfigSlot, Value{ static_cast<double>(*modConfig) }) &&
					a_main.entries.RemoveElement(*modConfig);
			}
			if (!a_replace && !modConfig && hasRemoved)
			{
				Value slot;
				(void)a_root.GetMember(kRemovedModConfigSlot, &slot);
				const auto at = (std::min)(
					static_cast<uint32_t>(Number(slot).value_or(0.0)),
					a_main.entries.GetArraySize());
				(void)a_root.SetMember(kRemovedModConfig, Value{ nullptr });
				return a_main.Insert(at, removed);
			}
			// MCM splices its row in a few frames after the pause menu opens.
			Value mcmLoader;
			a_pending = a_replace && !modConfig && !hasRemoved &&
				a_root.GetMember("mcm_loader", &mcmLoader) && mcmLoader.IsObject();
			return false;
		}

		struct RowState
		{
			bool disabled;
			bool waitingForLoad;
		};

		[[nodiscard]] RowState RowStateFor(DMUI_HostState a_state) noexcept
		{
			// Matches vanilla Audio: grey with a spinner until it can open.
			return {
				a_state != DMUI_HOST_STATE_READY,
				a_state != DMUI_HOST_STATE_READY && a_state != DMUI_HOST_STATE_UNAVAILABLE
			};
		}

		// Returns the row's slot when shown; no a_slot means the end.
		[[nodiscard]] std::optional<uint32_t> SyncEntry(
			Movie& a_movie,
			List& a_list,
			bool a_shown,
			std::optional<uint32_t> a_slot,
			RowState a_state,
			bool& a_changed)
		{
			const auto current = FindIndex(a_list.entries, kEntryIndex);
			if (!a_shown)
			{
				if (current)
					a_changed = a_list.entries.RemoveElement(*current) || a_changed;
				return std::nullopt;
			}

			const auto others = a_list.entries.GetArraySize() - (current ? 1u : 0u);
			const auto slot = (std::min)(a_slot.value_or(others), others);
			Value entry;
			if (current == slot)
				(void)a_list.entries.GetElement(slot, &entry);
			else
			{
				if (current)
					(void)a_list.entries.RemoveElement(*current);
				a_movie.CreateObject(&entry);
				if (!entry.SetMember("text", Value{ Localization::Text("$DMUI_MenuEntry", "DMUI") }) ||
					!entry.SetMember("index", Value{ kEntryIndex }) ||
					!a_list.Insert(slot, entry))
				{
					LogLayoutFailure(Failure::kRowRejected);
					return std::nullopt;
				}
				a_changed = true;
			}
			for (const auto [name, value] : {
					 std::pair{ "disabled", a_state.disabled },
					 std::pair{ "waitingForLoad", a_state.waitingForLoad } })
			{
				Value flag;
				if (entry.GetMember(name, &flag) && flag.IsBoolean() && flag.GetBoolean() == value)
					continue;
				(void)entry.SetMember(name, Value{ value });
				a_changed = true;
			}
			return slot;
		}

		// The Settings list only builds clips and height for its four vanilla rows.
		void GrowToFit(Movie& a_movie, List& a_list)
		{
			Value count;
			Value border;
			Value spacing;
			Value rollover;
			Value press;
			Value first;
			Value holder;
			const Value firstArgs[]{ Value{ 0.0 } };
			// Its clip holder is protected, so reach it through the first clip.
			if (!a_list.clip.GetMember("numListItems", &count) ||
				!a_list.clip.GetMember("border", &border) ||
				!a_list.clip.GetMember("verticalSpacing", &spacing) ||
				!a_list.clip.GetMember("onEntryRollover", &rollover) ||
				!a_list.clip.GetMember("onEntryPress", &press) ||
				!a_list.clip.Invoke("GetClipByIndex", &first, firstArgs, std::size(firstArgs)) ||
				!first.IsDisplayObject() || !first.GetMember("parent", &holder))
			{
				LogLayoutFailure(Failure::kSettingsListGrow);
				return;
			}
			auto clips = static_cast<uint32_t>(Number(count).value_or(0.0));
			const auto rows = a_list.entries.GetArraySize();
			if (clips == 0 || clips >= rows)
				return;

			Value height;
			Value borderHeight;
			if (!first.GetMember("height", &height) || !border.GetMember("height", &borderHeight))
				return;
			const auto rowHeight = Number(height).value_or(0.0) + Number(spacing).value_or(0.0);
			auto total = Number(borderHeight).value_or(0.0);
			for (; clips < rows; ++clips)
			{
				Value entry;
				a_movie.CreateObject(&entry, "SettingsCategoryListEntry");
				const Value overArgs[]{ Value{ "mouseOver" }, rollover };
				const Value clickArgs[]{ Value{ "click" }, press };
				const Value childArgs[]{ entry };
				if (!entry.IsDisplayObject() ||
					!entry.SetMember("clipIndex", Value{ static_cast<double>(clips) }) ||
					!entry.Invoke("addEventListener", nullptr, overArgs, std::size(overArgs)) ||
					!entry.Invoke("addEventListener", nullptr, clickArgs, std::size(clickArgs)) ||
					!holder.Invoke("addChild", nullptr, childArgs, std::size(childArgs)))
				{
					LogLayoutFailure(Failure::kSettingsListGrow);
					break;
				}
				total += rowHeight;
			}
			(void)a_list.clip.SetMember("numListItems", Value{ static_cast<double>(clips) });
			(void)border.SetMember("height", Value{ total });
		}

		// Lets the position setting name the row that lands next to Settings.
		[[nodiscard]] std::optional<uint32_t> ReportLayout(const List& a_main, StartMenu a_menu)
		{
			const auto settings = FindIndex(a_main.entries, kSettingsIndex);
			if (!settings)
				return std::nullopt;
			const auto entry = FindIndex(a_main.entries, kEntryIndex);
			HostSettings::ReportRowAboveSettings(
				a_menu,
				*settings - (entry && *entry < *settings ? 1u : 0u) + 1u);
			return settings;
		}

		[[nodiscard]] bool HoldsIndex(const List& a_list, std::optional<uint32_t> a_slot, double a_index)
		{
			Value entry;
			return !a_slot || (a_list.entries.GetElement(*a_slot, &entry) && HasIndex(entry, a_index));
		}

		void UpdateTrackedRows(const Movie& a_movie, const List& a_main)
		{
			Value menu;
			if (!a_movie.GetVariable(&menu, "root.Menu_mc"))
				return;
			for (const auto& row : kTrackedRows)
			{
				if (const auto at = FindIndex(a_main.entries, row.index))
				{
					const Value args[]{ Value{ static_cast<double>(*at) } };
					(void)menu.Invoke(row.update, nullptr, args, std::size(args));
				}
			}
		}

		// One per menu; runs every frame but works only when something moved.
		class FrameHandler final :
			public FunctionHandler
		{
		public:
			explicit FrameHandler(StartMenu a_menu) noexcept :
				menu_(a_menu)
			{}

			void Reset() noexcept
			{
				revision_.reset();
				settled_ = false;
				mainSlot_.reset();
				settingsRow_.reset();
				settingsSlot_.reset();
			}

			void Call(const Params& a_params) override
			{
				if (!a_params.movie)
					return;
				const auto revision = HostSettings::Revision();
				const auto hostState = CurrentHostState();
				if (settled_ && revision_ == revision && hostState_ == hostState && RowsIntact(*a_params.movie))
					return;
				revision_ = revision;
				hostState_ = hostState;
				settled_ = Sync(*a_params.movie, PlacementFor(menu_, HostSettings::Current()), RowStateFor(hostState));
			}

		private:
			// Other mods' rows can shift ours or Settings without our knowing.
			[[nodiscard]] bool RowsIntact(const Movie& a_movie) const
			{
				List main;
				List settings;
				return main.Load(a_movie, kMainListPath) &&
					HoldsIndex(main, mainSlot_, kEntryIndex) &&
					HoldsIndex(main, settingsRow_, kSettingsIndex) &&
					(!settingsSlot_ ||
						(settings.Load(a_movie, kSettingsListPath) &&
							HoldsIndex(settings, settingsSlot_, kEntryIndex)));
			}

			// Returns true once nothing else is expected to move.
			[[nodiscard]] bool Sync(Movie& a_movie, const Placement& a_placement, RowState a_state)
			{
				Value root;
				List main;
				if (!a_movie.GetVariable(&root, "root") || !main.Load(a_movie, kMainListPath))
				{
					LogLayoutFailure(Failure::kMainListMissing);
					return true;
				}
				// The menu fills its rows after it opens.
				if (main.entries.GetArraySize() == 0)
					return false;

				auto pending = false;
				auto mainChanged = menu_ == StartMenu::kPause &&
					SyncModConfig(root, main, a_placement.listShown && a_placement.replaceModConfig, pending);
				mainSlot_ = SyncEntry(a_movie, main, a_placement.listShown, a_placement.listSlot, a_state, mainChanged);
				if (mainChanged)
				{
					(void)main.clip.Invoke("InvalidateData");
					UpdateTrackedRows(a_movie, main);
				}
				settingsRow_ = ReportLayout(main, menu_);

				settingsSlot_.reset();
				List settings;
				if (!settings.Load(a_movie, kSettingsListPath))
				{
					if (a_placement.settingsShown)
						LogLayoutFailure(Failure::kSettingsListMissing);
					return !pending;
				}
				// Vanilla fills the categories once the list builds its rows.
				if (settings.entries.GetArraySize() == 0)
					return !pending && !a_placement.settingsShown;
				auto settingsChanged = false;
				settingsSlot_ = SyncEntry(a_movie, settings, a_placement.settingsShown, std::nullopt, a_state, settingsChanged);
				if (settingsChanged)
				{
					if (settingsSlot_)
						GrowToFit(a_movie, settings);
					(void)settings.clip.Invoke("InvalidateData");
				}
				return !pending;
			}

			StartMenu menu_;
			std::optional<uint64_t> revision_;
			DMUI_HostState hostState_{ DMUI_HOST_STATE_NOT_INITIALIZED };
			bool settled_{ false };
			std::optional<uint32_t> mainSlot_;
			std::optional<uint32_t> settingsRow_;
			std::optional<uint32_t> settingsSlot_;
		};

		class PressHandler final :
			public FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				// Both lists bubble their presses to Menu_mc.
				Value list;
				Value entry;
				if (a_params.argCount > 0 && s_open &&
					CurrentHostState() == DMUI_HOST_STATE_READY &&
					a_params.args[0].GetMember("target", &list) && list.IsObject() &&
					list.GetMember("selectedEntry", &entry) && HasIndex(entry, kEntryIndex))
					s_open();
			}
		};

		// The initial reference keeps these alive for every movie that holds them.
		FrameHandler s_pauseFrames{ StartMenu::kPause };
		FrameHandler s_mainFrames{ StartMenu::kMain };
		PressHandler s_pressHandler;

		void Attach(Movie& a_movie, FrameHandler& a_frames)
		{
			Value root;
			Value menu;
			if (!a_movie.GetVariable(&root, "root") || !a_movie.GetVariable(&menu, "root.Menu_mc") ||
				!menu.IsObject())
			{
				LogLayoutFailure(Failure::kMenuMissing);
				return;
			}
			a_frames.Reset();
			Value press;
			Value frame;
			a_movie.CreateFunction(&press, &s_pressHandler);
			a_movie.CreateFunction(&frame, &a_frames);
			const Value pressArgs[]{ Value{ "BSScrollingList::itemPress" }, press };
			const Value frameArgs[]{ Value{ "enterFrame" }, frame };
			if (!menu.Invoke("addEventListener", nullptr, pressArgs, std::size(pressArgs)) ||
				!root.Invoke("addEventListener", nullptr, frameArgs, std::size(frameArgs)))
				LogLayoutFailure(Failure::kListenersRejected);
		}

		void AttachOpenMenu(RE::UI& a_ui, const RE::BSFixedString& a_menuName)
		{
			FrameHandler* frames{ nullptr };
			if (a_menuName == RE::PauseMenu::MENU_NAME)
				frames = &s_pauseFrames;
			else if (a_menuName == RE::MainMenu::MENU_NAME)
				frames = &s_mainFrames;
			if (!frames)
				return;
			if (const auto menu = a_ui.GetMenu(a_menuName); menu && menu->uiMovie)
				Attach(*menu->uiMovie, *frames);
		}

		class MenuOpenSink final :
			public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(
				const RE::MenuOpenCloseEvent& a_event,
				RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (auto* ui = RE::UI::GetSingleton(); ui && a_event.opening)
					AttachOpenMenu(*ui, a_event.menuName);
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		MenuOpenSink s_menuOpenSink;
	}

	bool Register(void (*a_open)() noexcept) noexcept
	{
		if (s_open)
			return true;
		auto* ui = RE::UI::GetSingleton();
		if (!a_open || !ui)
			return false;
		s_open = a_open;
		ui->RegisterSink<RE::MenuOpenCloseEvent>(&s_menuOpenSink);
		// The title menu can open before registration.
		const RE::BSFixedString mainMenu{ RE::MainMenu::MENU_NAME };
		const auto mainMenuOpen = ui->GetMenuOpen(mainMenu);
		if (mainMenuOpen)
			AttachOpenMenu(*ui, mainMenu);
		REX::INFO("DearModdingUI: menu entries registered, main menu {}"sv, mainMenuOpen ? "already open" : "not open yet");
		return true;
	}
}
