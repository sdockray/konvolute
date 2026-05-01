#pragma once
#include "ofMain.h"
#include <vector>

struct ClusterInfo {
    int id;
    std::string label;
    int member_count;
};

struct DataPoint {
    float x, y;
    std::string filename;
    std::string text;

    ofVec2f pos_local;
    ofVec2f pos_mid;
    ofVec2f pos_global;
    float instability;
    std::vector<int> true_neighbors;
    std::vector<float> true_distances;
    int cluster_id;
    float cluster_membership;
    float attack;
    float brightness;

    DataPoint() : x(0), y(0), filename(""), text(""),
                  pos_local(0,0), pos_mid(0,0), pos_global(0,0),
                  instability(0), cluster_id(-1), cluster_membership(0),
                  attack(0), brightness(0) {}

    DataPoint(float _x, float _y, std::string _filename, std::string _text)
        : x(_x), y(_y), filename(_filename), text(_text),
          pos_local(0,0), pos_mid(0,0), pos_global(0,0),
          instability(0), cluster_id(-1), cluster_membership(0),
          attack(0), brightness(0) {}

    // Equality operator for HashSet equivalent (std::unordered_set)
    bool operator==(const DataPoint& other) const {
        return x == other.x && y == other.y && filename == other.filename;
    }
};

// Hash function for DataPoint to be used in std::unordered_set
namespace std {
    template <>
    struct hash<DataPoint> {
        std::size_t operator()(const DataPoint& k) const {
            using std::size_t;
            using std::hash;
            using std::string;

            return ((hash<string>()(k.filename)
                     ^ (hash<float>()(k.x) << 1)) >> 1)
                   ^ (hash<float>()(k.y) << 1);
        }
    };
}
