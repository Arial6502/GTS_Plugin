#pragma once

namespace GTS::Actions {

	//One lock for Posession and the ActionRegistry.
	inline std::recursive_mutex ActionLock;
}
