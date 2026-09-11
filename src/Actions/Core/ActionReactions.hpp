#pragma once

namespace GTS::Actions {

	struct ReactionDef {
		std::string_view Tag;
		void (*Handler)(RE::Actor*) = nullptr;
	};

	class Reactions {

		public:
		static void Register(std::span<const ReactionDef> a_Table, std::string_view a_Group);
		static bool Dispatch(RE::Actor* a_Actor, std::string_view a_Tag);

		[[nodiscard]] static std::string_view GroupOf(std::string_view a_Tag);
		[[nodiscard]] static std::size_t Count();

		private:
		struct Entry {
			void (*Handler)(RE::Actor*) = nullptr;
			std::string_view Group;
		};

		static inline absl::flat_hash_map<std::string, Entry> m_Table = {};
	};
}
