#include "Annotation.h"
#include <algorithm>
#include <cmath>
#include <limits>

// ============================================================
// Constants — all magic numbers live here
// ============================================================
static constexpr float kBoxPadX       = 10.0f;  // horizontal text padding inside box
static constexpr float kBoxPadY       =  6.0f;  // vertical text padding inside box
static constexpr float kBoxMinWidth   = 60.0f;
static constexpr float kBoxMinHeight  = 22.0f;
static constexpr float kCornerRadius  =  5.0f;

// Default label box placement relative to the anchor (screen pixels)
static constexpr float kDefaultOffsetX =  140.0f;
static constexpr float kDefaultOffsetY = -70.0f;

// Curve offset  — how far the control point is pushed perpendicularly
static constexpr float kCurveOffset   = 50.0f;

// Bezier sampling
static constexpr int   kBezierSteps   = 24;

// Hit tests
static constexpr float kCurveHitThreshold = 8.0f;

// Opacity when annotation mode is off
static constexpr float kInactiveAlpha = 0.55f;

// Opacity for cluster annotations belonging to a non-active cluster
// (matches the 15% dimming applied to non-active points)
static constexpr float kDimmedClusterAlpha = 0.15f;

// Blink period (ms)
static constexpr uint64_t kBlinkPeriodMs = 600;

// Cluster label default offset from centroid anchor (screen pixels)
static constexpr float kClusterOffsetX =  160.0f;
static constexpr float kClusterOffsetY = -110.0f;

// ============================================================
// AnnotationManager — public interface
// ============================================================

void AnnotationManager::init(float * zoomRef_, ofVec2f * panRef_,
                             ofTrueTypeFont * labelFontRef)
{
    zoomRef   = zoomRef_;
    panRef    = panRef_;
    labelFont = labelFontRef;

    ofLogNotice("AnnotationManager") <<
        "Annotation system ready.  Tab = toggle annotation mode, "
        "Shift+Tab = toggle visibility.";
}

// ---- Coordinate helpers -----------------------------------------------

glm::vec2 AnnotationManager::worldToScreen(glm::vec2 world) const {
    float z = zoomRef ? *zoomRef : 1.0f;
    ofVec2f p = panRef ? *panRef : ofVec2f(0, 0);
    ofVec2f center(ofGetWidth() / 2, ofGetHeight() / 2);
    ofVec2f s = (ofVec2f(world.x, world.y) * z) + p + center;
    return glm::vec2(s.x, s.y);
}

glm::vec2 AnnotationManager::screenToWorld(glm::vec2 screen) const {
    float z = zoomRef ? *zoomRef : 1.0f;
    ofVec2f p = panRef ? *panRef : ofVec2f(0, 0);
    ofVec2f center(ofGetWidth() / 2, ofGetHeight() / 2);
    ofVec2f w = (ofVec2f(screen.x, screen.y) - p - center) / z;
    return glm::vec2(w.x, w.y);
}

// ---- Bezier ---------------------------------------------------------------

glm::vec2 AnnotationManager::evaluateBezier(const Annotation & a, float t) const {
    // Quadratic bezier: anchor -> control -> labelBox centre
    glm::vec2 anchorScreen = worldToScreen(a.anchorPoint);
    ofRectangle box = labelBoxRect(a);
    glm::vec2 boxCenter(box.x + box.width * 0.5f, box.y + box.height * 0.5f);
    glm::vec2 P0 = anchorScreen;
    glm::vec2 P1 = a.controlPoint;
    glm::vec2 P2 = boxCenter;
    float mt = 1.0f - t;
    return (mt * mt) * P0 + (2.0f * mt * t) * P1 + (t * t) * P2;
}

// Recompute the control point as the midpoint of anchor<->box, offset
// perpendicularly by kCurveOffset.
void AnnotationManager::recomputeControlPoint(Annotation & a) {
    glm::vec2 anchorScreen = worldToScreen(a.anchorPoint);
    ofRectangle box = labelBoxRect(a);
    glm::vec2 boxCenter(box.x + box.width * 0.5f, box.y + box.height * 0.5f);

    glm::vec2 mid = (anchorScreen + boxCenter) * 0.5f;
    glm::vec2 dir = boxCenter - anchorScreen;
    float len = glm::length(dir);
    if (len > 0.001f) {
        glm::vec2 perp(-dir.y / len, dir.x / len);
        a.controlPoint = mid + perp * kCurveOffset;
    } else {
        a.controlPoint = mid + glm::vec2(0, -kCurveOffset);
    }
}

// ---- Box geometry ---------------------------------------------------------

ofRectangle AnnotationManager::labelBoxRect(const Annotation & a) const {
    float textW = kBoxMinWidth;
    float textH = kBoxMinHeight;
    if (labelFont && labelFont->isLoaded() && !a.labelText.empty()) {
        std::string display = a.labelText;
        ofRectangle b = labelFont->getStringBoundingBox(display, 0, 0);
        textW = std::max(b.width,  kBoxMinWidth);
        textH = std::max(b.height, kBoxMinHeight);
    }
    glm::vec2 screenPos = worldToScreen(a.labelBoxPos);
    return ofRectangle(screenPos.x,
                       screenPos.y,
                       textW + kBoxPadX * 2,
                       textH + kBoxPadY * 2);
}

// ---- Hit tests (screen coords) -------------------------------------------

int AnnotationManager::hitTestLabelBox(glm::vec2 pos) const {
    for (int i = 0; i < (int)annotations.size(); ++i) {
        ofRectangle r = labelBoxRect(annotations[i]);
        if (r.inside(pos.x, pos.y)) return i;
    }
    return -1;
}

int AnnotationManager::hitTestCurve(glm::vec2 pos, float threshold) const {
    for (int i = 0; i < (int)annotations.size(); ++i) {
        const Annotation & a = annotations[i];
        glm::vec2 prev = evaluateBezier(a, 0.0f);
        for (int s = 1; s <= kBezierSteps; ++s) {
            float t = (float)s / (float)kBezierSteps;
            glm::vec2 curr = evaluateBezier(a, t);
            // Distance from pos to segment [prev, curr]
            glm::vec2 seg  = curr - prev;
            float segLen   = glm::length(seg);
            float dist;
            if (segLen < 0.001f) {
                dist = glm::length(pos - prev);
            } else {
                float u = glm::dot(pos - prev, seg) / (segLen * segLen);
                u = std::clamp(u, 0.0f, 1.0f);
                glm::vec2 closest = prev + u * seg;
                dist = glm::length(pos - closest);
            }
            if (dist < threshold) return i;
            prev = curr;
        }
    }
    return -1;
}

// ---- Nearest DataPoint helper -------------------------------------------

int AnnotationManager::nearestPointTo(glm::vec2 worldPos,
                                      const std::vector<DataPoint> & points) const {
    int best = -1;
    float bestDist = std::numeric_limits<float>::max();
    for (int i = 0; i < (int)points.size(); ++i) {
        float d = glm::length(worldPos - glm::vec2(points[i].x, points[i].y));
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

bool AnnotationManager::computePointsBounds(const std::vector<DataPoint> & points,
                                            glm::vec2 & minPt,
                                            glm::vec2 & maxPt) const {
    if (points.empty()) return false;

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();

    for (const auto & p : points) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }

    minPt = glm::vec2(minX, minY);
    maxPt = glm::vec2(maxX, maxY);
    return true;
}

void AnnotationManager::clampAnnotationLabelToBounds(Annotation & a,
                                                     const glm::vec2 & minPt,
                                                     const glm::vec2 & maxPt) const {
    // Clamp in screen space against the projected point bounds, then convert
    // back to world space. This is robust even when legacy files mixed spaces.
    ofRectangle r = labelBoxRect(a);

    glm::vec2 sMin = worldToScreen(minPt);
    glm::vec2 sMax = worldToScreen(maxPt);
    float left = std::min(sMin.x, sMax.x);
    float right = std::max(sMin.x, sMax.x);
    float top = std::min(sMin.y, sMax.y);
    float bottom = std::max(sMin.y, sMax.y);

    float minX = left;
    float maxX = right - r.width;
    float minY = top;
    float maxY = bottom - r.height;

    float clampedX;
    float clampedY;

    if (maxX < minX) {
        clampedX = (left + right) * 0.5f - r.width * 0.5f;
    } else {
        clampedX = std::clamp(r.x, minX, maxX);
    }

    if (maxY < minY) {
        clampedY = (top + bottom) * 0.5f - r.height * 0.5f;
    } else {
        clampedY = std::clamp(r.y, minY, maxY);
    }

    a.labelBoxPos = screenToWorld(glm::vec2(clampedX, clampedY));
}

// ============================================================
// update — called each frame; tracks anchor points
// ============================================================

void AnnotationManager::update(const std::vector<DataPoint> & points) {
    for (auto & a : annotations) {
        if (a.nearestPointIdx >= 0 && a.nearestPointIdx < (int)points.size()) {
            const DataPoint & dp = points[a.nearestPointIdx];
            a.anchorPoint = glm::vec2(dp.x, dp.y);
        }
        // Recompute control point each frame — anchor may be moving (cloud transition)
        recomputeControlPoint(a);
    }
}

// ============================================================
// refreshAnchors — call after cloud mode change
// ============================================================

void AnnotationManager::refreshAnchors(const std::vector<DataPoint> & points) {
    for (auto & a : annotations) {
        if (a.nearestPointIdx >= 0 && a.nearestPointIdx < (int)points.size()) {
            const DataPoint & dp = points[a.nearestPointIdx];
            a.anchorPoint = glm::vec2(dp.x, dp.y);
        }
        recomputeControlPoint(a);
    }
}

void AnnotationManager::clampAllLabelsToPointsBounds(const std::vector<DataPoint> & points) {
    glm::vec2 minPt, maxPt;
    if (!computePointsBounds(points, minPt, maxPt)) return;

    for (auto & a : annotations) {
        clampAnnotationLabelToBounds(a, minPt, maxPt);
        recomputeControlPoint(a);
    }
}

// ============================================================
// toggleEnabled / toggleVisible
// ============================================================

void AnnotationManager::toggleEnabled() {
    enabled = !enabled;
    // Leave typing state if we're deactivating
    if (!enabled && state == EditState::TYPING) {
        state = EditState::IDLE;
        activeAnnotationIdx = -1;
    }
    ofLogNotice("AnnotationManager") << "Annotation mode: " << (enabled ? "ON" : "OFF");
}

void AnnotationManager::toggleVisible() {
    visible = !visible;
    ofLogNotice("AnnotationManager") << "Annotations: " << (visible ? "VISIBLE" : "HIDDEN");
}

// ============================================================
// Cluster annotation API
// ============================================================

void AnnotationManager::addClusterAnnotation(int clusterId,
                                              const std::string & label,
                                              const std::vector<DataPoint> & points)
{
    // Never annotate the "unclustered" bucket (cluster_id == -1)
    if (clusterId == -1) return;

    // Remove existing annotation for this cluster first (avoid duplicates)
    removeAnnotationsForCluster(clusterId);

    if (label.empty()) return;

    // Compute centroid of all points in this cluster
    float cx = 0, cy = 0;
    int count = 0;
    for (const auto & dp : points) {
        if (dp.cluster_id == clusterId) {
            cx += dp.x; cy += dp.y; ++count;
        }
    }
    if (count == 0) return;
    cx /= count; cy /= count;

    glm::vec2 worldCentroid(cx, cy);
    glm::vec2 anchorScreen = worldToScreen(worldCentroid);

    Annotation a;
    a.id                  = nextId++;
    a.anchorPoint         = worldCentroid;
    a.nearestPointIdx     = nearestPointTo(worldCentroid, points);
    a.labelText           = label;
    a.isClusterAnnotation = true;
    a.clusterId           = clusterId;
    a.labelBoxPos         = screenToWorld(anchorScreen + glm::vec2(kClusterOffsetX, kClusterOffsetY));

    glm::vec2 minPt, maxPt;
    if (computePointsBounds(points, minPt, maxPt)) {
        clampAnnotationLabelToBounds(a, minPt, maxPt);
    }

    recomputeControlPoint(a);
    annotations.push_back(a);
}

void AnnotationManager::removeAnnotationsForCluster(int clusterId) {
    annotations.erase(
        std::remove_if(annotations.begin(), annotations.end(),
            [clusterId](const Annotation & a) {
                return a.isClusterAnnotation && a.clusterId == clusterId;
            }),
        annotations.end());
}

// ============================================================
// Mouse events
// ============================================================

bool AnnotationManager::onMousePressed(glm::vec2 pos, int button) {
    if (!enabled) return false;

    // Already in typing — clicking outside commits, clicking a different
    // box starts typing there, clicking curve re-selects for typing.
    if (state == EditState::TYPING) {
        int boxIdx = hitTestLabelBox(pos);
        if (boxIdx >= 0) {
            activeAnnotationIdx = boxIdx;
            typingStartMs = ofGetElapsedTimeMillis();
            return true;
        }
        // Click elsewhere commits current annotation
        state = EditState::IDLE;
        activeAnnotationIdx = -1;
        // Fall through to handle as a new press
    }

    // Hit a label box -> drag or type
    int boxIdx = hitTestLabelBox(pos);
    if (boxIdx >= 0) {
        ofRectangle r = labelBoxRect(annotations[boxIdx]);
        dragOffset = glm::vec2(r.x, r.y) - pos;
        activeAnnotationIdx = boxIdx;
        state = EditState::DRAGGING_BOX;
        return true;
    }

    // Hit a curve -> retype that annotation
    int curveIdx = hitTestCurve(pos, kCurveHitThreshold);
    if (curveIdx >= 0) {
        activeAnnotationIdx = curveIdx;
        state = EditState::TYPING;
        typingStartMs = ofGetElapsedTimeMillis();
        return true;
    }

    // Empty space -> create new annotation
    glm::vec2 worldPos = screenToWorld(pos);
    Annotation a;
    a.id              = nextId++;
    a.anchorPoint     = worldPos;
    a.nearestPointIdx = -1; // set below after we have access to points
    a.labelText       = "";
    a.isClusterAnnotation = false;
    a.clusterId       = -1;
    a.labelBoxPos     = screenToWorld(pos + glm::vec2(kDefaultOffsetX, kDefaultOffsetY));

    // nearestPointIdx is resolved in onKeyPressed / update if points are available.
    // Store a sentinel; update() will not do anything with idx=-1 which is fine —
    // the annotation will anchor to the world position captured now.
    recomputeControlPoint(a);
    annotations.push_back(a);
    activeAnnotationIdx = (int)annotations.size() - 1;
    state = EditState::TYPING;
    typingStartMs = ofGetElapsedTimeMillis();
    return true;
}

bool AnnotationManager::onMouseDragged(glm::vec2 pos, glm::vec2 /*delta*/) {
    if (!enabled) return false;
    if (state == EditState::DRAGGING_BOX && activeAnnotationIdx >= 0) {
        Annotation & a = annotations[activeAnnotationIdx];
        a.labelBoxPos = screenToWorld(pos + dragOffset);
        recomputeControlPoint(a);
        return true;
    }
    return false;
}

bool AnnotationManager::onMouseReleased(glm::vec2 /*pos*/) {
    if (!enabled) return false;
    if (state == EditState::DRAGGING_BOX) {
        state = EditState::IDLE;
        activeAnnotationIdx = -1;
        return true;
    }
    return false;
}

// ============================================================
// Key events — returns true if key was consumed
// ============================================================

bool AnnotationManager::onKeyPressed(int key, const std::vector<DataPoint> & points) {
    // Tab always toggles mode regardless of typing state
    if (key == OF_KEY_TAB) {
        if (ofGetKeyPressed(OF_KEY_SHIFT)) {
            toggleVisible();
        } else {
            toggleEnabled();
        }
        return true;
    }

    if (!enabled) return false;

    // Delete / Backspace in IDLE: remove hovered annotation
    if (state == EditState::IDLE) {
        if (key == OF_KEY_DEL || key == OF_KEY_BACKSPACE) {
            glm::vec2 mousePos(ofGetMouseX(), ofGetMouseY());
            int boxIdx   = hitTestLabelBox(mousePos);
            int curveIdx = (boxIdx < 0) ? hitTestCurve(mousePos, kCurveHitThreshold) : -1;
            int hitIdx   = (boxIdx >= 0) ? boxIdx : curveIdx;
            if (hitIdx >= 0 && !annotations[hitIdx].isClusterAnnotation) {
                annotations.erase(annotations.begin() + hitIdx);
                return true;
            }
        }
        return false;
    }

    // TYPING state: consume all printable keys
    if (state == EditState::TYPING && activeAnnotationIdx >= 0
        && activeAnnotationIdx < (int)annotations.size())
    {
        Annotation & a = annotations[activeAnnotationIdx];

        if (key == OF_KEY_RETURN && ofGetKeyPressed(OF_KEY_SHIFT)) {
            a.labelText += '\n';
            return true;
        }

        if (key == OF_KEY_RETURN || key == OF_KEY_ESC) {
            // If we just created an empty annotation, remove it
            if (a.labelText.empty() && !a.isClusterAnnotation) {
                annotations.erase(annotations.begin() + activeAnnotationIdx);
            } else {
                // Resolve nearest point if not yet done
                if (a.nearestPointIdx < 0 && !points.empty()) {
                    a.nearestPointIdx = nearestPointTo(a.anchorPoint, points);
                }
            }
            state = EditState::IDLE;
            activeAnnotationIdx = -1;
            return true;
        }

        if (key == OF_KEY_BACKSPACE) {
            if (!a.labelText.empty()) {
                a.labelText.pop_back();
            }
            return true;
        }

        // Printable ASCII
        if (key >= 32 && key < 127) {
            a.labelText += (char)key;
            // Resolve nearest point on first character
            if (a.nearestPointIdx < 0 && !points.empty()) {
                a.nearestPointIdx = nearestPointTo(a.anchorPoint, points);
            }
            return true;
        }

        // Consume all other keys while typing to prevent instrument bindings
        return true;
    }

    return false;
}

// ============================================================
// Drawing
// ============================================================

void AnnotationManager::draw(const std::vector<DataPoint> & /*points*/, int activeClusterId) {
    if (!visible) return;

    float baseAlpha = enabled ? 1.0f : kInactiveAlpha;
    ofEnableAlphaBlending();

    for (int i = 0; i < (int)annotations.size(); ++i) {
        const Annotation & a = annotations[i];
        bool isActive = (i == activeAnnotationIdx);

        // Dim cluster annotations that don't belong to the foregrounded cluster.
        // activeClusterId == -999 means no filter — show everything at full alpha.
        float alpha = baseAlpha;
        if (activeClusterId != -999 && a.isClusterAnnotation && a.clusterId != activeClusterId) {
            alpha *= kDimmedClusterAlpha;
        }

        drawCurve(a, alpha);
        drawLabelBox(a, isActive, alpha);

        // Anchor dot — only when annotation mode is active
        if (enabled) {
            glm::vec2 anchorScreen = worldToScreen(a.anchorPoint);
            ofSetColor(255, 230, 80, (int)(220 * alpha));
            ofFill();
            ofDrawCircle(anchorScreen.x, anchorScreen.y, 4.5f);
        }
    }
}

void AnnotationManager::drawCurve(const Annotation & a, float alpha) const {
    glm::vec2 prev = evaluateBezier(a, 0.0f);
    ofNoFill();
    ofSetLineWidth(1.5f);

    // Dark outline
    ofSetColor(0, 0, 0, (int)(120 * alpha));
    ofSetLineWidth(3.0f);
    {
        ofPolyline pl;
        pl.addVertex(prev.x, prev.y, 0);
        for (int s = 1; s <= kBezierSteps; ++s) {
            float t = (float)s / (float)kBezierSteps;
            glm::vec2 p = evaluateBezier(a, t);
            pl.addVertex(p.x, p.y, 0);
        }
        pl.draw();
    }

    // Light curve on top
    ofSetColor(255, 240, 160, (int)(220 * alpha));
    ofSetLineWidth(1.5f);
    {
        glm::vec2 cur = evaluateBezier(a, 0.0f);
        ofPolyline pl;
        pl.addVertex(cur.x, cur.y, 0);
        for (int s = 1; s <= kBezierSteps; ++s) {
            float t = (float)s / (float)kBezierSteps;
            glm::vec2 p = evaluateBezier(a, t);
            pl.addVertex(p.x, p.y, 0);
        }
        pl.draw();
    }

    ofFill();
    ofSetLineWidth(1.0f);
}

void AnnotationManager::drawLabelBox(const Annotation & a, bool isActive,
                                     float alpha) const
{
    ofRectangle r = labelBoxRect(a);

    // Semi-transparent fill
    ofSetColor(20, 20, 30, (int)(190 * alpha));
    ofFill();
    ofDrawRectRounded(r, kCornerRadius);

    // Border — highlighted when active/typing
    if (isActive && (state == EditState::TYPING || state == EditState::DRAGGING_BOX)) {
        ofSetColor(255, 230, 80, (int)(255 * alpha)); // yellow when active
    } else {
        ofSetColor(200, 200, 170, (int)(180 * alpha));
    }
    ofNoFill();
    ofSetLineWidth(1.5f);
    ofDrawRectRounded(r, kCornerRadius);

    ofFill();
    ofSetLineWidth(1.0f);

    // Text
    std::string display = a.labelText;

    // Blinking cursor when typing this annotation
    if (isActive && state == EditState::TYPING) {
        uint64_t elapsed = ofGetElapsedTimeMillis() - typingStartMs;
        if ((elapsed / kBlinkPeriodMs) % 2 == 0) {
            display += "|";
        }
    }

    if (!display.empty()) {
        ofSetColor(240, 240, 220, (int)(255 * alpha));
        if (labelFont && labelFont->isLoaded()) {
            ofRectangle tb = labelFont->getStringBoundingBox(display, 0, 0);
            // Centre the text bounding box inside the label box.
            // drawString y is the baseline; tb.y is the (negative) offset from
            // baseline to the top of the bounding box, so -tb.y raises it.
            float textX = r.x + (r.width  - tb.width)  * 0.5f;
            float textY = r.y + (r.height - tb.height) * 0.5f - tb.y;
            labelFont->drawString(display, textX, textY);
        } else {
            ofDrawBitmapString(display,
                r.x + kBoxPadX,
                r.y + r.height * 0.65f);
        }
    } else if (isActive && state == EditState::TYPING) {
        // Empty label — show centred cursor
        ofSetColor(240, 240, 220, (int)(255 * alpha));
        ofDrawBitmapString("|", r.x + r.width * 0.5f - 3.0f, r.y + r.height * 0.65f);
    }
}

void AnnotationManager::expandWorldBounds(float & minX, float & minY,
                                          float & maxX, float & maxY) const {
    for (const auto & a : annotations) {
        minX = std::min(minX, a.anchorPoint.x);
        minY = std::min(minY, a.anchorPoint.y);
        maxX = std::max(maxX, a.anchorPoint.x);
        maxY = std::max(maxY, a.anchorPoint.y);

        ofRectangle r = labelBoxRect(a);
        glm::vec2 p0 = screenToWorld(glm::vec2(r.x, r.y));
        glm::vec2 p1 = screenToWorld(glm::vec2(r.x + r.width, r.y));
        glm::vec2 p2 = screenToWorld(glm::vec2(r.x, r.y + r.height));
        glm::vec2 p3 = screenToWorld(glm::vec2(r.x + r.width, r.y + r.height));

        minX = std::min(minX, std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x)));
        minY = std::min(minY, std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y)));
        maxX = std::max(maxX, std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x)));
        maxY = std::max(maxY, std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y)));
    }
}

// ============================================================
// Persistence (JSON)
// Note: cluster annotations are saved with isClusterAnnotation=true
// and loaded normally so they persist across sessions without needing
// the cluster data to be present at load time.
// ============================================================

void AnnotationManager::saveToFile(const std::string & path) {
    ofxJSONElement root;
    ofxJSONElement arr;
    for (int i = 0; i < (int)annotations.size(); ++i) {
        const Annotation & a = annotations[i];
        ofxJSONElement obj;
        obj["id"]               = a.id;
        obj["anchor_x"]         = a.anchorPoint.x;
        obj["anchor_y"]         = a.anchorPoint.y;
        obj["nearest_idx"]      = a.nearestPointIdx;
        obj["label_x"]          = a.labelBoxPos.x;
        obj["label_y"]          = a.labelBoxPos.y;
        obj["label_is_world"]   = true;
        obj["label_text"]       = a.labelText;
        obj["is_cluster"]       = a.isClusterAnnotation;
        obj["cluster_id"]       = a.clusterId;
        arr.append(obj);
    }
    root["annotations"] = arr;
    if (root.save(path, true)) {
        ofLogNotice("AnnotationManager") << "Saved " << annotations.size()
            << " annotations to " << path;
    } else {
        ofLogError("AnnotationManager") << "Failed to save annotations to " << path;
    }
}

void AnnotationManager::loadFromFile(const std::string & path,
                                     const std::vector<DataPoint> & points)
{
    annotations.clear();
    nextId = 0;

    ofxJSONElement root;
    if (!root.open(path)) {
        ofLogNotice("AnnotationManager") << "No annotation file found at " << path
            << " — starting fresh.";
        return;
    }

    if (!root.isMember("annotations") || !root["annotations"].isArray()) return;

    const ofxJSONElement & arr = root["annotations"];
    for (int i = 0; i < (int)arr.size(); ++i) {
        const ofxJSONElement & obj = arr[i];
        Annotation a;
        if (obj.isMember("id"))          a.id               = obj["id"].asInt();
        if (obj.isMember("anchor_x"))    a.anchorPoint.x    = obj["anchor_x"].asFloat();
        if (obj.isMember("anchor_y"))    a.anchorPoint.y    = obj["anchor_y"].asFloat();
        if (obj.isMember("nearest_idx")) a.nearestPointIdx  = obj["nearest_idx"].asInt();
        if (obj.isMember("label_x"))     a.labelBoxPos.x    = obj["label_x"].asFloat();
        if (obj.isMember("label_y"))     a.labelBoxPos.y    = obj["label_y"].asFloat();
        bool labelIsWorld = obj.isMember("label_is_world") ? obj["label_is_world"].asBool() : false;
        if (obj.isMember("label_text"))  a.labelText        = obj["label_text"].asString();
        if (obj.isMember("is_cluster"))  a.isClusterAnnotation = obj["is_cluster"].asBool();
        if (obj.isMember("cluster_id"))  a.clusterId        = obj["cluster_id"].asInt();

        // Backward compatibility: older files stored label box coordinates in
        // screen space. New files set label_is_world=true.
        if (!labelIsWorld) {
            a.labelBoxPos = screenToWorld(a.labelBoxPos);
        }

        // Never load unclustered-bucket annotations (cluster_id == -1);
        // they were blocked at creation time but may exist in older save files.
        if (a.isClusterAnnotation && a.clusterId == -1) continue;

        // Validate nearestPointIdx
        if (a.nearestPointIdx >= (int)points.size()) a.nearestPointIdx = -1;

        glm::vec2 minPt, maxPt;
        if (computePointsBounds(points, minPt, maxPt)) {
            clampAnnotationLabelToBounds(a, minPt, maxPt);
        }

        recomputeControlPoint(a);
        if (a.id >= nextId) nextId = a.id + 1;
        annotations.push_back(a);
    }

    ofLogNotice("AnnotationManager") << "Loaded " << annotations.size()
        << " annotations from " << path;
}
