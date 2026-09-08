#include <DearModdingUI/MCM/ScaleformSpike.h>

#if defined(DMUI_MCM_SCALEFORM_SPIKE)

#	include <DearModdingUI/MCM/ScaleformSpikeState.h>

#	include <DearModdingUI/Client.h>

#	include <F4SE/F4SE.h>
#	include <RE/B/BSFixedString.h>
#	include <RE/B/BSScaleformManager.h>
#	include <RE/I/IMenu.h>
#	include <RE/U/UI.h>
#	include <REX/REX.h>
#	include <Scaleform/G/GFx_Movie.h>
#	include <Scaleform/G/GFx_MovieDef.h>
#	include <Scaleform/G/GFx_Value.h>

#	include <algorithm>
#	include <atomic>
#	include <chrono>
#	include <cstdint>
#	include <format>
#	include <memory>
#	include <mutex>
#	include <optional>
#	include <string>
#	include <string_view>

namespace DearModdingUI::MCM
{
	using namespace std::literals;

	namespace
	{
		using ScaleformSpike::Advanced;
		using ScaleformSpike::CallFact;
		using ScaleformSpike::ContextGatePassed;
		using ScaleformSpike::Fact;
		using ScaleformSpike::MissingContextReason;
		using ScaleformSpike::MovieObservation;
		using ScaleformSpike::Phase;
		using ScaleformSpike::RunState;
		using ScaleformSpike::Terminal;
		using Clock = std::chrono::steady_clock;

		inline constexpr auto kLogTag = "[dmui.mcm.scaleform-spike]"sv;
		inline constexpr auto kPauseMenuName = "PauseMenu";
		inline constexpr auto kProbeMovieName = "MainMenu";
		inline constexpr auto kRunLimit = std::chrono::seconds(5);
		inline constexpr auto kCleanupLimit = std::chrono::seconds(5);

		struct PageSnapshot
		{
			Phase phase{ Phase::kIdle };
			bool resourceRetained{};
			bool inspectQueued{};
			bool canStart{ true };
			bool canStop{};
			std::string reason{ "No isolated run has been requested." };
			std::string referenceStatus{
				"Not inspected. Open the real PauseMenu/MCM first, then inspect."
			};
			MovieObservation reference;
			MovieObservation isolated;
		};

		[[nodiscard]] std::string_view PhaseText(Phase a_phase) noexcept
		{
			switch (a_phase)
			{
			case Phase::kIdle:
				return "idle";
			case Phase::kStartQueued:
				return "start queued";
			case Phase::kLoading:
				return "loading on UI task thread";
			case Phase::kRunning:
				return "observing isolated movie";
			case Phase::kReleasing:
				return "render shutdown pending; resource retained";
			case Phase::kPassed:
				return "passed and released";
			case Phase::kBlocked:
				return "blocked";
			case Phase::kTimedOut:
				return "blocked at deadline";
			case Phase::kStopped:
				return "stopped and released";
			case Phase::kCleanupBlocked:
				return "cleanup blocked; resource retained";
			default:
				return "unknown";
			}
		}

		[[nodiscard]] std::string_view FactText(Fact a_fact) noexcept
		{
			switch (a_fact)
			{
			case Fact::kMissing:
				return "missing";
			case Fact::kPresent:
				return "present";
			case Fact::kNotObserved:
			default:
				return "not observed";
			}
		}

		[[nodiscard]] std::string_view CallFactText(CallFact a_fact) noexcept
		{
			switch (a_fact)
			{
			case CallFact::kMethodMissing:
				return "method missing";
			case CallFact::kFailed:
				return "dispatch failed";
			case CallFact::kSucceeded:
				return "succeeded";
			case CallFact::kNotAttempted:
			default:
				return "not attempted";
			}
		}

		[[nodiscard]] Fact ObjectFact(
			Scaleform::GFx::Movie& a_movie,
			const char* a_path,
			Scaleform::GFx::Value* a_value = nullptr)
		{
			Scaleform::GFx::Value local;
			auto* value = a_value ? a_value : std::addressof(local);
			return a_movie.GetVariable(value, a_path) && value->IsObject() ?
				Fact::kPresent :
				Fact::kMissing;
		}

		[[nodiscard]] std::string_view ValueTypeText(
			Scaleform::GFx::Value::ValueType a_type) noexcept
		{
			using Type = Scaleform::GFx::Value::ValueType;
			switch (a_type)
			{
			case Type::kUndefined: return "undefined";
			case Type::kNull: return "null";
			case Type::kBoolean: return "Boolean";
			case Type::kInt: return "Int";
			case Type::kUInt: return "UInt";
			case Type::kNumber: return "Number";
			case Type::kString: return "String";
			case Type::kStringW: return "StringW";
			case Type::kObject: return "Object";
			case Type::kArray: return "Array";
			case Type::kDisplayObject: return "DisplayObject";
			case Type::kClosure: return "Closure";
			default: return "unknown";
			}
		}

		[[nodiscard]] std::string ValueText(
			const Scaleform::GFx::Value& a_value)
		{
			if (a_value.IsBoolean())
				return a_value.GetBoolean() ? "true" : "false";
			if (a_value.IsInt())
				return std::to_string(a_value.GetInt());
			if (a_value.IsUInt())
				return std::to_string(a_value.GetUInt());
			if (a_value.IsNumber())
				return std::format("{}", a_value.GetNumber());
			if (a_value.IsString())
				return a_value.GetString() ? a_value.GetString() : "<null string>";
			if (a_value.IsUndefined())
				return "undefined";
			if (a_value.GetType() == Scaleform::GFx::Value::ValueType::kNull)
				return "null";
			if (a_value.IsObject())
				return "<object>";
			return "<unsupported value type>";
		}

		[[nodiscard]] std::string TypedValueText(
			const Scaleform::GFx::Value& a_value)
		{
			return std::format(
				"type={} value={}",
				ValueTypeText(a_value.GetType()),
				ValueText(a_value));
		}

		void ObserveMovie(
			Scaleform::GFx::Movie& a_movie,
			MovieObservation& a_observation,
			bool a_initializeTiming)
		{
			if (auto* definition = a_movie.GetMovieDef())
			{
				if (const auto* url = definition->GetFileURL(); url && *url)
					a_observation.sourceMovieUrl = url;
				else
					a_observation.sourceMovieUrl = "movie definition URL is empty";
			}
			else
				a_observation.sourceMovieUrl = "movie definition is unavailable";

			Scaleform::GFx::Value loaderUrl;
			if (a_movie.GetVariable(&loaderUrl, "root.loaderInfo.url") &&
				loaderUrl.IsString() && loaderUrl.GetString())
				a_observation.loaderInfoUrl = loaderUrl.GetString();
			else
				a_observation.loaderInfoUrl = "missing or non-string";

			a_observation.root = ObjectFact(a_movie, "root");
			a_observation.f4se = ObjectFact(a_movie, "root.f4se");
			a_observation.mcm = ObjectFact(a_movie, "root.mcm");
			Scaleform::GFx::Value menu;
			a_observation.menu = ObjectFact(a_movie, "root.Menu_mc", &menu);
			Scaleform::GFx::Value pauseMode;
			a_observation.pauseModePath =
				a_movie.GetVariable(&pauseMode, "root.Menu_mc.PauseMode") ?
				TypedValueText(pauseMode) :
				"path lookup failed";
			Scaleform::GFx::Value pauseModeMember;
			a_observation.pauseModeMember =
				a_observation.menu == Fact::kPresent &&
					menu.GetMember("PauseMode", &pauseModeMember) ?
				TypedValueText(pauseModeMember) :
				"member lookup failed";

			Scaleform::GFx::Value bgsCodeObject;
			a_observation.bgsCodeObject =
				ObjectFact(
					a_movie,
					"root.Menu_mc.BGSCodeObj",
					&bgsCodeObject);
			a_observation.setBackgroundVisible =
				a_observation.bgsCodeObject == Fact::kPresent &&
					bgsCodeObject.HasMember("SetBackgroundVisible") ?
				Fact::kPresent :
				Fact::kMissing;

			Scaleform::GFx::Value content;
			a_observation.mcmLoaderContent =
				ObjectFact(
					a_movie,
					"root.mcm_loader.content",
					&content);
			Scaleform::GFx::Value mcmMenu;
			a_observation.mcmDocumentMenu =
				a_observation.mcmLoaderContent == Fact::kPresent &&
					content.GetMember("mcmMenu", &mcmMenu) &&
					mcmMenu.IsObject() ?
				Fact::kPresent :
				Fact::kMissing;

			Scaleform::GFx::Value codeObject;
			a_observation.mcmCodeObject =
				a_observation.mcmDocumentMenu == Fact::kPresent &&
					mcmMenu.GetMember("mcmCodeObj", &codeObject) &&
					codeObject.IsObject() ?
				Fact::kPresent :
				Fact::kMissing;
			a_observation.mcmVersionMethod =
				a_observation.mcmCodeObject == Fact::kPresent &&
					codeObject.HasMember("GetMCMVersionCode") ?
				Fact::kPresent :
				Fact::kMissing;
			if (a_observation.mcmVersionMethod == Fact::kPresent &&
				a_observation.mcmVersionCall != CallFact::kSucceeded)
			{
				Scaleform::GFx::Value result;
				if (codeObject.Invoke("GetMCMVersionCode", &result))
				{
					a_observation.mcmVersionCall = CallFact::kSucceeded;
					a_observation.mcmVersionResult = ValueText(result);
				}
				else
				{
					a_observation.mcmVersionCall = CallFact::kFailed;
					a_observation.mcmVersionResult = "native dispatch failed";
				}
			}
			else if (a_observation.mcmVersionMethod != Fact::kPresent)
			{
				a_observation.mcmVersionCall = CallFact::kMethodMissing;
				a_observation.mcmVersionResult = "method unavailable";
			}

			a_observation.movieVisible = a_movie.GetVisible();
			const auto timer = a_movie.GetASTimerMs();
			const auto frame = a_movie.GetCurrentFrame();
			if (a_initializeTiming || !a_observation.timingInitialized)
			{
				a_observation.timingInitialized = true;
				a_observation.initialTimerMs = timer;
				a_observation.initialFrame = frame;
			}
			a_observation.currentTimerMs = timer;
			a_observation.currentFrame = frame;
		}

		void LogObservation(
			std::string_view a_label,
			const MovieObservation& a_observation)
		{
			REX::INFO(
				"{} observation={} source_url=\"{}\" loader_url=\"{}\" "
				"root={} f4se={} mcm={} menu={} pause_mode_path=\"{}\" pause_mode_member=\"{}\" "
				"bgs={} background_method={} mcm_content={} mcm_menu={} mcm_code={} "
				"version_method={} native_call={} native_result=\"{}\" "
				"advances={} timer={}..{} frame={}..{} elapsed={:.3f}"sv,
				kLogTag,
				a_label,
				a_observation.sourceMovieUrl,
				a_observation.loaderInfoUrl,
				FactText(a_observation.root),
				FactText(a_observation.f4se),
				FactText(a_observation.mcm),
				FactText(a_observation.menu),
				a_observation.pauseModePath,
				a_observation.pauseModeMember,
				FactText(a_observation.bgsCodeObject),
				FactText(a_observation.setBackgroundVisible),
				FactText(a_observation.mcmLoaderContent),
				FactText(a_observation.mcmDocumentMenu),
				FactText(a_observation.mcmCodeObject),
				FactText(a_observation.mcmVersionMethod),
				CallFactText(a_observation.mcmVersionCall),
				a_observation.mcmVersionResult,
				a_observation.advanceCalls,
				a_observation.initialTimerMs,
				a_observation.currentTimerMs,
				a_observation.initialFrame,
				a_observation.currentFrame,
				a_observation.elapsedSeconds);
		}

		class Runtime
		{
		public:
			void Register() noexcept
			{
				const std::lock_guard lock{ mutex_ };
				if (registrationAttempted_)
					return;
				registrationAttempted_ = true;

				const auto version = F4SE::GetPluginVersion();
				client_ = std::make_unique<dmui::Client>(
					"dear-modding.mcm.scaleform-spike",
					"Scaleform Context Spike",
					dmui::Version{ version.major(), version.minor() },
					dmui::kForwardingClient,
					"",
					dmui::ClientOrigin{
						dmui::ClientOriginKind::kBridged,
						"MCM experiment"
					});
				if (!client_->Connect())
				{
					REX::ERROR(
						"{} client connection failed ({})"sv,
						kLogTag,
						DMUI_ResultToString(client_->LastResult()));
					reason_ = "Experimental client connection failed.";
					return;
				}

				if (!client_->AddCategory({
						.id = "experimental",
						.displayName = "Experimental"
					}))
				{
					REX::ERROR(
						"{} category registration failed ({})"sv,
						kLogTag,
						DMUI_ResultToString(client_->LastResult()));
					reason_ = "Experimental category registration failed.";
					return;
				}

				const auto page = client_->AddPage(
					{
						.id = "scaleform-context",
						.displayName = "Scaleform Context",
						.categoryId = "experimental",
						.summary = "Bounded read-only context/bootstrap probe",
						.sortKey = 1000
					},
					[this] { DrawPage(); });
				if (!page)
				{
					REX::ERROR(
						"{} page registration failed ({})"sv,
						kLogTag,
						DMUI_ResultToString(client_->LastResult()));
					reason_ = "Experimental page registration failed.";
					return;
				}

				const auto observer =
					client_->AddFrameObserver([this] { ObserveFrame(); });
				if (!observer)
				{
					REX::ERROR(
						"{} frame observer registration failed ({})"sv,
						kLogTag,
						DMUI_ResultToString(client_->LastResult()));
					reason_ =
						"Frame observer registration failed; isolated runs are disabled.";
					return;
				}

				registered_ = true;
				REX::INFO(
					"{} experimental client registered; no probe action is automatic"sv,
					kLogTag);
			}

			void RequestGameTransitionStop() noexcept
			{
				RequestStop("Game state transition requested safe teardown.");
			}

		private:
			void DrawPage() noexcept
			{
				PageSnapshot snapshot;
				{
					const std::lock_guard lock{ mutex_ };
					snapshot.phase = state_.CurrentPhase();
					snapshot.resourceRetained = state_.ResourceRetained();
					snapshot.inspectQueued =
						inspectQueued_.load(std::memory_order_acquire);
					snapshot.canStart = registered_ && !state_.IsBusy() &&
						!state_.ResourceRetained();
					snapshot.canStop =
						state_.IsBusy() || state_.ResourceRetained();
					snapshot.reason = reason_;
					snapshot.referenceStatus = referenceStatus_;
					snapshot.reference = reference_;
					snapshot.isolated = isolated_;
				}

				ImGui::TextWrapped(
					"EXPERIMENTAL CONTEXT GATE ONLY. This page measures engine "
					"bootstrap and private movie lifetime. A pass does not prove "
					"pixel rendering, input delivery, real settings writes, or "
					"embedded MCM compatibility.");
				ImGui::Spacing();
				ImGui::TextWrapped(
					"No screenshot is generated, no native objects are fabricated, "
					"and the isolated movie is never registered in or inserted into "
					"the normal menu stack.");
				ImGui::Separator();

				ImGui::BeginDisabled(snapshot.inspectQueued);
				if (ImGui::Button("Inspect real PauseMenu"))
					RequestInspect();
				ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::BeginDisabled(!snapshot.canStart);
				if (ImGui::Button("Start isolated context"))
					RequestStart();
				ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::BeginDisabled(!snapshot.canStop);
				if (ImGui::Button("Stop"))
					RequestStop("User requested safe teardown.");
				ImGui::EndDisabled();

				ImGui::Spacing();
				ImGui::Text(
					"Isolated phase: %.*s",
					static_cast<int>(PhaseText(snapshot.phase).size()),
					PhaseText(snapshot.phase).data());
				ImGui::Text(
					"Resource retained: %s",
					snapshot.resourceRetained ? "yes" : "no");
				ImGui::TextWrapped("%s", snapshot.reason.c_str());

				ImGui::Separator();
				ImGui::TextUnformatted("Real PauseMenu reference");
				ImGui::TextWrapped("%s", snapshot.referenceStatus.c_str());
				DrawObservation(snapshot.reference);

				ImGui::Separator();
				ImGui::TextUnformatted("Private engine-loaded MainMenu");
				DrawObservation(snapshot.isolated);
			}

			static void DrawObservation(const MovieObservation& a_observation)
			{
				const auto text = [&a_observation](
									  std::string_view a_label,
									  std::string_view a_value) {
					const auto line = std::format("{}: {}", a_label, a_value);
					ImGui::TextUnformatted(line.c_str());
				};
				text("Context", a_observation.context);
				text("Movie definition URL", a_observation.sourceMovieUrl);
				text("root.loaderInfo.url", a_observation.loaderInfoUrl);
				text("root", FactText(a_observation.root));
				text("root.f4se", FactText(a_observation.f4se));
				text("root.mcm", FactText(a_observation.mcm));
				text("root.Menu_mc", FactText(a_observation.menu));
				text("Menu_mc.PauseMode (path)", a_observation.pauseModePath);
				text("Menu_mc.PauseMode (member)", a_observation.pauseModeMember);
				text(
					"Menu_mc.BGSCodeObj",
					FactText(a_observation.bgsCodeObject));
				text(
					"BGSCodeObj.SetBackgroundVisible",
					FactText(a_observation.setBackgroundVisible));
				text(
					"mcm_loader.content",
					FactText(a_observation.mcmLoaderContent));
				text(
					"MCM_Main.mcmMenu",
					FactText(a_observation.mcmDocumentMenu));
				text(
					"mcmMenu.mcmCodeObj",
					FactText(a_observation.mcmCodeObject));
				text(
					"GetMCMVersionCode",
					FactText(a_observation.mcmVersionMethod));
				text(
					"Read-only native call",
					CallFactText(a_observation.mcmVersionCall));
				text("Native call result", a_observation.mcmVersionResult);
				text(
					"Movie visible",
					a_observation.timingInitialized ?
						(a_observation.movieVisible ? "true" : "false") :
						"not observed");
				text(
					"Advance calls",
					std::to_string(a_observation.advanceCalls));
				text(
					"AS timer",
					a_observation.timingInitialized ?
						std::format(
							"{} -> {} ms",
							a_observation.initialTimerMs,
							a_observation.currentTimerMs) :
						"not observed");
				text(
					"Current frame",
					a_observation.timingInitialized ?
						std::format(
							"{} -> {}",
							a_observation.initialFrame,
							a_observation.currentFrame) :
						"not observed");
				text(
					"Observed advancement",
					a_observation.advanceCalls == 0 ?
						"not measured (single observation)" :
						(Advanced(a_observation) ? "yes" : "no"));
				text(
					"Elapsed",
					a_observation.timingInitialized ?
						std::format("{:.3f} s", a_observation.elapsedSeconds) :
						"not observed");
			}

			void RequestInspect() noexcept
			{
				bool expected{ false };
				if (!inspectQueued_.compare_exchange_strong(
						expected,
						true,
						std::memory_order_acq_rel))
					return;
				{
					const std::lock_guard lock{ mutex_ };
					referenceStatus_ = "Inspection queued on the UI task thread.";
				}
				const auto* tasks = F4SE::GetTaskInterface();
				if (!tasks)
				{
					inspectQueued_.store(false, std::memory_order_release);
					const std::lock_guard lock{ mutex_ };
					referenceStatus_ =
						"Blocked: F4SE UI task interface is unavailable.";
					REX::ERROR("{} reference inspection could not be queued"sv, kLogTag);
					return;
				}
				tasks->AddUITask([this] {
					InspectReferenceUi();
					inspectQueued_.store(false, std::memory_order_release);
				});
			}

			void InspectReferenceUi() noexcept
			{
				const std::lock_guard lock{ mutex_ };
				reference_ = {};
				reference_.context = "real PauseMenu from UI::menuMap";
				auto* ui = RE::UI::GetSingleton();
				if (!ui)
				{
					referenceStatus_ = "Blocked: engine UI singleton is unavailable.";
					REX::WARN("{} real PauseMenu inspection blocked: UI unavailable"sv, kLogTag);
					return;
				}
				auto menu = ui->GetMenu(RE::BSFixedString{ kPauseMenuName });
				if (!menu || !menu->uiMovie)
				{
					referenceStatus_ =
						"Blocked: no real PauseMenu movie is loaded. Open the "
						"pause/MCM menu and inspect again.";
					REX::INFO("{} real PauseMenu inspection found no loaded movie"sv, kLogTag);
					return;
				}
				ObserveMovie(*menu->uiMovie, reference_, true);
				referenceStatus_ = menu->OnStack() ?
					"Inspected the real on-stack PauseMenu read-only." :
					"Inspected a loaded PauseMenu read-only; it was not on the menu stack.";
				REX::INFO(
					"{} real PauseMenu inspected: {}"sv,
					kLogTag,
					referenceStatus_);
				LogObservation("reference", reference_);
			}

			void RequestStart() noexcept
			{
				std::uint64_t generation{};
				{
					const std::lock_guard lock{ mutex_ };
					const auto request = state_.RequestStart();
					if (!request.accepted)
					{
						reason_ =
							"Start rejected: a run or retained cleanup is already active.";
						return;
					}
					generation = request.generation;
					isolated_ = {};
					isolated_.context =
						"private IMenu; not registered and not on the menu stack";
					reason_ = "Start queued on the UI task thread.";
				}
				REX::INFO("{} start requested (generation {})"sv, kLogTag, generation);
				const auto* tasks = F4SE::GetTaskInterface();
				if (!tasks)
				{
					const std::lock_guard lock{ mutex_ };
					(void)state_.BeginTerminal(generation, Terminal::kBlocked);
					reason_ = "Blocked: F4SE UI task interface is unavailable.";
					REX::ERROR("{} start could not be queued"sv, kLogTag);
					return;
				}
				tasks->AddUITask([this, generation] { StartUi(generation); });
			}

			void StartUi(std::uint64_t a_generation) noexcept
			{
				const std::lock_guard lock{ mutex_ };
				if (!state_.BeginLoad(a_generation))
					return;
				reason_ = "Loading MainMenu through BSScaleformManager::LoadMovie.";
				auto* ui = RE::UI::GetSingleton();
				if (!ui)
				{
					(void)state_.BeginTerminal(
						a_generation,
						Terminal::kBlocked);
					reason_ = "Blocked: engine UI singleton is unavailable.";
					LogFinalLocked();
					return;
				}
				if (ui->GetMenuOpen(RE::BSFixedString{ kPauseMenuName }))
				{
					(void)state_.BeginTerminal(
						a_generation,
						Terminal::kBlocked);
					reason_ =
						"Blocked: the real PauseMenu is active. Close it before "
						"starting an isolated run.";
					LogFinalLocked();
					return;
				}
				auto* scaleform = RE::BSScaleformManager::GetSingleton();
				if (!scaleform)
				{
					(void)state_.BeginTerminal(
						a_generation,
						Terminal::kBlocked);
					reason_ = "Blocked: BSScaleformManager is unavailable.";
					LogFinalLocked();
					return;
				}

				menu_ = std::make_unique<RE::IMenu>();
				const auto loaded = scaleform->LoadMovie(
					*menu_,
					menu_->uiMovie,
					kProbeMovieName,
					nullptr);
				if (menu_->uiMovie)
					(void)state_.AcquireResource(a_generation);
				if (!loaded || !menu_->uiMovie)
				{
					reason_ = loaded ?
						"Blocked: engine load returned no movie." :
						"Blocked: BSScaleformManager::LoadMovie rejected MainMenu.";
					if (!menu_->uiMovie)
					{
						menu_.reset();
						(void)state_.BeginTerminal(
							a_generation,
							Terminal::kBlocked);
						LogFinalLocked();
						return;
					}
					BeginTeardownLocked(
						a_generation,
						Terminal::kBlocked,
						reason_);
					return;
				}

				menu_->uiMovie->SetVisible(false);
				runStartedAt_ = Clock::now();
				lastAdvanceAt_ = runStartedAt_;
				runDeadline_ = runStartedAt_ + kRunLimit;
				ObserveMovie(*menu_->uiMovie, isolated_, true);
				if (!state_.MarkRunning(a_generation))
				{
					BeginTeardownLocked(
						a_generation,
						Terminal::kStopped,
						"Run was cancelled while the engine movie was loading.");
					return;
				}
				reason_ =
					"Observing bootstrap and advancement for at most 5 seconds.";
				REX::INFO(
					"{} private MainMenu loaded (generation {}, url=\"{}\")"sv,
					kLogTag,
					a_generation,
					isolated_.sourceMovieUrl);
			}

			void RequestStop(std::string_view a_reason) noexcept
			{
				std::optional<std::uint64_t> generation;
				{
					const std::lock_guard lock{ mutex_ };
					const auto settled = state_.CurrentPhase() == Phase::kReleasing ||
						state_.CurrentPhase() == Phase::kCleanupBlocked;
					generation = state_.Cancel();
					if (!generation)
						return;
					if (!settled)
						reason_ = a_reason;
				}
				REX::INFO(
					"{} stop requested (generation {}, reason=\"{}\")"sv,
					kLogTag, *generation, a_reason);
				const auto* tasks = F4SE::GetTaskInterface();
				if (!tasks)
				{
					const std::lock_guard lock{ mutex_ };
					if (state_.ResourceRetained())
					{
						(void)state_.MarkCleanupBlocked(*generation);
						reason_.append(
							" Cleanup blocked: F4SE UI task interface is unavailable; "
							"the movie remains retained.");
					}
					REX::ERROR("{} stop could not be queued"sv, kLogTag);
					return;
				}
				tasks->AddUITask(
					[this, generation = *generation] { StopUi(generation); });
			}

			void StopUi(std::uint64_t a_generation) noexcept
			{
				const std::lock_guard lock{ mutex_ };
				if (!state_.IsCurrent(a_generation))
					return;
				if (!menu_ || !menu_->uiMovie)
				{
					menu_.reset();
					if (state_.ResourceRetained())
						(void)state_.CompleteRelease(a_generation);
					LogFinalLocked();
					return;
				}
				if (!shutdownStarted_)
					ObserveMovie(*menu_->uiMovie, isolated_, false);
				BeginShutdownLocked(a_generation);
				PollReleaseLocked(a_generation);
			}

			void ObserveFrame() noexcept
			{
				std::uint64_t generation{};
				{
					const std::lock_guard lock{ mutex_ };
					if (!state_.NeedsOwnerTick())
						return;
					generation = state_.Generation();
				}
				bool expected{ false };
				if (!tickQueued_.compare_exchange_strong(
						expected,
						true,
						std::memory_order_acq_rel))
					return;
				const auto* tasks = F4SE::GetTaskInterface();
				if (!tasks)
				{
					tickQueued_.store(false, std::memory_order_release);
					const std::lock_guard lock{ mutex_ };
					if (state_.ResourceRetained())
						(void)state_.MarkCleanupBlocked(generation);
					reason_ =
						"Owner-thread progress blocked: F4SE UI task interface "
						"is unavailable.";
					return;
				}
				tasks->AddUITask([this, generation] {
					TickUi(generation);
					tickQueued_.store(false, std::memory_order_release);
				});
			}

			void TickUi(std::uint64_t a_generation) noexcept
			{
				const std::lock_guard lock{ mutex_ };
				if (!state_.IsCurrent(a_generation))
					return;
				if (state_.CurrentPhase() == Phase::kReleasing ||
					state_.CurrentPhase() == Phase::kCleanupBlocked)
				{
					PollReleaseLocked(a_generation);
					return;
				}
				if (state_.CurrentPhase() != Phase::kRunning ||
					!menu_ || !menu_->uiMovie)
					return;

				auto* ui = RE::UI::GetSingleton();
				if (ui &&
					ui->GetMenuOpen(RE::BSFixedString{ kPauseMenuName }))
				{
					BeginTeardownLocked(
						a_generation,
						Terminal::kBlocked,
						"Blocked: the real PauseMenu became active during the "
						"isolated run.");
					return;
				}

				const auto now = Clock::now();
				const auto delta =
					std::chrono::duration<float>(now - lastAdvanceAt_).count();
				lastAdvanceAt_ = now;
				menu_->uiMovie->Advance(
					(std::clamp)(delta, 0.0F, 0.1F),
					2,
					false);
				++isolated_.advanceCalls;
				isolated_.elapsedSeconds =
					std::chrono::duration<double>(now - runStartedAt_).count();
				ObserveMovie(*menu_->uiMovie, isolated_, false);

				if (ContextGatePassed(isolated_))
				{
					BeginTeardownLocked(
						a_generation,
						Terminal::kPassed,
						"Context gate passed: genuine PauseMenu/MCM/native "
						"context was observed and the private movie advanced. "
						"This does not prove rendering or input support.");
					return;
				}
				if (now >= runDeadline_)
					BeginTeardownLocked(
						a_generation,
						Terminal::kTimedOut,
						MissingContextReason(isolated_));
			}

			void BeginTeardownLocked(
				std::uint64_t a_generation,
				Terminal a_terminal,
				std::string a_reason)
			{
				if (!state_.IsCurrent(a_generation))
					return;
				if (menu_ && menu_->uiMovie)
					ObserveMovie(*menu_->uiMovie, isolated_, false);
				if (!state_.BeginTerminal(a_generation, a_terminal))
					return;
				reason_ = std::move(a_reason);
				if (!state_.ResourceRetained())
				{
					menu_.reset();
					LogFinalLocked();
					return;
				}
				LogFinalLocked();
				BeginShutdownLocked(a_generation);
				PollReleaseLocked(a_generation);
			}

			void BeginShutdownLocked(std::uint64_t a_generation)
			{
				if (!state_.IsCurrent(a_generation) ||
					!state_.ResourceRetained() ||
					shutdownStarted_)
					return;
				shutdownStarted_ = true;
				cleanupDeadline_ = Clock::now() + kCleanupLimit;
				menu_->uiMovie->ShutdownRendering(false);
			}

			void PollReleaseLocked(std::uint64_t a_generation)
			{
				if (!state_.IsCurrent(a_generation) ||
					!state_.ResourceRetained() ||
					!menu_ || !menu_->uiMovie)
					return;
				if (menu_->uiMovie->IsShutdownRenderingComplete())
				{
					menu_.reset();
					shutdownStarted_ = false;
					cleanupBlockedLogged_ = false;
					if (state_.CompleteRelease(a_generation))
					{
						reason_.append(" Private movie released on the UI task thread.");
						LogFinalLocked();
					}
					return;
				}
				if (Clock::now() < cleanupDeadline_)
					return;
				if (state_.MarkCleanupBlocked(a_generation))
				{
					reason_.append(
						" Rendering shutdown did not complete within 5 seconds; "
						"the resource remains retained and owner-thread polling "
						"will continue.");
				}
				if (!cleanupBlockedLogged_)
				{
					cleanupBlockedLogged_ = true;
					REX::WARN(
						"{} rendering shutdown still pending; private movie retained"sv,
						kLogTag);
				}
			}

			void LogFinalLocked() const
			{
				REX::INFO(
					"{} generation={} phase={} retained={} reason=\"{}\""sv,
					kLogTag,
					state_.Generation(),
					PhaseText(state_.CurrentPhase()),
					state_.ResourceRetained(),
					reason_);
				LogObservation("isolated", isolated_);
			}

			std::mutex mutex_;
			RunState state_;
			std::unique_ptr<dmui::Client> client_;
			std::unique_ptr<RE::IMenu> menu_;
			MovieObservation reference_;
			MovieObservation isolated_;
			std::string referenceStatus_{
				"Not inspected. Open the real PauseMenu/MCM first, then inspect."
			};
			std::string reason_{ "No isolated run has been requested." };
			Clock::time_point runStartedAt_{};
			Clock::time_point lastAdvanceAt_{};
			Clock::time_point runDeadline_{};
			Clock::time_point cleanupDeadline_{};
			std::atomic_bool inspectQueued_{ false };
			std::atomic_bool tickQueued_{ false };
			bool registrationAttempted_{};
			bool registered_{};
			bool shutdownStarted_{};
			bool cleanupBlockedLogged_{};
		};

		[[nodiscard]] Runtime& GetRuntime()
		{
			static auto* runtime = new Runtime();
			return *runtime;
		}
	}

	void RegisterScaleformSpike() noexcept
	{
		GetRuntime().Register();
	}

	void StopScaleformSpikeForGameTransition() noexcept
	{
		GetRuntime().RequestGameTransitionStop();
	}
}

#else

namespace DearModdingUI::MCM
{
	void RegisterScaleformSpike() noexcept {}
	void StopScaleformSpikeForGameTransition() noexcept {}
}

#endif
