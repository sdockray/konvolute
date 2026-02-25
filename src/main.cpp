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

	// 2. Setup Projector Window (Secondary Display)
	settings.setSize(1024, 768);
	settings.setPosition(ofVec2f(1500, 100)); // Default offset
	settings.resizable = true;
	settings.shareContextWith = mainWindow; // Key for shared GPU resources!
	shared_ptr<ofAppBaseWindow> projectorWindow = ofCreateWindow(settings);

	// 3. Initialize App and bind windows
	shared_ptr<ofApp> mainApp(new ofApp());
	mainApp->projectorWindow = projectorWindow; // Give app a ref to the second window

	ofRunApp(mainWindow, mainApp);
	ofRunMainLoop();
}
