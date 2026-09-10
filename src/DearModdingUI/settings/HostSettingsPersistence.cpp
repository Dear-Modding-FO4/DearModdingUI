#include <DearModdingUI/settings/HostSettingsPersistence.h>

#include <Windows.h>

#include <toml.hpp>

#include <algorithm>
#include <cmath>
#include <exception>
#include <format>
#include <limits>
#include <memory>
#include <system_error>
#include <utility>

namespace DearModdingUI
{
	namespace
	{
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

		[[nodiscard]] std::string WindowsFailure(
			std::string_view a_operation,
			const std::filesystem::path& a_path,
			uint32_t a_error)
		{
			auto detail = std::format(
				"{} {} failed: {} (Windows error {}",
				a_operation,
				a_path.filename().string(),
				DescribeWindowsError(a_error),
				a_error);
			if (a_error == ERROR_NOT_SAME_DEVICE)
				detail.append(", ERROR_NOT_SAME_DEVICE");
			detail.append(").");
			return detail;
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
			if (!a_result.detail.empty())
				a_result.detail.push_back(' ');
			a_result.detail.append(WindowsFailure(
				"Removing temporary settings file",
				a_path,
				a_cleanup.nativeError));
		}

		[[nodiscard]] std::string SerializeSettings(
			const PersistedHostInterfaceSettings& a_settings)
		{
			toml::value root{ toml::table{} };
			root["Additional"] = toml::table{};
			auto& section = root["Additional"];
			section["bMenuMonochromeIcons"] = a_settings.monochromeIcons;
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
			root["Hotkeys"] = toml::table{};
			for (const auto& [id, chord] : a_settings.hotkeys)
				root["Hotkeys"][id] = chord;
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
					"Opening temporary settings file",
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
						"Writing temporary settings file",
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
					"Flushing temporary settings file",
					a_path,
					a_result.nativeError);
				succeeded = false;
			}
			uint32_t closeError{};
			if (!handle.Close(closeError) && succeeded)
			{
				a_result.nativeError = closeError;
				a_result.detail = WindowsFailure(
					"Closing temporary settings file",
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
			std::vector<std::string>& a_corrections)
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
				a_corrections.push_back(
					std::format("{} had the wrong value type and used its default",
						a_key));
				return a_fallback;
			}
		}

		void AppendCorrection(
			std::vector<std::string>& a_corrections,
			std::string a_correction)
		{
			a_corrections.push_back(std::move(a_correction));
		}

		[[nodiscard]] HostInterfaceSettings NormalizeSettings(
			PersistedHostInterfaceSettings a_settings,
			std::vector<std::string>& a_corrections)
		{
			const auto runtime = DecodeHostInterfaceSettings(a_settings);

			if (!ParseUserSidebarLayout(a_settings.sidebarLayout))
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"sMenuSidebarLayout \"{}\" used \"{}\"",
						a_settings.sidebarLayout,
						SidebarLayoutKindName(runtime.sidebarLayout)));
			}
			if (!TryDecodeHostColor(a_settings.accentColor))
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"sMenuAccentColor \"{}\" used {}",
						a_settings.accentColor,
						EncodeHostAccentColor(runtime.accentColor)));
			}
			if (runtime.windowBackgroundOpacity !=
				a_settings.windowBackgroundOpacity)
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"fMenuWindowOpacity {} used {}",
						a_settings.windowBackgroundOpacity,
						runtime.windowBackgroundOpacity));
			}
			if (!TryDecodeHostColor(a_settings.paletteBackgroundColor))
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"sMenuPaletteBackgroundColor \"{}\" used {}",
						a_settings.paletteBackgroundColor,
						EncodeHostAccentColor(
							runtime.paletteBackgroundColor)));
			}
			if (runtime.paletteBackgroundOpacity !=
				a_settings.paletteBackgroundOpacity)
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"fMenuPaletteOpacity {} used {}",
						a_settings.paletteBackgroundOpacity,
						runtime.paletteBackgroundOpacity));
			}
			if (runtime.backgroundBlurStrength !=
				a_settings.backgroundBlurStrength)
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"fMenuBackgroundBlurStrength {} used {}",
						a_settings.backgroundBlurStrength,
						runtime.backgroundBlurStrength));
			}
			if (runtime.uiScale != a_settings.uiScale)
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"fMenuUiScale {} used {}",
						a_settings.uiScale,
						runtime.uiScale));
			}
			if (!IsValidBodyFontFamily(a_settings.bodyFontFamily))
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"sMenuBodyFontFamily \"{}\" used \"{}\"",
						a_settings.bodyFontFamily,
						runtime.bodyFontFamily));
			}
			if (!ParseMenuToggleKey(a_settings.menuToggleKey).recognized)
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"sMenuToggleKey \"{}\" used \"{}\"",
						a_settings.menuToggleKey,
						runtime.menuToggleKey));
			}
			return runtime;
		}

		[[nodiscard]] std::string CorrectionSummary(
			const std::vector<std::string>& a_corrections)
		{
			constexpr size_t kVisibleCorrectionLimit = 5;
			std::string result{ "Using corrected settings: " };
			const auto visible = (std::min)(
				a_corrections.size(),
				kVisibleCorrectionLimit);
			for (size_t index = 0; index < visible; ++index)
			{
				if (index != 0)
					result.append("; ");
				result.append(a_corrections[index]);
			}
			if (a_corrections.size() > visible)
			{
				result.append(std::format(
					"; and {} more correction{}",
					a_corrections.size() - visible,
					a_corrections.size() - visible == 1 ? "" : "s"));
			}
			result.append(". Save Settings to persist the accepted values.");
			return result;
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
			result.detail = std::format(
				"Could not inspect {}: {}. Using defaults; check file permissions and restart.",
				result.path,
				existsError.message());
			return result;
		}
		if (!exists)
		{
			result.disposition = HostSettingsLoadDisposition::kMissing;
			result.detail = "Using defaults; configuration file is absent.";
			return result;
		}

		try
		{
			const auto root = toml::parse(a_path.string());
			const auto& section = toml::find(root, "Additional");
			PersistedHostInterfaceSettings settings;
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

			if (root.contains("Hotkeys") && root.at("Hotkeys").is_table())
			{
				for (const auto& [id, value] : root.at("Hotkeys").as_table())
				{
					if (value.is_string())
						result.hotkeys.emplace(id, value.as_string());
					else
						result.corrections.push_back(
							std::format("Hotkeys.{} was ignored because it was not text",
								id));
				}
			}
			result.settings = NormalizeSettings(
				std::move(settings),
				result.corrections);
			result.disposition = result.corrections.empty() ?
				HostSettingsLoadDisposition::kLoaded :
				HostSettingsLoadDisposition::kCorrected;
			result.detail = result.corrections.empty() ?
				std::format("Loaded settings from {}.", result.path) :
				CorrectionSummary(result.corrections);
		}
		catch (const std::exception& error)
		{
			result.settings = {};
			result.hotkeys.clear();
			result.disposition = HostSettingsLoadDisposition::kFailed;
			result.detail = std::format(
				"Could not load {}: {}. Using defaults; correct or remove the file and restart.",
				result.path,
				error.what());
		}
		catch (...)
		{
			result.settings = {};
			result.hotkeys.clear();
			result.disposition = HostSettingsLoadDisposition::kFailed;
			result.detail = std::format(
				"Could not load {}. Using defaults; correct or remove the file and restart.",
				result.path);
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
					"Creating settings directory for",
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
				result.detail = std::format(
					"Saved settings to {}.",
					a_path.filename().string());
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
					result.detail = std::format(
						"Saved settings to {} using a non-atomic cross-volume copy fallback.",
						a_path.filename().string());
					AppendCleanupFailure(result, temporary, RemoveTemporary(temporary));
					return result;
				}

				result.nativeError = fallbackError;
				result.detail = WindowsFailure(
					"Replacing settings file during the cross-volume copy fallback for",
					a_path,
					fallbackError);
				result.detail.append(
					std::format(
						" Windows first reported ERROR_NOT_SAME_DEVICE (17): {}. ",
						DescribeWindowsError(ERROR_NOT_SAME_DEVICE)));
				result.detail.append(
					"This can occur when a mod manager maps the temporary file and "
					"the existing configuration to different drives. Check that "
					"the mod manager's overwrite or output location is writable "
					"and has available disk space, then save again.");
			}
			else
			{
				result.nativeError = moveError;
				result.detail = WindowsFailure(
					"Replacing settings file",
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
			result.detail = std::format(
				"Persisting {} failed: {}",
				a_path.filename().string(),
				error.what());
			AppendCleanupFailure(result, temporary, cleanup);
			return result;
		}
		catch (...)
		{
			const auto cleanup = temporaryCreated ?
				RemoveTemporary(temporary) : TemporaryCleanupResult{};
			result.detail = std::format(
				"Persisting {} failed for an unknown reason.",
				a_path.filename().string());
			AppendCleanupFailure(result, temporary, cleanup);
			return result;
		}
	}

}
