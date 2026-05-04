#include "ofApp.h"
#include "ofAppGLFWWindow.h"
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
	p0->mode = defaultPathMode;
	p0->isActive = false; // Not playing by itself
	p0->sendToVideo = false; // Don't trigger videos for browsing/hovering by default
	// User requested path-0 notification
	oscManager.sendUIPathAdd(p0->name);
	paths.push_back(p0);

	pathIdCounter = 1;
	currentMode = NAVIGATE;
	isDrawingPath = false;
	zoom = 1.0f;
	pan.set(0, 0);
	targetZoom = zoom;
	targetPan = pan;
	isViewAnimating = false;
	isDragging = false;
	isMarqueeZooming = false;
	isRecordingGesture = false;
	isDraggingPath = false;
	showVideo = false;
	showText = false;
	showTitle = false;
	compositionTitle = "";
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
	params.add(titleColor.set("Title Color", ofColor(255, 255, 255), ofColor(0, 0), ofColor(255, 255)));
	params.add(debugTextColor.set("Debug Text Color", ofColor(255), ofColor(0, 0), ofColor(255, 255)));

	params.add(pointSize.set("Point Size", 5.0f, 1.0f, 100.0f)); // Increased default to 5.0
	params.add(selectedPointSize.set("Sel Point Size", 8.0f, 1.0f, 30.0f));
	params.add(hoveredPointSize.set("Hover Point Size", 16.0f, 1.0f, 40.0f));
	params.add(fontSize.set("Font Size", 14.0f, 8.0f, 48.0f));
	params.add(annotationFontSize.set("Annotation Font Size", 14.0f, 8.0f, 48.0f));
	params.add(activeFontSize.set("Active Font Size", 14.0f, 8.0f, 48.0f));
	params.add(titleFontSize.set("Title Font Size", 48.0f, 12.0f, 120.0f));
	params.add(gridColor.set("Grid Color", ofColor(120, 140, 170, 70), ofColor(0, 0, 0, 0), ofColor(255, 255, 255, 255)));
	params.add(gridSpacing.set("Grid Spacing", 2.0f, 0.2f, 20.0f));
	params.add(zoomAnimationSpeed.set("Zoom Animation Speed", 0.2f, 0.02f, 1.0f));
	params.add(playheadSize.set("Playhead Size", 5.0f, 1.0f, 200.0f));
	params.add(pathThickness.set("Path Thickness", 1.0f, 0.1f, 20.0f));
	params.add(selectedPathThickness.set("Selected Path Thickness", 2.0f, 0.1f, 20.0f));
	params.add(playheadColor.set("Playhead Color", ofColor(255), ofColor(0, 0), ofColor(255, 255)));
	params.add(videoFitMode.set("Video Fit 0=stretch 1=height 2=width", 0, 0, 2));
	params.add(videoDisplayMode.set("Video Mode 0=single 1=grid 2=blendmix 3=mapped 4=collage", 0, 0, 4));
	params.add(videoFadeSpeed_param.set("Video Fade Speed", 15.0f, 1.0f, 60.0f));
	params.add(cloudTransitionSpeed.set("Cloud Transition Speed", 0.05f, 0.01f, 1.0f));
	params.add(neighbourSeqGapMs_param.set("Neighbour Seq Gap (ms)", 300.0f, 50.0f, 2000.0f));

	// Initialize Grid mode layers
	for (int i = 0; i < (GRID_COLS * GRID_ROWS); i++) {
		gridPlayers.push_back(std::make_shared<ofVideoPlayer>());
	}

	// Initialize Ghost mode pool
	for (int i = 0; i < kGhostLayers; i++) {
		ghostPlayers.push_back(std::make_shared<ofVideoPlayer>());
	}

	// Initialize Mapped mode pool (reused to avoid runtime player churn)
	for (int i = 0; i < kMappedMax; i++) {
		MappedClip mc;
		mc.player = std::make_shared<ofVideoPlayer>();
		mappedPlayers.push_back(mc);
	}

	// Initialize Tile Collage mode layers
	int collageCount = kCollageCols * kCollageRows;
	collagePlayers.reserve(collageCount);
	collageAnglesDeg.assign(collageCount, 0.0f);
	collageOffsetsPx.assign(collageCount, ofVec2f(0, 0));
	for (int i = 0; i < collageCount; i++) {
		collagePlayers.push_back(std::make_shared<ofVideoPlayer>());
	}

	gui.setup(params);
	gui.loadFromFile("settings.xml");

	// Load Font - try to load a system font or default
	bool fontLoaded = font.load("verdana.ttf", fontSize);
	if (!fontLoaded) fontLoaded = font.load(OF_TTF_SANS, fontSize);
	bool annotationFontLoaded = annotationFont.load("verdana.ttf", annotationFontSize);
	if (!annotationFontLoaded) annotationFontLoaded = annotationFont.load(OF_TTF_SANS, annotationFontSize);

	bool activeFontLoaded = activeFont.load("verdana.ttf", activeFontSize);
	if (!activeFontLoaded) activeFontLoaded = activeFont.load(OF_TTF_SANS, activeFontSize);

	bool titleFontLoaded = titleFont.load("verdana.ttf", titleFontSize);
	if (!titleFontLoaded) titleFontLoaded = titleFont.load(OF_TTF_SANS, titleFontSize);

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

	// Annotation system — loadFromFile is deferred to loadPoints() so that
	// annotations are never drawn against an empty point cloud on startup.
	annotationManager.init(&zoom, &pan, &annotationFont);
}

//--------------------------------------------------------------
void ofApp::update() {
	// Fade point/path visuals independently from video playback.
	float fadeStep = ofGetLastFrameTime() / 0.1f;
	if (dataVisualAlpha < targetDataVisualAlpha) {
		dataVisualAlpha = std::min(dataVisualAlpha + fadeStep, targetDataVisualAlpha);
	} else if (dataVisualAlpha > targetDataVisualAlpha) {
		dataVisualAlpha = std::max(dataVisualAlpha - fadeStep, targetDataVisualAlpha);
	}

	// Smoothly animate zoom/pan towards targets.
	if (isViewAnimating) {
		float k = std::clamp((float)zoomAnimationSpeed.get(), 0.0f, 1.0f);
		zoom = ofLerp(zoom, targetZoom, k);
		pan = pan.getInterpolated(targetPan, k);

		if (std::abs(zoom - targetZoom) < 0.0005f && pan.squareDistance(targetPan) < 0.01f) {
			zoom = targetZoom;
			pan = targetPan;
			isViewAnimating = false;
		}
	}

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
			pathToUse->isActive = !pathToUse->isActive;
			if (!pathToUse->isActive) {
				stopPathSamples(pathToUse);
			}
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

	// Cloud transition interpolation
	bool pointsMoved = false;
	for (auto & p : points) {
		ofVec2f target;
		switch (currentCloudMode) {
		case PointCloudMode::LOCAL:
			target = p.pos_local;
			break;
		case PointCloudMode::MID:
			target = p.pos_mid;
			break;
		case PointCloudMode::GLOBAL:
			target = p.pos_global;
			break;
		}

		if (std::abs(p.x - target.x) > 0.001f || std::abs(p.y - target.y) > 0.001f) {
			p.x = ofLerp(p.x, target.x, cloudTransitionSpeed.get());
			p.y = ofLerp(p.y, target.y, cloudTransitionSpeed.get());
			pointsMoved = true;
		} else {
			p.x = target.x;
			p.y = target.y;
		}
	}

	if (pointsMoved && spatialGrid && !points.empty()) {
		// Rebuild the spatial grid because point positions have changed
		spatialGrid = std::make_shared<SpatialGrid>(points, 50.0f / zoom);
	}

	// Navigate Mode Scrubbing
	if (currentMode == NAVIGATE && spatialGrid) {
	}

	// Browse Mode Logic
	if (currentMode == BROWSE && spatialGrid) {
		// While adjusting radius/speed with keyboard+drag, suppress browse triggering.
		if (bHoldingR || bHoldingV) {
			if (hasLastHoveredPoint) {
				oscManager.stopSample(mediaRoot + lastHoveredPoint.filename, "path-0");
				hasLastHoveredPoint = false;
				hasHoveredPoint = false;
			}
		} else {
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

					// Track selected point index for neighbour mode
					if (neighbourModeActive) {
						for (int pi = 0; pi < (int)points.size(); ++pi) {
							if (points[pi].filename == nearestPoint.filename) {
								selectedPointIdx = pi;
								break;
							}
						}
					}

					// If different from last hovered
					if (!hasLastHoveredPoint || !(hoveredPoint == lastHoveredPoint)) {
						if (hasLastHoveredPoint) {
							oscManager.stopSample(mediaRoot + lastHoveredPoint.filename, "path-0");
						}

						// Trigger new
						float vol = 0.5f;
						string mode = "once";
						if (paths.size() > 0) {
							vol = paths[0]->volume;
							mode = paths[0]->getMode();
						}

						oscManager.sendSample(mediaRoot + hoveredPoint.filename, "path-0", vol, mode);
						// possibly trigger video
						if (showVideo) triggerVideo(hoveredPoint);
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
	}

	// Neighbour mode: tick sequential playback queue
	neighbourSeqGapMs = neighbourSeqGapMs_param.get(); // sync from GUI
	if (neighbourSeqPlaying && !neighbourQueue.empty()) {
		uint64_t nowMs = ofGetElapsedTimeMillis();
		if ((nowMs - neighbourSeqLastTriggerMs) >= (uint64_t)neighbourSeqGapMs) {
			if (neighbourSeqIdx < (int)neighbourQueue.size()) {
				int ptIdx = neighbourQueue[neighbourSeqIdx];
				const DataPoint & np = points[ptIdx];
				float baseVol = paths.empty() ? 0.5f : paths[0]->volume;
				float volScale = 1.0f;
				if (neighbourQueueDistances.size() == neighbourQueue.size() && !neighbourQueueDistances.empty()) {
					float minD = *std::min_element(neighbourQueueDistances.begin(), neighbourQueueDistances.end());
					float maxD = *std::max_element(neighbourQueueDistances.begin(), neighbourQueueDistances.end());
					float range = std::max(maxD - minD, 1e-6f);
					float d = neighbourQueueDistances[neighbourSeqIdx];
					float nearWeight = 1.0f - ((d - minD) / range); // near=1, far=0
					nearWeight = std::clamp(nearWeight, 0.0f, 1.0f);
					volScale = ofLerp(0.2f, 1.0f, nearWeight); // far still audible
				}
				float vol = baseVol * volScale;
				string mode = paths.empty() ? "once" : paths[0]->getMode();
				oscManager.sendSample(mediaRoot + np.filename, "path-0", vol, mode);
				// Trigger video if enabled
				if (showVideo) {
					triggerVideo(np);
				}
				// Record for line illumination
				neighbourLastPlayedIdx = ptIdx;
				neighbourLastPlayedMs = nowMs;
				neighbourSeqLastTriggerMs = nowMs;
				++neighbourSeqIdx;
			} else {
				neighbourSeqPlaying = false; // Done
			}
		}
	}

	// Update Paths
	float dt = 1.0f / 60.0f; // Fixed time step for simplicity

	for (auto & path : paths) {
		int prevStepIndex = path->currentStepIndex; // snapshot before update (for stepMode detection)
		path->update(dt);

		float effectiveVolume = path->volume;

		// Audio Sampling Logic
		if (path->isActive && spatialGrid) {
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
								if (showVideo && path->sendToVideo) triggerVideo(curr);
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
							if (r < 0.15f) {
								// 15%: go back one
								nextStepIndex = path->currentStepIndex - 1;
							} else if (r < 0.85f) {
								// 70%: repeat (stay)
								nextStepIndex = path->currentStepIndex;
							} else {
								// 15%: advance one
								nextStepIndex = path->currentStepIndex + 1;
							}
							// Wrap circularly
							nextStepIndex = ((nextStepIndex % totalSteps) + totalSteps) % totalSteps;

							// Force trigger for jitter even if the step index didn't change
							stepChanged = true;
						} else {
							// No boundary crossed this frame
							nextStepIndex = path->currentStepIndex;
							stepChanged = false;
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
						// Stop previous (only if we are moving to a different index, or if it's the same index allow it to overlap or stop/restart)
						// For granular/loops, stopping and restarting the same sample might clip, but for 'once' it acts as a retrigger.
						if (path->currentStepIndex >= 0 && path->currentStepIndex < totalSteps) {
							DataPoint & prev = path->sequentialPoints[path->currentStepIndex];
							// Don't stop it if it's literally the same sample we are about to re-trigger, unless it's ONCE mode where we might want to restart?
							// Actually, to make "stay" audible as a rhythm, we should stop and restart, or just fire a new instance.
							// ofxOscManager stopSample sends an OSC array release. Let's stop the previous explicitly.
							oscManager.stopSample(mediaRoot + prev.filename, path->name);
							path->playingPoints.erase(prev);
						}
						// Play new (or same)
						if (nextStepIndex >= 0 && nextStepIndex < totalSteps) {
							DataPoint & curr = path->sequentialPoints[nextStepIndex];
							oscManager.sendSample(mediaRoot + curr.filename, path->name, effectiveVolume, path->getMode());
							path->playingPoints.insert(curr);
							if (showVideo && path->sendToVideo) triggerVideo(curr);
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

			if (!path->isActive) continue;
			if (path->name == "path-0") continue; // Skip the default browse path

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
						if (path->mode == PathObject::LOOP_MODE || path->mode == PathObject::MIXED_MODE || path->mode == PathObject::ONCE_MODE) {
							// Calculate volume based on distance
							float d = path->getDistanceToPoint(ofVec2f(p.x, p.y));
							float vol = effectiveVolume * (1.0f - (d / path->radius));
							vol = MAX(0, vol);
							oscManager.sendSample(mediaRoot + p.filename, path->name, vol, path->getMode());
						}

						// Trigger Video
						if (showVideo && path->sendToVideo) triggerVideo(p);
					}
				}
			}

			// 3. Continuous Updates for Playing Points
			if (path->mode == PathObject::LOOP_MODE || path->mode == PathObject::MIXED_MODE || path->mode == PathObject::CLOUD_MODE || path->mode == PathObject::ONCE_MODE) {
				for (const auto & p : path->playingPoints) {
					float d = path->getDistanceToPoint(ofVec2f(p.x, p.y));
					float vol = effectiveVolume * (1.0f - (d / path->radius));
					vol = MAX(0, vol);

					if (path->mode == PathObject::LOOP_MODE || path->mode == PathObject::MIXED_MODE || path->mode == PathObject::ONCE_MODE) {
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
		if (videoDisplayMode.get() == 0) {
			videoFront->update();
			videoBack->update();

			// Detect loop to hide gap
			float currentPos = videoFront->getPosition();
			if (currentPos < 0.1f && videoLastPos > 0.8f) {
				// We just looped (either naturally or manually)
				videoIgnoreFrames = 4; // hide video for 4 frames to let hold buffer show
			}
			videoLastPos = currentPos;

			// Hack for AVFoundation loop gap: manually restart video before it hits the absolute end
			// This prevents it from outputting a black frame during its native loop cycle
			if (videoFront->isPlaying() && currentPos > 0.98f) {
				videoFront->setPosition(0.0f);
			}
			// Sync fade speed from GUI param
			videoFadeSpeed = videoFadeSpeed_param.get();
			// Advance crossfade alpha toward target
			if (videoAlpha < videoAlphaTarget)
				videoAlpha = std::min(videoAlpha + videoFadeSpeed, videoAlphaTarget);
			else if (videoAlpha > videoAlphaTarget)
				videoAlpha = std::max(videoAlpha - videoFadeSpeed, videoAlphaTarget);

			if (videoIgnoreFrames > 0) {
				videoIgnoreFrames--;
			}

			// Captures the front player's current frame to the hold FBO.
			// Only fires when a genuinely new frame has been decoded AND the clip is fully faded in AND we aren't hiding a loop gap.
			if (videoFront->isFrameNew() && videoFront->getWidth() > 0 && videoAlpha >= 255.0f && videoIgnoreFrames == 0) {
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
		} else if (videoDisplayMode.get() == 2) {
			// GHOST MODE — update all active ghost layers
			for (auto & gp : ghostPlayers) {
				if (gp->isLoaded() && gp->isPlaying()) gp->update();
			}
		} else if (videoDisplayMode.get() == 3) {
			// MAPPED MODE — update all active mapped clips
			for (auto & mc : mappedPlayers) {
				if (mc.player->isLoaded() && mc.player->isPlaying()) mc.player->update();
			}
		} else if (videoDisplayMode.get() == 4) {
			// COLLAGE MODE
			if (!videoQueue.empty() && !collagePlayers.empty()) {
				string nextVideo = videoQueue.front();
				videoQueue.pop_front();

				int collageCount = (int)collagePlayers.size();
				int tileIdx = (collageWriteCounter * 7) % collageCount; // 7 is coprime with 20
				collageWriteCounter++;

				auto & player = collagePlayers[tileIdx];
				player->stop();
				player->close();
				player->load(nextVideo);
				player->setLoopState(OF_LOOP_NORMAL);
				player->play();
				player->setVolume(0);

				// Slight per-tile randomization for a photographic collage feel.
				collageAnglesDeg[tileIdx] = ofRandom(-5.0f, 5.0f);
				collageOffsetsPx[tileIdx] = ofVec2f(ofRandom(-10.0f, 10.0f), ofRandom(-10.0f, 10.0f));
			}

			for (auto & player : collagePlayers) {
				if (player->isLoaded() && player->isPlaying()) {
					player->update();
				}
			}
		} else {
			// GRID MODE

			// 1. Shift videos when a new one is queued
			if (!videoQueue.empty()) {
				string nextVideo = videoQueue.front();
				videoQueue.pop_front();

				// Retrieve the last player in the array (evicted)
				auto evictedPlayer = gridPlayers.back();
				evictedPlayer->stop();
				evictedPlayer->close();

				// Shift all elements one step back
				for (int i = gridPlayers.size() - 1; i > 0; --i) {
					gridPlayers[i] = gridPlayers[i - 1];
				}

				// Place the evicted (now fresh) player at [0]
				gridPlayers[0] = evictedPlayer;

				// Start the new clip in cell [0]
				gridPlayers[0]->load(nextVideo);
				gridPlayers[0]->setLoopState(OF_LOOP_NORMAL);
				gridPlayers[0]->play();
				gridPlayers[0]->setVolume(0);
			}

			// 2. Continuous updates on all active cell players
			for (auto & player : gridPlayers) {
				if (player->isLoaded() && player->isPlaying()) {
					player->update();
				}
			}
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

	// Update annotations (tracks anchor points each frame)
	annotationManager.update(points);
}

//--------------------------------------------------------------
void ofApp::drawVisuals() {

	// Draw Video Background
	if (showVideo) {
		if (videoDisplayMode.get() == 0) {
			if (videoHoldAllocated) {
				// Paint the last-captured frame as a persistent background.
				// This is always opaque — covers any gap/flicker from the live player.
				ofBackground(backgroundColor);
				ofSetColor(255, 255, 255, 255);
				videoHoldFbo.draw(0, 0, ofGetWidth(), ofGetHeight());
			} else {
				ofBackground(backgroundColor); // before first frame is ever captured
			}

			// Draw the live front player on top as it fades in (0 → 255 on clip switch).
			// If we are currently ignoring frames (to hide a loop gap), we just skip drawing it,
			// letting the frozen `videoHoldFbo` seamlessly fill the gap!
			if (videoFront->isLoaded() && videoFront->getWidth() > 0 && videoIgnoreFrames == 0) {
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
		} else if (videoDisplayMode.get() == 1) {
			// GRID MODE
			ofBackground(backgroundColor);

			float sw = (float)ofGetWidth();
			float sh = (float)ofGetHeight();
			float cellWidth = sw / GRID_COLS;
			float cellHeight = sh / GRID_ROWS;
			float gutter = 2.0f; // 2px uniform gutter

			ofSetColor(255);
			for (size_t i = 0; i < gridPlayers.size(); ++i) {
				auto & player = gridPlayers[i];
				if (player->isLoaded() && player->getWidth() > 0) {
					int col = i % GRID_COLS;
					int row = i / GRID_COLS;

					// Compute bounding box for cell with gutter
					float cx = col * cellWidth + gutter;
					float cy = row * cellHeight + gutter;
					float cw = cellWidth - (gutter * 2);
					float ch = cellHeight - (gutter * 2);

					// Render video based on fit mode
					float vw = player->getWidth(), vh = player->getHeight();
					float x = cx, y = cy, dw = cw, dh = ch;
					int fitMode = videoFitMode.get();

					if (fitMode == 1 && vh > 0) {
						dh = ch;
						dw = (vw / vh) * ch;
						x = cx + (cw - dw) * 0.5f;
						y = cy;
					} else if (fitMode == 2 && vw > 0) {
						dw = cw;
						dh = (vh / vw) * cw;
						x = cx;
						y = cy + (ch - dh) * 0.5f;
					}

					player->draw(x, y, dw, dh);
				}
			}
		} else if (videoDisplayMode.get() == 2) {
			// BLEND MIX MODE
			// A controlled collage stack using alternating blend modes to avoid overexposure.
			ofBackground(backgroundColor);

			int n = (int)ghostPlayers.size();
			for (int i = 0; i < n; ++i) {
				auto & gp = ghostPlayers[i];
				if (!gp->isLoaded() || gp->getWidth() <= 0) continue;

				float vw = gp->getWidth(), vh = gp->getHeight();
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

				if (i == 0) {
					// Newest layer as anchor image.
					ofEnableBlendMode(OF_BLENDMODE_ALPHA);
					ofSetColor(255, 255, 255, 210);
				} else if (i % 3 == 1) {
					ofEnableBlendMode(OF_BLENDMODE_MULTIPLY);
					ofSetColor(255, 255, 255, 110);
				} else if (i % 3 == 2) {
					ofEnableBlendMode(OF_BLENDMODE_SCREEN);
					ofSetColor(255, 255, 255, 70);
				} else {
					// Difference-like accent (subtractive) on occasional deeper layers.
					ofEnableBlendMode(OF_BLENDMODE_SUBTRACT);
					ofSetColor(255, 255, 255, 45);
				}

				gp->draw(x, y, dw, dh);
			}

			ofDisableBlendMode();
			ofSetColor(255);
		} else if (videoDisplayMode.get() == 3) {
			// DATA-MAPPED MODE
			// Each clip plays centred on the screen position of the data point that triggered it.
			ofBackground(backgroundColor);
			ofSetColor(255);
			// Thumbnail height in world units — 15% of visible screen height.
			float worldThumbH = (ofGetHeight() * 0.15f) / std::max(zoom, 0.001f);
			float centerX = ofGetWidth()  * 0.5f;
			float centerY = ofGetHeight() * 0.5f;
			for (auto & mc : mappedPlayers) {
				if (!mc.player->isLoaded() || mc.player->getWidth() <= 0) continue;
				float sx = mc.point.x * zoom + pan.x + centerX;
				float sy = mc.point.y * zoom + pan.y + centerY;
				float vw = mc.player->getWidth(), vh = mc.player->getHeight();
				float aspect = (vh > 0) ? vw / vh : 1.0f;
				float screenH = worldThumbH * zoom;
				float screenW = screenH * aspect;
				mc.player->draw(sx - screenW * 0.5f, sy - screenH * 0.5f, screenW, screenH);
			}
			ofSetColor(255);
		} else if (videoDisplayMode.get() == 4) {
			// TILE COLLAGE MODE
			ofBackground(backgroundColor);

			float sw = (float)ofGetWidth();
			float sh = (float)ofGetHeight();
			float cellWidth = sw / kCollageCols;
			float cellHeight = sh / kCollageRows;
			float gutter = 8.0f;

			for (int i = 0; i < (int)collagePlayers.size(); ++i) {
				auto & player = collagePlayers[i];
				if (!player->isLoaded() || player->getWidth() <= 0) continue;

				int col = i % kCollageCols;
				int row = i / kCollageCols;
				float cx = col * cellWidth + gutter;
				float cy = row * cellHeight + gutter;
				float cw = cellWidth - (gutter * 2.0f);
				float ch = cellHeight - (gutter * 2.0f);

				float centerX = cx + (cw * 0.5f) + collageOffsetsPx[i].x;
				float centerY = cy + (ch * 0.5f) + collageOffsetsPx[i].y;

				float vw = player->getWidth(), vh = player->getHeight();
				float x = -cw * 0.5f, y = -ch * 0.5f, dw = cw, dh = ch;
				int fitMode = videoFitMode.get();
				if (fitMode == 1 && vh > 0) {
					dh = ch;
					dw = (vw / vh) * ch;
					x = -dw * 0.5f;
					y = -dh * 0.5f;
				} else if (fitMode == 2 && vw > 0) {
					dw = cw;
					dh = (vh / vw) * cw;
					x = -dw * 0.5f;
					y = -dh * 0.5f;
				}

				ofPushMatrix();
				ofTranslate(centerX, centerY);
				ofRotateDeg(collageAnglesDeg[i]);
				ofSetColor(255);
				player->draw(x, y, dw, dh);
				// Thin matte border improves collage legibility.
				ofNoFill();
				ofSetColor(235, 235, 235, 180);
				ofSetLineWidth(1.0f);
				ofDrawRectangle(-cw * 0.5f, -ch * 0.5f, cw, ch);
				ofFill();
				ofPopMatrix();
			}
			ofSetColor(255);
		}
	} else {
		ofBackground(backgroundColor);
	}

	ofEnableAlphaBlending();

	// Draw grid above video and below points. We project world-grid lines into
	// screen space so changing spacing is immediately visible.
	{
		float worldSpacing = std::max(0.1f, (float)gridSpacing.get());
		ofColor gc = gridColor.get();
		if (gc.a > 0 && !points.empty()) {
			ofVec2f tl = screenToWorld(0, 0);
			ofVec2f tr = screenToWorld(ofGetWidth(), 0);
			ofVec2f bl = screenToWorld(0, ofGetHeight());
			ofVec2f br = screenToWorld(ofGetWidth(), ofGetHeight());

			float minWX = std::min(std::min(tl.x, tr.x), std::min(bl.x, br.x));
			float maxWX = std::max(std::max(tl.x, tr.x), std::max(bl.x, br.x));
			float minWY = std::min(std::min(tl.y, tr.y), std::min(bl.y, br.y));
			float maxWY = std::max(std::max(tl.y, tr.y), std::max(bl.y, br.y));

			float startWX = std::floor(minWX / worldSpacing) * worldSpacing;
			float endWX = std::ceil(maxWX / worldSpacing) * worldSpacing;
			float startWY = std::floor(minWY / worldSpacing) * worldSpacing;
			float endWY = std::ceil(maxWY / worldSpacing) * worldSpacing;

			ofSetColor(gc);
			ofSetLineWidth(1.0f);
			for (float wx = startWX; wx <= endWX; wx += worldSpacing) {
				float sx = wx * zoom + pan.x + ofGetWidth() * 0.5f;
				ofDrawLine(sx, 0.0f, sx, (float)ofGetHeight());
			}
			for (float wy = startWY; wy <= endWY; wy += worldSpacing) {
				float sy = wy * zoom + pan.y + ofGetHeight() * 0.5f;
				ofDrawLine(0.0f, sy, (float)ofGetWidth(), sy);
			}
		}
	}

	ofPushMatrix();
	ofTranslate(ofGetWidth() / 2, ofGetHeight() / 2);
	ofTranslate(pan);
	ofScale(zoom, zoom);
	float dataAlphaScale = std::clamp(dataVisualAlpha, 0.0f, 1.0f);

	// Draw Points
	ofFill(); // Ensure fill is enabled
	ofColor c = pointColor.get();
	float baseAlpha;
	if (showText) {
		baseAlpha = std::min((float)c.a, 25.0f); // cap opacity so text is readable
	} else {
		baseAlpha = (float)c.a;
	}

	for (const auto & p : points) {
		// Cluster foregrounding: dim non-active cluster points
		float alpha = baseAlpha;
		if (activeClusterId != -999 && p.cluster_id != activeClusterId) {
			alpha = baseAlpha * 0.15f; // 15% of normal
		}
		ofSetColor(c.r, c.g, c.b, (int)(alpha * dataAlphaScale));

		float scale = 1.0f;
		if (currentThirdDimMode != ThirdDimMode::NONE) {
			float val = 0.0f;
			switch (currentThirdDimMode) {
			case ThirdDimMode::INSTABILITY:
				val = p.instability;
				break;
			case ThirdDimMode::ATTACK:
				val = p.attack;
				break;
			case ThirdDimMode::BRIGHTNESS:
				val = p.brightness;
				break;
			default:
				break;
			}
			scale = ofLerp(0.2f, 2.0f, val);
		}
		ofDrawCircle(p.x, p.y, (pointSize / zoom) * scale);
	}

	// --- Neighbour Mode Highlighting ---
	if (neighbourModeActive && selectedPointIdx >= 0 && selectedPointIdx < (int)points.size()) {
		const DataPoint & sel = points[selectedPointIdx];

		// Compute neighbour weights once: weight 1.0 = nearest, 0.0 = furthest
		const auto & neighbours = sel.true_neighbors;
		const auto & distances = sel.true_distances;
		int numNeighbours = (int)std::min(neighbours.size(), distances.size());

		if (numNeighbours > 0) {
			float minDist = *std::min_element(distances.begin(), distances.begin() + numNeighbours);
			float maxDist = *std::max_element(distances.begin(), distances.begin() + numNeighbours);
			float distRange = std::max(maxDist - minDist, 1e-6f);

			uint64_t nowMs = ofGetElapsedTimeMillis();
			uint64_t msSincePlayed = (neighbourLastPlayedIdx >= 0)
				? (nowMs - neighbourLastPlayedMs) : kNeighbourFlashMs + 1;

			ofNoFill();
			ofSetLineWidth(1.5f);

			for (int ni = 0; ni < numNeighbours; ++ni) {
				int idx = neighbours[ni];
				if (idx < 0 || idx >= (int)points.size()) continue;
				const DataPoint & np = points[idx];

				// Proximity weight: nearest=1.0, furthest=0.0
				float weight = 1.0f - ((distances[ni] - minDist) / distRange);
				weight = std::max(0.0f, std::min(1.0f, weight));

				// Base alpha: 20% (dim) to 100% (nearest)
				float baseAlpha = ofLerp(50.0f, 255.0f, weight);

				// Flash: if this point was the last one played, overlay a bright flash
				if (idx == neighbourLastPlayedIdx && msSincePlayed < kNeighbourFlashMs) {
					float flashT = 1.0f - ((float)msSincePlayed / (float)kNeighbourFlashMs);
					// Blend line from flash-white toward the distance-tinted colour
					int flashAlpha = (int)(ofLerp(baseAlpha, 255.0f, flashT) * dataAlphaScale);
					ofSetColor(255, 255, 255, flashAlpha); // bright white flash
				} else {
					// Normal: cool blue-white, opacity = proximity
					ofSetColor(180, 210, 255, (int)(baseAlpha * dataAlphaScale));
				}

				ofDrawLine(sel.x, sel.y, np.x, np.y);
			}

			ofFill();
			ofSetLineWidth(1.0f);
		}

		// Draw selected point indicator: bright ring on top
		ofSetColor(255, 255, 100, (int)(230 * dataAlphaScale)); // bright yellow
		ofNoFill();
		ofSetLineWidth(2.0f);
		ofDrawCircle(sel.x, sel.y, (pointSize / zoom) * 2.5f);
		ofFill();
		ofSetLineWidth(1.0f);
	}

	// Draw Text
	// Draw Paths
	ofColor fadedPlayheadColor = playheadColor.get();
	fadedPlayheadColor.a = (int)(fadedPlayheadColor.a * dataAlphaScale);
	ofColor fadedPathColor = pathColor.get();
	fadedPathColor.a = (int)(fadedPathColor.a * dataAlphaScale);
	ofColor fadedSelectedPathColor = selectedPathColor.get();
	fadedSelectedPathColor.a = (int)(fadedSelectedPathColor.a * dataAlphaScale);
	for (auto & path : paths) {
		path->draw(playheadSize / zoom, fadedPlayheadColor, zoom, pathThickness.get(), selectedPathThickness.get(), fadedPathColor, fadedSelectedPathColor);
	}

	// Draw current path
	if (isDrawingPath && currentPath) {
		ofColor currentPathColor = pathColor.get();
		currentPathColor.a = (int)(currentPathColor.a * dataAlphaScale);
		ofSetColor(currentPathColor);
		ofSetLineWidth(selectedPathThickness.get());
		// Only draw the line, not the playhead circle
		currentPath->polyline.draw();
	}

	ofPopMatrix();

	// Update annotation font size if changed.
	static float lastAnnotationFontSize = annotationFontSize;
	if (std::abs(lastAnnotationFontSize - annotationFontSize.get()) > 0.5f) {
		bool result = annotationFont.load("Futura.ttc", annotationFontSize);
		if (!result) result = annotationFont.load("verdana.ttf", annotationFontSize);
		if (!result) annotationFont.load(OF_TTF_SANS, annotationFontSize);
		lastAnnotationFontSize = annotationFontSize;
	}

	// Draw annotations in screen space (above the point cloud)
	annotationManager.draw(points, activeClusterId);

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
		ofSetColor(phc.r, phc.g, phc.b, (int)(128 * dataAlphaScale)); // 50% opacity
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
				int alpha = (int)(((a.volume * 135) + 120) * dataAlphaScale);
				ofSetColor(100, 255, 150, alpha);
				ofDrawLine(x0, y0, x1, y1);
			}
			ofSetLineWidth(1);
		}
	}

	// Draw Text (Screen Space)
	if (showText) {
		ofColor fadedTextColor = textColor.get();
		fadedTextColor.a = (int)(fadedTextColor.a * dataAlphaScale);
		ofSetColor(fadedTextColor);
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

	// Draw Title (Screen Space)
	if (showTitle && !compositionTitle.empty()) {
		ofSetColor(titleColor);
		if (titleFont.isLoaded()) {
			// Update font size if changed
			static float lastTitleFontSize = titleFontSize;
			if (abs(lastTitleFontSize - titleFontSize) > 0.5f) {
				bool result = titleFont.load("Futura.ttc", titleFontSize);
				if (!result) result = titleFont.load("verdana.ttf", titleFontSize);
				if (!result) titleFont.load(OF_TTF_SANS, titleFontSize);
				lastTitleFontSize = titleFontSize;
			}

			ofRectangle bounds = titleFont.getStringBoundingBox(compositionTitle, 0, 0);
			float x = (ofGetWidth() - bounds.width) / 2.0f;
			float y = (ofGetHeight() - bounds.height) / 2.0f + bounds.height;

			// Optional drop shadow
			ofSetColor(0, 0, 0, 150);
			titleFont.drawString(compositionTitle, x + 2, y + 2);
			ofSetColor(titleColor);

			titleFont.drawString(compositionTitle, x, y);
		} else {
			float strWidth = compositionTitle.length() * 8.0f;
			float x = (ofGetWidth() - strWidth) / 2.0f;
			float y = ofGetHeight() / 2.0f;

			ofSetColor(0, 0, 0, 150);
			ofDrawBitmapString(compositionTitle, x + 2, y + 2);
			ofSetColor(titleColor);

			ofDrawBitmapString(compositionTitle, x, y);
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
	ofSetColor(debugTextColor.get());
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
	string dimStr = "NONE";
	switch (currentThirdDimMode) {
	case ThirdDimMode::INSTABILITY:
		dimStr = "INSTABILITY";
		break;
	case ThirdDimMode::ATTACK:
		dimStr = "ATTACK";
		break;
	case ThirdDimMode::BRIGHTNESS:
		dimStr = "SPECTRAL CENTROID";
		break;
	default:
		break;
	}
	ofDrawBitmapString("3D Dim: " + dimStr, 20, 80);
	ofDrawBitmapString("Audio Mode: " + string(defaultPathMode == PathObject::LOOP_MODE ? "LOOP" : "ONCE"), 20, 100);
	{
		string nbStr = neighbourModeActive ? "ON" : "OFF";
		if (neighbourModeActive && selectedPointIdx >= 0 && selectedPointIdx < (int)points.size()) {
			nbStr += " | " + ofFilePath::getFileName(points[selectedPointIdx].filename);
		}
		ofDrawBitmapString("Neighbour Mode (N): " + nbStr, 20, 120);
	}

	int textY = 140;
	if (selectedPath) {
		ofDrawBitmapString("Selected: " + selectedPath->name + " - Video: " + (selectedPath->sendToVideo ? "ON" : "OFF"), 20, textY);
		textY += 20;
	}

	ofDrawBitmapString("Video Mode (m): " + ofToString(showVideo ? "ON" : "OFF"), 20, textY);
	textY += 20;
	ofDrawBitmapString("Video Trigger (;): " + string(videoTriggerLocked ? "LOCKED" : "UNLOCKED"), 20, textY);
	textY += 20;
	if (showVideo) {
		ofDrawBitmapString("Media Root: " + mediaRoot, 20, textY);
		textY += 20;
		ofDrawBitmapString("Loaded Video: " + ofToString(videoFront->getMoviePath()), 20, textY);
		textY += 20;
		ofDrawBitmapString("Last Attempted: " + lastAttemptedVideoPath, 20, textY);
		textY += 20;
		ofDrawBitmapString("Video Playing: " + ofToString(videoFront->isPlaying()), 20, textY);
		textY += 20;
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
		ofDrawBitmapString("  + / - Increase / decrease sample layer count", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  r / R Increase / decrease catchment radius", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  v / V Increase / decrease playback volume", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("--- Path Types & Modes ---", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  q     Create Sequential path from points", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  e     Toggle step mode (Sequential paths)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  j     Toggle jitter mode (probabilistic advance)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  l     Toggle default path mode (LOOP/ONCE)", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("--- Scrub & Gesture ---", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Alt+move    Scrub position (X) & volume (Y)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Alt+drag    Record gesture (position+volume)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  g           Clear gesture on selected path", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  v+drag      Adjust volume on browse or selected path", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  r+drag      Adjust radius on selected path", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("--- View ---", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  z     Zoom to fit all points", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Shift+drag  Marquee zoom", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  m       Toggle video view", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Shift+m Toggle video triggering on selected path", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  ;       Lock/Unlock new video triggers", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  t       Toggle text labels", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  a       Fade points and paths in/out", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  d     Toggle debug info", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  ,     Toggle settings panel", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("  p     Toggle secondary projector fullscreen Window", lx, ly);
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
		ofDrawBitmapString("  s     Save current Composition to JSON", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  o     Load Points or Composition from JSON", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("--- Annotations ---", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Tab       Toggle annotation mode (draw callout labels)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Shift+Tab Toggle annotation visibility", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  0         Pull all annotation labels inside point bounds", lx, ly);
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
	annotationManager.saveToFile("annotations.json");
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	inputManager.updateModifiers(key, true);

	// Annotation system gets first crack — when typing, it consumes all keys
	if (annotationManager.onKeyPressed(key, points)) return;

	// Pull all labels inside point bounds
	if (key == '0') {
		annotationManager.clampAllLabelsToPointsBounds(points);
		ofLogNotice("AnnotationManager") << "Pulled annotation labels inside point bounds.";
		return;
	}

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
			if (!selectedPath->isActive) {
				stopPathSamples(selectedPath);
			}
		}
		break;

	case CMD_TOGGLE_GLOBAL_PLAYBACK: // Return/Enter
	{
		bool anyPlaying = false;
		for (auto & p : paths) {
			if (p->isActive) {
				anyPlaying = true;
				break;
			}
		}

		for (auto & p : paths) {
			if (anyPlaying) {
				p->isActive = false;
				stopPathSamples(p);
			} else {
				p->isActive = true;
			}
		}
	} break;

	case CMD_LOAD_POINTS: // 'o'
	{
		ofFileDialogResult result = ofSystemLoadDialog("Select JSON Data or Composition File");
		if (result.bSuccess) {
			loadCompositionOrPoints(result.getPath());
		}
	} break;

	case CMD_SAVE_COMPOSITION: // 's'
	{
		string newTitle = ofSystemTextBoxDialog("Enter Composition Title", compositionTitle);
		// If user cancels, it might return empty or the original. We will assume if they entered something we use it.
		// Actually, ofSystemTextBoxDialog returns the entered string, or empty if cancelled/empty.
		compositionTitle = newTitle;
		ofFileDialogResult result = ofSystemSaveDialog("konvolute-01.json", "Save Konvolute As");
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
			// Check if the window is actually still open and valid
			auto glfwWin = std::dynamic_pointer_cast<ofAppGLFWWindow>(projectorWindow);
			if (glfwWin && glfwWin->getGLFWWindow() != nullptr && !glfwWin->getWindowShouldClose()) {
				projectorWindow->toggleFullscreen();
			} else {
				// Window was closed by the user, clean up the pointer and recreate it
				ofRemoveListener(projectorWindow->events().draw, this, &ofApp::drawProjector);
				projectorWindow.reset();
			}
		}

		if (!projectorWindow) {
			// Create the projector window dynamically
			ofGLFWWindowSettings settings;
			settings.setSize(1024, 768);
			settings.setPosition(ofVec2f(1500, 100)); // Default offset for secondary display
			settings.resizable = true;
			settings.shareContextWith = mainWindow;

			projectorWindow = ofCreateWindow(settings);
			ofAddListener(projectorWindow->events().draw, this, &ofApp::drawProjector);
		}
		break;

	case CMD_TOGGLE_TEXT:
		showText = !showText;
		break;

	case CMD_TOGGLE_TITLE:
		showTitle = !showTitle;
		break;

	case CMD_TOGGLE_DATA_VISIBILITY:
		targetDataVisualAlpha = (targetDataVisualAlpha > 0.5f) ? 0.0f : 1.0f;
		break;

	case CMD_TOGGLE_PINGPONG:
		if (selectedPath) {
			selectedPath->isPingPong = !selectedPath->isPingPong;
		} else if (currentPath) {
			currentPath->isPingPong = !currentPath->isPingPong;
		}
		break;

	case CMD_TOGGLE_JITTER:
		if (selectedPath) {
			selectedPath->jitterMode = !selectedPath->jitterMode;
			ofLogNotice("ofApp") << "Jitter mode for path " << selectedPath->name << " set to: " << (selectedPath->jitterMode ? "ON" : "OFF");
		}
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

	case CMD_CLOUD_LOCAL:
		currentCloudMode = PointCloudMode::LOCAL;
		ofLogNotice("ofApp") << "Switched to LOCAL point cloud";
		annotationManager.refreshAnchors(points);
		break;

	case CMD_CLOUD_MID:
		currentCloudMode = PointCloudMode::MID;
		ofLogNotice("ofApp") << "Switched to MID point cloud";
		annotationManager.refreshAnchors(points);
		break;

	case CMD_CLOUD_GLOBAL:
		currentCloudMode = PointCloudMode::GLOBAL;
		ofLogNotice("ofApp") << "Switched to GLOBAL point cloud";
		annotationManager.refreshAnchors(points);
		break;

	case CMD_CYCLE_THIRD_DIM:
		switch (currentThirdDimMode) {
		case ThirdDimMode::NONE:
			currentThirdDimMode = ThirdDimMode::INSTABILITY;
			ofLogNotice("ofApp") << "Third Dim: INSTABILITY";
			break;
		case ThirdDimMode::INSTABILITY:
			currentThirdDimMode = ThirdDimMode::ATTACK;
			ofLogNotice("ofApp") << "Third Dim: ATTACK";
			break;
		case ThirdDimMode::ATTACK:
			currentThirdDimMode = ThirdDimMode::BRIGHTNESS;
			ofLogNotice("ofApp") << "Third Dim: BRIGHTNESS";
			break;
		case ThirdDimMode::BRIGHTNESS:
			currentThirdDimMode = ThirdDimMode::NONE;
			ofLogNotice("ofApp") << "Third Dim: NONE";
			break;
		}
		break;

	case CMD_CLUSTER_PREV:
		if (!sortedClusterIds.empty()) {
			if (activeClusterId == -999) {
				activeClusterId = sortedClusterIds.back();
			} else {
				auto it = std::find(sortedClusterIds.begin(), sortedClusterIds.end(), activeClusterId);
				if (it == sortedClusterIds.end() || it == sortedClusterIds.begin()) {
					activeClusterId = sortedClusterIds.back();
				} else {
					--it;
					activeClusterId = *it;
				}
			}
			std::string label = (clusters.count(activeClusterId) && !clusters[activeClusterId].label.empty())
				? clusters[activeClusterId].label : "id " + ofToString(activeClusterId);
			ofLogNotice("ofApp") << "Cluster filter: " << label;
		}
		break;

	case CMD_CLUSTER_NEXT:
		if (!sortedClusterIds.empty()) {
			if (activeClusterId == -999) {
				activeClusterId = sortedClusterIds.front();
			} else {
				auto it = std::find(sortedClusterIds.begin(), sortedClusterIds.end(), activeClusterId);
				if (it == sortedClusterIds.end() || std::next(it) == sortedClusterIds.end()) {
					activeClusterId = sortedClusterIds.front();
				} else {
					++it;
					activeClusterId = *it;
				}
			}
			std::string label = (clusters.count(activeClusterId) && !clusters[activeClusterId].label.empty())
				? clusters[activeClusterId].label : "id " + ofToString(activeClusterId);
			ofLogNotice("ofApp") << "Cluster filter: " << label;
		}
		break;

	case CMD_CLUSTER_CLEAR:
		activeClusterId = -999;
		ofLogNotice("ofApp") << "Cluster filter cleared";
		break;

	case CMD_TOGGLE_NEIGHBOUR:
		neighbourModeActive = !neighbourModeActive;
		if (!neighbourModeActive) {
			selectedPointIdx = -1;
			neighbourSeqPlaying = false;
			neighbourQueue.clear();
			neighbourQueueDistances.clear();
			neighbourLastPlayedIdx = -1;
			neighbourLastPlayedMs = 0;
		}
		ofLogNotice("ofApp") << "Neighbour mode: " << (neighbourModeActive ? "ON" : "OFF");
		break;

	case CMD_NEIGHBOUR_PLAY_SEQ:
		if (!neighbourModeActive) {
			// Fall through to title toggle when neighbour mode is off
			showTitle = !showTitle;
		} else if (selectedPointIdx >= 0 && selectedPointIdx < (int)points.size()) {
			// Build queue sorted by ascending distance
			const DataPoint & sel = points[selectedPointIdx];
			int numN = (int)std::min(sel.true_neighbors.size(), sel.true_distances.size());
			// Create index-distance pairs, sort by distance
			std::vector<std::pair<float, int>> distIdx;
			distIdx.reserve(numN);
			for (int ni = 0; ni < numN; ++ni) {
				distIdx.push_back({sel.true_distances[ni], sel.true_neighbors[ni]});
			}
			std::sort(distIdx.begin(), distIdx.end());
			neighbourQueue.clear();
			neighbourQueueDistances.clear();
			for (auto & di : distIdx) {
				if (di.second >= 0 && di.second < (int)points.size()) {
					neighbourQueue.push_back(di.second);
					neighbourQueueDistances.push_back(di.first);
				}
			}
			neighbourSeqIdx = 0;
			neighbourSeqLastTriggerMs = 0; // Trigger immediately on first tick
			neighbourSeqPlaying = true;
			ofLogNotice("ofApp") << "Neighbour seq play: " << neighbourQueue.size() << " neighbours";
		}
		break;

	case CMD_NEIGHBOUR_PLAY_ALL:
		if (!neighbourModeActive) {
			// No previous binding for u/U — do nothing
		} else if (selectedPointIdx >= 0 && selectedPointIdx < (int)points.size()) {
			const DataPoint & sel = points[selectedPointIdx];
			float baseVol = paths.empty() ? 0.5f : paths[0]->volume;
			string mode = paths.empty() ? "once" : paths[0]->getMode();
			int numN = (int)std::min(sel.true_neighbors.size(), sel.true_distances.size());

			float minD = std::numeric_limits<float>::max();
			float maxD = std::numeric_limits<float>::lowest();
			for (int ni = 0; ni < numN; ++ni) {
				int idx = sel.true_neighbors[ni];
				if (idx >= 0 && idx < (int)points.size()) {
					float d = sel.true_distances[ni];
					minD = std::min(minD, d);
					maxD = std::max(maxD, d);
				}
			}
			float range = std::max(maxD - minD, 1e-6f);

			for (int ni = 0; ni < numN; ++ni) {
				int idx = sel.true_neighbors[ni];
				if (idx >= 0 && idx < (int)points.size()) {
					float d = sel.true_distances[ni];
					float nearWeight = 1.0f - ((d - minD) / range); // near=1, far=0
					nearWeight = std::clamp(nearWeight, 0.0f, 1.0f);
					float volScale = ofLerp(0.2f, 1.0f, nearWeight);
					float vol = baseVol * volScale;
					oscManager.sendSample(mediaRoot + points[idx].filename, "path-0", vol, mode);
				}
			}
			ofLogNotice("ofApp") << "Neighbour play all: fired " << numN << " samples";
		}
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
	{
		auto targetPath = selectedPath ? selectedPath : (!paths.empty() ? paths[0] : nullptr);
		if (targetPath) {
			targetPath->volume -= 0.1f;
			if (targetPath->volume < 0.0f) targetPath->volume = 0.0f;
		}
	} break;

	case CMD_TOGGLE_PATH_VIDEO:
		if (selectedPath) {
			selectedPath->sendToVideo = !selectedPath->sendToVideo;
			ofLogNotice("ofApp") << "Selected path " << selectedPath->name << " video routing set to: " << (selectedPath->sendToVideo ? "ON" : "OFF");
		} else {
			// Find path-0 and toggle it
			for (auto & p : paths) {
				if (p->name == "path-0") {
					p->sendToVideo = !p->sendToVideo;
					ofLogNotice("ofApp") << "Browse path " << p->name << " video routing set to: " << (p->sendToVideo ? "ON" : "OFF");
					break;
				}
			}
		}
		break;

	case CMD_TOGGLE_VIDEO_LOCK: // ';'
		videoTriggerLocked = !videoTriggerLocked;
		ofLogNotice("ofApp") << "Video trigger locked set to: " << (videoTriggerLocked ? "ON" : "OFF");
		break;

	case CMD_DESELECT_PATH: // 'c'
		selectedPath = nullptr;
		break;

	case CMD_DELETE_PATH:
		if (selectedPath) {
			// Stop playing samples
			stopPathSamples(selectedPath);

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
		newPath->mode = defaultPathMode;
		newPath->isSequential = true;
		newPath->radius = 1.0f; // Minimal radius as we use step logic

		// Sort point pointers
		std::vector<const DataPoint *> sortedPtrs;
		sortedPtrs.reserve(points.size());
		for (const auto & p : points)
			sortedPtrs.push_back(&p);

		std::sort(sortedPtrs.begin(), sortedPtrs.end(), [](const DataPoint * a, const DataPoint * b) {
			if (a->filename.empty() && b->filename.empty()) return false;
			if (a->filename.empty()) return true;
			if (b->filename.empty()) return false;
			return naturalCompare(a->filename, b->filename) < 0;
		});

		newPath->attachedPoints = sortedPtrs;

		// Build polyline as straight lines — no smoothing for sequential paths
		for (const auto * p : sortedPtrs) {
			newPath->sequentialPoints.push_back(*p);
			newPath->controlPoints.push_back(ofVec2f(p->x, p->y));
			newPath->polyline.addVertex(p->x, p->y, 0);
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

		// Restore path-0
		{
			auto p0 = std::make_shared<PathObject>(0);
			p0->name = "path-0";
			p0->mode = defaultPathMode;
			p0->isActive = false;
			p0->sendToVideo = false;
			oscManager.sendUIPathAdd(p0->name);
			paths.push_back(p0);
		}
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

			// Include annotation anchors and label boxes only when annotations
			// are visible. Hidden annotations should not affect zoom-to-extents.
			if (annotationManager.isVisible()) {
				annotationManager.expandWorldBounds(minX, minY, maxX, maxY);
			}

			float dataWidth = (maxX - minX) * 1.2f;
			float dataHeight = (maxY - minY) * 1.2f;
			if (dataWidth <= 0) dataWidth = 1000;
			if (dataHeight <= 0) dataHeight = 1000;
			float newZoom = std::min(ofGetWidth() / dataWidth, ofGetHeight() / dataHeight);
			float centerX = (minX + maxX) / 2.0f;
			float centerY = (minY + maxY) / 2.0f;
			setViewTarget(newZoom, ofVec2f(-centerX * newZoom, -centerY * newZoom), true);
		} else {
			setViewTarget(1.0f, ofVec2f(0, 0), true);
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

	case CMD_TOGGLE_DEFAULT_MODE:
		if (defaultPathMode == PathObject::ONCE_MODE) {
			defaultPathMode = PathObject::LOOP_MODE;
		} else {
			defaultPathMode = PathObject::ONCE_MODE;
		}
		if (!paths.empty()) {
			paths[0]->mode = defaultPathMode;
			oscManager.sendUIPathUpdate(paths[0]->id, paths[0]->isActive,
				paths[0]->radius, paths[0]->direction, paths[0]->sampleNum,
				paths[0]->volume, paths[0]->falloff, paths[0]->speed, paths[0]->mode);
		}
		ofLogNotice("ofApp") << "Default Path Mode toggled to: " << (defaultPathMode == PathObject::LOOP_MODE ? "LOOP" : "ONCE");
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
		if (!ofGetKeyPressed(OF_KEY_SHIFT)) {
			auto targetPath = selectedPath ? selectedPath : (!paths.empty() ? paths[0] : nullptr);
			if (targetPath) {
				if (key == OF_KEY_UP) {
					targetPath->speed += 0.1f;
				} else if (key == OF_KEY_DOWN) {
					targetPath->speed -= 0.1f;
					if (targetPath->speed < 0.0f) targetPath->speed = 0.0f;
				}
			}
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
	// Forward to annotation system first
	if (annotationManager.onMousePressed(glm::vec2(x, y), button)) return;

	// While adjusting radius/speed via drag, consume press and block mode actions.
	if (bHoldingR || bHoldingV) {
		lastMouse.set(x, y);
		isDragging = false;
		isDraggingPath = false;
		isMarqueeZooming = false;
		return;
	}

	ofVec2f worldPos = screenToWorld(x, y);

	if (currentMode == DRAW_FREEHAND) {
		if (!isDrawingPath) {
			currentPath = std::make_shared<PathObject>(pathIdCounter++);
			currentPath->mode = defaultPathMode;
			isDrawingPath = true;
		}
		currentPath->addPoint(worldPos);
	} else {
		// Alt+click in any mode: start gesture recording on selected path
		if (ofGetKeyPressed(OF_KEY_ALT) && selectedPath) {
			selectedPath->gesturePoints.clear();
			selectedPath->hasGesture = false;
			selectedPath->gestureStartTime = ofGetElapsedTimeMillis();
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
	// Forward to annotation system
	if (annotationManager.onMouseDragged(glm::vec2(x, y),
	    glm::vec2(x - lastMouse.x, y - lastMouse.y))) {
		lastMouse.set(x, y);
		return;
	}

	if (bHoldingR || bHoldingV) {
		auto targetPath = selectedPath ? selectedPath : (!paths.empty() ? paths[0] : nullptr);

		// Compute point-cloud extents diagonal in world units.
		float diag = 0.0f;
		if (!points.empty()) {
			float minX = std::numeric_limits<float>::max();
			float maxX = std::numeric_limits<float>::lowest();
			float minY = std::numeric_limits<float>::max();
			float maxY = std::numeric_limits<float>::lowest();
			for (const auto & p : points) {
				minX = std::min(minX, p.x);
				maxX = std::max(maxX, p.x);
				minY = std::min(minY, p.y);
				maxY = std::max(maxY, p.y);
			}
			diag = ofVec2f(maxX - minX, maxY - minY).length();
		}
		if (diag <= 0.0f) {
			lastMouse.set(x, y);
			return;
		}

		// Bottom of screen is minimum, top is maximum.
		float t = std::clamp(1.0f - ((float)y / std::max(1.0f, (float)ofGetHeight())), 0.0f, 1.0f);

		if (targetPath) {
			if (bHoldingR) {
				// Radius uses log interpolation for better control at small values.
				// Max diameter is extents diagonal, so max radius is half of that.
				float minRadius = std::max(diag * 0.001f, 0.001f);
				float maxRadius = std::max(minRadius, diag * 0.5f);
				float ratio = maxRadius / minRadius;
				targetPath->radius = minRadius * std::pow(ratio, t);
			}

			if (bHoldingV) {
				// Max speed is extents diagonal / 4 (world units per frame).
				// Use log interpolation so lower-speed region has finer practical control.
				float minSpeed = std::max(diag * 0.00005f, 0.0001f);
				float maxSpeed = std::max(minSpeed, diag * 0.25f);
				float ratio = maxSpeed / minSpeed;
				targetPath->speed = minSpeed * std::pow(ratio, t);
			}

			oscManager.sendUIPathUpdate(targetPath->id, targetPath->isActive,
				targetPath->radius, targetPath->direction, targetPath->sampleNum,
				targetPath->volume, targetPath->falloff, targetPath->speed, targetPath->mode);
		}

		lastMouse.set(x, y);
		return; // consume drag so it cannot pan/wander/browse-drag
	}

	// Allow gesture recording even in BROWSE mode (before the early return)
	if (ofGetKeyPressed(OF_KEY_ALT) && selectedPath && isRecordingGesture) {
		float scrubPos = std::clamp((float)x / (float)ofGetWidth(), 0.0f, 1.0f);
		float vol = std::clamp(ofMap((float)y, (float)ofGetHeight(), 0.f, 0.f, 1.f), 0.f, 1.f);
		long timeMs = ofGetElapsedTimeMillis() - selectedPath->gestureStartTime;
		selectedPath->gesturePoints.push_back({ scrubPos, vol, timeMs });
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
			long timeMs = ofGetElapsedTimeMillis() - selectedPath->gestureStartTime;
			selectedPath->gesturePoints.push_back({ scrubPos, vol, timeMs });
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
	// Forward to annotation system
	if (annotationManager.onMouseReleased(glm::vec2(x, y))) return;

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

			setViewTarget(newZoom, -(targetWorld * newZoom), true);
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

void ofApp::setViewTarget(float newZoom, const ofVec2f & newPan, bool animate) {
	newZoom = std::max(newZoom, 0.0001f);
	if (animate) {
		targetZoom = newZoom;
		targetPan = newPan;
		isViewAnimating = true;
	} else {
		zoom = newZoom;
		pan = newPan;
		targetZoom = newZoom;
		targetPan = newPan;
		isViewAnimating = false;
	}
}

//--------------------------------------------------------------
bool ofApp::loadPoints(string jsonPath) {
	// Keep track of the currently loaded file for saving compositions
	lastLoadedPointsPath = jsonPath;

	// Reset state and clear UI / Synths
	paths.clear();
	selectedPath = nullptr;
	oscManager.sendReset(); // Notify SuperCollider of standard reset
	oscManager.sendClear();

	ofxJSONElement json;
	if (json.open(jsonPath)) {
		bool isLegacyArray = json.isArray();
		if (!json.isObject() && !isLegacyArray) {
			ofLogError("ofApp::loadPoints") << "Error: Expected JSON object with 'points' and 'clusters', or legacy array.";
			ofSystemAlertDialog("The selected file is not a valid points file.\nPlease ensure you select the raw points JSON and not a composition file.");
			return false;
		}

		// Ensure it is not a composition file (which has "points_file")
		if (json.isObject() && json.isMember("points_file")) {
			ofLogError("ofApp::loadPoints") << "Error: Attempted to load composition file as raw points data.";
			return false;
		}

		points.clear();
		clusters.clear();

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

		const Json::Value * pointsJson = nullptr;

		if (isLegacyArray) {
			pointsJson = &json;
		} else {
			if (json.isMember("clusters")) {
				std::vector<std::string> clusterKeys = json["clusters"].getMemberNames();
				for (size_t i = 0; i < clusterKeys.size(); ++i) {
					const Json::Value & c = json["clusters"][clusterKeys[i]];
					ClusterInfo ci;
					ci.id = c["id"].asInt();
					ci.label = c["label"].asString();
					ci.member_count = c["member_count"].asInt();
					clusters[ci.id] = ci;
				}
			}
			if (json.isMember("points")) {
				pointsJson = &json["points"];
			}
		}

		if (pointsJson && pointsJson->isArray()) {
			for (Json::ArrayIndex i = 0; i < pointsJson->size(); ++i) {
				const Json::Value & pt = (*pointsJson)[i];
				float x = pt["x"].asFloat();
				float y = pt["y"].asFloat();
				string filename = pt.isMember("file") ? pt["file"].asString() : (pt.isMember("filename") ? pt["filename"].asString() : "");
				string text = pt.isMember("text") ? pt["text"].asString() : "";

				DataPoint p(x, y, filename, text);

				// Parse new fields
				if (pt.isMember("pos_local") && pt["pos_local"].isArray()) {
					p.pos_local.set(pt["pos_local"][0].asFloat(), pt["pos_local"][1].asFloat());
				} else {
					p.pos_local.set(x, y); // Default fallback for legacy files
				}
				if (pt.isMember("pos_mid") && pt["pos_mid"].isArray()) {
					p.pos_mid.set(pt["pos_mid"][0].asFloat(), pt["pos_mid"][1].asFloat());
				} else {
					p.pos_mid.set(x, y); // Default fallback
				}
				if (pt.isMember("pos_global") && pt["pos_global"].isArray()) {
					p.pos_global.set(pt["pos_global"][0].asFloat(), pt["pos_global"][1].asFloat());
				} else {
					p.pos_global.set(x, y); // Default fallback for legacy files
				}

				if (pt.isMember("instability")) p.instability = pt["instability"].asFloat();
				if (pt.isMember("cluster_id")) p.cluster_id = pt["cluster_id"].asInt();
				if (pt.isMember("cluster_membership")) p.cluster_membership = pt["cluster_membership"].asFloat();
				if (pt.isMember("attack")) p.attack = pt["attack"].asFloat();
				if (pt.isMember("brightness")) p.brightness = pt["brightness"].asFloat();

				if (pt.isMember("true_neighbors") && pt["true_neighbors"].isArray()) {
					for (Json::ArrayIndex j = 0; j < pt["true_neighbors"].size(); ++j) {
						p.true_neighbors.push_back(pt["true_neighbors"][j].asInt());
					}
				}
				if (pt.isMember("true_distances") && pt["true_distances"].isArray()) {
					for (Json::ArrayIndex j = 0; j < pt["true_distances"].size(); ++j) {
						p.true_distances.push_back(pt["true_distances"][j].asFloat());
					}
				}

				points.push_back(p);

				if (x < minX) minX = x;
				if (x > maxX) maxX = x;
				if (y < minY) minY = y;
				if (y > maxY) maxY = y;
			}
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
			float newZoom = std::min(zoomX, zoomY);

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

			ofVec2f newPan(-centerX * newZoom, -centerY * newZoom);
			setViewTarget(newZoom, newPan, true);

			ofLogNotice("ofApp::loadPoints") << "Auto-framed. Target zoom: " << newZoom << " Target pan: " << newPan;
		}

		// Rebuild grid
		spatialGrid = std::make_shared<SpatialGrid>(points, 50.0f / zoom); // Adjust grid cell size based on zoom? Or keep world units?
		// SpatialGrid uses world units. 50.0f is arbitrary. Let's keep it 50 or adapt to density.

		// Build sorted cluster ID list for stepping
		sortedClusterIds.clear();
		for (const auto & kv : clusters) {
			sortedClusterIds.push_back(kv.first);
		}
		std::sort(sortedClusterIds.begin(), sortedClusterIds.end());
		activeClusterId = -999; // Reset filter on new load

		// Restore saved annotations now that points exist to anchor them to.
		// This also clears any previously loaded annotations when a new file is opened.
		annotationManager.loadFromFile("annotations.json", points);

		// Register cluster annotations for any labelled clusters
		for (const auto & kv : clusters) {
			if (!kv.second.label.empty()) {
				annotationManager.addClusterAnnotation(kv.second.id, kv.second.label, points);
			}
		}

		// Final safety pass: ensure every loaded/generated annotation label is
		// fully inside the current point bounds.
		annotationManager.clampAllLabelsToPointsBounds(points);

		return true;
	} else {
		ofLogError("ofApp::loadPoints") << "Failed to open JSON file: " << jsonPath;
		return false;
	}
}

void ofApp::sendOscMessage(string addr, float val) {
	ofxOscMessage m;
	m.setAddress(addr);
	m.addFloatArg(val);
	oscManager.sendMessage(m);
}

void ofApp::triggerVideo(const DataPoint & p) {
	if (videoTriggerLocked) return;

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
			if (videoDisplayMode.get() == 0) {
				// Swap: old front keeps running as back — no reload, no flash
				std::swap(videoFront, videoBack);
				videoFront->load(videoPath);
				videoFront->setLoopState(OF_LOOP_NORMAL);
				videoFront->play();
				videoFront->setVolume(0);
				videoAlpha = 0.0f;
				videoAlphaTarget = 255.0f;
			} else if (videoDisplayMode.get() == 2) {
				// GHOST MODE: evict the oldest layer, push a new clip in at front.
				auto evicted = ghostPlayers.back();
				evicted->stop();
				evicted->close();
				ghostPlayers.pop_back();
				evicted->load(videoPath);
				evicted->setLoopState(OF_LOOP_NORMAL);
				evicted->play();
				evicted->setVolume(0);
				ghostPlayers.push_front(evicted);
			} else if (videoDisplayMode.get() == 3) {
				// MAPPED MODE: recycle oldest player instead of creating new ones.
				if (mappedPlayers.empty()) {
					MappedClip mc;
					mc.player = std::make_shared<ofVideoPlayer>();
					mappedPlayers.push_back(mc);
				}

				auto evicted = mappedPlayers.back();
				evicted.player->stop();
				evicted.player->close();
				mappedPlayers.pop_back();

				evicted.point = p;
				evicted.player->load(videoPath);
				evicted.player->setLoopState(OF_LOOP_NORMAL);
				evicted.player->play();
				evicted.player->setVolume(0);
				mappedPlayers.push_front(evicted);
			} else {
				videoQueue.push_back(videoPath);
			}
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
	newPath->isWander = true;
	newPath->mode = defaultPathMode;

	// Add start directly to the polyline as a straight-line segment
	newPath->controlPoints.push_back(ofVec2f(start.x, start.y));
	newPath->polyline.addVertex(start.x, start.y, 0);
	newPath->attachedPoints.push_back(&start);

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
		newPath->controlPoints.push_back(ofVec2f(next->x, next->y));
		newPath->polyline.addVertex(next->x, next->y, 0);
		newPath->attachedPoints.push_back(next);
		visited.insert(*next);
		cx = next->x;
		cy = next->y;
	}

	return newPath;
}

//--------------------------------------------------------------
void ofApp::stopPathSamples(std::shared_ptr<PathObject> p) {
	if (!p) return;
	for (const auto & dp : p->playingPoints) {
		oscManager.stopSample(mediaRoot + dp.filename, p->name);
	}
	p->playingPoints.clear();
}

//--------------------------------------------------------------
void ofApp::saveComposition(string filepath) {
	ofxJSONElement root;

	// 1. Data Source
	root["points_file"] = lastLoadedPointsPath;

	// Title
	root["title"] = compositionTitle;

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
		pathJson["is_wander"] = p->isWander;
		pathJson["send_video"] = p->sendToVideo;

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

		if (!p->attachedPoints.empty()) {
			ofxJSONElement attachedArray;
			for (int j = 0; j < p->attachedPoints.size(); ++j) {
				attachedArray.append(p->attachedPoints[j]->filename);
			}
			pathJson["attached_points"] = attachedArray;
		}

		// Save gesture points if applicable
		if (p->hasGesture && !p->gesturePoints.empty()) {
			pathJson["has_gesture"] = true;
			ofxJSONElement gArray;
			for (int j = 0; j < p->gesturePoints.size(); ++j) {
				ofxJSONElement g;
				g["position"] = p->gesturePoints[j].position;
				g["volume"] = p->gesturePoints[j].volume;
				g["timeMs"] = (Json::Int64)p->gesturePoints[j].timeMs;
				gArray.append(g);
			}
			pathJson["gesture_points"] = gArray;
		} else {
			pathJson["has_gesture"] = false;
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

		if (!ofFile(targetPointsFile).exists()) {
			ofSystemAlertDialog("The points file could not be found:\n" + targetPointsFile + "\n\nPlease locate it now to repair the composition.");
			ofFileDialogResult result = ofSystemLoadDialog("Locate " + ofFilePath::getFileName(targetPointsFile));
			if (result.bSuccess) {
				targetPointsFile = result.getPath();
				root["points_file"] = targetPointsFile;
				// Rewrite the composition file with the new path
				root.save(filepath, true);
				ofLogNotice("ofApp::loadCompositionOrPoints") << "Repaired composition with new points file: " << targetPointsFile;
			} else {
				ofLogError("ofApp::loadCompositionOrPoints") << "Repair cancelled. Cannot load composition.";
				return;
			}
		}

		if (!loadPoints(targetPointsFile)) {
			// If loadPoints fails (e.g. wrong file type), abort composition load
			ofLogError("ofApp::loadCompositionOrPoints") << "Failed to load points data. Aborting composition load.";
			return;
		}

		// Load title
		if (root.isMember("title")) {
			compositionTitle = root["title"].asString();
			showTitle = true; // Auto-show if loaded? (Can keep it optional, let's auto-show if it exists and is not empty)
			if (compositionTitle.empty()) showTitle = false;
		}

		// 2. Clear existing paths and selection
		paths.clear();
		selectedPath = nullptr;
		oscManager.sendReset(); // Notify SuperCollider of standard reset
		oscManager.sendClear(); // Tell UI/SC to clear visuals/synths

		// Restore path-0 immediately
		auto p0 = std::make_shared<PathObject>(0);
		p0->name = "path-0";
		p0->mode = defaultPathMode;
		p0->isActive = false;
		p0->sendToVideo = false;
		oscManager.sendUIPathAdd(p0->name);
		paths.push_back(p0);

		// 3. Restore settings
		if (root.isMember("settings")) {
			float restoredZoom = root["settings"]["zoom"].asFloat();
			ofVec2f restoredPan(root["settings"]["pan_x"].asFloat(), root["settings"]["pan_y"].asFloat());
			setViewTarget(restoredZoom, restoredPan, true);
		}

		// 4. Restore paths
		if (root.isMember("paths") && root["paths"].isArray()) {
			const ofxJSONElement & jsonPaths = root["paths"];
			for (int i = 0; i < jsonPaths.size(); ++i) {
				const ofxJSONElement & pJson = jsonPaths[i];

				// Reconstruct PathObject
				auto newPath = std::make_shared<PathObject>(pathIdCounter++);
				newPath->name = pJson["name"].asString();
				newPath->isActive = false; // User requested paths not playing by default on load
				// (We could read but ignore pJson["is_active"].asBool())
				newPath->radius = pJson["radius"].asFloat();
				newPath->speed = pJson["speed"].asFloat();

				// Handle legacy missing properties gracefully
				if (pJson.isMember("volume")) newPath->volume = pJson["volume"].asFloat();
				if (pJson.isMember("sample_num")) newPath->sampleNum = pJson["sample_num"].asInt();
				if (pJson.isMember("direction")) newPath->direction = pJson["direction"].asInt();
				if (pJson.isMember("falloff")) newPath->falloff = pJson["falloff"].asFloat();
				if (pJson.isMember("mode")) newPath->mode = pJson["mode"].asInt();
				if (pJson.isMember("is_sequential")) newPath->isSequential = pJson["is_sequential"].asBool();
				if (pJson.isMember("is_wander")) newPath->isWander = pJson["is_wander"].asBool();
				if (pJson.isMember("send_video")) newPath->sendToVideo = pJson["send_video"].asBool();

				// Reconstruct control points
				if (pJson.isMember("control_points") && pJson["control_points"].isArray()) {
					const ofxJSONElement & cpArray = pJson["control_points"];
					for (int j = 0; j < cpArray.size(); ++j) {
						ofVec2f pt(cpArray[j]["x"].asFloat(), cpArray[j]["y"].asFloat());
						newPath->addPoint(pt);
					}
					if (!newPath->isSequential && !newPath->isWander) { // Normal drawn paths need finalization (smoothing)
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

				if (pJson.isMember("attached_points") && pJson["attached_points"].isArray()) {
					const ofxJSONElement & attArray = pJson["attached_points"];
					for (int j = 0; j < attArray.size(); ++j) {
						std::string fn = attArray[j].asString();
						for (const auto & dp : points) {
							if (dp.filename == fn) {
								newPath->attachedPoints.push_back(&dp);
								break;
							}
						}
					}
				}

				// Reconstruct gesture points (if applicable)
				if (pJson.isMember("has_gesture") && pJson["has_gesture"].asBool()) {
					if (pJson.isMember("gesture_points") && pJson["gesture_points"].isArray()) {
						newPath->hasGesture = true;
						const ofxJSONElement & gArray = pJson["gesture_points"];
						for (int j = 0; j < gArray.size(); ++j) {
							PathObject::GesturePoint gp;
							gp.position = gArray[j]["position"].asFloat();
							gp.volume = gArray[j]["volume"].asFloat();
							// Default timeMs to 0 for older un-versioned saves to avoid crash, but they won't playback dynamically
							gp.timeMs = gArray[j].isMember("timeMs") ? gArray[j]["timeMs"].asInt64() : 0;
							newPath->gesturePoints.push_back(gp);
						}
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