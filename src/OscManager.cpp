#include "OscManager.h"

void OscManager::setup(std::string host, int scPort, int uiPort, int listenPort) {
	sender.setup(host, scPort);
	uiSender.setup(host, uiPort);
	receiver.setup(listenPort);
	isReady = true;
}

bool OscManager::hasWaitingMessages() { return receiver.hasWaitingMessages(); }

bool OscManager::getNextMessage(ofxOscMessage & m) {
	if (receiver.getNextMessage(m)) {
		addToHistory(m);
		return true;
	}
	return false;
}

void OscManager::addToHistory(const ofxOscMessage & m) {
	std::string msg = m.getAddress();
	for (int i = 0; i < m.getNumArgs(); i++) {
		msg += " ";
		if (m.getArgType(i) == OFXOSC_TYPE_INT32 || m.getArgType(i) == OFXOSC_TYPE_INT64) {
			msg += std::to_string(m.getArgAsInt32(i));
		} else if (m.getArgType(i) == OFXOSC_TYPE_FLOAT) {
			msg += std::to_string(m.getArgAsFloat(i)).substr(0, 5); // truncate to 5 chars
		} else if (m.getArgType(i) == OFXOSC_TYPE_STRING) {
			msg += m.getArgAsString(i);
		} else {
			msg += "?";
		}
	}

	messageHistory.push_front(msg);
	if (messageHistory.size() > maxHistory) {
		messageHistory.pop_back();
	}
}

void OscManager::drawDebug(float x, float y) {
	ofDrawBitmapString("OSC DEBUG (57121 In, 57120 Out, 57122 UI)", x, y);
	y += 20;
	for (auto & s : messageHistory) {
		ofDrawBitmapString(s, x, y);
		y += 15;
	}
}

void OscManager::sendSample(const std::string & filename, const std::string & pathName,
	float vol, std::string mode) {
	ofxOscMessage m;
	m.setAddress("/play");
	m.addStringArg(filename);
	m.addStringArg(pathName);
	m.addFloatArg(1.0f); // rate
	m.addFloatArg(0.0f); // startPos
	m.addFloatArg(vol);
	m.addFloatArg(0.0f); // pan
	m.addFloatArg(0.01f); // attack
	m.addFloatArg(0.1f); // release
	m.addIntArg(1); // loop
	m.addStringArg(mode);

	// Random crossfade from processing code
	float cf = ofRandom(0.01f, 0.25f);
	m.addFloatArg(cf);

	sendMessage(m);
}

void OscManager::stopSample(const std::string & filename, const std::string & pathName) {
	ofxOscMessage m;
	m.setAddress("/stop");
	m.addStringArg(filename);
	m.addStringArg(pathName);
	m.addFloatArg(1.5f); // fadeout
	sendMessage(m);
}

void OscManager::updateGrain(const std::string & filename, const std::string & pathName,
	float rate, float pos, float amp, float dur,
	int grainRate, float posRand, int env) {
	ofxOscMessage m;
	m.setAddress("/grainupdate");
	m.addStringArg(filename);
	m.addStringArg(pathName);
	m.addFloatArg(rate);
	m.addFloatArg(pos);
	m.addFloatArg(amp);
	m.addFloatArg(0.0f); // pan
	m.addFloatArg(dur);
	m.addFloatArg((float)grainRate);
	m.addFloatArg(posRand);
	m.addIntArg(env);

	sendMessage(m);
}

void OscManager::sendPathVolume(const std::string & pathName, float vol) {
	ofxOscMessage m;
	m.setAddress("/pathvolume");
	m.addStringArg(pathName);
	m.addFloatArg(vol);
	sender.sendMessage(m);
}

void OscManager::sendPathRefresh(const std::string & pathName) {
	ofxOscMessage m;
	m.setAddress("/refreshpath");
	m.addStringArg(pathName);
	sender.sendMessage(m);
}

void OscManager::sendPathRemove(const std::string & pathName) {
	ofxOscMessage m;
	m.setAddress("/removepath");
	m.addStringArg(pathName);
	sender.sendMessage(m);
}

void OscManager::sendClear() {
	ofxOscMessage m;
	m.setAddress("/clear");
	sender.sendMessage(m);
}

void OscManager::sendReset() {
	ofxOscMessage m;
	m.setAddress("/reset");
	sender.sendMessage(m);
}

void OscManager::sendPathRemoveEffect(const std::string & pathName, const std::string & effect) {
	ofxOscMessage m;
	m.setAddress("/removeeffect");
	m.addStringArg(pathName);
	m.addStringArg(effect);
	sender.sendMessage(m);
}

void OscManager::sendUpdatePulser(const std::string & pathName, float mix,
	float rateMin, float rateMax, float rateRand, float attack, float release) {
	ofxOscMessage m;
	m.setAddress("/pulserupdate");
	m.addStringArg(pathName);
	m.addFloatArg(mix);
	m.addFloatArg(rateMin);
	m.addFloatArg(rateMax);
	m.addFloatArg(rateRand);
	m.addFloatArg(attack);
	m.addFloatArg(release);
	sender.sendMessage(m);
}

void OscManager::sendUIPathAdd(std::string name) {
	if (!isReady) return;

	ofxOscMessage m;
	m.setAddress("/o_pathadd");
	m.addStringArg(name);
	sendUIMessage(m);
}

void OscManager::sendUIPathRemove(std::string name) {
	if (!isReady) return;

	ofxOscMessage m;
	m.setAddress("/o_pathremove");
	m.addStringArg(name);
	sendUIMessage(m);
}

void OscManager::sendUIPathUpdate(int id, bool active, float radius, int direction, int samples,
	float vol, float falloff, float speed, int mode) {
	ofxOscMessage m;

	// active
	m.setAddress("/o_playPause");
	m.addIntArg(active ? 1 : 0);
	sendUIMessage(m);
	m.clear();

	// radius
	m.setAddress("/o_radius");
	m.addFloatArg(radius);
	sendUIMessage(m);
	m.clear();

	// direction
	m.setAddress("/o_direction");
	m.addIntArg(direction);
	sendUIMessage(m);
	m.clear();

	// samples
	m.setAddress("/o_samples");
	m.addIntArg(samples);
	sendUIMessage(m);
	m.clear();

	// amp
	m.setAddress("/o_amp");
	m.addFloatArg(vol);
	sendUIMessage(m);
	m.clear();

	// falloff
	m.setAddress("/o_falloff");
	m.addFloatArg(falloff);
	sendUIMessage(m);
	m.clear();

	// speed
	m.setAddress("/o_speed");
	m.addFloatArg(speed);
	sendUIMessage(m);
	m.clear();

	// mode (synth) — int constant matching Processing: CLOUD=1, LOOP=2, ONCE=3, MIXED=9
	m.setAddress("/o_synth");
	m.addIntArg(mode);
	sendUIMessage(m);
	m.clear();

	// Path ID
	m.setAddress("/o_path");
	m.addIntArg(id);
	sendUIMessage(m);
}

void OscManager::sendUIPathFullUpdate(int id, bool active, float radius, int direction, int samples,
	float vol, float falloff, float speed, int mode,
	float gRateMin, float gRateMax,
	float gDurMin, float gDurMax,
	int gGrainRateMin, int gGrainRateMax,
	float gPosMin, float gPosMax,
	float gRand, int granularMode, int gEnv,
	bool * fxEnabled, float * fxReverb, float * fxDelay,
	float * fxDistortion, float * fxCompressor, float * fxFilter,
	float pulserRateMin, float pulserRateMax, float pulserRateRand,
	float pulserAttack, float pulserRelease, float pulserMix) {

	// First send the basic update
	sendUIPathUpdate(id, active, radius, direction, samples, vol, falloff, speed, mode);

	ofxOscMessage m;

	// Granular
	m.setAddress("/o_gRate");
	m.addFloatArg(gRateMin);
	m.addFloatArg(gRateMax);
	sendUIMessage(m);
	m.clear();

	m.setAddress("/o_gDur");
	m.addFloatArg(gDurMin);
	m.addFloatArg(gDurMax);
	sendUIMessage(m);
	m.clear();

	m.setAddress("/o_gGrainRate");
	m.addIntArg(gGrainRateMin);
	m.addIntArg(gGrainRateMax);
	sendUIMessage(m);
	m.clear();

	m.setAddress("/o_gPos");
	m.addFloatArg(gPosMin);
	m.addFloatArg(gPosMax);
	sendUIMessage(m);
	m.clear();

	m.setAddress("/o_gRand");
	m.addFloatArg(gRand);
	sendUIMessage(m);
	m.clear();

	m.setAddress("/o_grainAlgo");
	m.addIntArg(granularMode);
	sendUIMessage(m);
	m.clear();

	m.setAddress("/o_grainEnv");
	m.addIntArg(gEnv);
	sendUIMessage(m);
	m.clear();

	// Reverb
	m.setAddress("/o_effReverb");
	m.addIntArg(fxEnabled[0] ? 1 : 0);
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_effReverbMix");
	m.addFloatArg(fxReverb[0]);
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_effReverbRoom");
	m.addFloatArg(fxReverb[1]);
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_effReverbDamp");
	m.addFloatArg(fxReverb[2]);
	sendUIMessage(m);
	m.clear();

	// Delay
	m.setAddress("/o_effDelay");
	m.addIntArg(fxEnabled[1] ? 1 : 0);
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_effDelayTime");
	m.addFloatArg(0.5f * fxDelay[0]); // UI expects half the stored value
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_effDelayFeedback");
	m.addFloatArg(fxDelay[1]);
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_effDelayMix");
	m.addFloatArg(fxDelay[2]);
	sendUIMessage(m);
	m.clear();

	// Distortion
	m.setAddress("/o_effDistortion");
	m.addIntArg(fxEnabled[2] ? 1 : 0);
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_effDistortionDrive");
	m.addFloatArg(fxDistortion[0]);
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_effDistortionMix");
	m.addFloatArg(fxDistortion[1]);
	sendUIMessage(m);
	m.clear();

	// Compressor
	m.setAddress("/o_effCompressor");
	m.addIntArg(fxEnabled[3] ? 1 : 0);
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_effCompressorThresh");
	m.addFloatArg(0.5f * fxCompressor[0]); // UI expects half the stored value
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_effCompressorRatio");
	m.addFloatArg(fxCompressor[1]);
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_effCompressorAttack");
	m.addFloatArg(fxCompressor[2]);
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_effCompressorRelease");
	m.addFloatArg(fxCompressor[3]);
	sendUIMessage(m);
	m.clear();

	// Filter
	m.setAddress("/o_effFilter");
	m.addIntArg(fxEnabled[4] ? 1 : 0);
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_effFilterFreq");
	m.addFloatArg(fxFilter[0]);
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_effFilterRes");
	m.addFloatArg(fxFilter[1]);
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_effFilterType");
	m.addIntArg((int)fxFilter[2]);
	sendUIMessage(m);
	m.clear();

	// Pulser
	m.setAddress("/o_pulserRate");
	m.addFloatArg(pulserRateMin);
	m.addFloatArg(pulserRateMax);
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_pulserRateRand");
	m.addFloatArg(pulserRateRand);
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_pulserAttack");
	m.addFloatArg(pulserAttack);
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_pulserRelease");
	m.addFloatArg(pulserRelease);
	sendUIMessage(m);
	m.clear();
	m.setAddress("/o_pulserMix");
	m.addFloatArg(pulserMix);
	sendUIMessage(m);
	m.clear();
}

void OscManager::sendMessage(ofxOscMessage & m) {
	sender.sendMessage(m);
	addToHistory(m);
}

void OscManager::sendUIMessage(ofxOscMessage & m) {
	uiSender.sendMessage(m);
	// Optional: add to history or logs?
	// addToHistory(m); // Maybe distinct history for UI?
}
