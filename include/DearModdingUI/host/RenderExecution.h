#pragma once

#include <DearModdingUI/API.h>

#include <cstdint>

namespace DearModdingUI::RenderExecution
{
	enum class Phase : uint32_t
	{
		kBackendInitialization,
		kFrameDraw,
		kFrameObservation
	};

	struct ThreadTransition
	{
		uint64_t previousThread{};
		uint64_t currentThread{};
		uint64_t bindingId{};
		Phase phase{ Phase::kFrameDraw };
		bool changed{};
	};

	class Guard
	{
	public:
		explicit Guard(Phase a_phase) noexcept;
		~Guard() noexcept;

		Guard(const Guard&) = delete;
		Guard(Guard&&) = delete;
		Guard& operator=(const Guard&) = delete;
		Guard& operator=(Guard&&) = delete;

		[[nodiscard]] ThreadTransition NoteBinding(uint64_t a_bindingId) noexcept;

	private:
		Phase m_previousPhase;
		uint64_t m_previousBinding;
		bool m_outermost;
		bool m_bindingNoted{};
	};

	class ClientGuard
	{
	public:
		ClientGuard(DMUI_ClientHandle a_client, bool a_drawing) noexcept;
		~ClientGuard() noexcept;

		ClientGuard(const ClientGuard&) = delete;
		ClientGuard(ClientGuard&&) = delete;
		ClientGuard& operator=(const ClientGuard&) = delete;
		ClientGuard& operator=(ClientGuard&&) = delete;

	private:
		DMUI_ClientHandle m_previousClient;
		bool m_previousDrawing;
	};

	[[nodiscard]] bool IsActive() noexcept;
	[[nodiscard]] bool IsActiveClient(
		DMUI_ClientHandle a_client,
		bool a_drawingRequired) noexcept;
	[[nodiscard]] Phase ActivePhase() noexcept;
	[[nodiscard]] uint64_t ActiveBinding() noexcept;
}
