#include "ofApp.h"
#include "ofMain.h"

//========================================================================
int main() {
	ofGLFWWindowSettings settings;

	// 1. Setup Control Window (Laptop)
	settings.setSize(1400, 800);
	settings.setPosition(ofVec2f(100, 100));
	settings.resizable = true;
	shared_ptr<ofAppBaseWindow> mainWindow = ofCreateWindow(settings);

	// 2. Initialize App and bind main window
	shared_ptr<ofApp> mainApp(new ofApp());
	mainApp->mainWindow = mainWindow; // Give app a ref to the main window for context sharing

	ofRunApp(mainWindow, mainApp);
	ofRunMainLoop();
}
