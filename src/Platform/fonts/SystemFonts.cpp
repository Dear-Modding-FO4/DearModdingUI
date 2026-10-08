#include <Platform/fonts/SystemFonts.h>

#include <REX/REX.h>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <climits>
#include <filesystem>
#include <string>

namespace DearModdingUI
{
	using namespace std::literals;

	namespace
	{
		struct FaceCandidate
		{
			std::string_view name;
			std::wstring_view file;
			std::string_view language;
		};

		inline constexpr FaceCandidate kScriptFace{
			"Segoe UI", L"segoeui.ttf", {}
		};
		// Han shapes differ by locale, so the active language's face leads.
		inline constexpr std::array<FaceCandidate, 4> kCjkFaces{
			FaceCandidate{ "Microsoft YaHei", L"msyh.ttc", "zhhans" },
			FaceCandidate{ "Microsoft JhengHei", L"msjh.ttc", "cn" },
			FaceCandidate{ "Yu Gothic", L"YuGothR.ttc", "ja" },
			FaceCandidate{ "Malgun Gothic", L"malgun.ttf", "ko" }
		};

		[[nodiscard]] std::filesystem::path FontDirectory()
		{
			std::array<wchar_t, MAX_PATH> buffer{};
			const auto length = ::GetWindowsDirectoryW(
				buffer.data(),
				static_cast<UINT>(buffer.size()));
			if (length == 0 || length >= buffer.size())
				return {};
			return std::filesystem::path{ std::wstring_view{ buffer.data(), length } } / L"Fonts";
		}

		[[nodiscard]] std::span<const std::byte> MapReadOnly(
			const std::filesystem::path& a_path) noexcept
		{
			const auto file = ::CreateFileW(
				a_path.c_str(),
				GENERIC_READ,
				FILE_SHARE_READ,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				nullptr);
			if (file == INVALID_HANDLE_VALUE)
				return {};
			LARGE_INTEGER size{};
			HANDLE mapping{ nullptr };
			// ImGui addresses font data with an int size.
			if (::GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart <= INT_MAX)
				mapping = ::CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
			::CloseHandle(file);
			if (!mapping)
				return {};
			const auto* view = ::MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
			::CloseHandle(mapping);
			if (!view)
				return {};
			return { static_cast<const std::byte*>(view), static_cast<size_t>(size.QuadPart) };
		}

		[[nodiscard]] std::vector<SystemFontFace> ResolveFaces(std::string_view a_language)
		{
			const auto directory = FontDirectory();
			if (directory.empty())
				return {};

			auto cjk = kCjkFaces;
			std::ranges::stable_partition(cjk, [&](const FaceCandidate& a_face) {
				return a_face.language == a_language;
			});

			std::vector<SystemFontFace> faces;
			const auto add = [&](const FaceCandidate& a_face) {
				const auto data = MapReadOnly(directory / a_face.file);
				if (data.empty())
				{
					REX::WARN("DearModdingUI: system fallback font {} is unavailable"sv, a_face.name);
					return;
				}
				faces.push_back({ a_face.name, data });
			};
			add(kScriptFace);
			for (const auto& face : cjk)
				add(face);
			return faces;
		}
	}

	const std::vector<SystemFontFace>& SystemFallbackFonts(std::string_view a_language) noexcept
	{
		static const auto faces = [&]() noexcept -> std::vector<SystemFontFace> {
			try
			{
				return ResolveFaces(a_language);
			}
			catch (...)
			{
				return {};
			}
		}();
		return faces;
	}
}
