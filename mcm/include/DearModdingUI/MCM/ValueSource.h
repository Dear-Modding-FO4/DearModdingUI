#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace DearModdingUI::MCM
{
	struct ReadyValue
	{
		dmui::SettingValue value;
		uint64_t generation{};
	};

	struct PendingValue
	{
		uint64_t generation{};
	};

	struct MissingValue
	{
		uint64_t generation{};
	};

	struct FailedValue
	{
		uint64_t generation{};
	};

	using ValueSnapshot =
		std::variant<ReadyValue, PendingValue, MissingValue, FailedValue>;

	[[nodiscard]] uint64_t Generation(const ValueSnapshot& a_snapshot) noexcept;
	[[nodiscard]] std::string MakeBindingKey(const MappedBinding& a_binding);

	class ValueCache
	{
	public:
		[[nodiscard]] ValueSnapshot Read(std::string_view a_key) const;
		[[nodiscard]] uint64_t BeginRefresh(std::string_view a_key);
		[[nodiscard]] ValueSnapshot Store(
			std::string_view a_key,
			dmui::SettingValue a_value);
		[[nodiscard]] bool Complete(
			std::string_view a_key,
			ValueSnapshot a_snapshot);

	private:
		struct TransparentHash
		{
			using is_transparent = void;

			[[nodiscard]] size_t operator()(std::string_view a_value) const noexcept
			{
				return std::hash<std::string_view>{}(a_value);
			}
		};

		mutable std::mutex mutex_;
		std::unordered_map<
			std::string,
			ValueSnapshot,
			TransparentHash,
			std::equal_to<>> values_;
	};

	enum class ConditionResult : uint8_t
	{
		kHidden,
		kVisible,
		kPending,
		kUnavailable
	};

	using ConditionValueResolver =
		std::function<ValueSnapshot(int64_t a_control)>;

	class ValueSource;

	[[nodiscard]] ConditionResult EvaluateCondition(
		const GroupCondition& a_condition,
		const ConditionValueResolver& a_resolve);

	[[nodiscard]] PageCompatibilitySummary SummarizeCompatibility(
		const MappedPage& a_page,
		const ValueSource& a_source);

	class McmEventDispatcher
	{
	public:
		McmEventDispatcher() = default;
		virtual ~McmEventDispatcher() = default;

		McmEventDispatcher(const McmEventDispatcher&) = delete;
		McmEventDispatcher(McmEventDispatcher&&) = delete;
		McmEventDispatcher& operator=(const McmEventDispatcher&) = delete;
		McmEventDispatcher& operator=(McmEventDispatcher&&) = delete;

		virtual void SettingChanged(
			std::string_view a_modName,
			std::string_view a_controlId) noexcept = 0;
		virtual void MenuOpened() noexcept = 0;
		virtual void MenuClosed() noexcept = 0;
	};

	void NotifyAcceptedModSettingWrite(
		McmEventDispatcher& a_dispatcher,
		std::string_view a_modName,
		const MappedBinding& a_binding) noexcept;

	class ValueSource
	{
	public:
		ValueSource() = default;
		virtual ~ValueSource() = default;

		ValueSource(const ValueSource&) = delete;
		ValueSource(ValueSource&&) = delete;
		ValueSource& operator=(const ValueSource&) = delete;
		ValueSource& operator=(ValueSource&&) = delete;

		[[nodiscard]] virtual bool Supports(
			SourceFamily a_family) const noexcept = 0;

		// Runs for every visible row every frame, so it must never dispatch or block.
		[[nodiscard]] virtual ValueSnapshot Read(
			const MappedBinding& a_binding) const = 0;

		[[nodiscard]] virtual uint64_t Refresh(
			const MappedBinding& a_binding) = 0;

		[[nodiscard]] virtual ValueSnapshot Write(
			const MappedBinding& a_binding,
			const dmui::SettingValue& a_value) = 0;

		virtual void RefreshPage(const MappedPage& a_page);
		virtual void Pump() noexcept {}
	};

	class CompositeValueSource final : public ValueSource
	{
	public:
		void Add(ValueSource& a_source);

		[[nodiscard]] bool Supports(
			SourceFamily a_family) const noexcept override;

		[[nodiscard]] ValueSnapshot Read(
			const MappedBinding& a_binding) const override;

		[[nodiscard]] uint64_t Refresh(
			const MappedBinding& a_binding) override;

		[[nodiscard]] ValueSnapshot Write(
			const MappedBinding& a_binding,
			const dmui::SettingValue& a_value) override;

		void RefreshPage(const MappedPage& a_page) override;
		void Pump() noexcept override;

	private:
		[[nodiscard]] ValueSource* Find(SourceFamily a_family) const noexcept;

		std::vector<std::reference_wrapper<ValueSource>> sources_;
	};

	// The source must outlive the page, whose descriptors capture it by reference.
	void BindPage(MappedPage& a_page, ValueSource& a_source);
}
