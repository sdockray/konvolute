#include "SpatialGrid.h"
#include <algorithm>
#include <cmath>

SpatialGrid::SpatialGrid()
	: cellSize(50)
	, minX(0)
	, maxX(0)
	, minY(0)
	, maxY(0) { }

SpatialGrid::SpatialGrid(const std::vector<DataPoint> & points,
	float _cellSize) {
	this->cellSize = _cellSize;

	if (points.size() > 0) {
		minX = maxX = points[0].x;
		minY = maxY = points[0].y;

		for (const auto & point : points) {
			minX = std::min(minX, point.x);
			maxX = std::max(maxX, point.x);
			minY = std::min(minY, point.y);
			maxY = std::max(maxY, point.y);
		}

		for (const auto & point : points) {
			std::string key = getGridKey(point.x, point.y);
			grid[key].push_back(point);
		}
	}
}

std::string SpatialGrid::getGridKey(float x, float y) {
	int gridX = (int)floor(x / cellSize);
	int gridY = (int)floor(y / cellSize);
	return std::to_string(gridX) + "," + std::to_string(gridY);
}

std::vector<DataPoint>
SpatialGrid::findPointsInRadius(float centerX, float centerY, float radius) {
	std::vector<DataPoint> result;

	int minGridX = (int)floor((centerX - radius) / cellSize);
	int maxGridX = (int)floor((centerX + radius) / cellSize);
	int minGridY = (int)floor((centerY - radius) / cellSize);
	int maxGridY = (int)floor((centerY + radius) / cellSize);

	for (int gridX = minGridX; gridX <= maxGridX; gridX++) {
		for (int gridY = minGridY; gridY <= maxGridY; gridY++) {
			std::string key = std::to_string(gridX) + "," + std::to_string(gridY);
			if (grid.find(key) != grid.end()) {
				for (const auto & point : grid[key]) {
					float distance = ofDist(centerX, centerY, point.x, point.y);
					// Use standard Euclidean distance check
					if (distance <= radius) {
						result.push_back(point);
					}
				}
			}
		}
	}

	return result;
}

struct DistancePoint {
	DataPoint point;
	float distance;

	bool operator>(const DistancePoint & other) const {
		return distance > other.distance; // Min-heap logic if needed, but
			// priority_queue is max-heap by default
	}
	// For Max Heap (priority_queue default), we want larger distances at the top
	// to pop them
	bool operator<(const DistancePoint & other) const {
		return distance < other.distance;
	}
};

std::vector<DataPoint> SpatialGrid::findNearestNeighbors(float centerX,
	float centerY, int n) {
	// Priority queue to store nearest neighbors (max heap to keep track of the n
	// smallest distances)
	std::priority_queue<DistancePoint> heap;

	float searchRadius = cellSize * 2;
	std::vector<DataPoint> candidates = findPointsInRadius(centerX, centerY, searchRadius);

	// Expand search if needed
	while (candidates.size() < n * 2 && searchRadius < std::max(maxX - minX, maxY - minY)) {
		searchRadius *= 2;
		candidates = findPointsInRadius(centerX, centerY, searchRadius);
	}

	for (const auto & point : candidates) {
		float distance = ofDist(centerX, centerY, point.x, point.y);

		if (heap.size() < n) {
			heap.push({ point, distance });
		} else if (distance < heap.top().distance) {
			heap.pop();
			heap.push({ point, distance });
		}
	}

	std::vector<DataPoint> result;
	// Extract elements from heap
	std::vector<DistancePoint> sortedList;
	while (!heap.empty()) {
		sortedList.push_back(heap.top());
		heap.pop();
	}

	// Sort by distance (nearest first)
	std::sort(sortedList.begin(), sortedList.end(),
		[](const DistancePoint & a, const DistancePoint & b) {
			return a.distance < b.distance;
		});

	for (const auto & dp : sortedList) {
		result.push_back(dp.point);
	}

	return result;
}

void SpatialGrid::clear() {
	grid.clear();
}
