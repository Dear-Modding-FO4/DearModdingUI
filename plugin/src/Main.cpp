#include <DearModdingUI/MCM/Availability.h>
#include <DearModdingUI/MCM/ExternalEventDispatcher.h>
#include <DearModdingUI/MCM/F4SETaskScheduler.h>
#include <DearModdingUI/MCM/GamePapyrusDispatcher.h>
#include <DearModdingUI/MCM/GameScaleformInvoker.h>
#include <DearModdingUI/MCM/GlobalValueSource.h>
#include <DearModdingUI/MCM/Keybinds.h>
#include <DearModdingUI/MCM/ModSettingValueSource.h>
#include <DearModdingUI/MCM/PapyrusActionExecutor.h>
#include <DearModdingUI/MCM/PropertyValueSource.h>
#include <DearModdingUI/MCM/SettingsIni.h>

#include <DearModdingUI/MCM/Compatibility.h>
#include <DearModdingUI/MCM/TextRendering.h>

#include <DearModdingUI/Client.h>

#include <F4SE/F4SE.h>
#include <RE/B/BSScript_IStackCallbackFunctor.h>
#include <REX/REX.h>

#include <Windows.h>
#undef ERROR

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace RE::BSScript
{
	IStackCallbackFunctor::~IStackCallbackFunctor() = default;
}

namespace DearModdingUI::MCM
{
	using namespace std::literals;

	namespace
	{
		struct RegisteredPage
		{
			std::unique_ptr<MappedPage> page;
			DMUI_PageHandle handle{ DMUI_INVALID_PAGE_HANDLE };
		};

		struct RegisteredMod
		{
			std::unique_ptr<dmui::Client> client;
			std::unique_ptr<ModSettingValueSource> modSettings;
			std::unique_ptr<PropertyValueSource> properties;
			std::unique_ptr<CompositeValueSource> values;
			std::unique_ptr<PapyrusActionExecutor> actions;
			std::vector<RegisteredPage> pages;
		};

		std::atomic_bool s_mcmInstalled{ false };
		std::atomic_bool s_runtimeReady{ false };
		GlobalValueSource s_globalValues;
		F4SETaskScheduler s_scheduler;
		GamePapyrusDispatcher s_dispatcher;
		GameScaleformInvoker s_scaleform;
		ExternalEventDispatcher s_events{ s_scheduler };
		McmEventLifecycle s_eventLifecycle;
		bool s_visibilityObserverRegistered{};
		std::vector<std::unique_ptr<RegisteredMod>> s_mods;

		[[nodiscard]] std::string PathText(
			const std::filesystem::path& a_path)
		{
			const auto value = a_path.u8string();
			return {
				reinterpret_cast<const char*>(value.data()),
				value.size()
			};
		}

		[[nodiscard]] uint64_t HashText(std::string_view a_value) noexcept
		{
			uint64_t hash{ 14695981039346656037ull };
			for (const auto character : a_value)
			{
				hash ^= static_cast<unsigned char>(character);
				hash *= 1099511628211ull;
			}
			return hash;
		}

		[[nodiscard]] std::string ClientId(
			std::string_view a_modName,
			std::string_view a_folder)
		{
			std::string sanitized;
			sanitized.reserve((std::min)(a_modName.size(), size_t{ 48 }));
			for (const auto character : a_modName)
			{
				const auto ascii = static_cast<unsigned char>(character);
				if ((ascii >= 'A' && ascii <= 'Z') ||
					(ascii >= 'a' && ascii <= 'z') ||
					(ascii >= '0' && ascii <= '9') ||
					ascii == '.' || ascii == '_' || ascii == '-')
					sanitized.push_back(static_cast<char>(ascii));
				else
					sanitized.push_back('-');
				if (sanitized.size() == 48)
					break;
			}
			if (sanitized.empty())
				sanitized = "mod";
			return std::format(
				"dear-modding.mcm.{}.{:016x}",
				sanitized,
				HashText(a_folder));
		}

		[[nodiscard]] size_t DescriptorCount(const MappedPage& a_page) noexcept
		{
			size_t count{};
			for (const auto& group : a_page.settings.groups)
				count += group.settings.size();
			return count;
		}

		[[nodiscard]] McmState CurrentMcmState() noexcept
		{
			return {
				s_mcmInstalled.load(std::memory_order_acquire),
				s_runtimeReady.load(std::memory_order_acquire)
			};
		}

		void LogErrors(const LoadResult& a_result)
		{
			for (const auto& diagnostic : a_result.diagnostics)
			{
				const auto warning =
					diagnostic.severity == DiagnosticSeverity::kWarning;
				if (diagnostic.location.empty())
				{
					if (warning)
						REX::WARN(
							"DearModdingUI-MCM: {}: {}"sv,
							diagnostic.source,
							diagnostic.message);
					else
						REX::ERROR(
							"DearModdingUI-MCM: {}: {}"sv,
							diagnostic.source,
							diagnostic.message);
				}
				else
				{
					if (warning)
						REX::WARN(
							"DearModdingUI-MCM: {}: {}: {}"sv,
							diagnostic.source,
							diagnostic.location,
							diagnostic.message);
					else
						REX::ERROR(
							"DearModdingUI-MCM: {}: {}: {}"sv,
							diagnostic.source,
							diagnostic.location,
							diagnostic.message);
				}
			}
		}

		void SurfaceCompatibility(
			MappedPage& a_page,
			const PageCompatibilitySummary& a_summary)
		{
			if (!a_summary.unsupported &&
				!a_summary.unknownBindings &&
				!a_summary.undeclaredModSettings &&
				!a_summary.actions &&
				!a_summary.images)
				return;
			a_page.settings.notes.push_back({
				std::format(
					"Compatibility: {} unsupported, {} unknown sources, "
					"{} undeclared settings, {} actions, {} images.",
					a_summary.unsupported,
					a_summary.unknownBindings,
					a_summary.undeclaredModSettings,
					a_summary.actions,
					a_summary.images),
				false
			});
		}

		void RegisterConfig(const std::filesystem::path& a_config) noexcept
		{
			try
			{
				auto result = LoadConfig(a_config);
				LogErrors(result);
				const auto folder = PathText(a_config.parent_path().filename());
				if (!result.configuration)
				{
					REX::ERROR(
						"DearModdingUI-MCM: {} could not be loaded ({} diagnostics)"sv,
						PathText(a_config),
						result.diagnostics.size());
					return;
				}

				const auto& configuration = *result.configuration;
				const auto displayName = configuration.displayName.empty() ?
					(configuration.modName.empty() ? folder : configuration.modName) :
					configuration.displayName;
				auto mod = std::make_unique<RegisteredMod>();
				mod->modSettings = std::make_unique<ModSettingValueSource>(
					configuration.modName,
					s_events,
					s_scheduler,
					s_dispatcher);
				mod->properties =
					std::make_unique<PropertyValueSource>(s_scheduler);
				mod->values = std::make_unique<CompositeValueSource>();
				mod->actions =
					std::make_unique<PapyrusActionExecutor>(
						s_scheduler,
						s_scaleform);
				mod->values->Add(s_globalValues);
				mod->values->Add(*mod->modSettings);
				mod->values->Add(*mod->properties);
				mod->client = std::make_unique<dmui::Client>(
					ClientId(configuration.modName, folder),
					displayName,
					dmui::Version{ 1, 0 },
					dmui::kForwardingClient,
					// MCM configs carry no icon field, so bridged mods cannot declare one.
					"plugs-connected",
					dmui::ClientOrigin{
						dmui::ClientOriginKind::kBridged,
						"MCM"
					});
				if (!mod->client->Connect())
				{
					if (mod->client->HostPresent())
					{
						REX::ERROR(
							"DearModdingUI-MCM: client {} was rejected ({})"sv,
							displayName,
							DMUI_ResultToString(mod->client->LastResult()));
					}
					s_mods.push_back(std::move(mod));
					return;
				}
				if (!s_visibilityObserverRegistered)
				{
					auto* observedClient = mod->client.get();
					s_visibilityObserverRegistered =
						mod->client->AddFrameObserver([observedClient] {
							if (const auto visible =
									observedClient->IsMenuVisible())
								s_events.DispatchEvents(
									s_eventLifecycle.OverlayVisibilityChanged(
										*visible));
						}).has_value();
				}

				size_t descriptors{};
				const auto declarations =
					LoadSettingsIni(a_config.parent_path() / "settings.ini");
				const auto definitions =
					LoadKeybindDefinitions(a_config.parent_path() / "keybinds.json");
				const auto keybinds = LoadUserKeybinds(
					std::filesystem::current_path() /
					"Data" / "MCM" / "Settings" / "Keybinds.json");
				for (size_t index = 0; index < result.pages.size(); ++index)
				{
					auto page =
						std::make_unique<MappedPage>(std::move(result.pages[index]));
					ApplyDeclarations(*page, declarations);
					ApplyKeybinds(*page, definitions, keybinds);
					ResolveActionAvailability(*page, *mod->actions);
					BindPage(*page, *mod->values, CurrentMcmState);
					const auto summary =
						SummarizeCompatibility(*page, *mod->values);
					SurfaceCompatibility(*page, summary);
					REX::INFO(
						"DearModdingUI-MCM: {} / {} compatibility: "
						"{} bindings, {} resolved keybinds, {} local UI-state rows, {} unsupported, {} unknown sources, "
						"{} undeclared settings, {} actions, {} images"sv,
						displayName,
						page->displayName,
						summary.bindings,
						summary.resolvedKeybinds,
						summary.localUiStateRows,
						summary.unsupported,
						summary.unknownBindings,
						summary.undeclaredModSettings,
						summary.actions,
						summary.images);
					const auto inert = SummarizeInertReasons(*page);
					REX::INFO(
						"DearModdingUI-MCM: {} / {} inert rows: "
						"{} condition false, {} condition pending, "
						"{} unsupported, {} undeclared, {} keybind unbound, "
						"{} keybind undeclared, {} keybind definitions missing, "
						"{} keybind definitions invalid, {} user keybinds invalid, "
						"{} MCM missing, "
						"{} load-save required, {} value pending, "
						"{} value unavailable, {} value failed"sv,
						displayName,
						page->displayName,
						inert[static_cast<size_t>(
							InertReason::kConditionFalse)],
						inert[static_cast<size_t>(
							InertReason::kConditionPending)],
						inert[static_cast<size_t>(
							InertReason::kUnsupported)],
						inert[static_cast<size_t>(
							InertReason::kUndeclaredModSetting)],
						inert[static_cast<size_t>(
							InertReason::kKeybindUnbound)],
						inert[static_cast<size_t>(
							InertReason::kKeybindDefinitionMissing)],
						inert[static_cast<size_t>(
							InertReason::kKeybindDefinitionsMissing)],
						inert[static_cast<size_t>(
							InertReason::kKeybindDefinitionsInvalid)],
						inert[static_cast<size_t>(
							InertReason::kKeybindBindingsInvalid)],
						inert[static_cast<size_t>(
							InertReason::kMcmNotInstalled)],
						inert[static_cast<size_t>(
							InertReason::kRuntimeNotReady)],
						inert[static_cast<size_t>(
							InertReason::kValuePending)],
						inert[static_cast<size_t>(
							InertReason::kValueMissing)],
						inert[static_cast<size_t>(
							InertReason::kValueFailed)]);
					descriptors += DescriptorCount(*page);
					BindActions(*page, *mod->actions, *mod->values);
					AttachTextRendering(*page);
					auto priorPrepare = std::move(page->settings.prepare);
					auto* values = mod->values.get();
					page->settings.prepare =
						[priorPrepare = std::move(priorPrepare), values] {
							if (priorPrepare)
								priorPrepare();
							values->Pump();
						};

					auto* settings = &page->settings;
					auto* client = mod->client.get();
					const auto registered = client->AddPage(
						{
							.id = page->id.c_str(),
							.displayName = page->displayName.c_str(),
							.sortKey = static_cast<int32_t>(index)
						},
						[settings, client] {
							try
							{
								settings->Draw(*client);
							}
							catch (const std::exception& a_error)
							{
								REX::ERROR(
									"DearModdingUI-MCM: page draw failed: {}"sv,
									a_error.what());
							}
							catch (...)
							{
								REX::ERROR(
									"DearModdingUI-MCM: page draw failed"sv);
							}
						});
					if (!registered)
					{
						REX::ERROR(
							"DearModdingUI-MCM: page {} for {} was rejected ({})"sv,
							page->displayName,
							displayName,
							DMUI_ResultToString(client->LastResult()));
						continue;
					}
					mod->pages.push_back({
						std::move(page),
						*registered
					});
				}
				auto* observed = mod.get();
				(void)mod->client->AddPageActivityObserver(
					[observed,
					 modName = configuration.modName](
						const dmui::PageActivity& a_activity) {
						if (a_activity.kind == dmui::PageActivityKind::kActivated)
							s_events.DispatchEvents(
								s_eventLifecycle.PageActivated(modName));
						if (a_activity.kind == dmui::PageActivityKind::kDeactivated)
						{
							const auto visible =
								observed->client->IsMenuVisible();
							s_events.DispatchEvents(
								s_eventLifecycle.PageDeactivated(
									modName,
									visible.value_or(true)));
							return;
						}
						for (const auto& registered : observed->pages)
						{
							if (registered.handle == a_activity.activePage)
							{
								observed->values->RefreshPage(
									*registered.page,
									CurrentMcmState());
								break;
							}
						}
					});

				REX::INFO(
					"DearModdingUI-MCM: {} registered ({} pages, {} descriptors, {} diagnostics)"sv,
					displayName,
					mod->pages.size(),
					descriptors,
					result.diagnostics.size());
				s_mods.push_back(std::move(mod));
			}
			catch (const std::exception& error)
			{
				REX::ERROR(
					"DearModdingUI-MCM: {} failed: {}"sv,
					PathText(a_config),
					error.what());
			}
			catch (...)
			{
				REX::ERROR(
					"DearModdingUI-MCM: {} failed"sv,
					PathText(a_config));
			}
		}

		void DiscoverAndRegister() noexcept
		{
			try
			{
				const auto installed =
					::GetModuleHandleW(L"mcm.dll") != nullptr;
				s_mcmInstalled.store(installed, std::memory_order_release);
				REX::INFO(
					"DearModdingUI-MCM: Mod Configuration Menu installation: {} "
					"(mcm.dll is {})"sv,
					installed ? "installed" : "not installed",
					installed ? "loaded" : "not loaded");
				REX::INFO(
					"DearModdingUI-MCM: Papyrus runtime: not ready "
					"(load a save to change mod settings and properties)"sv);
				const auto root =
					std::filesystem::current_path() / "Data" / "MCM" / "Config";
				std::error_code error;
				std::filesystem::directory_iterator entries{ root, error };
				if (error)
				{
					REX::INFO(
						"DearModdingUI-MCM: no MCM configuration directory was found"sv);
					return;
				}
				for (const auto& entry : entries)
				{
					if (!entry.is_directory(error))
					{
						error.clear();
						continue;
					}
					const auto config = entry.path() / "config.json";
					if (std::filesystem::is_regular_file(config, error))
						RegisterConfig(config);
					error.clear();
				}
			}
			catch (const std::exception& error)
			{
				REX::ERROR(
					"DearModdingUI-MCM: discovery failed: {}"sv,
					error.what());
			}
			catch (...)
			{
				REX::ERROR("DearModdingUI-MCM: discovery failed"sv);
			}
		}

		void RefreshValues() noexcept
		{
			for (const auto& mod : s_mods)
			{
				for (const auto& page : mod->pages)
					mod->values->RefreshPage(
						*page.page,
						CurrentMcmState());
			}
		}

		void MessageListener(
			F4SE::MessagingInterface::Message* a_message) noexcept
		{
			if (!a_message)
				return;
			if (a_message->type == F4SE::MessagingInterface::kPostPostLoad)
				DiscoverAndRegister();
			else if (a_message->type ==
					F4SE::MessagingInterface::kPreLoadGame)
				s_runtimeReady.store(false, std::memory_order_release);
			else if (a_message->type ==
					F4SE::MessagingInterface::kGameDataReady)
			{
				if (a_message->data)
					RefreshValues();
			}
			else if (a_message->type ==
						F4SE::MessagingInterface::kPostLoadGame ||
					a_message->type == F4SE::MessagingInterface::kNewGame ||
					a_message->type == F4SE::MessagingInterface::kGameLoaded)
			{
				const auto wasReady =
					s_runtimeReady.exchange(true, std::memory_order_acq_rel);
				if (!wasReady)
					REX::INFO(
						"DearModdingUI-MCM: Papyrus runtime: ready"sv);
				RefreshValues();
			}
		}

		[[nodiscard]] bool InitializePlugin(
			const F4SE::LoadInterface* a_f4se) noexcept
		{
			static std::once_flag once;
			static bool initialized{ false };
			std::call_once(once, [&]() noexcept {
				F4SE::Init(a_f4se);
				auto* messaging = F4SE::GetMessagingInterface();
				if (!messaging || !messaging->RegisterListener(MessageListener))
				{
					REX::ERROR(
						"DearModdingUI-MCM: F4SE message listener registration failed"sv);
					return;
				}
				initialized = true;
				REX::INFO("DearModdingUI-MCM initialized"sv);
			});
			return initialized;
		}
	}
}

F4SE_PLUGIN_QUERY(
	const F4SE::QueryInterface* a_f4se,
	F4SE::PluginInfo* a_info)
{
	if (!a_f4se || !a_info ||
		a_f4se->RuntimeVersion() < REL::Version(F4SE::RUNTIME_1_10_163))
		return false;

	if (const auto* data = F4SE::PluginVersionData::GetSingleton())
	{
		a_info->infoVersion = F4SE::PluginInfo::kVersion;
		a_info->name = data->GetPluginName().data();
		a_info->version = data->GetPluginVersion().pack();
	}
	return true;
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	return DearModdingUI::MCM::InitializePlugin(a_f4se);
}
