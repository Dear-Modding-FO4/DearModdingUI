#pragma once

#include <cstddef>

namespace DearModdingUI::UI
{
	struct LayoutDepths
	{
		size_t styles{};
		size_t panels{};
		[[nodiscard]] bool operator==(const LayoutDepths&) const noexcept = default;
	};

	[[nodiscard]] LayoutDepths GetLayoutDepths() noexcept;

	class LayoutRecovery
	{
	public:
		LayoutRecovery() noexcept;
		LayoutRecovery(LayoutRecovery&& a_other) noexcept;
		LayoutRecovery(const LayoutRecovery&) = delete;
		LayoutRecovery& operator=(const LayoutRecovery&) = delete;
		~LayoutRecovery() noexcept;
		void Recover() noexcept;

	private:
		LayoutDepths m_depth;
		LayoutDepths m_previousFloor;
		bool m_active{ true };
	};
}
