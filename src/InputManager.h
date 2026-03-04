#pragma once
#include "ofMain.h"

enum AppCommand {
	CMD_NONE,
	CMD_MODE_DRAW,
	CMD_MODE_NAVIGATE,
	CMD_TOGGLE_VIDEO,
	CMD_TOGGLE_PLAYBACK,
	CMD_LOAD_POINTS,
	CMD_CLEAR_SELECTION,
	CMD_CLEAR_ALL,
	CMD_RESET_ZOOM,
	CMD_INCREASE_RADIUS,
	CMD_DECREASE_RADIUS,
	CMD_INCREASE_VOLUME,
	CMD_DECREASE_VOLUME,
	CMD_TOGGLE_DEBUG,
	CMD_SAVE_PATH,
	CMD_SAVE_COMPOSITION,
	CMD_TOGGLE_BROWSE,
	CMD_REFRESH_PATH,
	CMD_TOGGLE_TEXT,
	CMD_TOGGLE_SETTINGS,
	CMD_DESELECT_PATH, // 'c'
	CMD_DELETE_PATH, // Delete/Backspace
	CMD_CREATE_SEQUENTIAL_PATH, // 'q'
	CMD_MODE_WANDER, // 'w'
	CMD_CLEAR_GESTURE, // 'g'
	CMD_TOGGLE_STEP_MODE, // 'e'
	CMD_TOGGLE_HELP, // 'h'
	CMD_TOGGLE_FULLSCREEN_PROJECTOR, // 'p'
	CMD_TOGGLE_TITLE // 'i'
};

class InputManager {
public:
	void setup();

	// Convert key to command
	AppCommand getCommandForKey(int key);

	// Modifier state
	bool isShiftDown = false;
	bool isAltDown = false;
	bool isCmdDown = false; // Command or Ctrl

	void updateModifiers(int key, bool isPressed);
};
