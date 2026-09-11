#pragma once

#include <Platform/rendering/ImGuiPlatformTargets.h>

namespace Addictol::ImguiPlatform
{
	class FrameSubmission
	{
	public:
		[[nodiscard]] bool Claim(PresentAttachmentToken a_attachment) noexcept
		{
			if (!a_attachment.Valid() || Matches(a_attachment))
				return false;
			m_attachment = a_attachment;
			m_complete = false;
			return true;
		}

		void Complete(PresentAttachmentToken a_attachment) noexcept
		{
			if (Matches(a_attachment))
				m_complete = true;
		}

		[[nodiscard]] bool Submitted(PresentAttachmentToken a_attachment) const noexcept
		{
			return Matches(a_attachment) && m_complete;
		}

		void FinishPresent(PresentAttachmentToken a_attachment, uint32_t a_flags) noexcept
		{
			if ((a_flags & kPresentTestFlag) == 0 && Matches(a_attachment))
				Reset();
		}

		void Reset() noexcept
		{
			m_attachment = {};
			m_complete = false;
		}

	private:
		[[nodiscard]] bool Matches(PresentAttachmentToken a_attachment) const noexcept
		{
			return a_attachment.Valid() &&
				m_attachment.swapChain == a_attachment.swapChain &&
				m_attachment.generation == a_attachment.generation;
		}

		PresentAttachmentToken m_attachment{};
		bool m_complete{};
	};
}
