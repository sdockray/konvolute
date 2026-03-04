#include "InputManager.h"

void InputManager::setup() {
	// defaults
}

AppCommand InputManager::getCommandForKey(int key) {
	// Update modifiers internally just in case (though updateModifiers calls are better)
	// Actually we handle specific keys here that map to commands

	switch (key) {
	case 'n':
	case 'N':
		return CMD_MODE_NAVIGATE;
	case 'f':
	case 'F':
		return CMD_MODE_DRAW;
	case ' ':
		return CMD_TOGGLE_PLAYBACK;
	case 'm':
		return CMD_TOGGLE_VIDEO;
	case 'o':
		return CMD_LOAD_POINTS;
	case 'x':
		return CMD_CLEAR_ALL;
	case 'c':
		return CMD_DESELECT_PATH;
	case 'z':
		return CMD_RESET_ZOOM;
	case 'r':
		return CMD_INCREASE_RADIUS; // 'r'
	case 'R':
		return CMD_DECREASE_RADIUS; // Shift+r
	case 'v':
		return CMD_INCREASE_VOLUME;
	case 'V':
		return CMD_DECREASE_VOLUME;
	case 'd':
		return CMD_TOGGLE_DEBUG;
	case 's':
	case 'S':
		return CMD_SAVE_COMPOSITION;
	case 'b':
	case 'B':
		return CMD_TOGGLE_BROWSE;
	case 'k':
	case 'K':
		return CMD_REFRESH_PATH;
	case 't':
	case 'T':
		return CMD_TOGGLE_TEXT;
	case ',':
		return CMD_TOGGLE_SETTINGS;
	case OF_KEY_DEL:
	case OF_KEY_BACKSPACE:
		return CMD_DELETE_PATH;
	case 'q':
	case 'Q':
		return CMD_CREATE_SEQUENTIAL_PATH;
	case 'w':
	case 'W':
		return CMD_MODE_WANDER;
	case 'g':
	case 'G':
		return CMD_CLEAR_GESTURE;
	case 'e':
	case 'E':
		return CMD_TOGGLE_STEP_MODE;
	case 'h':
	case 'H':
		return CMD_TOGGLE_HELP;
	case 'p':
	case 'P':
		return CMD_TOGGLE_FULLSCREEN_PROJECTOR;
	case 'i':
	case 'I':
		return CMD_TOGGLE_TITLE;
	default:
		return CMD_NONE;
	}
}

void InputManager::updateModifiers(int key, bool isPressed) {
	if (key == OF_KEY_LEFT_SHIFT || key == OF_KEY_RIGHT_SHIFT) {
		isShiftDown = isPressed;
	}
	if (key == OF_KEY_LEFT_ALT || key == OF_KEY_RIGHT_ALT) {
		isAltDown = isPressed;
	}
	if (key == OF_KEY_LEFT_COMMAND || key == OF_KEY_RIGHT_COMMAND || key == OF_KEY_LEFT_CONTROL || key == OF_KEY_RIGHT_CONTROL) {
		isCmdDown = isPressed;
	}
}
