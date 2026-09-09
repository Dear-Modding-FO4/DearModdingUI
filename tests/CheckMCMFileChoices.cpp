#include <DearModdingUI/MCM/ActionExecutor.h>
#include <DearModdingUI/MCM/FileChoices.h>
#include <DearModdingUI/MCM/ValueSource.h>
#include <DearModdingUI/MCM/Win32FileListingAdapter.h>

#include "Harness.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI::MCM;

		constexpr std::string_view kFileConfig = R"json({
			"modName":"FileChoices",
			"content":[
				{"id":"sPreset:Files","type":"dropdownFiles",
				 "text":"Preset","help":"Choose a preset.",
				 "valueOptions":{
					"sourceType":"ModSettingString",
					"path":"Data/Interface/Presets",
					"mask":"*.xml"
				 },
				 "action":{
					"type":"CallGlobalFunction",
					"script":"Fixture",
					"function":"Changed",
					"params":["{value}"]
				 }}
			]
		})json";

		[[nodiscard]] dmui::SettingDescriptor& Descriptor(MappedPage& a_page)
		{
			return a_page.settings.groups.front().settings.front();
		}

		[[nodiscard]] const dmui::SettingDescriptor& Descriptor(
			const MappedPage& a_page)
		{
			return a_page.settings.groups.front().settings.front();
		}

		[[nodiscard]] const dmui::ChoiceSettingControl& Choices(
			const MappedPage& a_page)
		{
			return std::get<dmui::ChoiceSettingControl>(
				a_page.settings.groups.front().settings.front().control);
		}

		class FakeFiles final : public FileListingAdapter
		{
		public:
			[[nodiscard]] FileListingResult List(
				std::string_view a_path,
				std::string_view a_mask) override
			{
				++calls;
				lastPath = a_path;
				lastMask = a_mask;
				if (results.empty())
					return std::unexpected("unexpected listing request");
				auto result = std::move(results.front());
				results.pop_front();
				return result;
			}

			std::deque<FileListingResult> results;
			size_t calls{};
			std::string lastPath;
			std::string lastMask;
		};

		class FakeDiagnostics final : public DiagnosticReporter
		{
		public:
			void Report(Diagnostic a_diagnostic) noexcept override
			{
				persistent.push_back(std::move(a_diagnostic));
			}

			void ReportTransient(Diagnostic a_diagnostic) noexcept override
			{
				transient.push_back(std::move(a_diagnostic));
			}

			std::vector<Diagnostic> persistent;
			std::vector<Diagnostic> transient;
		};

		class ThrowingFiles final : public FileListingAdapter
		{
		public:
			[[nodiscard]] FileListingResult List(
				std::string_view,
				std::string_view) override
			{
				throw std::runtime_error("adapter failure");
			}
		};

		class StringSource final : public ValueSource
		{
		public:
			[[nodiscard]] bool Supports(
				SourceFamily a_family) const noexcept override
			{
				return a_family == SourceFamily::kModSetting;
			}

			[[nodiscard]] ValueSnapshot Read(
				const MappedBinding& a_binding) const override
			{
				if (forced)
					return *forced;
				const auto found = values.find(a_binding.descriptorId);
				return found == values.end() ?
					ValueSnapshot{ MissingValue{ generation } } :
					ValueSnapshot{ ReadyValue{ found->second, generation } };
			}

			[[nodiscard]] uint64_t Refresh(const MappedBinding&) override
			{
				return ++generation;
			}

			[[nodiscard]] ValueSnapshot Write(
				const MappedBinding& a_binding,
				const dmui::SettingValue& a_value) override
			{
				++writes;
				values.insert_or_assign(a_binding.descriptorId, a_value);
				++generation;
				return ReadyValue{ a_value, generation };
			}

			std::unordered_map<std::string, dmui::SettingValue> values;
			std::optional<ValueSnapshot> forced;
			size_t writes{};
			uint64_t generation{};
		};

		class CaptureAction final : public ActionExecutor
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
				values.push_back(std::move(a_invocation.value));
				a_completion({ ActionExecutionStatus::kSucceeded, {} });
			}

			std::vector<std::optional<dmui::SettingValue>> values;
		};

		[[nodiscard]] std::filesystem::path Utf8Path(std::string_view a_value)
		{
			std::u8string value;
			value.reserve(a_value.size());
			for (const auto character : a_value)
				value.push_back(static_cast<char8_t>(character));
			return std::filesystem::path{ value };
		}

		[[nodiscard]] std::string PathText(
			const std::filesystem::path& a_path)
		{
			const auto value = a_path.u8string();
			return {
				reinterpret_cast<const char*>(value.data()),
				value.size()
			};
		}

		struct TemporaryDirectory
		{
			std::filesystem::path oldPath{ std::filesystem::current_path() };
			std::filesystem::path root;

			TemporaryDirectory()
			{
				const auto nonce = std::chrono::steady_clock::now()
					.time_since_epoch()
					.count();
				root = std::filesystem::temp_directory_path() /
					("dmui-file-choices-" + std::to_string(nonce));
				std::filesystem::create_directories(root);
				std::filesystem::current_path(root);
			}

			~TemporaryDirectory()
			{
				std::error_code error;
				std::filesystem::current_path(oldPath, error);
				std::filesystem::remove_all(root, error);
			}
		};

		void CreateFile(const std::filesystem::path& a_path)
		{
			std::ofstream stream{ a_path, std::ios::binary };
			stream << "fixture";
		}
	}

	void run_mcm_file_choice_checks(Runner& runner)
	{
		runner.test("MCM dropdownFiles retains file metadata and string binding", [] {
			const auto result = ParseConfig(kFileConfig, "file-config.json");
			require(result.configuration && result.pages.size() == 1,
				"dropdownFiles fixture did not parse");
			const auto& declared =
				result.configuration->pages.front().controls.front();
			require(declared.type == ControlType::kFileMenu &&
					declared.valueOptions &&
					declared.valueOptions->filePath ==
						std::optional<std::string>{ "Data/Interface/Presets" } &&
					declared.valueOptions->fileMask ==
						std::optional<std::string>{ "*.xml" },
				"dropdownFiles metadata was not retained");
			const auto& row = result.pages.front().rows.front();
			require(row.fileChoices &&
					row.fileChoices->path == declared.valueOptions->filePath &&
					row.fileChoices->mask == "*.xml" &&
					row.binding &&
					row.binding->valueKind == SourceValueKind::kString,
				"dropdownFiles did not map through an ordinary string binding");
			require(Choices(result.pages.front()).options.size() == 1 &&
					Choices(result.pages.front()).options.front().value.empty() &&
					Choices(result.pages.front()).options.front().label == "None" &&
					Choices(result.pages.front()).unmatchedLabel == "None" &&
					std::get<std::string>(
						Descriptor(result.pages.front()).defaultValue).empty(),
				"dropdownFiles did not map None as its ordinary empty default");
			require(!std::ranges::any_of(
						result.diagnostics,
						[](const Diagnostic& a_diagnostic) {
							return a_diagnostic.message.find("no options") !=
								std::string::npos;
						}),
				"dropdownFiles incorrectly required static options");

			const auto property = ParseConfig(R"json({
				"modName":"PropertyFiles",
				"content":[
					{"id":"propertyFile","type":"dropdownFiles",
					 "valueOptions":{
						"sourceType":"PropertyValueString",
						"sourceForm":"Fixture.esp|1",
						"propertyName":"SelectedFile",
						"path":"Data/Interface/Files"
					 }}
				]
			})json", "property-files.json");
			const auto& propertyRow = property.pages.front().rows.front();
			require(propertyRow.binding &&
					propertyRow.binding->Family() == SourceFamily::kProperty &&
					propertyRow.binding->valueKind == SourceValueKind::kString &&
					propertyRow.fileChoices &&
					propertyRow.fileChoices->path ==
						std::optional<std::string>{ "Data/Interface/Files" } &&
					propertyRow.fileChoices->mask == "*",
				"property string file dropdown or omitted mask was not supported");
		});

		runner.test("MCM dropdownFiles validates path and string source", [] {
			const auto result = ParseConfig(R"json({
				"modName":"InvalidFiles",
				"content":[
					{"id":"bad","type":"dropdownFiles",
					 "valueOptions":{"sourceType":"ModSettingInt"}}
				]
			})json", "invalid-files.json");
			require(std::ranges::any_of(
						result.diagnostics,
						[](const Diagnostic& a_diagnostic) {
							return a_diagnostic.message.find("non-empty path") !=
								std::string::npos;
						}) &&
					std::ranges::any_of(
						result.diagnostics,
						[](const Diagnostic& a_diagnostic) {
							return a_diagnostic.message.find("string value source") !=
								std::string::npos;
						}) &&
					!result.pages.front().rows.front().binding,
				"invalid dropdownFiles metadata stayed writable");
		});

		runner.test(
			"MCM dropdownFiles refreshes on lifecycle calls without draw-time IO",
			[] {
				auto result = ParseConfig(kFileConfig, "file-config.json");
				auto page = std::move(result.pages.front());
				FakeFiles files;
				files.results.push_back(std::vector<std::string>{
					"Alpha.xml",
					"None",
					"Beta.xml"
				});
				files.results.push_back(std::unexpected("access denied"));
				files.results.push_back(std::unexpected("access denied"));
				files.results.push_back(std::vector<std::string>{
					"Gamma.xml"
				});
				FakeDiagnostics diagnostics;
				StringSource source;
				source.values.emplace(
					"sPreset:Files",
					std::string{ "Removed.xml" });
				auto controller = AttachFileChoices(
					page,
					files,
					diagnostics,
					"file-config.json");
				BindPage(page, source);

				for (size_t index = 0; index < 8; ++index)
					page.settings.prepareView(page.settings);
				require(files.calls == 0,
					"prepareView performed filesystem IO");

				controller.Refresh();
				page.settings.prepareView(page.settings);
				const auto& options = Choices(page).options;
				require(files.calls == 1 &&
						files.lastPath == "Data/Interface/Presets" &&
						files.lastMask == "*.xml" &&
						options.size() == 4 &&
						options[0].value.empty() &&
						options[1].value == "Alpha.xml" &&
						options[2].value == "None" &&
						options[3].value == "Beta.xml",
					"file listing order, names, or None sentinel changed");
				auto& setting = Descriptor(page);
				require(setting.isEnabled && setting.isEnabled() &&
						std::get<std::string>(setting.binding.get()) ==
							"Removed.xml" &&
						!dmui::IsSettingDefault(
							setting,
							setting.binding.get()) &&
						source.writes == 0,
					"unknown storage was not retained as an ordinary dirty value");

				controller.Refresh();
				page.settings.prepareView(page.settings);
				require(!setting.isEnabled() &&
						setting.resolveDescription().find(
							"Data/Interface/Presets") != std::string::npos &&
						setting.resolveDescription().find(
							"access denied") != std::string::npos &&
						diagnostics.transient.size() == 1,
					"listing failure did not disable and explain the row");
				controller.Refresh();
				page.settings.prepareView(page.settings);
				require(diagnostics.transient.size() == 1,
					"identical listing failures spammed diagnostics");
				controller.Refresh();
				page.settings.prepareView(page.settings);
				require(setting.isEnabled() &&
						Choices(page).options.size() == 2 &&
						Choices(page).options[1].value == "Gamma.xml",
					"a later lifecycle refresh did not recover the file row");
			});

		runner.test(
			"MCM dropdownFiles converts adapter exceptions to one failure path",
			[] {
				auto result = ParseConfig(kFileConfig, "file-config.json");
				auto page = std::move(result.pages.front());
				ThrowingFiles files;
				FakeDiagnostics diagnostics;
				StringSource source;
				source.values.emplace(
					"sPreset:Files",
					std::string{});
				auto controller = AttachFileChoices(
					page,
					files,
					diagnostics,
					"file-config.json");
				BindPage(page, source);

				controller.Refresh();
				page.settings.prepareView(page.settings);
				auto& setting = Descriptor(page);
				require(setting.isEnabled && !setting.isEnabled() &&
						diagnostics.transient.size() == 1 &&
						diagnostics.transient.front().message.find(
							"adapter failure") != std::string::npos &&
						setting.resolveDescription().find(
							"adapter failure") != std::string::npos,
					"adapter exception did not use the normal listing failure path");
			});

		runner.test(
			"MCM dropdownFiles writes filenames and clears None to empty",
			[] {
				auto result = ParseConfig(kFileConfig, "file-config.json");
				auto page = std::move(result.pages.front());
				FakeFiles files;
				files.results.push_back(std::vector<std::string>{
					"Alpha.xml",
					"None"
				});
				FakeDiagnostics diagnostics;
				StringSource source;
				source.values.emplace(
					"sPreset:Files",
					std::string{ "Removed.xml" });
				CaptureAction actions;
				auto controller = AttachFileChoices(
					page,
					files,
					diagnostics,
					"file-config.json");
				BindPage(page, source);
				BindActions(page, actions, source, diagnostics);
				controller.Refresh();
				page.settings.prepareView(page.settings);
				auto& setting = Descriptor(page);
				size_t edits{};
				setting.onEdit =
					[&edits](const dmui::SettingEditEvent& a_event) {
						++edits;
						require(a_event.changed && a_event.completed &&
								std::get<std::string>(a_event.value).empty(),
							"file reset emitted an invalid edit event");
					};

				auto effective = setting.binding.set(
					dmui::SettingValue{ std::string{ "None" } });
				require(std::get<std::string>(effective) == "None" &&
						std::get<std::string>(
							source.values.at("sPreset:Files")) == "None" &&
						actions.values.size() == 1 &&
						std::get<std::string>(*actions.values.back()) == "None",
					"a real filename named None was confused with the sentinel");

				effective = setting.binding.set(
					dmui::SettingValue{ std::string{} });
				require(std::get<std::string>(effective).empty() &&
						std::get<std::string>(
							source.values.at("sPreset:Files")).empty() &&
						actions.values.size() == 2 &&
						std::get<std::string>(*actions.values.back()).empty(),
					"None did not persist and dispatch an empty string");

				source.values.insert_or_assign(
					"sPreset:Files",
					std::string{ "Removed.xml" });
				const auto reset = dmui::ResetSettingToDefault(setting);
				require(reset &&
						std::get<std::string>(*reset).empty() &&
						std::get<std::string>(
							source.values.at("sPreset:Files")).empty() &&
						actions.values.size() == 3 &&
						edits == 1 &&
						dmui::IsSettingDefault(
							setting,
							setting.binding.get()) &&
						std::get<std::string>(*actions.values.back()).empty(),
					"reset did not clear unknown storage to an empty string");

				const auto repeated = dmui::ResetSettingToDefault(setting);
				require(repeated &&
						std::get<std::string>(*repeated).empty() &&
						source.writes == 3 &&
						actions.values.size() == 3 &&
						edits == 1,
					"already-empty reset wrote or emitted another edit");
			});

		runner.test("Win32 MCM file listing preserves native names and masks", [] {
			TemporaryDirectory temporary;
			const auto directory =
				Utf8Path("files with spaces") / Utf8Path("nested");
			std::filesystem::create_directories(directory);
			const auto unicodeName =
				std::string{ "Preset \xE2\x98\x83.xml" };
			CreateFile(directory / Utf8Path(unicodeName));
			CreateFile(directory / "ExactCase.XML");
			CreateFile(directory / "ignored.txt");
			std::filesystem::create_directories(directory / "Folder.xml");

			Win32FileListingAdapter files;
			auto relative = files.List(
				"files with spaces/nested",
				"*.xml");
			require(relative &&
					std::ranges::contains(*relative, unicodeName) &&
					std::ranges::contains(*relative, "ExactCase.XML") &&
					std::ranges::contains(*relative, "Folder.xml") &&
					!std::ranges::contains(*relative, "ignored.txt"),
				"relative slash paths, Unicode, case, masks, or directories changed");

			auto absoluteText = PathText(directory);
			std::ranges::replace(absoluteText, '/', '\\');
			auto absolute = files.List(absoluteText, "*.XML");
			require(absolute &&
					std::ranges::contains(*absolute, unicodeName) &&
					std::ranges::contains(*absolute, "ExactCase.XML"),
				"absolute backslash paths did not use Win32 mask semantics");

			const auto missing = files.List("missing directory", "*");
			require(missing && missing->empty(),
				"a missing directory was reported as a successful-looking failure");

			const auto emptyPath = files.List("", "*");
			const auto emptyMask = files.List("files with spaces", "");
			require(!emptyPath && !emptyMask,
				"empty path or mask was accepted as a root listing");

			auto nulPath = std::string{ "files with spaces" };
			nulPath.push_back('\0');
			nulPath.append("ignored");
			auto nulMask = std::string{ "*.xml" };
			nulMask.push_back('\0');
			nulMask.append("ignored");
			require(!files.List(nulPath, "*") &&
					!files.List("files with spaces", nulMask),
				"embedded NUL input reached Win32 as a truncated listing");

			const auto malformed = files.List(
				std::string_view{ "\xC3\x28", 2 },
				"*");
			require(!malformed,
				"malformed UTF-8 path was accepted");

			if constexpr (sizeof(size_t) > sizeof(int))
			{
				const char marker = 'x';
				const auto oversized = std::string_view{
					&marker,
					static_cast<size_t>(
						(std::numeric_limits<int>::max)()) +
						1
				};
				require(!files.List(oversized, "*") &&
						!files.List("files with spaces", oversized),
					"oversized path or mask was narrowed to a Win32 int");
			}
		});
	}
}
