#pragma once

#include "Actions/Core/ActionTypes.hpp"

namespace GTS::Actions {

	class IActionDriver {

		public:
		virtual ~IActionDriver() = default;

		[[nodiscard]] virtual std::string_view Name() const = 0;
		virtual void Poll(RE::Actor* a_Actor, ActionId a_Current) = 0;

		virtual void OnForget(RE::FormID a_Owner) {}
		virtual void OnReset() {}
	};
}
