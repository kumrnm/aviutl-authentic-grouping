#pragma once

#include <vector>
#include <iostream>

namespace debug_util {

	using t_address = unsigned int;

	/*
	* メモリ上から、T型の値として読んだときに特定の値となる箇所を検索します。
	* set_range(begin, size) で範囲を指定した後
	* filter系メソッドで候補を絞り込みます。
	*/
	template<typename T>
	class MemorySearcher {
	public:
		struct Range {
			t_address begin;
			t_address size;
		};

		struct Candidate {
			T* address;
			T value;
		};

	private:
		Range range{ NULL, 0 };
		std::vector<Candidate> candidates;
		bool not_filtered = true;

	public:

		void set_range(const t_address& begin, const t_address& size) {
			range = Range{ begin, size };
			candidates.clear();
			not_filtered = true;
		}

		void filter_by_value(const T& value) {
			std::vector<Candidate> filtered_candidates;

			if (not_filtered) {
				for (t_address pt = range.begin, pt_end = range.begin + range.size - sizeof(T); pt <= pt_end; ++pt) {
					if (*(T*)pt == value) {
						filtered_candidates.push_back(Candidate{ (T*)pt, *(T*)pt });
					}
				}
				not_filtered = false;
			}
			else {
				for (const auto& [pt, old_value] : candidates) {
					if (*(T*)pt == value) {
						filtered_candidates.push_back(Candidate{ (T*)pt, *(T*)pt });
					}
				}
			}

			candidates = filtered_candidates;
		}

		inline const Range get_range() const {
			return range;
		}

		inline const std::vector<Candidate> get_candidates() const {
			return candidates;
		}
	};

	int max_print_lines = 64;

	template<typename T>
	std::ostream& operator<<(std::ostream& stream, const MemorySearcher<T>& ms) {
		std::ios_base::fmtflags stream_flags(stream.flags());

		const auto range = ms.get_range();

		const auto candidates = ms.get_candidates();
		stream << "candidates: " << candidates.size() << "\n";
		stream << std::hex;
		for (int i = 0; i < candidates.size(); ++i) {
			const auto c = candidates[i];
			stream << "\t0x" << (t_address)c.address << "\t(offset: 0x" << (t_address)c.address - range.begin << ")\n";

			if (i >= max_print_lines) {
				stream << "\tand more...\n";
				break;
			}
		}

		stream.flags(stream_flags);
		return stream;
	}

}