#pragma once
#include "DataPoint.h"
#include "ofMain.h"
#include <queue>
#include <unordered_map>
#include <vector>

class SpatialGrid {
public:
	SpatialGrid();
	SpatialGrid(const std::vector<DataPoint> & points, float cellSize);

	std::string getGridKey(float x, float y);
	std::vector<DataPoint> findPointsInRadius(float centerX, float centerY,
		float radius);
	std::vector<DataPoint> findNearestNeighbors(float centerX, float centerY,
		int n);
	void clear();

	float minX, maxX, minY, maxY;

private:
	float cellSize;
	std::unordered_map<std::string, std::vector<DataPoint>> grid;
};
