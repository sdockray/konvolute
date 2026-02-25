#pragma once
#include "ofMain.h"

struct DataPoint {
    float x, y;
    std::string filename;
    std::string text;

    DataPoint() : x(0), y(0), filename(""), text("") {}
    DataPoint(float _x, float _y, std::string _filename, std::string _text)
        : x(_x), y(_y), filename(_filename), text(_text) {}

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
