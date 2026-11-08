#pragma once

#include <array>

// 在类外或命名空间内
enum class AnglePointRole {
	None = 0,
	Start,
	Middle, // 顶点
	End
};

struct EditableAnglePoint {
	int measurementId = -1;
	AnglePointRole role = AnglePointRole::None;
};

// 一条可导出的测量记录（纯数据，无 VTK）
struct MeasurementRecord {
	enum class Kind { Distance, Angle };
	Kind kind = Kind::Distance;
	int id = -1;
	int viewIndex = 0;
	bool complete = false;
	double value = 0.0;                  // Distance = mm；Angle = 度
	// Distance: [0]=start [1]=end；Angle: [0]=start [1]=vertex [2]=end
	std::array<double, 3> points[3] = {};
	int pointCount = 0;
};

// 一条可导出的 ROI 统计记录（纯数据，无 VTK）
// 矩形 ROI 无 perimeter（保持 0），手绘 ROI 有周长
struct RoiStatsRecord {
	enum class Kind { Rect, Freehand };
	Kind kind = Kind::Rect;
	int id = -1;
	int viewIndex = 0;
	bool complete = false;
	double area = 0.0;        // mm²
	double perimeter = 0.0;   // mm（仅手绘）
	double mean = 0.0;        // HU
	double stdDev = 0.0;      // HU
	double minVal = 0.0;      // HU
	double maxVal = 0.0;      // HU
	int pixelCount = 0;
};