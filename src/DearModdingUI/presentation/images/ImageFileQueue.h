#pragma once

#include "ImageFileDecode.h"
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace DearModdingUI::PresentationServices::ImageFiles
{
	struct LoadIdentity
	{
		DMUI_ClientHandle owner{};
		DMUI_ImageHandle handle{};
		uint64_t loadGeneration{};
		uint64_t deviceGeneration{};
	};

	struct LoadJob
	{
		LoadIdentity identity;
		std::string path;
		DecodedImage decoded;
		DMUI_Result result{ DMUI_RESULT_OK };
		bool cancelled{};
	};

	class FileQueue
	{
	public:
		static constexpr size_t kCapacity{ 32 };
		static constexpr size_t kClientCapacity{ 4 };
		~FileQueue();
		[[nodiscard]] DMUI_Result Submit(LoadIdentity a_identity, const std::string& a_path) noexcept;
		void Cancel(DMUI_ClientHandle a_owner, DMUI_ImageHandle a_handle) noexcept;
		[[nodiscard]] std::shared_ptr<LoadJob> TakeCompletion() noexcept;
		[[nodiscard]] bool HasCompletion() noexcept;

	private:
		void Run() noexcept;
		std::mutex m_mutex;
		std::condition_variable m_changed;
		std::deque<std::shared_ptr<LoadJob>> m_queue;
		std::shared_ptr<LoadJob> m_active;
		std::shared_ptr<LoadJob> m_completed;
		bool m_stopping{};
		std::thread m_worker;
	};
}
