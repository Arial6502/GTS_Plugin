#pragma once

namespace GTS::Actions {

	template <typename T>
	class ActorStateMap {

		public:
		T& GetOrAdd(RE::FormID a_Owner) {
			return m_Data[a_Owner];
		}

		[[nodiscard]] T* Find(RE::FormID a_Owner) {
			auto it = m_Data.find(a_Owner);
			return it == m_Data.end() ? nullptr : &it->second;
		}

		[[nodiscard]] const T* Find(RE::FormID a_Owner) const {
			auto it = m_Data.find(a_Owner);
			return it == m_Data.end() ? nullptr : &it->second;
		}

		void Forget(RE::FormID a_Owner) {
			m_Data.erase(a_Owner);
		}

		void Clear() {
			m_Data.clear();
		}

		[[nodiscard]] bool Contains(RE::FormID a_Owner) const {
			return m_Data.contains(a_Owner);
		}

		[[nodiscard]] std::size_t Size() const {
			return m_Data.size();
		}

		private:
		absl::flat_hash_map<RE::FormID, T> m_Data = {};
	};
}
