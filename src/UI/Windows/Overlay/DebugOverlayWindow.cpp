#include "UI/Windows/Overlay/DebugOverlayWindow.hpp"

#include "Debug/DebugDraw.hpp"
#include "Systems/Misc/State.hpp"

namespace GTS {

	void DebugOverlayWindow::Init() {

		m_name = "DebugOverlay";
		m_title = "Debug Overlay";

		//kHUD context: depth 0, no cursor, no input theft.
		m_windowType = kWidget;
		m_anchorPos = WindowAnchor::kTopLeft;
		m_drawOrder = -1000;
		m_fadeSettings.enabled = false;

		m_flags =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoNavFocus |
			ImGuiWindowFlags_NoNavInputs |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoBringToFrontOnFocus;
	}

	void DebugOverlayWindow::Draw() {

		const ImGuiViewport* viewport = ImGui::GetMainViewport();

		ImGui::SetWindowPos(viewport->Pos);
		ImGui::SetWindowSize(viewport->Size);

		DebugDraw::GetSingleton().Render();
	}

	bool DebugOverlayWindow::WantsToDraw() {
		return DebugDraw::Active() && State::InGame();
	}

	float DebugOverlayWindow::GetFullAlpha() {
		return 1.0f;
	}

	float DebugOverlayWindow::GetBackgroundAlpha() {
		return 0.0f;
	}

	std::string DebugOverlayWindow::GetWindowName() {
		return m_name;
	}

	void DebugOverlayWindow::RequestClose() {}
}
