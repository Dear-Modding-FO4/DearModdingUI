#include "RegistryCallbackDispatch.h"

namespace DearModdingUI::RegistryCallbackDispatch
{
	namespace
	{
		[[nodiscard]] bool InvokeReadyCpp(
			DMUI_HostReadyCallback a_callback,
			const DMUI_HostReadyInfo* a_info,
			void* a_userData) noexcept
		{
			try
			{
				a_callback(a_info, a_userData);
				return true;
			}
			catch (...)
			{
				return false;
			}
		}

		[[nodiscard]] bool InvokeUnavailableCpp(
			DMUI_HostUnavailableCallback a_callback,
			DMUI_UnavailableReason a_reason,
			void* a_userData) noexcept
		{
			try
			{
				a_callback(a_reason, a_userData);
				return true;
			}
			catch (...)
			{
				return false;
			}
		}

		[[nodiscard]] DMUI_Result InvokePageCpp(
			DMUI_PageDrawCallback a_callback,
			void* a_userData) noexcept
		{
			try
			{
				return a_callback(a_userData);
			}
			catch (...)
			{
				return DMUI_RESULT_CALLBACK_FAILED;
			}
		}

		[[nodiscard]] bool InvokeActionCpp(
			DMUI_ActionCallback a_callback,
			void* a_userData) noexcept
		{
			try
			{
				a_callback(a_userData);
				return true;
			}
			catch (...)
			{
				return false;
			}
		}

		[[nodiscard]] bool InvokePageActivityCpp(
			DMUI_PageActivityCallback a_callback,
			const DMUI_PageActivityInfo* a_info,
			void* a_userData) noexcept
		{
			try
			{
				a_callback(a_info, a_userData);
				return true;
			}
			catch (...)
			{
				return false;
			}
		}
	}

	bool InvokeReady(
		DMUI_HostReadyCallback a_callback,
		const DMUI_HostReadyInfo* a_info,
		void* a_userData) noexcept
	{
#if defined(_MSC_VER)
		__try
		{
			return InvokeReadyCpp(a_callback, a_info, a_userData);
		}
		__except (1)
		{
			return false;
		}
#else
		return InvokeReadyCpp(a_callback, a_info, a_userData);
#endif
	}

	bool InvokeUnavailable(
		DMUI_HostUnavailableCallback a_callback,
		DMUI_UnavailableReason a_reason,
		void* a_userData) noexcept
	{
#if defined(_MSC_VER)
		__try
		{
			return InvokeUnavailableCpp(a_callback, a_reason, a_userData);
		}
		__except (1)
		{
			return false;
		}
#else
		return InvokeUnavailableCpp(a_callback, a_reason, a_userData);
#endif
	}

	DMUI_Result InvokePage(
		DMUI_PageDrawCallback a_callback,
		void* a_userData) noexcept
	{
#if defined(_MSC_VER)
		__try
		{
			return InvokePageCpp(a_callback, a_userData);
		}
		__except (1)
		{
			return DMUI_RESULT_CALLBACK_FAILED;
		}
#else
		return InvokePageCpp(a_callback, a_userData);
#endif
	}

	bool InvokeAction(
		DMUI_ActionCallback a_callback,
		void* a_userData) noexcept
	{
#if defined(_MSC_VER)
		__try
		{
			return InvokeActionCpp(a_callback, a_userData);
		}
		__except (1)
		{
			return false;
		}
#else
		return InvokeActionCpp(a_callback, a_userData);
#endif
	}

	bool InvokePageActivity(
		DMUI_PageActivityCallback a_callback,
		const DMUI_PageActivityInfo* a_info,
		void* a_userData) noexcept
	{
#if defined(_MSC_VER)
		__try
		{
			return InvokePageActivityCpp(a_callback, a_info, a_userData);
		}
		__except (1)
		{
			return false;
		}
#else
		return InvokePageActivityCpp(a_callback, a_info, a_userData);
#endif
	}
}
