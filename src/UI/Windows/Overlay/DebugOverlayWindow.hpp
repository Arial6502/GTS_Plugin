#pragma once

#include "UI/Core/ImWindow.hpp"

namespace GTS {

	class DebugOverlayWindow final : public ImWindow {

		public:
		void Draw() override;
		bool WantsToDraw() override;
		void Init() override;
		float GetFullAlpha() override;
		float GetBackgroundAlpha() override;
		std::string GetWindowName() override;
		void RequestClose() override;
	};
}
