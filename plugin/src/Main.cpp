#include <DearModdingUI/MCM/Availability.h>
#include <DearModdingUI/MCM/ExternalEventDispatcher.h>
#include <DearModdingUI/MCM/F4SETaskScheduler.h>
#include <DearModdingUI/MCM/GlobalValueSource.h>
#include <DearModdingUI/MCM/ModSettingValueSource.h>
#include <DearModdingUI/MCM/PapyrusActionExecutor.h>
#include <DearModdingUI/MCM/PropertyValueSource.h>
#include <DearModdingUI/MCM/SettingsIni.h>

#include <DearModdingUI/MCM/Compatibility.h>
#include <DearModdingUI/MCM/TextRendering.h>

#include <DearModdingUI/Client.h>

#include <F4SE/F4SE.h>
#include <RE/B/BSScript_IStackCallbackFunctor.h>
#include <RE/B/BSScript_IVirtualMachine.h>
#include <RE/B/BSScript_Variable.h>
#include <RE/B/BSScriptUtil.h>
#include <RE/G/GameScript.h>
#include <REX/REX.h>

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

		constexpr auto kAbsentNote =
			"Mod Configuration Menu is not installed, so these values cannot be changed."sv;

		std::atomic<AvailabilityState> s_mcmAvailability{
			AvailabilityState::kUnknown
		};
		GlobalValueSource s_globalValues;
		F4SETaskScheduler s_scheduler;
		ExternalEventDispatcher s_events{ s_scheduler };
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

		void ComposeMcmAvailability(MappedPage& a_page)
		{
			auto hasModSettings = false;
			for (const auto& row : a_page.rows)
			{
				if (!row.binding)
					continue;
				const auto family = row.binding->Family();
				hasModSettings = hasModSettings ||
					family == SourceFamily::kModSetting;
				for (auto& group : a_page.settings.groups)
				{
					for (auto& descriptor : group.settings)
					{
						if (descriptor.id != row.binding->descriptorId)
							continue;
						auto prior = std::move(descriptor.isEnabled);
						descriptor.isEnabled =
							[family, prior = std::move(prior)] {
								if (!IsControlOperable(
										s_mcmAvailability.load(
											std::memory_order_acquire),
										family))
									return false;
								return !prior || prior();
							};
					}
				}
			}

			auto prior = std::move(a_page.settings.prepareView);
			a_page.settings.prepareView =
				[prior = std::move(prior), hasModSettings, noteAdded = false](
					dmui::SettingsPage& a_settings) mutable {
					if (prior)
						prior(a_settings);
					if (hasModSettings &&
						!noteAdded &&
						s_mcmAvailability.load(std::memory_order_acquire) ==
							AvailabilityState::kAbsent)
					{
						a_settings.notes.push_back({
							std::string{ kAbsentNote },
							false
						});
						noteAdded = true;
					}
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
					s_scheduler);
				mod->properties =
					std::make_unique<PropertyValueSource>(s_scheduler);
				mod->values = std::make_unique<CompositeValueSource>();
				mod->actions =
					std::make_unique<PapyrusActionExecutor>(s_scheduler);
				mod->values->Add(s_globalValues);
				mod->values->Add(*mod->modSettings);
				mod->values->Add(*mod->properties);
				mod->client = std::make_unique<dmui::Client>(
					ClientId(configuration.modName, folder),
					displayName,
					dmui::Version{ 1, 0 },
					dmui::kForwardingClient);
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

				size_t descriptors{};
				const auto declarations =
					LoadSettingsIni(a_config.parent_path() / "settings.ini");
				for (size_t index = 0; index < result.pages.size(); ++index)
				{
					auto page =
						std::make_unique<MappedPage>(std::move(result.pages[index]));
					ApplyDeclarations(*page, declarations);
					const auto summary = SummarizeCompatibility(*page);
					SurfaceCompatibility(*page, summary);
					REX::INFO(
						"DearModdingUI-MCM: {} / {} compatibility: "
						"{} bindings, {} unsupported, {} unknown sources, "
						"{} undeclared settings, {} actions, {} images"sv,
						displayName,
						page->displayName,
						summary.bindings,
						summary.unsupported,
						summary.unknownBindings,
						summary.undeclaredModSettings,
						summary.actions,
						summary.images);
					descriptors += DescriptorCount(*page);
					BindPage(*page, *mod->values);
					BindActions(*page, *mod->actions, *mod->values);
					AttachTextRendering(*page);
					ComposeMcmAvailability(*page);
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
						page->id.c_str(),
						page->displayName.c_str(),
						displayName.c_str(),
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
						},
						nullptr,
						static_cast<int32_t>(index));
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
					[observed](const dmui::PageActivity& a_activity) {
						if (a_activity.kind == dmui::PageActivityKind::kActivated)
							s_events.MenuOpened();
						if (a_activity.kind == dmui::PageActivityKind::kDeactivated)
						{
							s_events.MenuClosed();
							return;
						}
						for (const auto& registered : observed->pages)
						{
							if (registered.handle == a_activity.activePage)
							{
								observed->values->RefreshPage(*registered.page);
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
					mod->values->RefreshPage(*page.page);
			}
		}

		class McmInstalledCallback final :
			public RE::BSScript::IStackCallbackFunctor
		{
		public:
			void CallQueued() override {}
			void CallCanceled() override {}
			void StartMultiDispatch() override {}
			void EndMultiDispatch() override {}

			void operator()(RE::BSScript::Variable a_result) override
			{
				if (!a_result.is<bool>())
					return;
				const auto present = RE::BSScript::get<bool>(a_result);
				s_mcmAvailability.store(
					present ? AvailabilityState::kPresent :
							  AvailabilityState::kAbsent,
					std::memory_order_release);
				REX::INFO(
					"DearModdingUI-MCM: Mod Configuration Menu is {}"sv,
					present ? "installed" : "not installed");
			}
		};

		void QueryMcmAvailability() noexcept
		{
			try
			{
				s_scheduler.Schedule([] {
					auto* gameVm = RE::GameVM::GetSingleton();
					auto vm = gameVm ? gameVm->GetVM() : nullptr;
					if (!vm)
						return;

					RE::BSTSmartPointer<
						RE::BSScript::IStackCallbackFunctor> callback{
						new McmInstalledCallback
					};
					if (!vm->DispatchStaticCall(
							RE::BSFixedString{ "MCM" },
							RE::BSFixedString{ "IsInstalled" },
							callback))
					{
						s_mcmAvailability.store(
							AvailabilityState::kAbsent,
							std::memory_order_release);
						REX::INFO(
							"DearModdingUI-MCM: Mod Configuration Menu is not installed"sv);
					}
				});
			}
			catch (...)
			{
				REX::ERROR(
					"DearModdingUI-MCM: MCM.IsInstalled query failed"sv);
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
					 F4SE::MessagingInterface::kGameDataReady &&
					 a_message->data)
			{
				RefreshValues();
				QueryMcmAvailability();
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
