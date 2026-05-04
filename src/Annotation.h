#pragma once

#include "DataPoint.h"
#include "ofMain.h"
#include "ofxJSON.h"
#include <string>
#include <vector>

// ------------------------------------------------------------
// An annotation is a curved callout label sitting above the
// point cloud. anchorPoint and labelBoxPos are in WORLD coordinates
// (same space as DataPoint::x / y), so they move with pan/zoom and can
// go offscreen naturally. controlPoint remains in SCREEN coordinates and
// is derived automatically for the rendered Bezier.
// ------------------------------------------------------------
struct Annotation {
    int id = -1;

    // World-space anchor — updated every frame to track nearestPointIdx
    glm::vec2 anchorPoint { 0, 0 };

    // Index into the DataPoint array; -1 = no tracking
    int nearestPointIdx = -1;

    // Bezier control point in screen coords (auto-computed)
    glm::vec2 controlPoint { 0, 0 };

    // World-space label box position (top-left corner)
    glm::vec2 labelBoxPos { 0, 0 };

    std::string labelText;

    // Cluster provenance — cluster annotations are regenerated on load,
    // so they are saved with isClusterAnnotation=true and reconstructed
    // from the cluster data rather than the stored position.
    bool isClusterAnnotation = false;
    int  clusterId = -1;
};

// ------------------------------------------------------------
class AnnotationManager {
public:
    // Pass zoom / pan references so the manager can do world<->screen
    // conversions without depending on ofApp directly.
    void init(float * zoomRef, ofVec2f * panRef, ofTrueTypeFont * labelFontRef);

    // Called every frame from ofApp::update()
    void update(const std::vector<DataPoint> & points);

    // Called from ofApp::drawVisuals() AFTER ofPopMatrix() so annotations
    // are drawn in screen space.
    // activeClusterId: pass ofApp::activeClusterId (-999 = no filter).
    // Cluster annotations for non-active clusters are drawn at reduced opacity.
    void draw(const std::vector<DataPoint> & points, int activeClusterId = -999);

    // Forwarded from ofApp mouse/key handlers.
    // pos is raw screen position.  Returns true if the event was consumed
    // (annotation mode handled it) so the caller can skip its own logic.
    bool onMousePressed(glm::vec2 pos, int button);
    bool onMouseDragged(glm::vec2 pos, glm::vec2 delta);
    bool onMouseReleased(glm::vec2 pos);
    // Returns true when in TYPING state (key is consumed, do not forward)
    bool onKeyPressed(int key, const std::vector<DataPoint> & points);

    // Toggle annotation mode (Tab key)
    void toggleEnabled();
    // Toggle annotation visibility (Shift+Tab)
    void toggleVisible();

    bool isEnabled()  const { return enabled;  }
    bool isVisible()  const { return visible;  }
    bool isTyping()   const { return state == EditState::TYPING; }

    // Cluster annotation API
    void addClusterAnnotation(int clusterId,
                              const std::string & label,
                              const std::vector<DataPoint> & points);
    void removeAnnotationsForCluster(int clusterId);

    // Recompute world-space anchors from new point positions (call after
    // cloud mode changes)
    void refreshAnchors(const std::vector<DataPoint> & points);

    // Move every label box so it is fully inside the current point bounds.
    void clampAllLabelsToPointsBounds(const std::vector<DataPoint> & points);

    // Persistence — JSON
    void saveToFile(const std::string & path);
    void loadFromFile(const std::string & path,
                      const std::vector<DataPoint> & points);

    // Expand world bounds to include annotation anchors and label boxes.
    void expandWorldBounds(float & minX, float & minY, float & maxX, float & maxY) const;

private:
    // ---- State machine ----
    enum class EditState { IDLE, TYPING, DRAGGING_BOX };
    EditState state = EditState::IDLE;

    int  activeAnnotationIdx = -1;
    glm::vec2 dragOffset { 0, 0 };

    bool enabled  = false;
    bool visible  = true;
    int  nextId   = 0;

    // Blink cursor timer
    uint64_t typingStartMs = 0;

    // ---- Storage ----
    std::vector<Annotation> annotations;

    // ---- External references (set in init) ----
    float *          zoomRef  = nullptr;
    ofVec2f *        panRef   = nullptr;
    ofTrueTypeFont * labelFont = nullptr;

    // ---- Helpers ----
    glm::vec2 worldToScreen(glm::vec2 world) const;
    glm::vec2 screenToWorld(glm::vec2 screen) const;

    void recomputeControlPoint(Annotation & a);

    // Box geometry
    ofRectangle labelBoxRect(const Annotation & a) const;

    // Hit tests (screen coords)
    int hitTestLabelBox(glm::vec2 pos) const;   // returns idx or -1
    int hitTestCurve(glm::vec2 pos, float threshold = 8.0f) const;

    // Bezier
    glm::vec2 evaluateBezier(const Annotation & a, float t) const;

    // Find nearest DataPoint to a world-space position
    int nearestPointTo(glm::vec2 worldPos,
                       const std::vector<DataPoint> & points) const;

    bool computePointsBounds(const std::vector<DataPoint> & points,
                             glm::vec2 & minPt,
                             glm::vec2 & maxPt) const;
    void clampAnnotationLabelToBounds(Annotation & a,
                                      const glm::vec2 & minPt,
                                      const glm::vec2 & maxPt) const;

    // Draw helpers
    void drawCurve(const Annotation & a, float alpha) const;
    void drawLabelBox(const Annotation & a, bool active, float alpha) const;
};
