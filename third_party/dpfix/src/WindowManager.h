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
	void applyCursorCapture();
	void toggleCursorCapture();
	void toggleCursorVisibility();
	void toggleBorderlessFullscreen();
	void maintainBorderlessFullscreen();
	void maintainWindowSize();
	void resize(unsigned clientW, unsigned clientH);
};
