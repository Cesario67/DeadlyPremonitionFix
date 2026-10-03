#pragma once

#include <Windows.h>

class WindowManager {
	static WindowManager instance;
	
	bool captureCursor, cursorVisible;

	bool borderlessFullscreen;
	RECT prevWindowRect;
	long prevStyle, prevExStyle;

	// Modifié pour DPStabilityFix : fenêtre du jeu mémorisée à la création du périphérique.
	// GetActiveWindow() ne renvoie la fenêtre que sur le thread qui la possède ; DP.exe 1.01b dessine
	// depuis un autre thread (patch « multi-cœurs »), où elle renvoie NULL : le mode sans bordure se
	// réappliquait à chaque image et les raccourcis clavier ne fonctionnaient jamais.
	HWND gameWindow;

public:
	static WindowManager& get() {
		return instance;
	}

	WindowManager() : captureCursor(false), cursorVisible(true), borderlessFullscreen(false), gameWindow(NULL) { }
	void setGameWindow(HWND window) { gameWindow = window; }
	HWND getGameWindow() const { return gameWindow ? gameWindow : ::GetActiveWindow(); }
	// Appelé depuis le thread de rendu, SetWindowPos attendrait que le thread de la fenêtre traite ses
	// messages : on le rend asynchrone pour ne jamais bloquer le rendu.
	static UINT asyncIfForeignThread(HWND hwnd) {
		return ::GetWindowThreadProcessId(hwnd, NULL) != ::GetCurrentThreadId() ? SWP_ASYNCWINDOWPOS : 0;
	}
	void applyCursorCapture();
	void toggleCursorCapture();
	void toggleCursorVisibility();
	void toggleBorderlessFullscreen();
	void maintainBorderlessFullscreen();
	void maintainWindowSize();
	void resize(unsigned clientW, unsigned clientH);
};
