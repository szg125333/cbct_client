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