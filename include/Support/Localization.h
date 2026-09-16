#pragma once

#include <string>
#include <vector>

#include <REX/REX.h>

namespace DearModdingUI::Support
{
	struct ILocalizeString
	{
		virtual void Load(void* a_data) = 0;
	};

	struct ILocalizeStore
	{
		virtual void Init(const std::string& a_file, bool a_isMultilang = false) noexcept = 0;
		virtual bool Exists() const noexcept = 0;
		virtual void Add(ILocalizeString* a_setting) noexcept = 0;
		virtual void Load() = 0;
		virtual std::string GetFileName() const noexcept = 0;
	};

	class BaseLocalizeString :
		public ILocalizeString
	{
	protected:
		std::string value;
		std::string valueDefault;
	public:
		BaseLocalizeString() = delete;
		BaseLocalizeString(const std::string& a_default) noexcept;

		std::string GetValue() const noexcept;
		std::string GetValueDefault() const noexcept;
		void SetValue(const std::string& a_value) noexcept;

		operator std::string& () noexcept;
		operator const std::string& () const noexcept;
	};

	class LocalizeStore :
		public ILocalizeStore
	{
	protected:
		std::string						file;
		std::vector<ILocalizeString*>	localizes;
	public:
		void Init(const std::string& a_file, bool a_isMultilang = false) noexcept override;
		bool Exists() const noexcept override;
		void Add(ILocalizeString* a_localize) noexcept override;
		std::string GetFileName() const noexcept override;
	};

	class LocalizationManager :
		public LocalizeStore,
		public REX::TSingleton<LocalizationManager>
	{
	public:
		void Load() override;
	};

	class LocalizeString :
		public BaseLocalizeString
	{
		std::string key;
	public:
		LocalizeString(const std::string& a_key, const std::string& a_default) noexcept;

		void Load(void* a_data) noexcept override;
	};

	using LOC = LocalizeString;
}
