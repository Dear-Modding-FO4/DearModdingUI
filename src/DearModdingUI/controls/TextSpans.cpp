#include <DearModdingUI/controls/TextSpans.h>

#include <DearModdingUI/controls/TextRuns.h>
#include <Support/Utf8.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <new>
#include <vector>

namespace DearModdingUI
{
	namespace
	{
		constexpr uint32_t kKnownOptionFlags{
			DMUI_TEXT_OPTION_WRAP | DMUI_TEXT_OPTION_BULLET
		};

		enum class PieceKind : uint8_t
		{
			kWord,
			// May break on either side: CJK and fragments of overlong words.
			kBreakable,
			kSpace,
			kNewline
		};

		struct Piece
		{
			const char* begin;
			const char* end;
			float width;
			uint32_t segment;
			PieceKind kind;
			// No break opportunity before this piece (a word split across spans).
			bool joinsPrevious;
		};

		// A span, or the default-styled text between spans.
		struct Segment
		{
			const char* begin;
			const char* end;
			const DMUI_TextSpan* span;
		};

		struct Placed
		{
			const char* begin;
			const char* end;
			float x;
			float width;
			uint32_t segment;
		};

		struct Line
		{
			size_t first;
			size_t count;
			float width;
			float height;
		};

		// Reused across calls on the render thread to avoid per-frame allocation.
		struct Scratch
		{
			std::vector<Segment> segments;
			std::vector<Piece> pieces;
			std::vector<TextRuns::Style> styles;
			std::vector<Placed> placed;
			std::vector<Line> lines;
			std::vector<TextRuns::Run> runs;

			void Clear() noexcept
			{
				segments.clear();
				pieces.clear();
				styles.clear();
				placed.clear();
				lines.clear();
				runs.clear();
			}

			// One huge call must not pin its peak buffers for the session.
			void Trim() noexcept
			{
				constexpr size_t kRetainedPieces{ 4096 };
				if (pieces.capacity() > kRetainedPieces ||
					placed.capacity() > kRetainedPieces)
					*this = {};
			}
		};

		struct ScratchLease
		{
			Scratch& scratch;

			~ScratchLease() noexcept
			{
				scratch.Trim();
			}
		};

		struct FontScope
		{
			explicit FontScope(const TextRuns::Style& a_style) noexcept
			{
				ImGui::PushFont(a_style.font, a_style.size);
			}

			~FontScope() noexcept
			{
				ImGui::PopFont();
			}

			FontScope(const FontScope&) = delete;
			FontScope& operator=(const FontScope&) = delete;
		};

		[[nodiscard]] bool BreaksAnywhere(unsigned int a_codepoint) noexcept
		{
			return (a_codepoint >= 0x2E80u && a_codepoint <= 0xA4CFu) ||
				(a_codepoint >= 0xAC00u && a_codepoint <= 0xD7AFu) ||
				(a_codepoint >= 0xF900u && a_codepoint <= 0xFAFFu) ||
				(a_codepoint >= 0xFE30u && a_codepoint <= 0xFE4Fu) ||
				(a_codepoint >= 0xFF00u && a_codepoint <= 0xFFEFu) ||
				(a_codepoint >= 0x20000u && a_codepoint <= 0x3FFFFu);
		}

		[[nodiscard]] bool IsBlank(unsigned int a_codepoint) noexcept
		{
			return a_codepoint == ' ' || a_codepoint == '\t';
		}

		struct Decoded
		{
			unsigned int codepoint;
			const char* next;
		};

		[[nodiscard]] Decoded Decode(const char* a_at, const char* a_end) noexcept
		{
			unsigned int codepoint{};
			const auto length = ImTextCharFromUtf8(&codepoint, a_at, a_end);
			return { codepoint, a_at + (std::max)(length, 1) };
		}

		[[nodiscard]] float Measure(const char* a_begin, const char* a_end) noexcept
		{
			return ImGui::CalcTextSize(a_begin, a_end, false).x;
		}

		void PushPiece(
			Scratch& a_scratch, const char* a_begin, const char* a_end,
			uint32_t a_segment, PieceKind a_kind, bool a_joins = false)
		{
			a_scratch.pieces.push_back({
				a_begin, a_end,
				a_kind == PieceKind::kNewline ? 0.0f : Measure(a_begin, a_end),
				a_segment, a_kind, a_joins });
		}

		// Splits one segment into words, spaces, newlines, and break-anywhere pieces.
		void Tokenize(
			Scratch& a_scratch, const Segment& a_segment,
			uint32_t a_index, float a_lineWidth, bool a_wrap)
		{
			const char* const end = a_segment.end;
			for (const char* at = a_segment.begin; at < end;)
			{
				const auto head = Decode(at, end);
				if (head.codepoint == '\r')
				{
					at = head.next;
					continue;
				}
				if (head.codepoint == '\n')
				{
					PushPiece(a_scratch, at, head.next, a_index, PieceKind::kNewline);
					at = head.next;
					continue;
				}
				if (IsBlank(head.codepoint))
				{
					const char* stop = head.next;
					while (stop < end && IsBlank(static_cast<unsigned char>(*stop)))
						++stop;
					PushPiece(a_scratch, at, stop, a_index, PieceKind::kSpace);
					at = stop;
					continue;
				}
				if (BreaksAnywhere(head.codepoint))
				{
					PushPiece(a_scratch, at, head.next, a_index, PieceKind::kBreakable);
					at = head.next;
					continue;
				}
				const char* stop = head.next;
				while (stop < end)
				{
					const auto next = Decode(stop, end);
					if (IsBlank(next.codepoint) || next.codepoint == '\n' ||
						next.codepoint == '\r' || BreaksAnywhere(next.codepoint))
						break;
					stop = next.next;
				}
				if (a_wrap && Measure(at, stop) > a_lineWidth)
				{
					for (const char* cut = at; cut < stop;)
					{
						const auto step = Decode(cut, stop);
						PushPiece(a_scratch, cut, step.next, a_index, PieceKind::kBreakable);
						cut = step.next;
					}
				}
				else
				{
					const auto joins = at == a_segment.begin && !a_scratch.pieces.empty() &&
						a_scratch.pieces.back().kind == PieceKind::kWord;
					PushPiece(a_scratch, at, stop, a_index, PieceKind::kWord, joins);
				}
				at = stop;
			}
		}

		// Greedy line breaking over the measured pieces.
		class LineBuilder
		{
		public:
			LineBuilder(
				Scratch& a_scratch, float a_lineWidth, bool a_wrap,
				float a_blankHeight) noexcept :
				m_scratch(a_scratch), m_lineWidth(a_lineWidth), m_wrap(a_wrap),
				m_blankHeight(a_blankHeight)
			{}

			void Layout()
			{
				const auto& pieces = m_scratch.pieces;
				for (size_t index = 0; index < pieces.size();)
				{
					const auto& piece = pieces[index];
					if (piece.kind == PieceKind::kNewline)
					{
						Finish();
						++index;
						continue;
					}
					if (piece.kind == PieceKind::kSpace)
					{
						// Spaces that would overflow or lead a wrapped line vanish.
						if (m_wrap && m_x > 0.0f && m_x + piece.width > m_lineWidth)
							Wrap();
						else if (!(m_softWrapped && m_x == 0.0f))
							Place(piece);
						++index;
						continue;
					}
					// A word split across spans moves as one unit when it fits a line.
					size_t stop = index + 1;
					float groupWidth = piece.width;
					while (stop < pieces.size() && pieces[stop].joinsPrevious)
						groupWidth += pieces[stop++].width;
					if (m_wrap && m_x > 0.0f && m_x + groupWidth > m_lineWidth &&
						groupWidth <= m_lineWidth)
						Wrap();
					for (; index < stop; ++index)
						Place(pieces[index]);
				}
				if (m_scratch.placed.size() > m_first || m_scratch.lines.empty())
					Finish();
			}

		private:
			void Place(const Piece& a_piece)
			{
				if (m_wrap && a_piece.kind != PieceKind::kSpace && m_x > 0.0f &&
					m_x + a_piece.width > m_lineWidth)
					Wrap();
				m_scratch.placed.push_back({
					a_piece.begin, a_piece.end, m_x, a_piece.width, a_piece.segment });
				m_x += a_piece.width;
				m_height = (std::max)(m_height, m_scratch.styles[a_piece.segment].size);
				m_trailing = a_piece.kind == PieceKind::kSpace ? m_trailing + a_piece.width : 0.0f;
			}

			void Wrap()
			{
				Finish();
				m_softWrapped = true;
			}

			void Finish()
			{
				const auto count = m_scratch.placed.size() - m_first;
				m_scratch.lines.push_back({
					m_first, count, (std::max)(m_x - m_trailing, 0.0f),
					count != 0 ? m_height : m_blankHeight });
				m_first = m_scratch.placed.size();
				m_x = 0.0f;
				m_height = 0.0f;
				m_trailing = 0.0f;
				m_softWrapped = false;
			}

			Scratch& m_scratch;
			float m_lineWidth;
			bool m_wrap;
			float m_blankHeight;
			size_t m_first{};
			float m_x{};
			float m_height{};
			float m_trailing{};
			bool m_softWrapped{};
		};
	}

	bool ValidTextOptions(const DMUI_TextOptions& a_options) noexcept
	{
		return (a_options.flags & ~kKnownOptionFlags) == 0 &&
			std::isfinite(a_options.wrapWidth) && a_options.wrapWidth >= 0.0f;
	}

	DMUI_Result ValidateTextSpans(
		std::string_view a_text,
		std::span<const DMUI_TextSpan> a_spans,
		const DMUI_TextOptions& a_options) noexcept
	{
		if (!ValidTextOptions(a_options) ||
			a_spans.size() > kMaxTextSpans || a_text.size() > kMaxStyledTextBytes ||
			!Support::IsValidUtf8(a_text.data(), a_text.size()))
			return DMUI_RESULT_INVALID_ARGUMENT;
		return TextRuns::Validate(TextRuns::Consumer::kText, a_text, a_spans);
	}

	DMUI_Result DrawTextSpans(
		std::string_view a_text,
		std::span<const DMUI_TextSpan> a_spans,
		const DMUI_TextOptions& a_options) noexcept
	{
		auto* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return DMUI_RESULT_OK;
		try
		{
			thread_local Scratch scratch;
			scratch.Clear();
			const ScratchLease lease{ scratch };

			const auto& style = ImGui::GetStyle();
			const auto wrap = (a_options.flags & DMUI_TEXT_OPTION_WRAP) != 0;
			const auto bullet = (a_options.flags & DMUI_TEXT_OPTION_BULLET) != 0;
			const auto indent = bullet ?
				ImGui::GetFontSize() + style.FramePadding.x * 2.0f : 0.0f;
			const auto regionWidth = a_options.wrapWidth > 0.0f ?
				a_options.wrapWidth : ImGui::GetContentRegionAvail().x;
			const auto lineWidth = (std::max)(regionWidth - indent, 1.0f);

			// Spans and the default-styled gaps between them, in text order.
			auto& segments = scratch.segments;
			const char* const textBegin = a_text.data();
			const char* const textEnd = textBegin + a_text.size();
			const char* cursor = textBegin;
			for (const auto& span : a_spans)
			{
				const char* const begin = textBegin + span.offset;
				if (begin > cursor)
					segments.push_back({ cursor, begin, nullptr });
				if (span.length != 0)
					segments.push_back({ begin, begin + span.length, &span });
				cursor = begin + span.length;
			}
			if (textEnd > cursor)
				segments.push_back({ cursor, textEnd, nullptr });

			for (uint32_t index = 0; index < segments.size(); ++index)
			{
				scratch.styles.push_back(
					TextRuns::Resolve(TextRuns::Consumer::kText, segments[index].span));
				// Pieces are measured in their own face.
				const FontScope face{ scratch.styles.back() };
				Tokenize(scratch, segments[index], index, lineWidth, wrap);
			}

			LineBuilder{ scratch, lineWidth, wrap, ImGui::GetFontSize() }.Layout();

			float width{}, height{};
			for (const auto& line : scratch.lines)
			{
				width = (std::max)(width, line.width);
				height += line.height;
			}
			width += indent;

			const ImVec2 origin{
				window->DC.CursorPos.x,
				window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset
			};
			const ImRect bounds{ origin, { origin.x + width, origin.y + height } };
			ImGui::ItemSize(bounds.GetSize(), 0.0f);
			if (!ImGui::ItemAdd(bounds, 0))
				return DMUI_RESULT_OK;

			auto* draw = window->DrawList;
			if (bullet)
			{
				ImGui::RenderBullet(
					draw,
					{ origin.x + style.FramePadding.x + ImGui::GetFontSize() * 0.5f,
						origin.y + scratch.lines.front().height * 0.5f },
					scratch.styles.empty() ?
						ImGui::GetColorU32(ImGuiCol_Text) : scratch.styles.front().color);
			}
			const ImVec4 clip{
				window->ClipRect.Min.x, window->ClipRect.Min.y,
				window->ClipRect.Max.x, window->ClipRect.Max.y };
			float top = origin.y;
			for (const auto& line : scratch.lines)
			{
				scratch.runs.clear();
				for (size_t offset = 0; offset < line.count; ++offset)
				{
					const auto& piece = scratch.placed[line.first + offset];
					const auto& face = scratch.styles[piece.segment];
					// Bottom alignment approximates a shared baseline.
					scratch.runs.push_back({
						piece.begin, piece.end, piece.x, piece.width,
						line.height - face.size, face });
				}
				TextRuns::Paint(
					*draw, { origin.x + indent, top }, line.height, scratch.runs, {}, clip);
				top += line.height;
			}
			return DMUI_RESULT_OK;
		}
		catch (const std::bad_alloc&)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}
}
