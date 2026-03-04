#include "DataPoint.h"
#include "PathObject.h"
#include "ofMain.h"
#include "ofxOsc.h"
#include <deque>

class OscManager {
public:
	void setup(std::string host, int scPort, int uiPort, int listenPort);

	// Message Retrieval
	bool hasWaitingMessages();
	bool getNextMessage(ofxOscMessage & m);

	// Sample Playback
	void sendSample(const std::string & filename, const std::string & pathName, float vol,
		std::string mode);
	void stopSample(const std::string & filename, const std::string & pathName);

	// Granular Update
	void updateGrain(const std::string & filename, const std::string & pathName, float rate,
		float pos, float amp, float dur, int grainRate,
		float posRand, int env);

	// Path Params
	void sendPathVolume(const std::string & pathName, float vol);
	void sendPathRefresh(const std::string & pathName);
	void sendPathRemove(const std::string & pathName); // Tell SC to free synth
	void sendPathRemoveEffect(const std::string & pathName, const std::string & effect);
	void sendClear();
	void sendReset();

	// Pulser
	void sendUpdatePulser(const std::string & pathName, float mix,
		float rateMin, float rateMax, float rateRand, float attack, float release);

	// UI Feedback (To Open Stage Control)
	void sendUIPathAdd(std::string name);
	void sendUIPathRemove(std::string name);
	void sendUIPathUpdate(int id, bool active, float radius, int direction, int samples,
		float vol, float falloff, float speed, int mode);
	void sendUIPathFullUpdate(int id, bool active, float radius, int direction, int samples,
		float vol, float falloff, float speed, int mode,
		// granular
		float gRateMin, float gRateMax,
		float gDurMin, float gDurMax,
		int gGrainRateMin, int gGrainRateMax,
		float gPosMin, float gPosMax,
		float gRand, int granularMode, int gEnv,
		// effects
		bool * fxEnabled, float * fxReverb, float * fxDelay,
		float * fxDistortion, float * fxCompressor, float * fxFilter,
		// pulser
		float pulserRateMin, float pulserRateMax, float pulserRateRand,
		float pulserAttack, float pulserRelease, float pulserMix);
	// Add more UI update methods as needed

	// Send arbitrary message
	void sendMessage(ofxOscMessage & m); // To SuperCollider
	void sendUIMessage(ofxOscMessage & m); // To UI

	// Debugging
	void drawDebug(float x, float y);

private:
	ofxOscSender sender; // To SuperCollider (57120)
	ofxOscSender uiSender; // To UI (57122)
	ofxOscReceiver receiver; // From UI (57121)
	bool isReady = false;

	std::deque<std::string> messageHistory;
	const int maxHistory = 20;

	void addToHistory(const ofxOscMessage & m);
};
