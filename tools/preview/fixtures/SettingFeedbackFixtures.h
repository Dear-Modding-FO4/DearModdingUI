#pragma once

#include <DearModdingUI/Client.h>

#include <memory>
#include <string>

namespace DmuiTestFixtures
{
	class SettingFeedbackFixture
	{
	public:
		[[nodiscard]] bool Register(std::string& a_error);

	private:
		enum class Placement
		{
			kLabel,
			kControl,
			kStrip
		};

		void Draw(Placement a_placement);

		int m_minimum{ 80 };
		int m_maximum{ 65 };
		int m_reach{ 300 };
		int m_standaloneDistance{ 350 };
		bool m_showChance{ true };
		bool m_showMessages{ true };
		std::unique_ptr<dmui::Client> m_client;
	};
}
