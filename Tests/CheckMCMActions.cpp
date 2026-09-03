#include <DearModdingUI/MCM/ActionExecutor.h>
#include <DearModdingUI/MCM/ValueSource.h>

#include "Harness.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI::MCM;

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
				return ++refreshes;
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
			uint64_t refreshes{};
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
				const Action& a_action) const noexcept override
			{
				if (std::holds_alternative<CallExternalFunctionAction>(a_action))
					return "Scaleform movie unavailable.";
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
				if (requireSingleInt &&
					(!arguments ||
					 arguments->size() != 1 ||
					 !std::holds_alternative<int64_t>(
						 arguments->front())))
					arguments = std::unexpected(
						"fixture function argument signature does not match");
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
			bool requireSingleInt{};
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
	}

	void run_mcm_action_checks(Runner& runner)
	{
		runner.test("MCM action executor receives member and global actions", [] {
			auto result = ParseConfig(R"json({
				"modName":"Actions",
				"content":[
					{"id":"member","type":"button","text":"Member","action":{
						"type":"CallFunction","form":"Fixture.esp|1",
						"function":"Apply","params":[true,4,1.5,"text"]}},
					{"id":"global","type":"button","text":"Global","action":{
						"type":"CallGlobalFunction","script":"Fixture",
						"function":"Apply"}}
				]
			})json");
			auto& page = result.pages.front();
			ActionValueSource values;
			FakeActionExecutor executor;
			BindPage(page, values);
			BindActions(page, executor, values);

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
		});

		runner.test("MCM action value placeholders bind effective values", [] {
			auto result = ParseConfig(R"json({
				"modName":"Actions",
				"content":[
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
			BindActions(page, executor, values);

			const auto effective = SettingNamed(page, "setting").binding.set(
				dmui::SettingValue{ int64_t{ 12 } });
			require(std::get<int64_t>(effective) == 12 &&
					executor.bound.size() == 1 &&
					executor.bound.front() ==
						std::vector<BoundActionArgument>{ int64_t{ 12 } },
				"the value placeholder did not receive the effective value");
		});

		runner.test("MCM action argument validation failures are diagnosed", [] {
			auto result = ParseConfig(R"json({
				"modName":"Actions",
				"content":[{"id":"invalid","type":"button","action":{
					"type":"CallGlobalFunction","script":"Fixture",
					"function":"Apply","params":["{i}{value}"]}}]
			})json");
			auto& page = result.pages.front();
			ActionValueSource values;
			FakeActionExecutor executor;
			BindPage(page, values);
			BindActions(page, executor, values);

			ActionNamed(page, "invalid").activate();
			page.settings.prepareView(page.settings);
			require(HasActionFailure(page),
				"a missing placeholder value produced no page diagnostic");
		});

		runner.test("MCM action signature mismatches are diagnosed", [] {
			auto result = ParseConfig(R"json({
				"modName":"Actions",
				"content":[{"id":"invalid","type":"button","action":{
					"type":"CallGlobalFunction","script":"Fixture",
					"function":"Apply","params":[true]}}]
			})json");
			auto& page = result.pages.front();
			ActionValueSource values;
			FakeActionExecutor executor;
			executor.requireSingleInt = true;
			BindPage(page, values);
			BindActions(page, executor, values);

			ActionNamed(page, "invalid").activate();
			page.settings.prepareView(page.settings);
			require(HasActionFailure(page),
				"an argument type mismatch produced no page diagnostic");
		});

		runner.test("MCM unsupported external actions render disabled", [] {
			auto result = ParseConfig(R"json({
				"modName":"Actions",
				"content":[
					{"id":"external","type":"button","action":{
						"type":"CallExternalFunction","plugin":"Fixture",
						"function":"Apply"}},
					{"id":"external-setting","type":"switcher",
					 "valueOptions":{"sourceType":"GlobalValueBool",
						"sourceForm":"Fixture.esp|1","default":false},
					 "action":{"type":"CallExternalFunction","plugin":"Fixture",
						"function":"Apply","params":["{value}"]}}
				]
			})json");
			auto& page = result.pages.front();
			ActionValueSource values;
			FakeActionExecutor executor;
			BindPage(page, values);
			BindActions(page, executor, values);

			auto& action = ActionNamed(page, "external");
			require(action.isEnabled && !action.isEnabled() &&
					action.description.find("Scaleform") != std::string::npos,
				"external action did not expose its disabled reason");
			auto& setting = SettingNamed(page, "external-setting");
			require(setting.isEnabled && !setting.isEnabled() &&
					setting.description.find("Scaleform") != std::string::npos,
				"external setting action did not disable with a reason");
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
			BindActions(page, executor, values);

			ActionNamed(page, "throwing").activate();
			page.settings.prepareView(page.settings);
			require(HasActionFailure(page),
				"an executor exception escaped or was silently discarded");
		});

		runner.test("MCM scheduled action exceptions complete as failures", [] {
			ImmediateTaskScheduler scheduler;
			std::optional<ActionExecutionResult> result;
			ScheduleActionExecution(
				scheduler,
				[](const ActionCompletion&) {
					throw std::runtime_error("scheduled fixture exception");
				},
				[&](ActionExecutionResult a_result) {
					result = std::move(a_result);
				});
			require(result &&
					result->status == ActionExecutionStatus::kFailed &&
					result->message &&
					result->message->find("scheduled fixture") !=
						std::string::npos,
				"a scheduled executor exception lost its completion");
		});

		runner.test("MCM successful actions refresh displayed values", [] {
			auto result = ParseConfig(R"json({
				"modName":"Actions",
				"content":[
					{"id":"value","type":"slider","valueOptions":{
						"sourceType":"GlobalValueInt",
						"sourceForm":"Fixture.esp|1","default":0}},
					{"id":"refresh","type":"button","action":{
						"type":"CallFunction","form":"Fixture.esp|1",
						"function":"Apply"}}
				]
			})json");
			auto& page = result.pages.front();
			ActionValueSource values;
			FakeActionExecutor executor;
			BindPage(page, values);
			BindActions(page, executor, values);

			ActionNamed(page, "refresh").activate();
			page.settings.prepareView(page.settings);
			require(values.refreshes == 1,
				"a completed action did not refresh the page bindings");
		});

		runner.test("MCM action failures replace the note for their row", [] {
			auto result = ParseConfig(R"json({
				"modName":"Actions",
				"content":[{"id":"invalid","type":"button","action":{
					"type":"CallGlobalFunction","script":"Fixture",
					"function":"Apply","params":["{i}{value}"]}}]
			})json");
			auto& page = result.pages.front();
			ActionValueSource values;
			FakeActionExecutor executor;
			BindPage(page, values);
			BindActions(page, executor, values);

			ActionNamed(page, "invalid").activate();
			page.settings.prepareView(page.settings);
			ActionNamed(page, "invalid").activate();
			page.settings.prepareView(page.settings);
			require(std::ranges::count_if(
						page.settings.notes,
						[](const dmui::SettingsPageNote& a_note) {
							return a_note.noteId ==
								"dearmodding.mcm.action.invalid";
						}) == 1,
				"repeated action failures grew duplicate notes");
		});
	}
}
