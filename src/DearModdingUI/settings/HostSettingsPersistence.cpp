#include <DearModdingUI/settings/HostSettingsPersistence.h>

#include <Windows.h>

#include <toml.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <format>
#include <limits>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

namespace DearModdingUI
{
	namespace
	{
		constexpr Support::Phrase kCreateDirectoryFailed{
			"$DMUI_Config_CreateDirectoryFailed",
			"Creating settings directory for {} failed: {} (Windows error {})."
		};
		constexpr Support::Phrase kOpenTemporaryFailed{
			"$DMUI_Config_OpenTemporaryFailed",
			"Opening temporary settings file {} failed: {} (Windows error {})."
		};
		constexpr Support::Phrase kWriteTemporaryFailed{
			"$DMUI_Config_WriteTemporaryFailed",
			"Writing temporary settings file {} failed: {} (Windows error {})."
		};
		constexpr Support::Phrase kFlushTemporaryFailed{
			"$DMUI_Config_FlushTemporaryFailed",
			"Flushing temporary settings file {} failed: {} (Windows error {})."
		};
		constexpr Support::Phrase kCloseTemporaryFailed{
			"$DMUI_Config_CloseTemporaryFailed",
			"Closing temporary settings file {} failed: {} (Windows error {})."
		};
		constexpr Support::Phrase kRemoveTemporaryFailed{
			"$DMUI_Config_RemoveTemporaryFailed",
			"Removing temporary settings file {} failed: {} (Windows error {})."
		};
		constexpr Support::Phrase kReplaceFallbackFailed{
			"$DMUI_Config_ReplaceFallbackFailed",
			"Replacing settings file during the cross-volume copy fallback for {} failed: {} (Windows error {})."
		};
		constexpr Support::Phrase kReplaceFailed{
			"$DMUI_Config_ReplaceFailed",
			"Replacing settings file {} failed: {} (Windows error {})."
		};
		constexpr Support::Phrase kCrossVolumeAdvice{
			"$DMUI_Config_CrossVolumeAdvice",
			"Windows first reported ERROR_NOT_SAME_DEVICE (17): {}. "
			"This can occur when a mod manager maps the temporary file and "
			"the existing configuration to different drives. Check that "
			"the mod manager's overwrite or output location is writable "
			"and has available disk space, then save again."
		};
		constexpr Support::Phrase kCorrectionWrongType{
			"$DMUI_Config_CorrectionWrongType",
			"{} had the wrong value type and used its default"
		};
		constexpr Support::Phrase kCorrectionReplaced{
			"$DMUI_Config_CorrectionReplaced",
			"{} {} used {}"
		};
		constexpr Support::Phrase kBindingIgnored{
			"$DMUI_Config_BindingIgnored",
			"{}.{} was ignored because it was not text"
		};
		constexpr Support::PluralPhrase kMoreCorrections{
			"$DMUI_Config_MoreCorrections",
			"and {} more correction",
			"and {} more corrections"
		};
		constexpr Support::Phrase kCorrectedSummary{
			"$DMUI_Config_CorrectedSummary",
			"Using corrected settings: {}. Use {} on the Settings page to persist the accepted values."
		};
		constexpr Support::Phrase kInspectFailed{
			"$DMUI_Config_InspectFailed",
			"Could not inspect {}: {}. Using defaults; check file permissions and restart."
		};
		constexpr Support::Phrase kAbsent{
			"$DMUI_Config_Absent",
			"Using defaults; configuration file is absent."
		};
		constexpr Support::Phrase kLoaded{
			"$DMUI_Config_Loaded",
			"Loaded settings from {}."
		};
		constexpr Support::Phrase kLoadFailed{
			"$DMUI_Config_LoadFailed",
			"Could not load {}: {}. Using defaults; correct or remove the file and restart."
		};
		constexpr Support::Phrase kLoadFailedUnknown{
			"$DMUI_Config_LoadFailedUnknown",
			"Could not load {}. Using defaults; correct or remove the file and restart."
		};
		constexpr Support::Phrase kSaved{
			"$DMUI_Config_Saved",
			"Saved settings to {}."
		};
		constexpr Support::Phrase kSavedByCopy{
			"$DMUI_Config_SavedByCopy",
			"Saved settings to {} using a non-atomic cross-volume copy fallback."
		};
		constexpr Support::Phrase kPersistFailed{
			"$DMUI_Config_PersistFailed",
			"Persisting {} failed: {}"
		};

		class UniqueHandle
		{
		public:
			explicit UniqueHandle(HANDLE a_handle) noexcept :
				handle_(a_handle)
			{}

			UniqueHandle(const UniqueHandle&) = delete;
			UniqueHandle& operator=(const UniqueHandle&) = delete;

			~UniqueHandle()
			{
				if (handle_ != INVALID_HANDLE_VALUE)
					(void)CloseHandle(handle_);
			}

			[[nodiscard]] HANDLE Get() const noexcept
			{
				return handle_;
			}

			[[nodiscard]] bool Close(uint32_t& a_nativeError) noexcept
			{
				const auto handle = handle_;
				handle_ = INVALID_HANDLE_VALUE;
				if (CloseHandle(handle))
				{
					a_nativeError = ERROR_SUCCESS;
					return true;
				}
				a_nativeError = GetLastError();
				return false;
			}

		private:
			HANDLE handle_{ INVALID_HANDLE_VALUE };
		};

		struct LocalFreeDeleter
		{
			void operator()(wchar_t* a_buffer) const noexcept
			{
				if (a_buffer)
					(void)LocalFree(a_buffer);
			}
		};

		struct TemporaryCleanupResult
		{
			bool cleaned{ true };
			uint32_t nativeError{ ERROR_SUCCESS };
		};

		[[nodiscard]] std::string TrimSystemMessage(std::string a_message)
		{
			while (!a_message.empty() &&
				(a_message.back() == '\r' || a_message.back() == '\n' ||
					a_message.back() == ' ' || a_message.back() == '\t'))
				a_message.pop_back();
			return a_message;
		}

		[[nodiscard]] std::string Narrow(std::wstring_view a_text)
		{
			if (a_text.empty())
				return {};
			const auto required = WideCharToMultiByte(
				CP_UTF8,
				0,
				a_text.data(),
				static_cast<int>(a_text.size()),
				nullptr,
				0,
				nullptr,
				nullptr);
			if (required <= 0)
				return {};
			std::string result(static_cast<size_t>(required), '\0');
			if (WideCharToMultiByte(
					CP_UTF8,
					0,
					a_text.data(),
					static_cast<int>(a_text.size()),
					result.data(),
					required,
					nullptr,
					nullptr) <= 0)
				return {};
			return result;
		}

		// The OS explanation stays in the system language.
		[[nodiscard]] Support::Message WindowsFailure(
			const Support::Phrase& a_operation,
			const std::filesystem::path& a_path,
			uint32_t a_error)
		{
			auto code = std::to_string(a_error);
			if (a_error == ERROR_NOT_SAME_DEVICE)
				code.append(", ERROR_NOT_SAME_DEVICE");
			return Support::Message{
				a_operation,
				a_path.filename().string(),
				DescribeWindowsError(a_error),
				code
			};
		}

		[[nodiscard]] bool DefaultMoveFile(
			void*,
			const std::filesystem::path& a_source,
			const std::filesystem::path& a_destination,
			uint32_t a_flags,
			uint32_t& a_nativeError) noexcept
		{
			if (MoveFileExW(
					a_source.c_str(),
					a_destination.c_str(),
					a_flags))
			{
				a_nativeError = ERROR_SUCCESS;
				return true;
			}
			a_nativeError = GetLastError();
			return false;
		}

		[[nodiscard]] TemporaryCleanupResult RemoveTemporary(
			const std::filesystem::path& a_path) noexcept
		{
			TemporaryCleanupResult result;
			if (DeleteFileW(a_path.c_str()))
				return result;
			result.nativeError = GetLastError();
			if (result.nativeError == ERROR_FILE_NOT_FOUND ||
				result.nativeError == ERROR_PATH_NOT_FOUND)
			{
				result.nativeError = ERROR_SUCCESS;
				return result;
			}
			result.cleaned = false;
			return result;
		}

		void AppendCleanupFailure(
			HostSettingsSaveResult& a_result,
			const std::filesystem::path& a_path,
			const TemporaryCleanupResult& a_cleanup)
		{
			if (a_cleanup.cleaned)
				return;
			a_result.temporaryCleanupFailed = true;
			if (a_result.nativeError == ERROR_SUCCESS)
				a_result.nativeError = a_cleanup.nativeError;
			a_result.detail = Support::Message::Join(
				{ a_result.detail,
					WindowsFailure(
						kRemoveTemporaryFailed,
						a_path,
						a_cleanup.nativeError) },
				" ");
		}

		[[nodiscard]] std::string SerializeSettings(
			const PersistedHostInterfaceSettings& a_settings)
		{
			toml::value root{ toml::table{} };
			root["Additional"] = toml::table{};
			auto& section = root["Additional"];
			section["bMenuMonochromeIcons"] = a_settings.monochromeIcons;
			section["sMenuLogoColors"] = a_settings.logoColors;
			section["sMenuSidebarLayout"] = a_settings.sidebarLayout;
			section["sMenuAccentColor"] = a_settings.accentColor;
			section["fMenuWindowOpacity"] = static_cast<double>(
				a_settings.windowBackgroundOpacity);
			section["sMenuPaletteBackgroundColor"] =
				a_settings.paletteBackgroundColor;
			section["fMenuPaletteOpacity"] = static_cast<double>(
				a_settings.paletteBackgroundOpacity);
			section["bMenuBackgroundBlur"] = a_settings.backgroundBlur;
			section["fMenuBackgroundBlurStrength"] = static_cast<double>(
				a_settings.backgroundBlurStrength);
			section["fMenuUiScale"] = static_cast<double>(a_settings.uiScale);
			section["sMenuBodyFontFamily"] = a_settings.bodyFontFamily;
			section["sMenuToggleKey"] = a_settings.menuToggleKey;
			section["sMenuToggleGamepad"] = a_settings.menuToggleGamepad;
			section["bMenuFallSoulsMode"] = a_settings.fallSoulsMode;
			section["sFieldFeedbackLayout"] = a_settings.feedbackPlacement;
			section["sFieldFeedbackInfoColor"] =
				a_settings.feedbackInfoColor;
			section["sFieldFeedbackWarningColor"] =
				a_settings.feedbackWarningColor;
			section["sFieldFeedbackErrorColor"] =
				a_settings.feedbackErrorColor;
			const auto writeBindings = [&](const char* a_name, const auto& a_bindings) {
				root[a_name] = toml::table{};
				for (const auto& [id, chord] : a_bindings)
					root[a_name][id] = chord;
			};
			writeBindings("Hotkeys", a_settings.hotkeys);
			writeBindings("GamepadHotkeys", a_settings.gamepadHotkeys);
			return toml::format(root);
		}

		[[nodiscard]] bool WriteTemporary(
			const std::filesystem::path& a_path,
			std::string_view a_contents,
			HostSettingsSaveResult& a_result,
			bool& a_created)
		{
			UniqueHandle handle{ CreateFileW(
				a_path.c_str(),
				GENERIC_WRITE,
				FILE_SHARE_READ,
				nullptr,
				CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				nullptr) };
			if (handle.Get() == INVALID_HANDLE_VALUE)
			{
				a_result.nativeError = GetLastError();
				a_result.detail = WindowsFailure(
					kOpenTemporaryFailed,
					a_path,
					a_result.nativeError);
				return false;
			}
			a_created = true;

			auto succeeded = true;
			size_t written = 0;
			while (written < a_contents.size())
			{
				const auto remaining = a_contents.size() - written;
				const auto chunk = static_cast<DWORD>((std::min)(
					remaining,
					static_cast<size_t>((std::numeric_limits<DWORD>::max)())));
				DWORD count{};
				if (!WriteFile(
						handle.Get(),
						a_contents.data() + written,
						chunk,
						&count,
						nullptr) ||
					count == 0)
				{
					a_result.nativeError = GetLastError();
					if (a_result.nativeError == ERROR_SUCCESS)
						a_result.nativeError = ERROR_WRITE_FAULT;
					a_result.detail = WindowsFailure(
						kWriteTemporaryFailed,
						a_path,
						a_result.nativeError);
					succeeded = false;
					break;
				}
				written += count;
			}

			if (succeeded && !FlushFileBuffers(handle.Get()))
			{
				a_result.nativeError = GetLastError();
				a_result.detail = WindowsFailure(
					kFlushTemporaryFailed,
					a_path,
					a_result.nativeError);
				succeeded = false;
			}
			uint32_t closeError{};
			if (!handle.Close(closeError) && succeeded)
			{
				a_result.nativeError = closeError;
				a_result.detail = WindowsFailure(
					kCloseTemporaryFailed,
					a_path,
					a_result.nativeError);
				succeeded = false;
			}
			return succeeded;
		}

		template <class T>
		[[nodiscard]] T ReadSetting(
			const toml::value& a_section,
			std::string_view a_key,
			T a_fallback,
			std::vector<Support::Message>& a_corrections)
		{
			const std::string key{ a_key };
			if (!a_section.contains(key))
				return a_fallback;
			try
			{
				return toml::find<T>(a_section, key);
			}
			catch (const std::exception&)
			{
				a_corrections.push_back(Support::Message{ kCorrectionWrongType, a_key });
				return a_fallback;
			}
		}

		// Values read as the TOML file spells them.
		[[nodiscard]] Support::Message Quoted(std::string_view a_text)
		{
			return Support::Message::Text(std::format("\"{}\"", a_text));
		}

		template <class Value, class Used>
		[[nodiscard]] Support::Message Replacement(
			std::string_view a_key,
			const Value& a_value,
			const Used& a_used)
		{
			return Support::Message{ kCorrectionReplaced, a_key, a_value, a_used };
		}

		void AppendCorrection(
			std::vector<Support::Message>& a_corrections,
			Support::Message a_correction)
		{
			a_corrections.push_back(std::move(a_correction));
		}

		[[nodiscard]] HostInterfaceSettings NormalizeSettings(
			PersistedHostInterfaceSettings a_settings,
			std::vector<Support::Message>& a_corrections)
		{
			const auto runtime = DecodeHostInterfaceSettings(a_settings);

			if (!ParseLogoColorMode(a_settings.logoColors))
			{
				AppendCorrection(
					a_corrections,
					Replacement(
						"sMenuLogoColors",
						Quoted(a_settings.logoColors),
						Quoted(LogoColorModeName(runtime.logoColors))));
			}
			if (!ParseUserSidebarLayout(a_settings.sidebarLayout))
			{
				AppendCorrection(
					a_corrections,
					Replacement(
						"sMenuSidebarLayout",
						Quoted(a_settings.sidebarLayout),
						Quoted(SidebarLayoutKindName(runtime.sidebarLayout))));
			}
			if (!TryDecodeHostColor(a_settings.accentColor))
			{
				AppendCorrection(
					a_corrections,
					Replacement(
						"sMenuAccentColor",
						Quoted(a_settings.accentColor),
						EncodeHostAccentColor(runtime.accentColor)));
			}
			if (runtime.windowBackgroundOpacity !=
				a_settings.windowBackgroundOpacity)
			{
				AppendCorrection(
					a_corrections,
					Replacement(
						"fMenuWindowOpacity",
						a_settings.windowBackgroundOpacity,
						runtime.windowBackgroundOpacity));
			}
			if (!TryDecodeHostColor(a_settings.paletteBackgroundColor))
			{
				AppendCorrection(
					a_corrections,
					Replacement(
						"sMenuPaletteBackgroundColor",
						Quoted(a_settings.paletteBackgroundColor),
						EncodeHostAccentColor(runtime.paletteBackgroundColor)));
			}
			if (runtime.paletteBackgroundOpacity !=
				a_settings.paletteBackgroundOpacity)
			{
				AppendCorrection(
					a_corrections,
					Replacement(
						"fMenuPaletteOpacity",
						a_settings.paletteBackgroundOpacity,
						runtime.paletteBackgroundOpacity));
			}
			if (runtime.backgroundBlurStrength !=
				a_settings.backgroundBlurStrength)
			{
				AppendCorrection(
					a_corrections,
					Replacement(
						"fMenuBackgroundBlurStrength",
						a_settings.backgroundBlurStrength,
						runtime.backgroundBlurStrength));
			}
			if (runtime.uiScale != a_settings.uiScale)
			{
				AppendCorrection(
					a_corrections,
					Replacement(
						"fMenuUiScale",
						a_settings.uiScale,
						runtime.uiScale));
			}
			if (!IsValidBodyFontFamily(a_settings.bodyFontFamily))
			{
				AppendCorrection(
					a_corrections,
					Replacement(
						"sMenuBodyFontFamily",
						Quoted(a_settings.bodyFontFamily),
						Quoted(runtime.bodyFontFamily)));
			}
			if (!ParseMenuToggleChord(a_settings.menuToggleKey).recognized)
			{
				AppendCorrection(
					a_corrections,
					Replacement(
						"sMenuToggleKey",
						Quoted(a_settings.menuToggleKey),
						Quoted(runtime.menuToggleKey)));
			}
			if (!ParseMenuToggleChord(a_settings.menuToggleGamepad, HotkeySlot::kGamepad).recognized)
			{
				AppendCorrection(
					a_corrections,
					Replacement(
						"sMenuToggleGamepad",
						Quoted(a_settings.menuToggleGamepad),
						Quoted(runtime.menuToggleGamepad)));
			}
			if (!ParseFieldFeedbackLayout(a_settings.feedbackPlacement))
			{
				AppendCorrection(
					a_corrections,
					Replacement(
						"sFieldFeedbackLayout",
						Quoted(a_settings.feedbackPlacement),
						Quoted(FieldFeedbackLayoutName(runtime.feedbackPlacement))));
			}
			const auto checkFeedbackColor =
				[&](std::string_view a_key,
					const std::string& a_value,
					HostPaletteColor a_runtime) {
					if (!TryDecodeHostColor(a_value))
					{
						AppendCorrection(
							a_corrections,
							Replacement(
								a_key,
								Quoted(a_value),
								EncodeHostAccentColor(a_runtime)));
					}
				};
			checkFeedbackColor(
				"sFieldFeedbackInfoColor",
				a_settings.feedbackInfoColor,
				runtime.feedbackInfoColor);
			checkFeedbackColor(
				"sFieldFeedbackWarningColor",
				a_settings.feedbackWarningColor,
				runtime.feedbackWarningColor);
			checkFeedbackColor(
				"sFieldFeedbackErrorColor",
				a_settings.feedbackErrorColor,
				runtime.feedbackErrorColor);
			return runtime;
		}

		[[nodiscard]] Support::Message CorrectionSummary(
			const std::vector<Support::Message>& a_corrections)
		{
			constexpr size_t kVisibleCorrectionLimit = 5;
			const auto visible = (std::min)(
				a_corrections.size(),
				kVisibleCorrectionLimit);
			std::vector<Support::Message> listed(
				a_corrections.begin(),
				a_corrections.begin() + static_cast<std::ptrdiff_t>(visible));
			if (a_corrections.size() > visible)
			{
				const auto hidden = a_corrections.size() - visible;
				listed.push_back(Support::Message::Plural(
					kMoreCorrections,
					hidden,
					hidden));
			}
			return Support::Message{
				kCorrectedSummary,
				Support::Message::Join(std::move(listed), "; "),
				Support::Message{ kSettingsApplyControl }
			};
		}
	}

	HostSettingsLoadResult LoadHostInterfaceSettings(
		const std::filesystem::path& a_path)
	{
		HostSettingsLoadResult result;
		result.path = a_path.filename().string();
		std::error_code existsError;
		const auto exists = std::filesystem::exists(a_path, existsError);
		if (existsError)
		{
			result.disposition = HostSettingsLoadDisposition::kFailed;
			result.detail = Support::Message{
				kInspectFailed,
				result.path,
				existsError.message()
			};
			return result;
		}
		if (!exists)
		{
			result.disposition = HostSettingsLoadDisposition::kMissing;
			result.detail = Support::Message{ kAbsent };
			return result;
		}

		try
		{
			const auto root = toml::parse(a_path.string());
			const auto& section = toml::find(root, "Additional");
			PersistedHostInterfaceSettings settings;
			settings.logoColors = ReadSetting<std::string>(
				section,
				"sMenuLogoColors",
				settings.logoColors,
				result.corrections);
			settings.monochromeIcons = ReadSetting<bool>(
				section,
				"bMenuMonochromeIcons",
				settings.monochromeIcons,
				result.corrections);
			settings.sidebarLayout = ReadSetting<std::string>(
				section,
				"sMenuSidebarLayout",
				settings.sidebarLayout,
				result.corrections);
			settings.accentColor = ReadSetting<std::string>(
				section,
				"sMenuAccentColor",
				settings.accentColor,
				result.corrections);
			settings.windowBackgroundOpacity = ReadSetting<float>(
				section,
				"fMenuWindowOpacity",
				settings.windowBackgroundOpacity,
				result.corrections);
			settings.paletteBackgroundColor = ReadSetting<std::string>(
				section,
				"sMenuPaletteBackgroundColor",
				settings.paletteBackgroundColor,
				result.corrections);
			settings.paletteBackgroundOpacity = ReadSetting<float>(
				section,
				"fMenuPaletteOpacity",
				settings.paletteBackgroundOpacity,
				result.corrections);
			settings.backgroundBlur = ReadSetting<bool>(
				section,
				"bMenuBackgroundBlur",
				settings.backgroundBlur,
				result.corrections);
			settings.backgroundBlurStrength = ReadSetting<float>(
				section,
				"fMenuBackgroundBlurStrength",
				settings.backgroundBlurStrength,
				result.corrections);
			settings.uiScale = ReadSetting<float>(
				section,
				"fMenuUiScale",
				settings.uiScale,
				result.corrections);
			settings.bodyFontFamily = ReadSetting<std::string>(
				section,
				"sMenuBodyFontFamily",
				settings.bodyFontFamily,
				result.corrections);
			settings.menuToggleKey = ReadSetting<std::string>(
				section,
				"sMenuToggleKey",
				settings.menuToggleKey,
				result.corrections);
			settings.menuToggleGamepad = ReadSetting<std::string>(
				section,
				"sMenuToggleGamepad",
				settings.menuToggleGamepad,
				result.corrections);
			settings.fallSoulsMode = ReadSetting<bool>(
				section,
				"bMenuFallSoulsMode",
				settings.fallSoulsMode,
				result.corrections);
			settings.feedbackPlacement = ReadSetting<std::string>(
				section,
				"sFieldFeedbackLayout",
				settings.feedbackPlacement,
				result.corrections);
			settings.feedbackInfoColor = ReadSetting<std::string>(
				section,
				"sFieldFeedbackInfoColor",
				settings.feedbackInfoColor,
				result.corrections);
			settings.feedbackWarningColor = ReadSetting<std::string>(
				section,
				"sFieldFeedbackWarningColor",
				settings.feedbackWarningColor,
				result.corrections);
			settings.feedbackErrorColor = ReadSetting<std::string>(
				section,
				"sFieldFeedbackErrorColor",
				settings.feedbackErrorColor,
				result.corrections);

			const auto readBindings = [&](const char* a_name, auto& a_bindings) {
				if (!root.contains(a_name) || !root.at(a_name).is_table())
					return;
				for (const auto& [id, value] : root.at(a_name).as_table())
				{
					if (value.is_string())
						a_bindings.emplace(id, value.as_string());
					else
						result.corrections.push_back(
							Support::Message{ kBindingIgnored, a_name, id });
				}
			};
			readBindings("Hotkeys", result.hotkeys);
			readBindings("GamepadHotkeys", result.gamepadHotkeys);
			result.settings = NormalizeSettings(
				std::move(settings),
				result.corrections);
			result.disposition = result.corrections.empty() ?
				HostSettingsLoadDisposition::kLoaded :
				HostSettingsLoadDisposition::kCorrected;
			result.detail = result.corrections.empty() ?
				Support::Message{ kLoaded, result.path } :
				CorrectionSummary(result.corrections);
		}
		catch (const std::exception& error)
		{
			result.settings = {};
			result.hotkeys.clear();
			result.gamepadHotkeys.clear();
			result.disposition = HostSettingsLoadDisposition::kFailed;
			result.detail = Support::Message{ kLoadFailed, result.path, error.what() };
		}
		catch (...)
		{
			result.settings = {};
			result.hotkeys.clear();
			result.gamepadHotkeys.clear();
			result.disposition = HostSettingsLoadDisposition::kFailed;
			result.detail = Support::Message{ kLoadFailedUnknown, result.path };
		}
		return result;
	}

	std::string DescribeWindowsError(uint32_t a_error)
	{
		wchar_t* buffer{};
		const auto length = FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			a_error,
			0,
			reinterpret_cast<wchar_t*>(&buffer),
			0,
			nullptr);
		if (length == 0 || !buffer)
			return "No system explanation is available";
		const std::unique_ptr<wchar_t, LocalFreeDeleter> ownedBuffer{ buffer };
		const auto message = TrimSystemMessage(Narrow(
			std::wstring_view{ ownedBuffer.get(), static_cast<size_t>(length) }));
		return message.empty() ?
			"No system explanation is available" :
			message;
	}

	HostSettingsSaveResult PersistHostInterfaceSettings(
		const std::filesystem::path& a_path,
		const PersistedHostInterfaceSettings& a_settings,
		HostSettingsPersistenceOperations a_operations)
	{
		HostSettingsSaveResult result;
		std::filesystem::path temporary;
		bool temporaryCreated = false;
		try
		{
			std::error_code directoryError;
			std::filesystem::create_directories(
				a_path.parent_path(),
				directoryError);
			if (directoryError)
			{
				result.nativeError = static_cast<uint32_t>(
					directoryError.value());
				result.detail = WindowsFailure(
					kCreateDirectoryFailed,
					a_path,
					result.nativeError);
				return result;
			}

			const auto contents = SerializeSettings(a_settings);
			temporary = a_path;
			temporary += L".tmp";
			if (!WriteTemporary(
					temporary,
					contents,
					result,
					temporaryCreated))
			{
				if (temporaryCreated)
					AppendCleanupFailure(result, temporary, RemoveTemporary(temporary));
				return result;
			}

			const auto moveFile = a_operations.moveFile ?
				a_operations.moveFile :
				&DefaultMoveFile;
			constexpr uint32_t normalFlags =
				MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH;
			uint32_t moveError{};
			if (moveFile(
					a_operations.context,
					temporary,
					a_path,
					normalFlags,
					moveError))
			{
				result.saved = true;
				result.detail = Support::Message{ kSaved, a_path.filename().string() };
				AppendCleanupFailure(result, temporary, RemoveTemporary(temporary));
				return result;
			}

			if (moveError == ERROR_NOT_SAME_DEVICE)
			{
				result.usedCrossVolumeFallback = true;
				uint32_t fallbackError{};
				if (moveFile(
						a_operations.context,
						temporary,
						a_path,
						normalFlags | MOVEFILE_COPY_ALLOWED,
						fallbackError))
				{
					result.saved = true;
					result.detail = Support::Message{
						kSavedByCopy,
						a_path.filename().string()
					};
					AppendCleanupFailure(result, temporary, RemoveTemporary(temporary));
					return result;
				}

				result.nativeError = fallbackError;
				result.detail = Support::Message::Join(
					{ WindowsFailure(kReplaceFallbackFailed, a_path, fallbackError),
						Support::Message{
							kCrossVolumeAdvice,
							DescribeWindowsError(ERROR_NOT_SAME_DEVICE) } },
					" ");
			}
			else
			{
				result.nativeError = moveError;
				result.detail = WindowsFailure(
					kReplaceFailed,
					a_path,
					moveError);
			}
			AppendCleanupFailure(result, temporary, RemoveTemporary(temporary));
			return result;
		}
		catch (const std::exception& error)
		{
			const auto cleanup = temporaryCreated ?
				RemoveTemporary(temporary) : TemporaryCleanupResult{};
			result.detail = Support::Message{
				kPersistFailed,
				a_path.filename().string(),
				error.what()
			};
			AppendCleanupFailure(result, temporary, cleanup);
			return result;
		}
		catch (...)
		{
			const auto cleanup = temporaryCreated ?
				RemoveTemporary(temporary) : TemporaryCleanupResult{};
			result.detail = Support::Message{
				kPersistFailedUnknown,
				a_path.filename().string()
			};
			AppendCleanupFailure(result, temporary, cleanup);
			return result;
		}
	}

}
