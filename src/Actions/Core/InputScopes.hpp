#pragma once

#include "Actions/Core/ActionTypes.hpp"
#include "Actions/Core/IMovementState.hpp"
#include "Managers/Input/InputScope.hpp"

namespace GTS::Actions {

	[[nodiscard]] InputScope RootScope();
	[[nodiscard]] InputScope EntryScope(ActionId a_Entry);
	[[nodiscard]] InputScope StanceScope(MovementId a_Id);
	[[nodiscard]] InputScope NodeScope(ActionId a_Id);
}
