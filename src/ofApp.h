#pragma once

#include "DataPoint.h"
#include "InputManager.h"
#include "OscManager.h"
#include "PathObject.h"
#include "SpatialGrid.h"
#include "ofMain.h"
#include "ofxGui.h"
#include "ofxOsc.h"

class ofApp : public ofBaseApp {

public:
	void setup();
	void update();
	void draw();
	void exit();

	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y);
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void mouseEntered(int x, int y);
	void mouseExited(int x, int y);
	void windowResized(int w, int h);
	void dragEvent(ofDragInfo dragInfo);
	void gotMessage(ofMessage msg);

	// Dual Window Support
	shared_ptr<ofAppBaseWindow> projectorWindow;
	void drawProjector(ofEventArgs & args);
	void drawVisuals(); // Core drawing logic shared by both windows

	// Core Data
	std::vector<DataPoint> points;
	std::shared_ptr<SpatialGrid> spatialGrid;

	// Path System
	std::vector<std::shared_ptr<PathObject>> paths;
	std::shared_ptr<PathObject> currentPath; // Path being drawn
	std::shared_ptr<PathObject> selectedPath;

	// Interaction State
	DrawMode currentMode;
	bool isDrawingPath;
	int pathIdCounter;
	bool bHoldingR = false;
	bool bHoldingV = false;

	// View
	float zoom;
	ofVec2f pan;
	bool isDragging;
	ofVec2f lastMouse;
	std::unordered_set<DataPoint> mouseActivePoints;

	// Browse Mode
	DataPoint hoveredPoint;
	DataPoint lastHoveredPoint;
	bool hasHoveredPoint = false;
	bool hasLastHoveredPoint = false;

	// Marquee Zoom
	bool isMarqueeZooming;
	ofVec2f marqueeStart;
	ofVec2f marqueeEnd;

	// Gesture Recording
	bool isRecordingGesture;

	// Path Dragging (Browse mode)
	bool isDraggingPath;
	ofVec2f lastDragWorld;

	// OSC
	OscManager oscManager;
	ofxOscReceiver oscReceiver; // If we need to receive params

	// Input
	InputManager inputManager;

	// Video — double-buffered; swap pointers on transition (no reload of back)
	ofVideoPlayer _vp1, _vp2;
	ofVideoPlayer * videoFront; // clip fading IN
	ofVideoPlayer * videoBack; // clip fading OUT (keeps running, never reloaded)
	ofFbo videoHoldFbo; // last-good-frame FBO — never goes black
	bool videoHoldAllocated;
	float videoAlpha; // alpha of front player (0-255)
	float videoAlphaTarget;
	float videoFadeSpeed;
	bool showVideo;
	bool showText;
	bool showGui;
	bool showDebug;
	bool showHelp;
	long lastVideoSwitchTime;
	string mediaRoot;
	string lastAttemptedVideoPath;

	// GUI & Settings
	ofxPanel gui;
	ofParameterGroup params;
	ofParameter<ofColor> backgroundColor;
	ofParameter<ofColor> pointColor;
	ofParameter<ofColor> selectedColor;
	ofParameter<ofColor> hoveredColor;
	ofParameter<ofColor> pathColor;
	ofParameter<ofColor> selectedPathColor;
	ofParameter<ofColor> activePointColor;
	ofParameter<ofColor> textColor;
	ofParameter<ofColor> activeTextColor;

	ofParameter<float> pointSize;
	ofParameter<float> selectedPointSize;
	ofParameter<float> hoveredPointSize;
	ofParameter<float> fontSize;
	ofParameter<float> activeFontSize;
	ofParameter<float> playheadSize;
	ofParameter<ofColor> playheadColor;
	ofParameter<int> videoFitMode; // 0=stretch 1=fit-height 2=fit-width
	ofParameter<float> videoFadeSpeed_param; // crossfade speed (alpha/frame)

	// Fonts
	ofTrueTypeFont font;
	ofTrueTypeFont activeFont;

	// Helpers
	ofVec2f screenToWorld(float x, float y);
	void loadPoints(string jsonPath);
	void sendOscMessage(string num, float val);
	void triggerVideo(const DataPoint & p);
	void sendFullUIUpdate(std::shared_ptr<PathObject> p);
	std::shared_ptr<PathObject> createWanderingPath(const DataPoint & start,
		int maxPoints, float randomness, int numNeighbors);

	// Composition Save/Load
	string lastLoadedPointsPath;
	void saveComposition(string filepath);
	void loadCompositionOrPoints(string filepath);
};
