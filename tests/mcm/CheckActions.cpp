#include <DearModdingUI/MCM/ActionExecutor.h>
#include <DearModdingUI/MCM/CachedAsyncValueSource.h>
#include <DearModdingUI/MCM/ValueSource.h>

#include "../Harness.h"
#include "../FakeDiagnosticReporter.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI::MCM;

		FakeDiagnosticReporter diagnostics;

		class ActionValueSource final : public ValueSource
		{
		public:
			[[nodiscard]] bool Supports(SourceFamily) const noexcept override
			{
				return true;
			}

			[[nodiscard]] ValueSnapshot Read(
				const MappedBinding&) const override
			{
				return ReadyValue{ value, generation };
			}

			[[nodiscard]] uint64_t Refresh(const MappedBinding&) override
			{
				return ++generation;
			}

			[[nodiscard]] ValueSnapshot Write(
				const MappedBinding&,
				const dmui::SettingValue& a_value) override
			{
				value = a_value;
				return ReadyValue{ value, ++generation };
			}

			dmui::SettingValue value{ int64_t{ 7 } };
			uint64_t generation{};
		};

		class DeferredActionValueSource final : public CachedAsyncValueSource
		{
		public:
			explicit DeferredActionValueSource(
				DiagnosticReporter& a_diagnostics) :
				CachedAsyncValueSource(a_diagnostics)
			{}

			[[nodiscard]] bool Supports(SourceFamily) const noexcept override
			{
				return true;
			}

			[[nodiscard]] uint64_t Refresh(
				const MappedBinding& a_binding) override
			{
				return Cache().BeginRefresh(a_binding.cacheKey);
			}

			[[nodiscard]] ValueSnapshot Write(
				const MappedBinding& a_binding,
				const dmui::SettingValue& a_value) override
			{
				return Write(a_binding, a_value, {});
			}

			[[nodiscard]] ValueSnapshot Write(
				const MappedBinding& a_binding,
				const dmui::SettingValue& a_value,
				ValueWriteCompletion a_completion) override
			{
				auto stored = Cache().Store(a_binding.cacheKey, a_value);
				pending.push_back({
					a_binding.cacheKey,
					stored.snapshot,
					stored.settlementToken,
					a_value,
					std::move(a_completion)
				});
				return stored.snapshot;
			}

			void Release(size_t a_index, bool a_succeeded)
			{
				auto& write = pending.at(a_index);
				ValueWriteResult result = a_succeeded ?
					ValueWriteResult{ write.value } :
					ValueWriteResult{
						std::unexpected("fixture Papyrus write was rejected")
					};
				QueueWriteCompletion(
					write.key,
					write.settlementToken,
					a_succeeded ?
						write.snapshot :
						ValueSnapshot{ FailedValue{
							Generation(write.snapshot)
						} },
					[completion = std::move(write.completion),
					 result = std::move(result)]() mutable {
						if (completion)
							completion(std::move(result));
					});
				Pump();
			}

			void ReleaseThrowingCompletion()
			{
				const auto generation = Cache().BeginRefresh("throwing");
				QueueCompletion(
					"throwing",
					ReadyValue{ true, generation },
					[] { throw std::runtime_error("fixture completion"); });
				Pump();
			}

			struct PendingWrite
			{
				std::string key;
				ValueSnapshot snapshot;
				uint64_t settlementToken{};
				dmui::SettingValue value;
				ValueWriteCompletion completion;
			};

			std::vector<PendingWrite> pending;
		};

		[[nodiscard]] const std::vector<ActionArgument>& Arguments(
			const Action& a_action)
		{
			return std::visit(
				[](const auto& a_typed)
					-> const std::vector<ActionArgument>& {
					using T = std::remove_cvref_t<decltype(a_typed)>;
					if constexpr (std::same_as<T, RunConsoleCommandAction>)
					{
						static const std::vector<ActionArgument> empty;
						return empty;
					}
					else
					{
						return a_typed.arguments;
					}
				},
				a_action);
		}

		class FakeActionExecutor final : public ActionExecutor
		{
		public:
			[[nodiscard]] std::optional<std::string> UnsupportedReason(
				const Action&) const noexcept override
			{
				return std::nullopt;
			}

			void Execute(
				ActionInvocation a_invocation,
				ActionCompletion a_completion) override
			{
				if (throws)
					throw std::runtime_error("fixture exception");
				auto arguments = BindActionArguments(
					Arguments(a_invocation.action),
					a_invocation.value);
				invocations.push_back(std::move(a_invocation));
				bound.push_back(arguments ?
					*arguments :
					std::vector<BoundActionArgument>{});
				a_completion({
					arguments ?
						ActionExecutionStatus::kSucceeded :
						ActionExecutionStatus::kFailed,
					arguments ?
						std::optional<std::string>{} :
						std::optional<std::string>{
							std::move(arguments.error())
						}
				});
			}
			bool throws{};
			std::vector<ActionInvocation> invocations;
			std::vector<std::vector<BoundActionArgument>> bound;
		};

		class ImmediateTaskScheduler final : public TaskScheduler
		{
		public:
			void Schedule(std::function<void()> a_work) override
			{
				a_work();
			}

			void ScheduleUi(std::function<void()> a_work) override
			{
				a_work();
			}

			void ScheduleBackground(std::function<void()> a_work) override
			{
				a_work();
			}

		};

		class FakeScaleformInvoker final : public ScaleformInvoker
		{
		public:
			[[nodiscard]] ScaleformInvocationStatus Invoke(
				std::string_view,
				std::string_view,
				const std::vector<ScaleformArgument>& a_arguments) noexcept override
			{
				arguments = a_arguments;
				return ScaleformInvocationStatus::kSucceeded;
			}

			std::vector<ScaleformArgument> arguments;
		};

		[[nodiscard]] dmui::SettingsActionRow& ActionNamed(
			MappedPage& a_page,
			std::string_view a_id)
		{
			for (auto& group : a_page.settings.groups)
				for (auto& action : group.actionRows)
					if (action.id == a_id)
						return action;
			throw Failure("missing action row");
		}

		[[nodiscard]] dmui::SettingDescriptor& SettingNamed(
			MappedPage& a_page,
			std::string_view a_id)
		{
			for (auto& group : a_page.settings.groups)
				for (auto& setting : group.settings)
					if (setting.id == a_id)
						return setting;
			throw Failure("missing action setting");
		}

		[[nodiscard]] bool HasActionFailure(const MappedPage& a_page)
		{
			return std::ranges::any_of(
				a_page.settings.notes,
				[](const dmui::SettingsPageNote& a_note) {
					return a_note.text.find("Action '") != std::string::npos;
				});
		}

		[[nodiscard]] std::string_view ActionFailureText(
			const MappedPage& a_page)
		{
			const auto found = std::ranges::find_if(
				a_page.settings.notes,
				[](const dmui::SettingsPageNote& a_note) {
					return a_note.text.find("Action '") != std::string::npos;
				});
			return found == a_page.settings.notes.end() ?
				std::string_view{} :
				found->text;
		}
	}

	void run_mcm_action_checks(Runner& runner)
	{
		runner.test("MCM dropped async completion reaches the diagnostic reporter", [] {
			diagnostics.diagnostics.clear();
			DeferredActionValueSource values{ diagnostics };
			values.ReleaseThrowingCompletion();
			require(
				diagnostics.diagnostics.size() == 1 &&
					diagnostics.diagnostics.front().severity ==
						DiagnosticSeverity::kError &&
					diagnostics.diagnostics.front().source ==
						"async value completion" &&
					diagnostics.diagnostics.front().location == "throwing",
				"a dropped async completion remained silent");
		});

		runner.test("MCM action executor receives member and global actions", [] {
			auto result = ParseConfig(R"json({
				"modName":"Actions",
				"content":[
					{"id":"member","type":"button","text":"Member","action":{
						"type":"CallFunction","form":"Fixture.esp|1",
						"function":"Apply","params":[true,4,1.5,"text"]}},
					{"id":"global","type":"button","text":"Global","action":{
						"type":"CallGlobalFunction","script":"Fixture",
						"function":"Apply"}},
					{"id":"setting","type":"slider",
					 "valueOptions":{"sourceType":"GlobalValueInt",
						"sourceForm":"Fixture.esp|1","default":0},
					 "action":{"type":"CallGlobalFunction","script":"Fixture",
						"function":"Apply","params":["{i}{value}"]}}
				]
			})json");
			auto& page = result.pages.front();
			ActionValueSource values;
			FakeActionExecutor executor;
			BindPage(page, values);
			BindActions(page, executor, values, diagnostics);

			ActionNamed(page, "member").activate();
			ActionNamed(page, "global").activate();
			require(executor.invocations.size() == 2 &&
					std::holds_alternative<CallFunctionAction>(
						executor.invocations[0].action) &&
					std::holds_alternative<CallGlobalFunctionAction>(
						executor.invocations[1].action),
				"implemented action forms did not reach the executor");
			require(executor.bound[0] ==
					std::vector<BoundActionArgument>{
						true,
						uint64_t{ 4 },
						1.5,
						std::string{ "text" }
					},
				"typed action arguments changed while binding");

			const auto effective = SettingNamed(page, "setting").binding.set(
				dmui::SettingValue{ int64_t{ 12 } });
			require(std::get<int64_t>(effective) == 1 &&
					executor.bound.size() == 3 &&
					executor.bound.back() ==
						std::vector<BoundActionArgument>{ int64_t{ 1 } },
				"a synchronous value action did not fire with the effective value");
		});

		runner.test("MCM superseded writes collapse to the latest action", [] {
			auto result = ParseConfig(R"json({
				"modName":"Actions",
				"content":[{"id":"setting","type":"switcher",
					"valueOptions":{"sourceType":"GlobalValueBool",
						"sourceForm":"Fixture.esp|1","default":false},
					"action":{"type":"CallExternalFunction",
						"plugin":"Fixture","function":"Apply",
						"params":["{value}"]}}]
			})json");
			auto& page = result.pages.front();
			DeferredActionValueSource values{ diagnostics };
			FakeActionExecutor executor;
			BindPage(page, values);
			BindActions(page, executor, values, diagnostics);

			auto& setting = SettingNamed(page, "setting");
			(void)setting.binding.set(dmui::SettingValue{ true });
			(void)setting.binding.set(dmui::SettingValue{ false });
			require(executor.invocations.empty(),
				"a value action fired before its deferred write settled");
			values.Release(0, true);
			values.Release(1, true);
			require(executor.invocations.size() == 1 &&
					executor.bound.front() ==
						std::vector<BoundActionArgument>{ false },
				"a superseded write fired or displaced the latest action");

			(void)setting.binding.set(dmui::SettingValue{ true });
			values.RefreshPage(page, { true, true });
			values.Release(2, true);
			require(executor.invocations.size() == 2,
				"a page refresh canceled a pending value action callback");

			(void)setting.binding.set(dmui::SettingValue{ false });
			values.Release(3, false);
			page.settings.prepareView(page.settings);
			require(executor.invocations.size() == 2 &&
					ActionFailureText(page).find("Papyrus write was rejected") !=
						std::string_view::npos,
				"a failed write fired its action or lost its specific reason");
		});

		runner.test("MCM Scaleform seam substitutes embedded value parameters", [] {
			auto parsed = ParseConfig(R"json({
				"modName":"Actions",
				"content":[{"id":"external","type":"slider",
					"valueOptions":{"sourceType":"GlobalValueInt",
						"sourceForm":"Fixture.esp|1","default":0},
					"action":{"type":"CallExternalFunction",
						"plugin":"FixturePlugin","function":"Apply",
						"params":["offset-{value}","{s}copy-{value}",
							"{i}{value}","{f}{value}","{b}{value}"]}}]
			})json");
			const auto& action = std::get<CallExternalFunctionAction>(
				*parsed.pages.front().rows.front().action);
			FakeScaleformInvoker scaleform;
			const auto result = InvokeExternalFunction(
				scaleform,
				action,
				dmui::SettingValue{ int64_t{ 12 } });
			require(result.status == ActionExecutionStatus::kSucceeded &&
					scaleform.arguments == std::vector<ScaleformArgument>{
						std::string{ "offset-12" },
						std::string{ "copy-12" },
						int64_t{ 12 },
						double{ 12.0 },
						true
					},
				"Scaleform parameter substitution lost MCM value semantics");
		});

		runner.test("MCM action executor exceptions stay inside the page", [] {
			auto result = ParseConfig(R"json({
				"modName":"Actions",
				"content":[{"id":"throwing","type":"button","action":{
					"type":"CallFunction","form":"Fixture.esp|1",
					"function":"Apply"}}]
			})json");
			auto& page = result.pages.front();
			ActionValueSource values;
			FakeActionExecutor executor;
			executor.throws = true;
			BindPage(page, values);
			BindActions(page, executor, values, diagnostics);

			ActionNamed(page, "throwing").activate();
			page.settings.prepareView(page.settings);
			require(HasActionFailure(page),
				"an executor exception escaped or was silently discarded");

			ImmediateTaskScheduler scheduler;
			std::optional<ActionExecutionResult> scheduled;
			ScheduleActionExecution(
				scheduler,
				diagnostics,
				[](const ActionCompletion&) {
					throw std::runtime_error("scheduled fixture exception");
				},
				[&](ActionExecutionResult a_result) {
					scheduled = std::move(a_result);
				});
			require(scheduled &&
					scheduled->status == ActionExecutionStatus::kFailed,
				"a scheduled executor exception lost its completion");
		});


	}
}
