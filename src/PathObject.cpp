#include "PathObject.h"
#include <algorithm>
#include <cmath>

PathObject::PathObject(int _id)
	: id(_id)
	, isActive(false)
	, isSelected(false)
	, position(0)
	, speed(0.001)
	, direction(1)
	, volume(0.5)
	, radius(0.01)
	, sampleNum(1)
	, playbackRate(1.0f)
	, gRateMin(1.0f)
	, gRateMax(2.0f)
	, gDurMin(0.05f)
	, gDurMax(0.2f)
	, gGrainRateMin(10)
	, gGrainRateMax(20)
	, gPosMin(0.0f)
	, gPosMax(1.0f)
	, gRand(0.01f)
	, granularMode(GRANULAR_ALGO_STRETCH)
	, gEnv(1)
	, grainRateWalk(0.5f)
	, grainDurWalk(0.5f)
	, grainDensityWalk(0.5f)
	, grainMotionPhase(0.0f)
	// Pulser defaults
	, pulserMix(0.0f)
	, pulserRateMin(1.0f)
	, pulserRateMax(2.0f)
	, pulserRateRand(0.0f)
	, pulserAttack(0.01f)
	, pulserRelease(0.1f)
	// Sequential / Jitter / Wander
	, isSequential(false)
	, isWander(false)
	, currentStepIndex(-1)
	, jitterMode(false)
	, lastJitterStepFloor(-1)
	, stepMode(false)
	, stepTimer(0.0f)
	, hasGesture(false)
	, gestureStartTime(0)
	, gesturePlaybackTime(0.0f)
	, isPingPong(false)
	, sendToVideo(true) {
	name = "path-" + std::to_string(id);
	mode = ONCE_MODE;

	// Effects: all disabled by default
	for (int i = 0; i < 5; i++)
		fxEnabled[i] = false;

	// Reverb defaults: mix=0.33, room=0.5, damp=0.5
	fxReverb[0] = 0.33f;
	fxReverb[1] = 0.5f;
	fxReverb[2] = 0.5f;

	// Delay defaults: time=0.5, feedback=0.3, mix=0.3
	fxDelay[0] = 0.5f;
	fxDelay[1] = 0.3f;
	fxDelay[2] = 0.3f;

	// Distortion defaults: drive=0.5, mix=0.5
	fxDistortion[0] = 0.5f;
	fxDistortion[1] = 0.5f;

	// Compressor defaults: thresh=0.5, ratio=4.0, attack=0.01, release=0.1
	fxCompressor[0] = 0.5f;
	fxCompressor[1] = 4.0f;
	fxCompressor[2] = 0.01f;
	fxCompressor[3] = 0.1f;

	// Filter defaults: freq=1000, res=0.5, type=0 (lpf)
	fxFilter[0] = 1000.0f;
	fxFilter[1] = 0.5f;
	fxFilter[2] = 0.0f;
}

void PathObject::addPoint(ofVec2f pt) {
	controlPoints.push_back(pt);
	polyline.addVertex(pt.x, pt.y, 0);
}

void PathObject::finalize() {
	polyline.clear();
	if (controlPoints.size() < 2)
		return;

	// Build raw polyline from control points
	ofPolyline temp;
	for (const auto & pt : controlPoints) {
		temp.addVertex(pt.x, pt.y, 0);
	}

	// Light smoothing — keeps the path close to what was drawn
	ofPolyline smoothed = temp.getSmoothed(4);
	polyline = (smoothed.size() >= 2) ? smoothed : temp;
}

void PathObject::togglePlayback() { isActive = !isActive; }

void PathObject::clearGesture() {
	hasGesture = false;
	gesturePoints.clear();
}

void PathObject::translate(ofVec2f delta) {
	for (auto & pt : controlPoints)
		pt += delta;
	// Rebuild polyline from translated control points
	polyline.clear();
	for (const auto & pt : controlPoints)
		polyline.addVertex(pt.x, pt.y, 0);
	// Also shift sequential data points
	for (auto & dp : sequentialPoints) {
		dp.x += delta.x;
		dp.y += delta.y;
	}
}

float PathObject::getGestureVolume(float pos) const {
	if (gesturePoints.empty()) return volume;
	if (gesturePoints.size() == 1) return gesturePoints[0].volume;

	// Linear interpolate across the gesture curve by position
	for (size_t i = 0; i + 1 < gesturePoints.size(); ++i) {
		const auto & a = gesturePoints[i];
		const auto & b = gesturePoints[i + 1];
		if (pos >= a.position && pos <= b.position) {
			if (b.position == a.position) return a.volume;
			float t = (pos - a.position) / (b.position - a.position);
			return a.volume + t * (b.volume - a.volume);
		}
	}
	// Outside range: clamp to nearest endpoint
	if (pos <= gesturePoints.front().position) return gesturePoints.front().volume;
	return gesturePoints.back().volume;
}

// Direct port of Processing's getGranularMode()
std::string PathObject::getGranularMode() const {
	if (granularMode == GRANULAR_ALGO_STRETCH) {
		return "stretch";
	} else if (granularMode == GRANULAR_ALGO_GHOST) {
		return "ghost";
	} else if (granularMode == GRANULAR_ALGO_MIXED) {
		return (ofRandom(100) > 50) ? "ghost" : "stretch";
	} else { // GRANULAR_ALGO_ORIG or any unknown
		return "cloud";
	}
}

// Direct port of Processing's getMode()
std::string PathObject::getMode() const {
	if (mode == CLOUD_MODE) {
		return getGranularMode();
	} else if (mode == MIXED_MODE) {
		if (ofRandom(100) > 50) {
			return getGranularMode();
		} else if (ofRandom(100) > 50) {
			return "loop";
		} else {
			return "once";
		}
	} else if (mode == ONCE_MODE) {
		return "once";
	} else {
		return "loop";
	}
}

void PathObject::update(float dt) {
	if (!attachedPoints.empty() && attachedPoints.size() == controlPoints.size()) {
		bool changed = false;
		for (size_t i = 0; i < attachedPoints.size(); ++i) {
			if (controlPoints[i].x != attachedPoints[i]->x || controlPoints[i].y != attachedPoints[i]->y) {
				changed = true;
				break;
			}
		}
		if (changed) {
			polyline.clear();
			for (size_t i = 0; i < attachedPoints.size(); ++i) {
				controlPoints[i].x = attachedPoints[i]->x;
				controlPoints[i].y = attachedPoints[i]->y;
				polyline.addVertex(controlPoints[i].x, controlPoints[i].y, 0);
			}
		}
	}

	if (!isActive)
		return;

	// Step mode: advance currentStepIndex via timer, skip polyline position logic
	if (stepMode && isSequential && !sequentialPoints.empty()) {
		stepTimer += speed * dt; // 1.0 speed = 1 step per second
		if (stepTimer >= 1.0f) {
			stepTimer -= 1.0f;
			int n = (int)sequentialPoints.size();
			if (direction == -1) {
				currentStepIndex = (currentStepIndex <= 0) ? n - 1 : currentStepIndex - 1;
			} else {
				currentStepIndex = (currentStepIndex + 1) % n;
			}
			// Keep position in sync for visual/gesture purposes
			position = (n > 1) ? (float)currentStepIndex / (float)(n - 1) : 0.0f;
		}
		return; // Don't advance position the normal way
	}

	if (polyline.size() == 0)
		return;

	// -------------------------------------------------------------
	// GESTURE PLAYBACK
	// -------------------------------------------------------------
	if (hasGesture && gesturePoints.size() >= 2) {
		float durationMs = gesturePoints.back().timeMs - gesturePoints.front().timeMs;
		if (durationMs > 0.001f) {
			gesturePlaybackTime += (dt * 1000.0f) * speed; // Advance time based on path speed

			// Wrap playback time over the gesture duration
			float localTime = std::fmod(gesturePlaybackTime, durationMs);
			if (localTime < 0) localTime += durationMs;

			long targetTimeMs = gesturePoints.front().timeMs + (long)localTime;

			// Find bounding gesture points and interpolate
			for (size_t i = 0; i + 1 < gesturePoints.size(); ++i) {
				const auto & a = gesturePoints[i];
				const auto & b = gesturePoints[i + 1];
				if (targetTimeMs >= a.timeMs && targetTimeMs <= b.timeMs) {
					float t = 0.0f;
					if (b.timeMs > a.timeMs) {
						t = (float)(targetTimeMs - a.timeMs) / (float)(b.timeMs - a.timeMs);
					}
					position = a.position + t * (b.position - a.position);
					volume = a.volume + t * (b.volume - a.volume);
					break;
				}
			}
		}
		return; // Skip normal playhead movement
	}

	// -------------------------------------------------------------
	// NORMAL PLAYHEAD MOVEMENT
	// -------------------------------------------------------------

	// Update position based on speed and direction
	// Speed logic needs calibration to match pixels/sec
	// Processing speed was "units per frame", assumed 60fps
	float moveAmount = speed * dt * 60.0f;

	position += (moveAmount / polyline.getPerimeter());

	// Loop Logic
	if (position >= 1.0f) {
		if (isPingPong) {
			speed = -std::abs(speed);
			position = 1.0f;
		} else if (direction == 2) { // Oscillate
			speed = -std::abs(speed);
			position = 1.0f;
		} else { // Loop
			position = 0.0f;
		}
	} else if (position <= 0.0f) {
		if (isPingPong) {
			speed = std::abs(speed);
			position = 0.0f;
		} else if (direction == 2) { // Oscillate
			speed = std::abs(speed);
			position = 0.0f;
		} else {
			speed = -std::abs(speed); // Should flip back to forward? Loop logic varies
			// Standard loop behavior for backward playback: wrap to 1.0
			position = 1.0f;
		}
	}
}

ofVec2f PathObject::getCurrentPosition() const {
	if (polyline.size() == 0)
		return ofVec2f(0, 0);
	return polyline.getPointAtPercent(position);
}

void PathObject::draw(float playheadSize, ofColor playheadColor, float zoom, float pathThickness, float selectedPathThickness, ofColor pathColor, ofColor selectedPathColor, int lineStyle) {
	if (polyline.size() < 2)
		return;

	if (isSelected) {
		ofSetColor(selectedPathColor);
		ofSetLineWidth(selectedPathThickness);
	} else {
		ofSetColor(pathColor);
		ofSetLineWidth(pathThickness);
	}

	if (lineStyle == 1) {
		// Dashed line: draw segments
		float perimeter = polyline.getPerimeter();
		float scaledDash = 10.0f / zoom;
		float scaledGap = 10.0f / zoom;
		float step = scaledDash + scaledGap;
		for (float len = 0; len < perimeter; len += step) {
			float endLen = std::min(len + scaledDash, perimeter);
			ofVec2f p1 = polyline.getPointAtLength(len);
			ofVec2f p2 = polyline.getPointAtLength(endLen);
			ofDrawLine(p1.x, p1.y, p2.x, p2.y);
		}
	} else if (lineStyle == 2) {
		// Dotted line: draw circles
		float perimeter = polyline.getPerimeter();
		float scaledDotSpacing = 8.0f / zoom;
		float dotRadius = (isSelected ? selectedPathThickness : pathThickness) * 0.5f;
		float dotWorldRadius = dotRadius / zoom;
		ofPushStyle();
		ofFill();
		for (float len = 0; len < perimeter; len += scaledDotSpacing) {
			ofVec2f p = polyline.getPointAtLength(len);
			ofDrawCircle(p.x, p.y, dotWorldRadius);
		}
		ofPopStyle();
	} else {
		// Solid line (default)
		polyline.draw();
	}

	// Reset line width for cursor and other elements
	ofSetLineWidth(1.0f);

	// Draw cursor
	ofVec2f pos = getCurrentPosition();
	ofNoFill();
	ofSetColor(playheadColor);
	ofDrawCircle(pos, radius); // actual catchment radius in world units
	ofFill();
	ofDrawCircle(pos, playheadSize); // already /zoom from caller

	// Draw lines to active points
	ofSetColor(255, 100);
	for (const auto & p : lastActivePoints) {
		ofDrawLine(pos, ofVec2f(p.x, p.y));
		ofDrawCircle(p.x, p.y, 3.0f / zoom); // screen-pixel dot
	}
	// Gesture overlay is drawn in screen-space by ofApp::draw()
}

float PathObject::getDistanceToPoint(ofVec2f pt) const {
	if (polyline.size() == 0)
		return std::numeric_limits<float>::max();

	// Find closest point on polyline
	// Find closest point on polyline
	glm::vec3 target(pt.x, pt.y, 0.0f);
	glm::vec3 closestPt = polyline.getClosestPoint(target);
	return glm::distance(glm::vec2(closestPt), glm::vec2(pt));
}

void PathObject::moveRelative(ofVec2f delta) {
	for (auto & pt : controlPoints) {
		pt += delta;
	}
	finalize();
}

std::vector<DataPoint>
PathObject::getActivePoints(const std::vector<DataPoint> & allPoints,
	SpatialGrid & grid) {
	if (!isActive || polyline.size() == 0) {
		return std::vector<DataPoint>();
	}

	ofVec2f currentPos = getCurrentPosition();
	std::vector<DataPoint> pointsInRadius = grid.findPointsInRadius(currentPos.x, currentPos.y, radius);

	// Standard audio sampling logic (simplified for now)
	// 1. Identify which points valid
	// 2. Identify new points vs old points
	// 3. Return the set of active points

	// For now, return all points in radius
	return pointsInRadius;
}
