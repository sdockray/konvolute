#pragma once
#include "DataPoint.h"
#include "SpatialGrid.h"
#include "ofMain.h"
#include <string>
#include <unordered_set>

enum DrawMode { NAVIGATE,
	DRAW_FREEHAND,
	DRAW_LINE,
	EDIT,
	WANDER,
	BROWSE };

class PathObject {
public:
	// Mode constants (matching Processing)
	static constexpr int LOOP_MODE = 2;
	static constexpr int CLOUD_MODE = 1;
	static constexpr int ONCE_MODE = 3;
	static constexpr int MIXED_MODE = 9;

	// Granular algorithm constants (matching Processing)
	static constexpr int GRANULAR_ALGO_ORIG = 1;
	static constexpr int GRANULAR_ALGO_STRETCH = 2;
	static constexpr int GRANULAR_ALGO_GHOST = 3;
	static constexpr int GRANULAR_ALGO_MIXED = 9;

	PathObject(int id);
	virtual ~PathObject() { }

	// Core Geometry
	ofPolyline polyline;
	std::vector<ofVec2f> controlPoints; // Stores original points for editing

	// Identity & State
	int id;
	std::string name;
	bool isActive;
	bool isSelected;

	// Playback State
	float position; // 0.0 - 1.0 along path
	float speed;
	int direction; // 1 = forward, 2 = oscillate (mirrors Processing constants)
	int mode; // LOOP_MODE / CLOUD_MODE / ONCE_MODE / MIXED_MODE
	bool isPingPong; // true = back-and-forth traversal

	// Audio Parameters
	float volume;
	float radius;
	float falloff;
	int sampleNum;

	// Sequential / Jitter / Wander
	bool isSequential;
	bool isWander;
	std::vector<DataPoint> sequentialPoints;
	int currentStepIndex;
	bool jitterMode; // Probabilistic step advance
	int lastJitterStepFloor; // Position floor from last frame (detects step boundary crossings)
	bool stepMode; // Timer-driven step index (not derived from polyline position)
	float stepTimer; // 0-1 accumulator for step mode advancement

	// Gesture Recording
	struct GesturePoint {
		float position;
		float volume;
		long timeMs; // elapsed time since gesture start
	}; // position 0-1, volume 0-1
	bool hasGesture;
	long gestureStartTime;
	float gesturePlaybackTime;
	std::vector<GesturePoint> gesturePoints;

	// Video Triggering
	bool sendToVideo = true;

	// Granular / Synth Params
	// Matching Processing PathDrawing
	float gRateMin, gRateMax;
	float gDurMin, gDurMax;
	int gGrainRateMin, gGrainRateMax;
	float gPosMin, gPosMax;
	float gRand;
	int granularMode; // GRANULAR_ALGO_ORIG/STRETCH/GHOST/MIXED
	int gEnv;

	float playbackRate;

	// Effects (matching Processing PathDrawing fxEnabled/fxReverb/fxDelay etc.)
	// fxEnabled: [0]=reverb, [1]=delay, [2]=distortion, [3]=compressor, [4]=filter
	bool fxEnabled[5];
	float fxReverb[3]; // [0]=mix, [1]=room, [2]=damp
	float fxDelay[3]; // [0]=time, [1]=feedback, [2]=mix
	float fxDistortion[2]; // [0]=drive, [1]=mix
	float fxCompressor[4]; // [0]=thresh, [1]=ratio, [2]=attack, [3]=release
	float fxFilter[3]; // [0]=freq, [1]=res, [2]=type (0=lpf,1=hpf,2=bpf)

	// Pulser
	float pulserMix;
	float pulserRateMin;
	float pulserRateMax;
	float pulserRateRand;
	float pulserAttack;
	float pulserRelease;

	// Logic
	void update(float dt);
	void addPoint(ofVec2f pt);
	void finalize(); // Generates polyline from controlPoints
	void togglePlayback();
	void clearGesture(); // Clear any recorded gesture
	void translate(ofVec2f delta); // Move entire path by delta
	float getGestureVolume(float pos) const; // Interpolated volume from gesture curve

	// Mode resolution — mirrors Processing's getMode() / getGranularMode()
	std::string getGranularMode() const;
	std::string getMode() const;

	// Interaction
	ofVec2f getCurrentPosition() const;
	float getDistanceToPoint(ofVec2f pt) const;
	void moveRelative(ofVec2f delta);

	// Spatial Query
	std::vector<DataPoint>
	getActivePoints(const std::vector<DataPoint> & allPoints, SpatialGrid & grid);

	// Render
	void draw(float playheadSize = 5.0f, ofColor playheadColor = ofColor(255), float zoom = 1.0f, float pathThickness = 1.0f, float selectedPathThickness = 2.0f, ofColor pathColor = ofColor(255, 0, 144), ofColor selectedPathColor = ofColor(255, 255, 0));

	// Internal helpers
	std::unordered_set<DataPoint> lastActivePoints; // For edge detection (entering/leaving radius)
	std::unordered_set<DataPoint> playingPoints; // For limiting concurrent samples

private:
};
