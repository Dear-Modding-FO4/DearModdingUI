#include "ImageFileQueue.h"
#include <Windows.h>
#include <objbase.h>
#include <algorithm>

namespace DearModdingUI::PresentationServices::ImageFiles
{
	FileQueue::~FileQueue()
	{
		{
			const std::scoped_lock lock{ m_mutex };
			m_stopping = true;
		}
		m_changed.notify_one();
		if (m_worker.joinable())
			m_worker.join();
	}

	DMUI_Result FileQueue::Submit(LoadIdentity a_identity, const std::string& a_path) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		const auto owned = [owner = a_identity.owner](const auto& job) {
			return job && job->identity.owner == owner;
		};
		if (m_queue.size() + !!m_active + !!m_completed >= kCapacity ||
			std::count_if(m_queue.begin(), m_queue.end(), owned) +
				owned(m_active) + owned(m_completed) >= kClientCapacity)
			return DMUI_RESULT_BUSY;
		try
		{
			if (!m_worker.joinable())
				m_worker = std::thread{ [this] { Run(); } };
			auto job = std::make_shared<LoadJob>();
			job->identity = a_identity;
			job->path = a_path;
			m_queue.push_back(std::move(job));
			m_changed.notify_one();
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	void FileQueue::Cancel(DMUI_ClientHandle a_owner, DMUI_ImageHandle a_handle) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		const auto matches = [&](const auto& job) {
			return job && job->identity.owner == a_owner &&
				job->identity.handle == a_handle;
		};
		std::erase_if(m_queue, matches);
		if (matches(m_active))
			m_active->cancelled = true;
		if (matches(m_completed))
			m_completed.reset();
		m_changed.notify_one();
	}

	std::shared_ptr<LoadJob> FileQueue::TakeCompletion() noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		auto result = std::move(m_completed);
		m_changed.notify_one();
		return result;
	}

	bool FileQueue::HasCompletion() noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		return !!m_completed;
	}

	void FileQueue::Run() noexcept
	{
		const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		std::unique_lock lock{ m_mutex };
		for (;;)
		{
			// Only one decoded completion may wait for the render thread.
			m_changed.wait(lock, [&] { return m_stopping || (!m_completed && !m_queue.empty()); });
			if (m_stopping)
				break;
			m_active = std::move(m_queue.front());
			m_queue.pop_front();
			auto job = m_active;
			lock.unlock();
			job->result = SUCCEEDED(initialized) ?
				DecodeFile(job->path, job->decoded) : DMUI_RESULT_IMAGE_DECODE_FAILED;
			lock.lock();
			if (!job->cancelled && !m_stopping)
				m_completed = job;
			m_active.reset();
		}
		lock.unlock();
		if (SUCCEEDED(initialized))
			CoUninitialize();
	}
}
