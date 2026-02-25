#include "ofApp.h"
#include "ofxJSON.h"

// Natural Sort Helper
int naturalCompare(const std::string & s1, const std::string & s2) {
	size_t i = 0, j = 0;
	while (i < s1.length() && j < s2.length()) {
		char c1 = s1[i];
		char c2 = s2[j];

		if (isdigit(c1) && isdigit(c2)) {
			// Extract numbers
			std::string n1, n2;
			while (i < s1.length() && isdigit(s1[i]))
				n1 += s1[i++];
			while (j < s2.length() && isdigit(s2[j]))
				n2 += s2[j++];

			// Compare length then value
			if (n1.length() != n2.length()) return (int)(n1.length() - n2.length());
			if (n1 != n2) return n1 < n2 ? -1 : 1;
		} else {
			if (c1 != c2) return c1 - c2;
			i++;
			j++;
		}
	}
	return (int)(s1.length() - s2.length());
}

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetBackgroundColor(20, 25, 40);
	ofSetFrameRate(60);

	// Initialize OSC
	// SC: 57120, UI: 57122, Listen: 57121
	oscManager.setup("127.0.0.1", 57120, 57122, 57121);

	// Bind Projector Window if available
	if (projectorWindow) {
		ofAddListener(projectorWindow->events().draw, this, &ofApp::drawProjector);
	}

	// Send clear
	oscManager.sendClear();
	oscManager.sendUIPathRemove("all");

	// Initialize state
	points.clear();
	paths.clear();

	// Initialize default path 0 for settings
	auto p0 = std::make_shared<PathObject>(0);
	p0->name = "path-0";
	p0->mode = PathObject::LOOP_MODE;
	p0->isActive = false; // Not playing by itself
	// User requested path-0 notification
	oscManager.sendUIPathAdd(p0->name);
	paths.push_back(p0);

	pathIdCounter = 1;
	currentMode = NAVIGATE;
	isDrawingPath = false;
	zoom = 1.0f;
	pan.set(0, 0);
	isDragging = false;
	isMarqueeZooming = false;
	isRecordingGesture = false;
	isDraggingPath = false;
	showVideo = false;
	showText = false;
	showGui = false;
	showHelp = false;
	lastVideoSwitchTime = 0;
	videoFront = &_vp1;
	videoBack = &_vp2;
	videoHoldAllocated = false;
	videoAlpha = 255.0f;
	videoAlphaTarget = 255.0f;
	videoFadeSpeed = 15.0f;

	// GUI Setup
	params.setName("Visual Settings");
	params.add(backgroundColor.set("Background", ofColor(20, 25, 40), ofColor(0, 0), ofColor(255, 255)));
	params.add(pointColor.set("Point Color", ofColor(100, 150, 255), ofColor(0, 0), ofColor(255, 255)));
	params.add(selectedColor.set("Selected Color", ofColor(255, 100, 100), ofColor(0, 0), ofColor(255, 255)));
	params.add(hoveredColor.set("Hovered Color", ofColor(255, 255, 100), ofColor(0, 0), ofColor(255, 255)));
	params.add(pathColor.set("Path Color", ofColor(255, 0, 144), ofColor(0, 0), ofColor(255, 255)));
	params.add(selectedPathColor.set("Active Path Color", ofColor(255, 255, 0), ofColor(0, 0), ofColor(255, 255)));
	params.add(activePointColor.set("Active Point Color", ofColor(255, 255, 255), ofColor(0, 0), ofColor(255, 255)));
	params.add(textColor.set("Text Color", ofColor(255), ofColor(0, 0), ofColor(255, 255)));
	params.add(activeTextColor.set("Active Text Color", ofColor(0, 255, 0), ofColor(0, 0), ofColor(255, 255)));

	params.add(pointSize.set("Point Size", 5.0f, 1.0f, 100.0f)); // Increased default to 5.0
	params.add(selectedPointSize.set("Sel Point Size", 8.0f, 1.0f, 30.0f));
	params.add(hoveredPointSize.set("Hover Point Size", 16.0f, 1.0f, 40.0f));
	params.add(fontSize.set("Font Size", 14.0f, 8.0f, 48.0f));
	params.add(activeFontSize.set("Active Font Size", 14.0f, 8.0f, 48.0f));
	params.add(playheadSize.set("Playhead Size", 5.0f, 1.0f, 200.0f));
	params.add(playheadColor.set("Playhead Color", ofColor(255), ofColor(0, 0), ofColor(255, 255)));
	params.add(videoFitMode.set("Video Fit 0=stretch 1=height 2=width", 0, 0, 2));
	params.add(videoFadeSpeed_param.set("Video Fade Speed", 15.0f, 1.0f, 255.0f));

	gui.setup(params);
	gui.loadFromFile("settings.xml");

	// Load Font - try to load a system font or default
	bool fontLoaded = font.load("verdana.ttf", fontSize);
	if (!fontLoaded) fontLoaded = font.load(OF_TTF_SANS, fontSize);

	bool activeFontLoaded = activeFont.load("verdana.ttf", activeFontSize);
	if (!activeFontLoaded) activeFontLoaded = activeFont.load(OF_TTF_SANS, activeFontSize);

	// Load default data
	// In a real app we'd use a file dialog or settings file
	// For now, let's look for a json file in bin/data
	/*
	ofDirectory dir("");
	dir.allowExt("json");
	dir.listDir();
	if (dir.size() > 0) {
		loadPoints(dir.getPath(0));
	}
	*/
}

//--------------------------------------------------------------
void ofApp::update() {
	// Poll OSC Messages
	ofxOscMessage m;
	while (oscManager.getNextMessage(m)) {
		std::string addr = m.getAddress();

		// Find "selected" path or default to path-0
		// Processing logic: "pathToUse = browsePath; if (selectedPath != null) pathToUse = selectedPath;"
		// For us, let's use the last selected path or path-0
		auto pathToUse = paths[0];
		for (auto & p : paths) {
			if (p->isSelected) {
				pathToUse = p;
				break;
			}
		}

		if (addr == "/o_playPause") {
			// Processing: active = msg.get(0).intValue();
			// But Processing logic calls togglePlayback(). logic in oscEvent:
			// if (msg.addrPattern().indexOf("/o_playPause")==0) { pathToUse.togglePlayback(); }
			// Wait, the message sends 1 or 0?
			// Processing sendOSCPlayPauseUpdate sends active ? 1 : 0.
			// oscEvent logic ignores the argument and just calls togglePlayback().
			// Let's match logic: toggle.
			pathToUse->isActive = !pathToUse->isActive;
			// Send feedback? Processing does NOT send feedback inside oscEvent to avoid loops,
			oscManager.sendUIPathUpdate(pathToUse->id, pathToUse->isActive, pathToUse->radius, pathToUse->direction, pathToUse->sampleNum, pathToUse->volume, pathToUse->falloff, pathToUse->speed, pathToUse->mode);

		} else if (addr == "/o_path") {
			// Select path by name or ID?
			// Processing: String pathName = msg.get(0).stringValue();
			if (m.getArgType(0) == OFXOSC_TYPE_STRING) {
				std::string pathName = m.getArgAsString(0);
				// Deselect all
				for (auto & p : paths)
					p->isSelected = false;
				// Select matching
				for (auto & p : paths) {
					if (p->name == pathName) {
						p->isSelected = true;
						// Send full feedback with all params including effects/pulser
						sendFullUIUpdate(p);
						break;
					}
				}
			} else if (m.getArgType(0) == OFXOSC_TYPE_INT32) {
				int id = m.getArgAsInt32(0);
				for (auto & p : paths)
					p->isSelected = false;
				for (auto & p : paths) {
					if (p->id == id) {
						p->isSelected = true;
						sendFullUIUpdate(p);
						break;
					}
				}
			}

		} else if (addr == "/o_synth") {
			// Mode int constants from Processing: CLOUD=1, LOOP=2, ONCE=3, MIXED=9
			pathToUse->mode = m.getArgAsInt32(0);
		} else if (addr == "/o_direction") {
			pathToUse->direction = m.getArgAsInt32(0);
		} else if (addr == "/o_radius") {
			pathToUse->radius = m.getArgAsFloat(0);
		} else if (addr == "/o_speed") {
			pathToUse->speed = m.getArgAsFloat(0);
		} else if (addr == "/o_samples") {
			pathToUse->sampleNum = m.getArgAsInt32(0);
		} else if (addr == "/o_amp") {
			pathToUse->volume = m.getArgAsFloat(0);
			oscManager.sendPathVolume(pathToUse->name, pathToUse->volume);
		} else if (addr == "/o_falloff") {
			pathToUse->falloff = m.getArgAsFloat(0);
		}
		// Granular params
		else if (addr == "/o_gRate") {
			pathToUse->gRateMin = m.getArgAsFloat(0);
			pathToUse->gRateMax = m.getArgAsFloat(1);
		} else if (addr == "/o_gDur") {
			pathToUse->gDurMin = m.getArgAsFloat(0);
			pathToUse->gDurMax = m.getArgAsFloat(1);
		} else if (addr == "/o_gGrainRate") {
			pathToUse->gGrainRateMin = m.getArgAsInt32(0);
			pathToUse->gGrainRateMax = m.getArgAsInt32(1);
		} else if (addr == "/o_gPos") {
			pathToUse->gPosMin = m.getArgAsFloat(0);
			pathToUse->gPosMax = m.getArgAsFloat(1);
		} else if (addr == "/o_gRand") {
			pathToUse->gRand = m.getArgAsFloat(0);
		} else if (addr == "/o_grainAlgo") {
			pathToUse->granularMode = m.getArgAsInt32(0);
		} else if (addr == "/o_grainEnv") {
			pathToUse->gEnv = m.getArgAsInt32(0);
		}

		// ---- Effects (prefix /o_eff) ----
		else if (addr.rfind("/o_eff", 0) == 0) {
			ofxOscMessage fx;

			// --- Reverb ---
			if (addr.rfind("/o_effReverb", 0) == 0) {
				if (addr == "/o_effReverb" && m.getArgAsInt32(0) == 0) {
					pathToUse->fxEnabled[0] = false;
					oscManager.sendPathRemoveEffect(pathToUse->name, "reverb");
				} else {
					pathToUse->fxEnabled[0] = true;
					if (addr == "/o_effReverbMix")
						pathToUse->fxReverb[0] = m.getArgAsFloat(0);
					else if (addr == "/o_effReverbRoom")
						pathToUse->fxReverb[1] = m.getArgAsFloat(0);
					else if (addr == "/o_effReverbDamp")
						pathToUse->fxReverb[2] = m.getArgAsFloat(0);
					fx.setAddress("/pathreverb");
					fx.addStringArg(pathToUse->name);
					fx.addFloatArg(pathToUse->fxReverb[0]);
					fx.addFloatArg(pathToUse->fxReverb[1]);
					fx.addFloatArg(pathToUse->fxReverb[2]);
					oscManager.sendMessage(fx);
				}
			}

			// --- Delay ---
			else if (addr.rfind("/o_effDelay", 0) == 0) {
				if (addr == "/o_effDelay" && m.getArgAsInt32(0) == 0) {
					pathToUse->fxEnabled[1] = false;
					oscManager.sendPathRemoveEffect(pathToUse->name, "delay");
				} else {
					pathToUse->fxEnabled[1] = true;
					// Processing stores 2x the UI value for delay time
					if (addr == "/o_effDelayTime")
						pathToUse->fxDelay[0] = 2.0f * m.getArgAsFloat(0);
					else if (addr == "/o_effDelayFeedback")
						pathToUse->fxDelay[1] = m.getArgAsFloat(0);
					else if (addr == "/o_effDelayMix")
						pathToUse->fxDelay[2] = m.getArgAsFloat(0);
					fx.setAddress("/pathdelay");
					fx.addStringArg(pathToUse->name);
					fx.addFloatArg(pathToUse->fxDelay[0]);
					fx.addFloatArg(pathToUse->fxDelay[1]);
					fx.addFloatArg(pathToUse->fxDelay[2]);
					oscManager.sendMessage(fx);
				}
			}

			// --- Distortion ---
			else if (addr.rfind("/o_effDistortion", 0) == 0) {
				if (addr == "/o_effDistortion" && m.getArgAsInt32(0) == 0) {
					pathToUse->fxEnabled[2] = false;
					oscManager.sendPathRemoveEffect(pathToUse->name, "distortion");
				} else {
					pathToUse->fxEnabled[2] = true;
					if (addr == "/o_effDistortionDrive")
						pathToUse->fxDistortion[0] = m.getArgAsFloat(0);
					else if (addr == "/o_effDistortionMix")
						pathToUse->fxDistortion[1] = m.getArgAsFloat(0);
					fx.setAddress("/pathdistortion");
					fx.addStringArg(pathToUse->name);
					fx.addFloatArg(pathToUse->fxDistortion[0]);
					fx.addFloatArg(pathToUse->fxDistortion[1]);
					oscManager.sendMessage(fx);
				}
			}

			// --- Filter ---
			else if (addr.rfind("/o_effFilter", 0) == 0) {
				if (addr == "/o_effFilter" && m.getArgAsInt32(0) == 0) {
					pathToUse->fxEnabled[4] = false;
					oscManager.sendPathRemoveEffect(pathToUse->name, "filter");
				} else {
					pathToUse->fxEnabled[4] = true;
					if (addr == "/o_effFilterFreq")
						pathToUse->fxFilter[0] = m.getArgAsFloat(0);
					else if (addr == "/o_effFilterRes")
						pathToUse->fxFilter[1] = m.getArgAsFloat(0);
					else if (addr == "/o_effFilterType")
						pathToUse->fxFilter[2] = (float)m.getArgAsInt32(0);
					static const std::string filterTypes[] = { "lpf", "hpf", "bpf" };
					int typeIdx = (int)pathToUse->fxFilter[2];
					if (typeIdx < 0) typeIdx = 0;
					if (typeIdx > 2) typeIdx = 2;
					fx.setAddress("/pathfilter");
					fx.addStringArg(pathToUse->name);
					fx.addFloatArg(pathToUse->fxFilter[0]);
					fx.addFloatArg(pathToUse->fxFilter[1]);
					fx.addStringArg(filterTypes[typeIdx]);
					oscManager.sendMessage(fx);
				}
			}

			// --- Compressor ---
			else if (addr.rfind("/o_effCompressor", 0) == 0) {
				if (addr == "/o_effCompressor" && m.getArgAsInt32(0) == 0) {
					pathToUse->fxEnabled[3] = false;
					oscManager.sendPathRemoveEffect(pathToUse->name, "compressor");
				} else {
					pathToUse->fxEnabled[3] = true;
					// Processing stores 2x the UI value for threshold
					if (addr == "/o_effCompressorThresh")
						pathToUse->fxCompressor[0] = 2.0f * m.getArgAsFloat(0);
					else if (addr == "/o_effCompressorRatio")
						pathToUse->fxCompressor[1] = m.getArgAsFloat(0);
					else if (addr == "/o_effCompressorAttack")
						pathToUse->fxCompressor[2] = m.getArgAsFloat(0);
					else if (addr == "/o_effCompressorRelease")
						pathToUse->fxCompressor[3] = m.getArgAsFloat(0);
					fx.setAddress("/pathcompressor");
					fx.addStringArg(pathToUse->name);
					fx.addFloatArg(pathToUse->fxCompressor[0]);
					fx.addFloatArg(pathToUse->fxCompressor[1]);
					fx.addFloatArg(pathToUse->fxCompressor[2]);
					fx.addFloatArg(pathToUse->fxCompressor[3]);
					oscManager.sendMessage(fx);
				}
			}
		}

		// ---- Pulser (prefix /o_pulser) ----
		else if (addr.rfind("/o_pulser", 0) == 0) {
			if (addr == "/o_pulserMix")
				pathToUse->pulserMix = m.getArgAsFloat(0);
			else if (addr == "/o_pulserRate") {
				pathToUse->pulserRateMin = m.getArgAsFloat(0);
				pathToUse->pulserRateMax = m.getArgAsFloat(1);
			} else if (addr == "/o_pulserRateRand")
				pathToUse->pulserRateRand = m.getArgAsFloat(0);
			else if (addr == "/o_pulserAttack")
				pathToUse->pulserAttack = m.getArgAsFloat(0);
			else if (addr == "/o_pulserRelease")
				pathToUse->pulserRelease = m.getArgAsFloat(0);

			// Always forward updated pulser state to SuperCollider
			oscManager.sendUpdatePulser(
				pathToUse->name,
				pathToUse->pulserMix,
				pathToUse->pulserRateMin,
				pathToUse->pulserRateMax,
				pathToUse->pulserRateRand,
				pathToUse->pulserAttack,
				pathToUse->pulserRelease);
		}

		// ---- Jitter toggle ----
		else if (addr == "/o_jitter") {
			pathToUse->jitterMode = (m.getArgAsInt32(0) != 0);
			if (!pathToUse->jitterMode) {
				// Sync lastJitterStepFloor so normal mode resumes cleanly
				pathToUse->lastJitterStepFloor = -1;
			}
		}
	}

	// Navigate Mode Scrubbing
	if (currentMode == NAVIGATE && spatialGrid) {
	}

	// Browse Mode Logic
	if (currentMode == BROWSE && spatialGrid) {
		if (ofGetMousePressed()) {
			ofVec2f worldPos = screenToWorld(ofGetMouseX(), ofGetMouseY());
			std::vector<DataPoint> nearest = spatialGrid->findNearestNeighbors(worldPos.x, worldPos.y, 1);

			if (!nearest.empty()) {
				DataPoint nearestPoint = nearest[0];
				float distance = worldPos.distance(ofVec2f(nearestPoint.x, nearestPoint.y));
				float threshold = 20.0f / zoom;

				if (distance > threshold) {
					// Too far
					if (hasLastHoveredPoint) {
						oscManager.stopSample(mediaRoot + lastHoveredPoint.filename, "path-0");
						hasLastHoveredPoint = false;
						hasHoveredPoint = false;
					}
				} else {
					// Close enough
					hoveredPoint = nearestPoint;
					hasHoveredPoint = true;

					// If different from last hovered
					if (!hasLastHoveredPoint || !(hoveredPoint == lastHoveredPoint)) {
						if (hasLastHoveredPoint) {
							oscManager.stopSample(mediaRoot + lastHoveredPoint.filename, "path-0");
						}

						// Trigger new
						float vol = 0.5f;
						string mode = "loop";
						if (paths.size() > 0) {
							vol = paths[0]->volume;
							mode = paths[0]->getMode();
						}

						oscManager.sendSample(mediaRoot + hoveredPoint.filename, "path-0", vol, mode);

						lastHoveredPoint = hoveredPoint;
						hasLastHoveredPoint = true;
					}
				}
			}
		} else {
			// Mouse not pressed - stop playing if we were
			if (hasLastHoveredPoint) {
				oscManager.stopSample(mediaRoot + lastHoveredPoint.filename, "path-0");
				hasLastHoveredPoint = false;
				hasHoveredPoint = false;
			}
		}
	}

	// Update Paths
	float dt = 1.0f / 60.0f; // Fixed time step for simplicity

	for (auto & path : paths) {
		int prevStepIndex = path->currentStepIndex; // snapshot before update (for stepMode detection)
		path->update(dt);

		// ---- Gesture playback constraint ----
		// If a gesture is recorded, wrap path position into the gesture range
		// and override volume with the gesture curve.
		float effectiveVolume = path->volume;
		if (path->hasGesture && path->gesturePoints.size() >= 2) {
			float gMin = path->gesturePoints.front().position;
			float gMax = path->gesturePoints.back().position;
			float span = gMax - gMin;
			if (span > 0.001f) {
				// Wrap position into [gMin, gMax]
				float localPos = std::fmod(path->position - gMin, span);
				if (localPos < 0) localPos += span;
				path->position = gMin + localPos;
			} else {
				path->position = gMin;
			}
			effectiveVolume = path->getGestureVolume(path->position);
		}

		// Audio Sampling Logic
		if (path->isActive) {
			// Get active points
			std::vector<DataPoint> activePoints = path->getActivePoints(points, *spatialGrid);

			// ---------------------------------------------------------
			// SEQUENTIAL PATH LOGIC
			// ---------------------------------------------------------
			if (path->isSequential) {
				int totalSteps = path->sequentialPoints.size();
				if (totalSteps > 0) {

					// Step mode: PathObject::update() already advanced currentStepIndex via timer.
					// Just detect the change and trigger/stop samples.
					if (path->stepMode) {
						if (path->currentStepIndex != prevStepIndex && path->currentStepIndex >= 0) {
							// Stop previous
							if (prevStepIndex >= 0 && prevStepIndex < totalSteps) {
								DataPoint & prev = path->sequentialPoints[prevStepIndex];
								oscManager.stopSample(mediaRoot + prev.filename, path->name);
								path->playingPoints.erase(prev);
							}
							// Play new
							if (path->currentStepIndex < totalSteps) {
								DataPoint & curr = path->sequentialPoints[path->currentStepIndex];
								oscManager.sendSample(mediaRoot + curr.filename, path->name, effectiveVolume, path->getMode());
								path->playingPoints.insert(curr);
								if (showVideo) triggerVideo(curr);
							}
						}
						continue; // skip posFloor logic
					}
					// Compute the position-derived floor (used for boundary detection in both modes)
					int posFloor = (int)(path->position * totalSteps);
					if (posFloor >= totalSteps) posFloor = totalSteps - 1;

					int nextStepIndex;
					bool stepChanged = false;

					if (path->jitterMode) {
						// Jitter mode: use posFloor only to detect when a boundary is crossed.
						// The actual step is chosen probabilistically from currentStepIndex.
						if (posFloor != path->lastJitterStepFloor) {
							// A boundary was crossed — decide where to go
							path->lastJitterStepFloor = posFloor;
							float r = ofRandom(1.0f);
							if (r < 0.25f) {
								// 25%: go back one
								nextStepIndex = path->currentStepIndex - 1;
							} else if (r < 0.50f) {
								// 25%: repeat (stay)
								nextStepIndex = path->currentStepIndex;
							} else {
								// 50%: advance one
								nextStepIndex = path->currentStepIndex + 1;
							}
							// Wrap circularly
							nextStepIndex = ((nextStepIndex % totalSteps) + totalSteps) % totalSteps;
							stepChanged = (nextStepIndex != path->currentStepIndex);
						} else {
							// No boundary crossed this frame
							nextStepIndex = path->currentStepIndex;
						}
					} else {
						// Normal sequential: advance exactly ONE step per frame toward posFloor,
						// but jump directly if posFloor wrapped (big negative jump = loop restart).
						if (path->currentStepIndex < 0) {
							nextStepIndex = posFloor; // first frame
						} else {
							int diff = posFloor - path->currentStepIndex;
							bool wrapped = diff < -(totalSteps / 2);
							if (wrapped) {
								// Position looped — jump directly to start of new cycle
								nextStepIndex = posFloor;
							} else if (diff > 0) {
								nextStepIndex = path->currentStepIndex + 1;
							} else if (diff < 0) {
								nextStepIndex = path->currentStepIndex - 1;
							} else {
								nextStepIndex = path->currentStepIndex;
							}
						}
						stepChanged = (nextStepIndex != path->currentStepIndex);
					}

					// Handle step transition
					if (stepChanged) {
						// Stop previous
						if (path->currentStepIndex >= 0 && path->currentStepIndex < totalSteps) {
							DataPoint & prev = path->sequentialPoints[path->currentStepIndex];
							oscManager.stopSample(mediaRoot + prev.filename, path->name);
							path->playingPoints.erase(prev);
						}
						// Play new
						if (nextStepIndex >= 0 && nextStepIndex < totalSteps) {
							DataPoint & curr = path->sequentialPoints[nextStepIndex];
							oscManager.sendSample(mediaRoot + curr.filename, path->name, effectiveVolume, path->getMode());
							path->playingPoints.insert(curr);
							if (showVideo) triggerVideo(curr);
						}
						path->currentStepIndex = nextStepIndex;
					} else if (path->currentStepIndex < 0 && posFloor >= 0) {
						// First frame: start playback
						int startIdx = path->jitterMode ? 0 : posFloor;
						if (startIdx < totalSteps) {
							DataPoint & curr = path->sequentialPoints[startIdx];
							oscManager.sendSample(mediaRoot + curr.filename, path->name, effectiveVolume, path->getMode());
							path->playingPoints.insert(curr);
							if (showVideo) triggerVideo(curr);
							path->currentStepIndex = startIdx;
							path->lastJitterStepFloor = posFloor;
						}
					}
				}

				// Skip the spatial radius logic below for sequential paths
				continue;
			}

			// ---------------------------------------------------------
			// NEW LOGIC: Managed Playing Points
			// ---------------------------------------------------------

			// 1. Clean up playing points that have left the radius
			// Create a list of points to remove to avoid invalidating iterator
			std::vector<DataPoint> toRemove;
			for (const auto & p : path->playingPoints) {
				bool stillInRadius = false;
				for (const auto & ap : activePoints) {
					if (ap == p) {
						stillInRadius = true;
						break;
					}
				}
				if (!stillInRadius) {
					toRemove.push_back(p);
				}
			}

			// Remove and Stop
			for (const auto & p : toRemove) {
				oscManager.stopSample(mediaRoot + p.filename, path->name);
				path->playingPoints.erase(p);
			}

			// 2. Add new points if we have slots available
			int slotsAvailable = path->sampleNum - path->playingPoints.size();
			if (slotsAvailable > 0) {
				for (const auto & p : activePoints) {
					if (slotsAvailable <= 0) break;

					// If not already playing
					if (path->playingPoints.find(p) == path->playingPoints.end()) {
						// Add and Start
						path->playingPoints.insert(p);
						slotsAvailable--;

						// Trigger Sound
						if (path->mode == PathObject::LOOP_MODE || path->mode == PathObject::MIXED_MODE) {
							// Calculate volume based on distance
							float d = path->getDistanceToPoint(ofVec2f(p.x, p.y));
							float vol = effectiveVolume * (1.0f - (d / path->radius));
							vol = MAX(0, vol);
							oscManager.sendSample(mediaRoot + p.filename, path->name, vol, path->getMode());
						}

						// Trigger Video (Only for the first playing point? Or all? Let's keep existing logic: Trigger on start)
						if (showVideo) {
							// Rate limited video switching
							long now = ofGetElapsedTimeMillis();
							if (now - lastVideoSwitchTime > 50) { // 50ms limit
								lastVideoSwitchTime = now;

								string videoPath = "";
								string baseName = ofFilePath::removeExt(p.filename);

								if (mediaRoot != "") {
									string nestedPath = mediaRoot + "video_segments/" + baseName + ".mp4";
									lastAttemptedVideoPath = nestedPath;
									if (ofFile(nestedPath).exists()) {
										videoPath = nestedPath;
									} else {
										lastAttemptedVideoPath += " (Missing)";
										videoPath = mediaRoot + baseName + ".mp4";
									}
								} else {
									videoPath = baseName + ".mp4";
									lastAttemptedVideoPath = videoPath;
								}

								ofFile vFile(videoPath);
								string curPath = videoFront->isLoaded() ? videoFront->getMoviePath() : "";
								if (vFile.exists() && curPath != videoPath) {
									std::swap(videoFront, videoBack);
									videoFront->load(videoPath);
									videoFront->setLoopState(OF_LOOP_NORMAL);
									videoFront->play();
									videoFront->setVolume(0);
									videoAlpha = 0.0f;
									videoAlphaTarget = 255.0f;
								}
							}
						}
					}
				}
			}

			// 3. Continuous Updates for Playing Points
			if (path->mode == PathObject::LOOP_MODE || path->mode == PathObject::MIXED_MODE || path->mode == PathObject::CLOUD_MODE) {
				for (const auto & p : path->playingPoints) {
					float d = path->getDistanceToPoint(ofVec2f(p.x, p.y));
					float vol = effectiveVolume * (1.0f - (d / path->radius));
					vol = MAX(0, vol);

					if (path->mode == PathObject::LOOP_MODE || path->mode == PathObject::MIXED_MODE) {
						// Send volume update (if we had a generic update message, or just re-send sample command with updated vol?
						// OscManager::sendSample triggers a new play. We need a volume update or parameter update.
						// OscManager currently has sendPathVolume (global for path) but not per-voice volume unless we use specific voice allocation API.
						// The previous code re-sent sendSample? No, previous code only sent sendSample on 'isNew'.
						// It didn't update volume continuously for loop mode?
						// Looking at previous code:
						// if (isNew) { ... oscManager.sendSample(...) }
						// Continuous update was ONLY for "cloud" or "mixed".
						// So for "loop", we typically just start it.
						// BUT, if we want distance-based attenuation, we should probably update it.
						// However, sendSample restarts the sample in many simple OSC implementations.
						// Let's stick to previous behavior: Only start it.
						// The only continuous update in previous code was for Granular ("cloud").
					}

					if (path->mode == PathObject::CLOUD_MODE || path->mode == PathObject::MIXED_MODE) {
						// Map path position to grain position
						float pos = ofMap(path->position, 0.0f, 1.0f, path->gPosMin, path->gPosMax, true);

						// Randomized grain params
						float rate = ofRandom(path->gRateMin, path->gRateMax);
						float dur = ofRandom(path->gDurMin, path->gDurMax);
						int grainDensity = (int)ofRandom(path->gGrainRateMin, path->gGrainRateMax);

						oscManager.updateGrain(mediaRoot + p.filename, path->name,
							rate,
							pos,
							vol,
							dur,
							grainDensity,
							path->gRand,
							path->gEnv);
					}
				}
			}

			// Update lastActivePoints just in case other things need it, or remove it if unused.
			// The draw loop uses it to draw lines! So we must keep it matching playingPoints or activePoints?
			// Draw loop: "Draw lines to active points ... for (const auto & p : lastActivePoints)"
			// If we want lines to only drawn to PLAYING points, we should update lastActivePoints to be playingPoints.
			path->lastActivePoints = path->playingPoints;
		}
	}

	if (showVideo) {
		videoFront->update();
		videoBack->update();
		// Sync fade speed from GUI param
		videoFadeSpeed = videoFadeSpeed_param.get();
		// Advance crossfade alpha toward target
		if (videoAlpha < videoAlphaTarget)
			videoAlpha = std::min(videoAlpha + videoFadeSpeed, videoAlphaTarget);
		else if (videoAlpha > videoAlphaTarget)
			videoAlpha = std::max(videoAlpha - videoFadeSpeed, videoAlphaTarget);

		// Capture the front player's current frame to the hold FBO.
		// Only fires when a genuinely new frame has been decoded — never captures
		// a loading artefact or black frame.
		if (videoFront->isFrameNew() && videoFront->getWidth() > 0) {
			// Lazily allocate here so window size is definitely known
			if (!videoHoldAllocated) {
				videoHoldFbo.allocate(ofGetWidth(), ofGetHeight(), GL_RGB);
				videoHoldAllocated = true;
			}
			videoHoldFbo.begin();
			ofClear(backgroundColor.get().r,
				backgroundColor.get().g,
				backgroundColor.get().b, 255);
			ofSetColor(255);
			// Draw with current fit mode into the FBO
			float vw = videoFront->getWidth(), vh = videoFront->getHeight();
			float sw = (float)ofGetWidth(), sh = (float)ofGetHeight();
			float x = 0, y = 0, dw = sw, dh = sh;
			int fitMode = videoFitMode.get();
			if (fitMode == 1 && vh > 0) {
				dh = sh;
				dw = (vw / vh) * sh;
				x = (sw - dw) * 0.5f;
				y = 0;
			} else if (fitMode == 2 && vw > 0) {
				dw = sw;
				dh = (vh / vw) * sw;
				x = 0;
				y = (sh - dh) * 0.5f;
			}
			videoFront->draw(x, y, dw, dh);
			videoHoldFbo.end();
		}
	}

	// Mouse Scrubbing Update
	// Mouse Scrubbing Update - REMOVED for Navigate Mode
	if (currentMode == NAVIGATE && spatialGrid && !paths.empty()) {
		// Disabled scrubbing in NAVIGATE mode as per request.
		// Clearing any lingering mouse active points if needed.
		if (!mouseActivePoints.empty()) {
			for (const auto & p : mouseActivePoints) {
				oscManager.stopSample(mediaRoot + p.filename, "mouse");
			}
			mouseActivePoints.clear();
		}
	}
}

//--------------------------------------------------------------
void ofApp::drawVisuals() {

	// Draw Video Background
	if (showVideo) {
		if (videoHoldAllocated) {
			// Paint the last-captured frame as a persistent background.
			// This is always opaque — covers any gap/flicker from the live player.
			ofSetColor(255, 255, 255, 255);
			videoHoldFbo.draw(0, 0);
		} else {
			ofBackground(backgroundColor); // before first frame is ever captured
		}

		// Draw the live front player on top as it fades in (0 → 255 on clip switch).
		// When alpha reaches 255 it fully covers the held background; the FBO then
		// captures this live frame on the next update, keeping them in sync.
		if (videoFront->isLoaded() && videoFront->getWidth() > 0) {
			ofEnableBlendMode(OF_BLENDMODE_ALPHA);
			ofSetColor(255, 255, 255, (int)std::clamp(videoAlpha, 0.0f, 255.0f));
			float vw = videoFront->getWidth(), vh = videoFront->getHeight();
			float sw = (float)ofGetWidth(), sh = (float)ofGetHeight();
			float x = 0, y = 0, dw = sw, dh = sh;
			int fitMode = videoFitMode.get();
			if (fitMode == 1 && vh > 0) {
				dh = sh;
				dw = (vw / vh) * sh;
				x = (sw - dw) * 0.5f;
				y = 0;
			} else if (fitMode == 2 && vw > 0) {
				dw = sw;
				dh = (vh / vw) * sw;
				x = 0;
				y = (sh - dh) * 0.5f;
			}
			videoFront->draw(x, y, dw, dh);
			ofDisableBlendMode();
		}
		ofSetColor(255);
	} else {
		ofBackground(backgroundColor);
	}

	ofPushMatrix();
	ofTranslate(ofGetWidth() / 2, ofGetHeight() / 2);
	ofTranslate(pan);
	ofScale(zoom, zoom);

	// Draw Points
	ofFill(); // Ensure fill is enabled
	if (showText) {
		ofColor c = pointColor.get();
		ofSetColor(c.r, c.g, c.b, 25); // 10% opacity
	} else {
		ofColor c = pointColor.get();
		ofSetColor(c.r, c.g, c.b, 255); // Force full opacity if not showing text
	}

	for (const auto & p : points) {
		ofDrawCircle(p.x, p.y, pointSize / zoom);
	}

	// Draw Text
	// Draw Paths
	for (auto & path : paths) {
		path->draw(playheadSize / zoom, playheadColor, zoom);
	}

	// Draw current path
	if (isDrawingPath && currentPath) {
		ofSetColor(255, 100, 100);
		// Only draw the line, not the playhead circle
		currentPath->polyline.draw();
	}

	ofPopMatrix();

	// ---- Screen-space overlays for selected path ----
	if (selectedPath) {
		float sw = (float)ofGetWidth();
		float sh = (float)ofGetHeight();

		// Crosshair: X = position (0-1), Y = volume (0=bottom, 1=top)
		// When ALT is held, use mouse position directly to avoid one-frame lag
		float cx, cy;
		if (ofGetKeyPressed(OF_KEY_ALT)) {
			cx = (float)ofGetMouseX();
			cy = (float)ofGetMouseY();
		} else {
			cx = selectedPath->position * sw;
			// Use gesture volume when a gesture is active, otherwise static volume
			float displayVol = (selectedPath->hasGesture && !selectedPath->gesturePoints.empty())
				? selectedPath->getGestureVolume(selectedPath->position)
				: selectedPath->volume;
			cy = (1.0f - displayVol) * sh;
		}
		float crossSize = 10.0f;
		ofColor phc = playheadColor.get();
		ofSetColor(phc.r, phc.g, phc.b, 128); // 50% opacity
		ofSetLineWidth(2);
		ofDrawLine(cx - crossSize, cy, cx + crossSize, cy);
		ofDrawLine(cx, cy - crossSize, cx, cy + crossSize);
		ofSetLineWidth(1);

		// Gesture overlay (screen-space): draw recorded gesture as a visible curve
		if (selectedPath->hasGesture && !selectedPath->gesturePoints.empty()) {
			ofSetLineWidth(3);
			for (size_t i = 0; i + 1 < selectedPath->gesturePoints.size(); ++i) {
				const auto & a = selectedPath->gesturePoints[i];
				const auto & b = selectedPath->gesturePoints[i + 1];
				float x0 = a.position * sw;
				float y0 = (1.0f - a.volume) * sh;
				float x1 = b.position * sw;
				float y1 = (1.0f - b.volume) * sh;
				int alpha = (int)(a.volume * 135) + 120;
				ofSetColor(100, 255, 150, alpha);
				ofDrawLine(x0, y0, x1, y1);
			}
			ofSetLineWidth(1);
		}
	}

	// Draw Text (Screen Space)
	if (showText) {
		ofSetColor(textColor);
		if (font.isLoaded()) {
			// Update font size if changed
			static float lastFontSize = fontSize;
			if (abs(lastFontSize - fontSize) > 0.5f) {
				bool result = font.load("Futura.ttc", fontSize);
				if (!result) font.load(OF_TTF_SANS, fontSize);
				lastFontSize = fontSize;
			}

			ofVec2f center(ofGetWidth() / 2, ofGetHeight() / 2);
			for (const auto & p : points) {
				if (!p.text.empty()) {
					// Calculate screen position: (world * zoom) + pan + center
					ofVec2f screenPos = (ofVec2f(p.x, p.y) * zoom) + pan + center;

					// Simple culling
					if (screenPos.x > -100 && screenPos.x < ofGetWidth() + 100 && screenPos.y > -100 && screenPos.y < ofGetHeight() + 100) {

						ofRectangle bounds = font.getStringBoundingBox(p.text, 0, 0);
						// Draw with drop shadow for readability? optional.
						font.drawString(p.text, screenPos.x - bounds.width / 2, screenPos.y - bounds.height / 2 + bounds.height);
					}
				}
			}
		} else {
			// Fallback Bitmap String
			ofVec2f center(ofGetWidth() / 2, ofGetHeight() / 2);
			for (const auto & p : points) {
				if (!p.text.empty()) {
					ofVec2f screenPos = (ofVec2f(p.x, p.y) * zoom) + pan + center;
					if (screenPos.x > -100 && screenPos.x < ofGetWidth() + 100 && screenPos.y > -100 && screenPos.y < ofGetHeight() + 100) {
						float strWidth = p.text.length() * 8.0f;
						ofDrawBitmapString(p.text, screenPos.x - strWidth / 2, screenPos.y + 4);
					}
				}
			}
		}
	}

	// Draw Mouse Scrubbing Feedback
	if (currentMode == NAVIGATE && !mouseActivePoints.empty()) {
		ofPushMatrix();
		ofTranslate(ofGetWidth() / 2, ofGetHeight() / 2);
		ofTranslate(pan);
		ofScale(zoom, zoom);

		ofSetColor(255, 200);
		ofNoFill();
		ofVec2f mousePos(ofGetMouseX(), ofGetMouseY());
		ofVec2f worldMouse = screenToWorld(mousePos.x, mousePos.y);
		ofDrawCircle(worldMouse.x, worldMouse.y, 50.0f / zoom); // Draw scrubbing radius

		for (const auto & p : mouseActivePoints) {
			ofDrawLine(worldMouse.x, worldMouse.y, p.x, p.y);
			ofDrawCircle(p.x, p.y, 4.0f / zoom);
		}
		ofPopMatrix();
	}

	// Draw Marquee
	if (currentMode == NAVIGATE && isMarqueeZooming) {
		ofSetColor(255, 100);
		ofNoFill();
		ofDrawRectangle(marqueeStart.x, marqueeStart.y, marqueeEnd.x - marqueeStart.x, marqueeEnd.y - marqueeStart.y);
		ofSetColor(255, 50);
		ofFill();
		ofDrawRectangle(marqueeStart.x, marqueeStart.y, marqueeEnd.x - marqueeStart.x, marqueeEnd.y - marqueeStart.y);
	}

	// Draw UI
	ofSetColor(255);
	string modeStr = "";
	switch (currentMode) {
	case NAVIGATE:
		modeStr = "NAVIGATE";
		break;
	case DRAW_FREEHAND:
		modeStr = "DRAW_FREEHAND";
		break;
	case DRAW_LINE:
		modeStr = "DRAW_LINE";
		break;
	case EDIT:
		modeStr = "EDIT";
		break;
	case WANDER:
		modeStr = "WANDER";
		break;
	case BROWSE:
		modeStr = "BROWSE";
		break;
	}
	ofDrawBitmapString("Mode: " + modeStr, 20, 20);
	ofDrawBitmapString("Zoom: " + ofToString(zoom), 20, 40);
	ofDrawBitmapString("Paths: " + ofToString(paths.size()), 20, 60);
	ofDrawBitmapString("Video Mode (m): " + ofToString(showVideo ? "ON" : "OFF"), 20, 80);
	if (showVideo) {
		ofDrawBitmapString("Media Root: " + mediaRoot, 20, 100);
		ofDrawBitmapString("Loaded Video: " + ofToString(videoFront->getMoviePath()), 20, 120);
		ofDrawBitmapString("Last Attempted: " + lastAttemptedVideoPath, 20, 140);
		ofDrawBitmapString("Video Playing: " + ofToString(videoFront->isPlaying()), 20, 160);
	}

	// Draw OSC Debug
	// Draw OSC Debug
	if (showDebug) {
		oscManager.drawDebug(ofGetWidth() - 400, 20);
		// helper text
		ofDrawBitmapString("Keys: Space(Play) f(Draw) n(Nav) m(Video) d(Debug) ,(Settings)", 20, ofGetHeight() - 30);
	}

	// Help Overlay — drawn last, on top of everything
	if (showHelp) {
		float sw = (float)ofGetWidth();
		float sh = (float)ofGetHeight();
		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
		ofFill();
		ofSetColor(0, 0, 0, 180);
		ofDrawRectangle(0, 0, sw, sh);

		ofSetColor(255);
		int lx = 60, ly = 60, lineH = 18;
		ofDrawBitmapString("=== KEYBOARD SHORTCUTS ===", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("--- Modes ---", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  n     Navigate", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  f     Freehand Draw", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  w     Wander (auto-path from nearest point)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  b     Browse (click to audition points)", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("--- Playback ---", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Space Toggle play/pause on selected path", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  q     Create sequential path (all points)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  e     Toggle step mode (timer-driven steps)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Up/Dn Adjust speed", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  r / R Increase / decrease radius", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  v / V Increase / decrease volume", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("--- Scrub & Gesture ---", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Alt+move    Scrub position (X) & volume (Y)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Alt+drag    Record gesture (position+volume)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  g           Clear gesture on selected path", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("--- View ---", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  z     Zoom to fit all points", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Shift+drag  Marquee zoom", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  m     Toggle video view", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  t     Toggle text labels", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  d     Toggle debug info", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  ,     Toggle settings panel", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("--- Paths ---", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  c     Deselect path", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Del   Delete selected path", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  x     Clear all paths & points", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  k     Refresh path", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  s     Save path", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  o     Load points from JSON", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("Press H to close", lx, ly);
		ofDisableBlendMode();
	}
}

//--------------------------------------------------------------
void ofApp::draw() {
	drawVisuals();

	// Draw GUI only on the main control window
	if (showGui) {
		gui.draw();
	}
}

//--------------------------------------------------------------
void ofApp::drawProjector(ofEventArgs & args) {
	drawVisuals();
}

void ofApp::exit() {
	gui.saveToFile("settings.xml");
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	inputManager.updateModifiers(key, true);

	// Delegate to InputManager for command mapping
	AppCommand cmd = inputManager.getCommandForKey(key);

	switch (cmd) {
	case CMD_MODE_DRAW:
		currentMode = DRAW_FREEHAND;
		isDrawingPath = false; // Reset
		break;

	case CMD_MODE_NAVIGATE:
		currentMode = NAVIGATE;
		break;

	case CMD_MODE_WANDER:
		currentMode = WANDER;
		break;

	case CMD_TOGGLE_PLAYBACK: // Spacebar
		if (selectedPath) {
			selectedPath->togglePlayback();
		}
		break;

	case CMD_LOAD_POINTS: // 'o'
	{
		ofFileDialogResult result = ofSystemLoadDialog("Select JSON Data or Composition File");
		if (result.bSuccess) {
			loadCompositionOrPoints(result.getPath());
		}
	} break;

	case CMD_SAVE_COMPOSITION: // 's'
	{
		ofFileDialogResult result = ofSystemSaveDialog("composition.json", "Save Composition As");
		if (result.bSuccess) {
			saveComposition(result.getPath());
		}
	} break;

	case CMD_TOGGLE_VIDEO:
		if (key == 'm' || key == 'M') {
			showVideo = !showVideo;
		}
		if (!showVideo) videoFront->stop();
		break;

	case CMD_TOGGLE_FULLSCREEN_PROJECTOR:
		if (projectorWindow) {
			projectorWindow->toggleFullscreen();
		}
		break;

	case CMD_TOGGLE_TEXT:
		showText = !showText;
		break;

	case CMD_TOGGLE_SETTINGS:
		showGui = !showGui;
		break;

	case CMD_TOGGLE_BROWSE:
		if (currentMode == BROWSE)
			currentMode = NAVIGATE;
		else
			currentMode = BROWSE;

		// Stop any playing sound when switching
		if (hasLastHoveredPoint) {
			oscManager.stopSample(mediaRoot + lastHoveredPoint.filename, "path-0");
			hasLastHoveredPoint = false;
			hasHoveredPoint = false;
		}
		break;

	case CMD_REFRESH_PATH:
		if (selectedPath) {
			oscManager.sendPathRefresh(selectedPath->name);
		} else {
			oscManager.sendPathRefresh("path-0");
		}
		break;

	case CMD_INCREASE_RADIUS: // 'r'
		bHoldingR = true;
		break;

	case CMD_DECREASE_RADIUS: // 'R'
		if (selectedPath) {
			selectedPath->radius -= 10.0f;
			if (selectedPath->radius < 0) selectedPath->radius = 0;
		}
		break;

	case CMD_INCREASE_VOLUME: // 'v'
		bHoldingV = true;
		break;

	case CMD_DECREASE_VOLUME: // 'V'
		if (selectedPath) {
			selectedPath->volume -= 0.1f;
			if (selectedPath->volume < 0) selectedPath->volume = 0;
		}
		break;

	case CMD_DESELECT_PATH: // 'c'
		selectedPath = nullptr;
		break;

	case CMD_DELETE_PATH:
		if (selectedPath) {
			// Stop playing samples
			for (const auto & p : selectedPath->playingPoints) {
				oscManager.stopSample(mediaRoot + p.filename, selectedPath->name);
			}

			// Send OSC Removal
			oscManager.sendPathRemove(selectedPath->name);
			oscManager.sendUIPathRemove(selectedPath->name);

			// Remove from paths vector
			// std::remove moves elements to end and returns iterator to first "removed" element
			auto it = std::remove(paths.begin(), paths.end(), selectedPath);
			paths.erase(it, paths.end());

			selectedPath = nullptr;
		}
		break;

	case CMD_CREATE_SEQUENTIAL_PATH: {
		// Create new path
		auto newPath = std::make_shared<PathObject>(pathIdCounter++);
		newPath->name = "seq-" + ofToString(newPath->id);
		newPath->isSequential = true;
		newPath->radius = 1.0f; // Minimal radius as we use step logic

		// Sort points
		std::vector<DataPoint> sortedPoints = points; // Copy
		std::sort(sortedPoints.begin(), sortedPoints.end(), [](const DataPoint & a, const DataPoint & b) {
			// Handle empty filenames?
			if (a.filename.empty() && b.filename.empty()) return false;
			if (a.filename.empty()) return true; // Empty first? or last?
			if (b.filename.empty()) return false;
			return naturalCompare(a.filename, b.filename) < 0;
		});

		// Build polyline as straight lines — no smoothing for sequential paths
		newPath->sequentialPoints = sortedPoints;
		for (const auto & p : sortedPoints) {
			newPath->controlPoints.push_back(ofVec2f(p.x, p.y));
			newPath->polyline.addVertex(p.x, p.y, 0);
		}
		// Do not call finalize() — we want straight lines, not a smoothed spline

		oscManager.sendUIPathAdd(newPath->name);
		paths.push_back(newPath);

		// Auto-select
		selectedPath = newPath;
		selectedPath->isSelected = true;
		for (auto & p : paths)
			if (p != selectedPath) p->isSelected = false;
	} break;

	case CMD_CLEAR_ALL: // 'x'
		paths.clear();
		points.clear();
		selectedPath = nullptr;
		if (spatialGrid) spatialGrid->clear();
		oscManager.sendClear();
		break;

	case CMD_RESET_ZOOM: // 'z' — zoom to extents of loaded points
		if (!points.empty()) {
			float minX = std::numeric_limits<float>::max();
			float maxX = std::numeric_limits<float>::lowest();
			float minY = std::numeric_limits<float>::max();
			float maxY = std::numeric_limits<float>::lowest();
			for (const auto & p : points) {
				if (p.x < minX) minX = p.x;
				if (p.x > maxX) maxX = p.x;
				if (p.y < minY) minY = p.y;
				if (p.y > maxY) maxY = p.y;
			}
			float dataWidth = (maxX - minX) * 1.2f;
			float dataHeight = (maxY - minY) * 1.2f;
			if (dataWidth <= 0) dataWidth = 1000;
			if (dataHeight <= 0) dataHeight = 1000;
			zoom = std::min(ofGetWidth() / dataWidth, ofGetHeight() / dataHeight);
			float centerX = (minX + maxX) / 2.0f;
			float centerY = (minY + maxY) / 2.0f;
			pan.set(-centerX * zoom, -centerY * zoom);
		} else {
			zoom = 1.0f;
			pan.set(0, 0);
		}
		break;

	case CMD_TOGGLE_DEBUG: // 'd'
		showDebug = !showDebug;
		ofLogNotice() << "Toggle Debug: " << showDebug;
		break;

	case CMD_CLEAR_GESTURE:
		if (selectedPath) {
			selectedPath->clearGesture();
			ofLogNotice("ofApp") << "Gesture cleared.";
		}
		break;

	case CMD_TOGGLE_STEP_MODE:
		if (selectedPath && selectedPath->isSequential) {
			selectedPath->stepMode = !selectedPath->stepMode;
			selectedPath->stepTimer = 0.0f;
			if (selectedPath->stepMode) selectedPath->currentStepIndex = 0;
			ofLogNotice("ofApp") << "Step mode: " << (selectedPath->stepMode ? "ON" : "OFF");
		}
		break;

	case CMD_TOGGLE_HELP:
		showHelp = !showHelp;
		break;

	default:
		// Plus / Minus for SampleNum
		if ((key == '+' || key == '=') && selectedPath) {
			selectedPath->sampleNum++;
		} else if ((key == '-' || key == '_') && selectedPath) {
			selectedPath->sampleNum--;
			if (selectedPath->sampleNum < 1) selectedPath->sampleNum = 1;
		}

		// Handle arrow keys manually
		if (key == OF_KEY_UP && selectedPath && !ofGetKeyPressed(OF_KEY_SHIFT)) {
			// Previous behavior: increment sampleNum?
			selectedPath->sampleNum++;
		}

		// Path Cycling (Shift + Arrow)
		if (ofGetKeyPressed(OF_KEY_SHIFT)) {
			if (key == OF_KEY_LEFT) {
				// Cycle Previous
				if (paths.empty()) return;
				int idx = -1;
				for (int i = 0; i < paths.size(); ++i)
					if (paths[i]->isSelected) idx = i;

				if (idx != -1) paths[idx]->isSelected = false;
				int newIdx = (idx - 1 + paths.size()) % paths.size();
				if (newIdx < 0) newIdx = 0; // Should be handled by modulo but safety

				selectedPath = paths[newIdx];
				selectedPath->isSelected = true;
				sendFullUIUpdate(selectedPath);

			} else if (key == OF_KEY_RIGHT) {
				// Cycle Next
				if (paths.empty()) return;
				int idx = -1;
				for (int i = 0; i < paths.size(); ++i)
					if (paths[i]->isSelected) idx = i;

				if (idx != -1) paths[idx]->isSelected = false;
				int newIdx = (idx + 1) % paths.size();

				selectedPath = paths[newIdx];
				selectedPath->isSelected = true;
				sendFullUIUpdate(selectedPath);

			} else if (key == OF_KEY_DOWN) {
				// Deselect All
				for (auto & p : paths)
					p->isSelected = false;
				selectedPath = nullptr;
			}
		}
		break;
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key) {
	inputManager.updateModifiers(key, false);

	if (key == 'r' || key == 'R') bHoldingR = false;
	if (key == 'v' || key == 'V') bHoldingV = false;
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {
	ofVec2f worldPos = screenToWorld(x, y);

	if (currentMode == DRAW_FREEHAND) {
		if (!isDrawingPath) {
			currentPath = std::make_shared<PathObject>(pathIdCounter++);
			isDrawingPath = true;
		}
		currentPath->addPoint(worldPos);
	} else {
		// Alt+click in any mode: start gesture recording on selected path
		if (ofGetKeyPressed(OF_KEY_ALT) && selectedPath) {
			selectedPath->gesturePoints.clear();
			selectedPath->hasGesture = false;
			isRecordingGesture = true;
			return;
		}
	}

	if (currentMode == NAVIGATE) {
		// Marquee Zoom Check (Shift + Click)
		if (ofGetKeyPressed(OF_KEY_SHIFT)) {
			isMarqueeZooming = true;
			marqueeStart.set(x, y);
			marqueeEnd.set(x, y);
		} else {
			lastMouse.set(x, y);
			isDragging = true;

			// Selection Logic
			// Simple closest path selection
			float minDist = 20.0f / zoom;
			selectedPath = nullptr;
			for (auto & p : paths) {
				float d = p->getDistanceToPoint(worldPos);
				if (d < minDist) {
					minDist = d;
					selectedPath = p;
				}
			}

			// Update selection state
			for (auto & p : paths)
				p->isSelected = (p == selectedPath);
		}
	} else if (currentMode == BROWSE) {
		// In BROWSE, click near a path to select+drag it
		if (!ofGetKeyPressed(OF_KEY_ALT)) {
			float minDist = 20.0f / zoom;
			std::shared_ptr<PathObject> clickedPath = nullptr;
			for (auto & p : paths) {
				float d = p->getDistanceToPoint(worldPos);
				if (d < minDist) {
					minDist = d;
					clickedPath = p;
				}
			}
			if (clickedPath) {
				selectedPath = clickedPath;
				for (auto & p : paths)
					p->isSelected = (p == selectedPath);
				isDraggingPath = true;
				lastDragWorld = worldPos;
			}
		}
	} else if (currentMode == WANDER) {
		// Find the nearest DataPoint to the click, build a wandering path from it
		if (!points.empty()) {
			const DataPoint * nearest = nullptr;
			float nearestDist = std::numeric_limits<float>::max();
			for (const auto & p : points) {
				float d = ofVec2f(p.x, p.y).distance(worldPos);
				if (d < nearestDist) {
					nearestDist = d;
					nearest = &p;
				}
			}
			if (nearest != nullptr) {
				auto path = createWanderingPath(*nearest, 50, 0.5f, 3);
				oscManager.sendUIPathAdd(path->name);
				paths.push_back(path);
				// Auto-select
				for (auto & p : paths)
					p->isSelected = false;
				selectedPath = path;
				path->isSelected = true;
				sendFullUIUpdate(path);
			}
		} else {
			ofLogWarning("WANDER") << "No points loaded — cannot create wandering path";
		}
	}

	// Always track last mouse for generic dragging calculations
	lastMouse.set(x, y);
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {
	if (selectedPath) {
		if (bHoldingR) {
			float deltaY = lastMouse.y - y;
			// Scale sensitivity based on current zoom so it feels consistent
			selectedPath->radius += deltaY * (2.0f / zoom);

			// Bound radius
			if (selectedPath->radius < 0.001f) selectedPath->radius = 0.001f;

			// Max radius of screen height (adjusted for scale)
			float maxRadius = ofGetHeight() / zoom;
			if (selectedPath->radius > maxRadius) selectedPath->radius = maxRadius;

			oscManager.sendUIPathUpdate(selectedPath->id, selectedPath->isActive,
				selectedPath->radius, selectedPath->direction, selectedPath->sampleNum,
				selectedPath->volume, selectedPath->falloff, selectedPath->speed, selectedPath->mode);

			lastMouse.set(x, y);
			return; // handled
		}

		if (bHoldingV) {
			float deltaY = lastMouse.y - y;
			selectedPath->speed += deltaY * 0.01f;
			// Limit speed intuitively
			if (selectedPath->speed < 0.0f) selectedPath->speed = 0.0f;

			oscManager.sendUIPathUpdate(selectedPath->id, selectedPath->isActive,
				selectedPath->radius, selectedPath->direction, selectedPath->sampleNum,
				selectedPath->volume, selectedPath->falloff, selectedPath->speed, selectedPath->mode);

			lastMouse.set(x, y);
			return; // handled
		}
	}
	// Allow gesture recording even in BROWSE mode (before the early return)
	if (ofGetKeyPressed(OF_KEY_ALT) && selectedPath && isRecordingGesture) {
		float scrubPos = std::clamp((float)x / (float)ofGetWidth(), 0.0f, 1.0f);
		float vol = std::clamp(ofMap((float)y, (float)ofGetHeight(), 0.f, 0.f, 1.f), 0.f, 1.f);
		selectedPath->gesturePoints.push_back({ scrubPos, vol });
		selectedPath->position = scrubPos; // live preview during record
		return;
	}
	// Path dragging in BROWSE mode
	if (currentMode == BROWSE && isDraggingPath && selectedPath) {
		ofVec2f worldPos = screenToWorld(x, y);
		ofVec2f delta = worldPos - lastDragWorld;
		selectedPath->translate(delta);
		lastDragWorld = worldPos;
		return;
	}
	if (currentMode == BROWSE) return;

	ofVec2f worldPos = screenToWorld(x, y);

	// Alt/Option: gesture recording (if started) or scrubbing
	if (ofGetKeyPressed(OF_KEY_ALT) && selectedPath) {
		float scrubPos = std::clamp((float)x / (float)ofGetWidth(), 0.0f, 1.0f);
		selectedPath->position = scrubPos;

		// Sequential path: also advance the step index
		if (selectedPath->isSequential && !selectedPath->sequentialPoints.empty()) {
			int maxIdx = (int)selectedPath->sequentialPoints.size() - 1;
			selectedPath->currentStepIndex = (int)std::clamp(
				ofMap((float)x, 0.f, (float)ofGetWidth(),
					0.f, (float)maxIdx),
				0.f, (float)maxIdx);
		}

		// Y axis → volume (bottom=0, top=1)
		float vol = std::clamp(
			ofMap((float)y, (float)ofGetHeight(), 0.f, 0.f, 1.f), 0.f, 1.f);

		if (isRecordingGesture) {
			// Accumulate gesture point
			selectedPath->gesturePoints.push_back({ scrubPos, vol });
		} else {
			// Classic scrub: update volume live
			float prevVol = selectedPath->volume;
			selectedPath->volume = vol;
			if (selectedPath->volume != prevVol) {
				oscManager.sendPathVolume(selectedPath->name, selectedPath->volume);
				oscManager.sendUIPathUpdate(selectedPath->id, selectedPath->isActive,
					selectedPath->radius, selectedPath->direction, selectedPath->sampleNum,
					selectedPath->volume, selectedPath->falloff, selectedPath->speed, selectedPath->mode);
			}
		}
		return; // skip normal drag handling
	}

	if (currentMode == DRAW_FREEHAND && isDrawingPath) {
		currentPath->addPoint(worldPos);
	} else if (currentMode == NAVIGATE) {
		if (isMarqueeZooming) {
			marqueeEnd.set(x, y);
		} else if (isDragging) {
			ofVec2f diff = ofVec2f(x, y) - lastMouse;
			pan += diff;
			lastMouse.set(x, y);
		}
	}
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button) {
	// Stop path dragging
	isDraggingPath = false;

	// Commit gesture recording if active
	if (isRecordingGesture && selectedPath) {
		selectedPath->hasGesture = !selectedPath->gesturePoints.empty();
		if (selectedPath->hasGesture)
			ofLogNotice("ofApp") << "Gesture recorded: " << selectedPath->gesturePoints.size() << " points.";
		isRecordingGesture = false;
		return;
	}
	if (currentMode == DRAW_FREEHAND && isDrawingPath) {
		// Min Length Check
		if (currentPath->polyline.getPerimeter() < 10.0f / zoom || currentPath->polyline.size() < 3) {
			// Discard
			ofLogNotice() << "Path too short/small, discarding.";
			isDrawingPath = false;
			currentPath = nullptr;
		} else {
			currentPath->finalize();
			oscManager.sendUIPathAdd(currentPath->name);
			paths.push_back(currentPath);

			// Auto-select the new path
			selectedPath = currentPath;
			selectedPath->isSelected = true;
			// Deselect others
			for (auto & p : paths) {
				if (p != selectedPath) p->isSelected = false;
			}
		}

		isDrawingPath = false;
		currentPath = nullptr;
	} else if (currentMode == NAVIGATE && isMarqueeZooming) {
		// Apply Zoom
		float width = abs(marqueeEnd.x - marqueeStart.x);
		float height = abs(marqueeEnd.y - marqueeStart.y);

		if (width > 10 && height > 10) {
			// Calculate center of selection in screen space
			ofVec2f selectionCenter = (marqueeStart + marqueeEnd) / 2.0f;

			// Calculate target zoom to fit the box
			// We want 'width' to become 'screenWidth'
			// zoomFactor = screenWidth / width
			float zoomFactor = MIN(ofGetWidth() / width, ofGetHeight() / height);

			// New Zoom
			float newZoom = zoom * zoomFactor;

			// New Pan
			// We want the world point under 'selectionCenter' to move to screen center
			// currentWorld = screenToWorld(selectionCenter)
			// newScreenCenter = (currentWorld * newZoom) + newPan + centerOffset

			ofVec2f centerOffset(ofGetWidth() / 2, ofGetHeight() / 2);
			ofVec2f targetWorld = screenToWorld(selectionCenter.x, selectionCenter.y);

			// We want targetWorld to be at centerOffset
			// centerOffset = (targetWorld * newZoom) + newPan + centerOffset
			// 0 = (targetWorld * newZoom) + newPan
			// newPan = -(targetWorld * newZoom)

			pan = -(targetWorld * newZoom);
			zoom = newZoom;
		}
		isMarqueeZooming = false;
	}
	isDragging = false;
}

void ofApp::mouseMoved(int x, int y) {
	// Alt/Option scrub — works when key is held (no mouse button needed)
	if (ofGetKeyPressed(OF_KEY_ALT) && selectedPath) {
		if (selectedPath->stepMode && selectedPath->isSequential
			&& !selectedPath->sequentialPoints.empty()) {
			// Snap to discrete step
			int n = (int)selectedPath->sequentialPoints.size();
			int stepIdx = (int)std::clamp(
				ofMap((float)x, 0.f, (float)ofGetWidth(), 0.f, (float)(n - 1)),
				0.f, (float)(n - 1));
			selectedPath->currentStepIndex = stepIdx;
			selectedPath->position = (n > 1) ? (float)stepIdx / (float)(n - 1) : 0.0f;
		} else {
			selectedPath->position = std::clamp((float)x / (float)ofGetWidth(), 0.0f, 1.0f);
			if (selectedPath->isSequential && !selectedPath->sequentialPoints.empty()) {
				int maxIdx = (int)selectedPath->sequentialPoints.size() - 1;
				selectedPath->currentStepIndex = (int)std::clamp(
					ofMap((float)x, 0.f, (float)ofGetWidth(), 0.f, (float)maxIdx),
					0.f, (float)maxIdx);
			}
		}
		// Y axis → volume (bottom=0, top=1) — always
		float prevVol = selectedPath->volume;
		selectedPath->volume = std::clamp(
			ofMap((float)y, (float)ofGetHeight(), 0.f, 0.f, 1.f), 0.f, 1.f);
		if (selectedPath->volume != prevVol) {
			oscManager.sendPathVolume(selectedPath->name, selectedPath->volume);
			oscManager.sendUIPathUpdate(selectedPath->id, selectedPath->isActive,
				selectedPath->radius, selectedPath->direction, selectedPath->sampleNum,
				selectedPath->volume, selectedPath->falloff, selectedPath->speed, selectedPath->mode);
		}
		return;
	}

	// Mouse Scrubbing in Navigate Mode
	/*
	if (currentMode == NAVIGATE) {
		ofVec2f worldPos = screenToWorld(x, y);
		if (spatialGrid) {
			float scrubRadius = 50.0f;
			std::vector<DataPoint> nearby = spatialGrid->findPointsInRadius(worldPos.x, worldPos.y, scrubRadius);
		}
	}
	*/
}
void ofApp::mouseEntered(int x, int y) { }
void ofApp::mouseExited(int x, int y) { }
void ofApp::windowResized(int w, int h) { }
void ofApp::gotMessage(ofMessage msg) { }
void ofApp::dragEvent(ofDragInfo dragInfo) { }

// Helpers
ofVec2f ofApp::screenToWorld(float x, float y) {
	// Inverse transform
	// screen = (world * zoom) + pan + center
	// world * zoom = screen - pan - center
	// world = (screen - pan - center) / zoom

	ofVec2f center(ofGetWidth() / 2, ofGetHeight() / 2);
	return (ofVec2f(x, y) - pan - center) / zoom;
}

//--------------------------------------------------------------
void ofApp::loadPoints(string jsonPath) {
	ofxJSONElement json;
	if (json.open(jsonPath)) {
		lastLoadedPointsPath = jsonPath;
		points.clear();
		paths.clear();
		selectedPath = nullptr;
		oscManager.sendClear();

		ofLogNotice("ofApp::loadPoints") << "Loading points from " << jsonPath;

		// Intelligent Media Path Resolution
		ofFile file(jsonPath);
		string dir = file.getEnclosingDirectory();
		ofDirectory segDir(dir + "segments");
		if (segDir.exists()) {
			mediaRoot = segDir.getAbsolutePath() + "/";
			ofLogNotice("ofApp::loadPoints") << "Found segments directory. Media Root: " << mediaRoot;
		} else {
			mediaRoot = ""; // Assume bin/data or relative
			ofLogWarning("ofApp::loadPoints") << "Segments directory not found at " << dir << "segments";
		}

		float minX = std::numeric_limits<float>::max();
		float maxX = std::numeric_limits<float>::lowest();
		float minY = std::numeric_limits<float>::max();
		float maxY = std::numeric_limits<float>::lowest();

		for (int i = 0; i < json.size(); ++i) {
			float x = json[i]["x"].asFloat();
			float y = json[i]["y"].asFloat();
			string file = json[i].isMember("file") ? json[i]["file"].asString() : (json[i].isMember("filename") ? json[i]["filename"].asString() : "");
			string text = json[i].isMember("text") ? json[i]["text"].asString() : "";

			points.emplace_back(x, y, file, text);

			if (x < minX) minX = x;
			if (x > maxX) maxX = x;
			if (y < minY) minY = y;
			if (y > maxY) maxY = y;
		}

		ofLogNotice("ofApp::loadPoints") << "Loaded " << points.size() << " points.";
		ofLogNotice("ofApp::loadPoints") << "Bounds: [" << minX << ", " << minY << "] to [" << maxX << ", " << maxY << "]";

		// Auto-frame
		if (!points.empty()) {
			float dataWidth = maxX - minX;
			float dataHeight = maxY - minY;

			// If points are all at 0,0 or very generic, don't zoom weirdly
			if (dataWidth <= 0) dataWidth = 1000;
			if (dataHeight <= 0) dataHeight = 1000;

			// Add padding (10%)
			dataWidth *= 1.2f;
			dataHeight *= 1.2f;

			float screenW = ofGetWidth();
			float screenH = ofGetHeight();

			float zoomX = screenW / dataWidth;
			float zoomY = screenH / dataHeight;
			zoom = std::min(zoomX, zoomY);

			// Center
			float centerX = (minX + maxX) / 2.0f;
			float centerY = (minY + maxY) / 2.0f;

			// Pan logic:
			// screenCenter = (worldCenter * zoom) + pan + centerOffset
			// We want worldCenter to be at screen center (centerOffset)
			// centerOffset = (worldCenter * zoom) + pan + centerOffset
			// 0 = (worldCenter * zoom) + pan
			// pan = -(worldCenter * zoom)

			// Wait, my screenToWorld logic:
			// screen = (world * zoom) + pan + center
			// If we want world point (centerX, centerY) to be at screen center (width/2, height/2):
			// center = (centerWorld * zoom) + pan + center
			// 0 = (centerWorld * zoom) + pan
			// pan = -(centerWorld * zoom)

			pan.set(-centerX * zoom, -centerY * zoom);

			ofLogNotice("ofApp::loadPoints") << "Auto-framed. Zoom: " << zoom << " Pan: " << pan;
		}

		// Rebuild grid
		spatialGrid = std::make_shared<SpatialGrid>(points, 50.0f / zoom); // Adjust grid cell size based on zoom? Or keep world units?
		// SpatialGrid uses world units. 50.0f is arbitrary. Let's keep it 50 or adapt to density.
	} else {
		ofLogError("ofApp::loadPoints") << "Failed to open JSON file: " << jsonPath;
	}
}

void ofApp::sendOscMessage(string addr, float val) {
	ofxOscMessage m;
	m.setAddress(addr);
	m.addFloatArg(val);
	oscManager.sendMessage(m);
}

void ofApp::triggerVideo(const DataPoint & p) {
	// Rate limited video switching
	long now = ofGetElapsedTimeMillis();
	if (now - lastVideoSwitchTime > 50) { // 50ms limit
		lastVideoSwitchTime = now;

		string videoPath = "";
		string baseName = ofFilePath::removeExt(p.filename);

		if (mediaRoot != "") {
			string nestedPath = mediaRoot + "video_segments/" + baseName + ".mp4";
			lastAttemptedVideoPath = nestedPath;
			if (ofFile(nestedPath).exists()) {
				videoPath = nestedPath;
			} else {
				lastAttemptedVideoPath += " (Missing)";
				videoPath = mediaRoot + baseName + ".mp4";
			}
		} else {
			videoPath = baseName + ".mp4";
			lastAttemptedVideoPath = videoPath;
		}

		ofFile vFile(videoPath);
		string currentPath = videoFront->isLoaded() ? videoFront->getMoviePath() : "";
		if (vFile.exists() && currentPath != videoPath) {
			// Swap: old front keeps running as back — no reload, no flash
			std::swap(videoFront, videoBack);
			videoFront->load(videoPath);
			videoFront->setLoopState(OF_LOOP_NORMAL); // always loop so clip never stops
			videoFront->play();
			videoFront->setVolume(0);
			videoAlpha = 0.0f;
			videoAlphaTarget = 255.0f;
		}
	}
}

void ofApp::sendFullUIUpdate(std::shared_ptr<PathObject> p) {
	oscManager.sendUIPathFullUpdate(
		p->id, p->isActive, p->radius, p->direction, p->sampleNum,
		p->volume, p->falloff, p->speed, p->mode,
		// granular
		p->gRateMin, p->gRateMax,
		p->gDurMin, p->gDurMax,
		p->gGrainRateMin, p->gGrainRateMax,
		p->gPosMin, p->gPosMax,
		p->gRand, p->granularMode, p->gEnv,
		// effects
		p->fxEnabled, p->fxReverb, p->fxDelay,
		p->fxDistortion, p->fxCompressor, p->fxFilter,
		// pulser
		p->pulserRateMin, p->pulserRateMax, p->pulserRateRand,
		p->pulserAttack, p->pulserRelease, p->pulserMix);
}

// ------------------------------------------------------------------
// createWanderingPath
// Ported directly from Processing's createWanderingPath():
//   - start: DataPoint where the path begins
//   - maxPoints: maximum number of points to visit
//   - randomness: 0=always closest; 1=always random from pool
//   - numNeighbors: size of the candidate pool to pick from
// ------------------------------------------------------------------
std::shared_ptr<PathObject> ofApp::createWanderingPath(
	const DataPoint & start, int maxPoints, float randomness, int numNeighbors) {
	auto newPath = std::make_shared<PathObject>(pathIdCounter++);

	// Add start directly to the polyline as a straight-line segment
	newPath->polyline.addVertex(start.x, start.y, 0);

	// Track visited; track current position as floats
	std::unordered_set<DataPoint> visited;
	visited.insert(start);
	float cx = start.x, cy = start.y;

	for (int i = 0; i < maxPoints - 1; ++i) {
		// Build list of unvisited candidates with their squared distances
		std::vector<std::pair<float, const DataPoint *>> candidates;
		candidates.reserve(points.size());
		for (const auto & p : points) {
			if (visited.find(p) == visited.end()) {
				float dx = p.x - cx, dy = p.y - cy;
				candidates.push_back({ dx * dx + dy * dy, &p });
			}
		}
		if (candidates.empty()) break;

		// Sort closest first
		std::sort(candidates.begin(), candidates.end(),
			[](const auto & a, const auto & b) { return a.first < b.first; });

		// Choose next with randomness
		int poolSize = std::min(numNeighbors, (int)candidates.size());
		const DataPoint * next;
		if (ofRandom(1.0f) < randomness) {
			int idx = (int)ofRandom((float)poolSize);
			next = candidates[std::min(idx, poolSize - 1)].second;
		} else {
			next = candidates[0].second;
		}

		// Add straight-line vertex to path
		newPath->polyline.addVertex(next->x, next->y, 0);
		visited.insert(*next);
		cx = next->x;
		cy = next->y;
	}

	return newPath;
}

//--------------------------------------------------------------
void ofApp::saveComposition(string filepath) {
	ofxJSONElement root;

	// 1. Data Source
	root["points_file"] = lastLoadedPointsPath;

	// 2. Settings / Camera
	root["settings"]["zoom"] = zoom;
	root["settings"]["pan_x"] = pan.x;
	root["settings"]["pan_y"] = pan.y;

	// 3. Paths
	ofxJSONElement pathsArray;
	for (int i = 0; i < paths.size(); ++i) {
		auto & p = paths[i];
		ofxJSONElement pathJson;
		pathJson["name"] = p->name;
		pathJson["is_active"] = p->isActive;
		pathJson["radius"] = p->radius;
		pathJson["speed"] = p->speed;
		pathJson["volume"] = p->volume;
		pathJson["sample_num"] = p->sampleNum;
		pathJson["direction"] = p->direction;
		pathJson["falloff"] = p->falloff;
		pathJson["mode"] = p->mode;
		pathJson["is_sequential"] = p->isSequential;

		// Save control points (the actual drawn/generated vertices)
		ofxJSONElement cpArray;
		for (int j = 0; j < p->controlPoints.size(); ++j) {
			ofxJSONElement cp;
			cp["x"] = p->controlPoints[j].x;
			cp["y"] = p->controlPoints[j].y;
			cpArray.append(cp);
		}
		pathJson["control_points"] = cpArray;

		// Save specific sequential points if applicable
		if (p->isSequential) {
			ofxJSONElement seqArray;
			for (int j = 0; j < p->sequentialPoints.size(); ++j) {
				ofxJSONElement sq;
				sq["x"] = p->sequentialPoints[j].x;
				sq["y"] = p->sequentialPoints[j].y;
				sq["filename"] = p->sequentialPoints[j].filename;
				seqArray.append(sq);
			}
			pathJson["sequential_points"] = seqArray;
		}

		pathsArray.append(pathJson);
	}
	root["paths"] = pathsArray;

	// Save to disk
	bool success = root.save(filepath, true);
	if (success) {
		ofLogNotice("ofApp::saveComposition") << "Successfully saved composition to " << filepath;
	} else {
		ofLogError("ofApp::saveComposition") << "Failed to save composition to " << filepath;
	}
}

//--------------------------------------------------------------
void ofApp::loadCompositionOrPoints(string filepath) {
	ofxJSONElement root;
	if (!root.open(filepath)) {
		ofLogError("ofApp::loadCompositionOrPoints") << "Failed to open JSON file: " << filepath;
		return;
	}

	// 1. Check if this is a composition by looking if it's an object containing "points_file"
	if (root.isObject() && root.isMember("points_file")) {
		ofLogNotice("ofApp::loadCompositionOrPoints") << "Loading composition from " << filepath;

		// 1. Load the underlying data points
		string targetPointsFile = root["points_file"].asString();
		loadPoints(targetPointsFile);

		// 2. Clear existing paths and selection
		paths.clear();
		selectedPath = nullptr;
		oscManager.sendClear();

		// 3. Restore settings
		if (root.isMember("settings")) {
			zoom = root["settings"]["zoom"].asFloat();
			pan.set(root["settings"]["pan_x"].asFloat(), root["settings"]["pan_y"].asFloat());
		}

		// 4. Restore paths
		if (root.isMember("paths") && root["paths"].isArray()) {
			const ofxJSONElement & jsonPaths = root["paths"];
			for (int i = 0; i < jsonPaths.size(); ++i) {
				const ofxJSONElement & pJson = jsonPaths[i];

				// Reconstruct PathObject
				auto newPath = std::make_shared<PathObject>(pathIdCounter++);
				newPath->name = pJson["name"].asString();
				newPath->isActive = pJson["is_active"].asBool();
				newPath->radius = pJson["radius"].asFloat();
				newPath->speed = pJson["speed"].asFloat();

				// Handle legacy missing properties gracefully
				if (pJson.isMember("volume")) newPath->volume = pJson["volume"].asFloat();
				if (pJson.isMember("sample_num")) newPath->sampleNum = pJson["sample_num"].asInt();
				if (pJson.isMember("direction")) newPath->direction = pJson["direction"].asInt();
				if (pJson.isMember("falloff")) newPath->falloff = pJson["falloff"].asFloat();
				if (pJson.isMember("mode")) newPath->mode = pJson["mode"].asInt();
				if (pJson.isMember("is_sequential")) newPath->isSequential = pJson["is_sequential"].asBool();

				// Reconstruct control points
				if (pJson.isMember("control_points") && pJson["control_points"].isArray()) {
					const ofxJSONElement & cpArray = pJson["control_points"];
					for (int j = 0; j < cpArray.size(); ++j) {
						ofVec2f pt(cpArray[j]["x"].asFloat(), cpArray[j]["y"].asFloat());
						newPath->addPoint(pt);
					}
					if (!newPath->isSequential) { // Normal drawn paths need finalization
						newPath->finalize();
					}
				}

				// Reconstruct sequential points (if applicable)
				if (newPath->isSequential && pJson.isMember("sequential_points") && pJson["sequential_points"].isArray()) {
					const ofxJSONElement & seqArray = pJson["sequential_points"];
					for (int j = 0; j < seqArray.size(); ++j) {
						DataPoint dp;
						dp.x = seqArray[j]["x"].asFloat();
						dp.y = seqArray[j]["y"].asFloat();
						dp.filename = seqArray[j]["filename"].asString();
						newPath->sequentialPoints.push_back(dp);
					}
				}

				paths.push_back(newPath);
				oscManager.sendUIPathAdd(newPath->name);
				sendFullUIUpdate(newPath);
			}
		}

		ofLogNotice("ofApp::loadCompositionOrPoints") << "Composition loaded successfully!";
	} else {
		// 2. Otherwise, treat it as a standard raw points file
		ofLogNotice("ofApp::loadCompositionOrPoints") << "No composition metadata found; treating as raw points file.";
		loadPoints(filepath);
	}
}