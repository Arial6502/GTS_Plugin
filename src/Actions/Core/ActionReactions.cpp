#include "Actions/Core/ActionReactions.hpp"

namespace GTS::Actions {

	void Reactions::Register(std::span<const ReactionDef> a_Table, std::string_view a_Group) {

		for (const auto& def : a_Table) {

			if (def.Tag.empty()) {
				logger::error("Reaction in group {} has no tag", a_Group);
				continue;
			}

			auto [it, added] = m_Table.try_emplace(std::string(def.Tag), Entry{ def.Handler, a_Group });

			if (!added) {
				logger::error("Reaction tag {} is claimed by {} and by {}", def.Tag, it->second.Group, a_Group);
			}
		}
	}

	bool Reactions::Dispatch(RE::Actor* a_Actor, std::string_view a_Tag) {

		auto it = m_Table.find(a_Tag);

		if (it == m_Table.end()) {
			return false;
		}

		if (it->second.Handler) {
			it->second.Handler(a_Actor);
		}

		return true;
	}

	std::string_view Reactions::GroupOf(std::string_view a_Tag) {

		auto it = m_Table.find(a_Tag);
		return it != m_Table.end() ? it->second.Group : std::string_view{};
	}

	std::size_t Reactions::Count() {
		return m_Table.size();
	}
}
