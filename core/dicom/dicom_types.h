#pragma once
// DICOM 解析层公共数据结构：纯 C++ + STL，不依赖 Qt/VTK（core 分层硬约束）

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cbct::dicom {

/// 单序列体数据（内存内普通结构，与 VTK/Qt 无关）
struct DicomVolumeData {
    // ---- 像素 ----
    // 尺寸 = dims[0]*dims[1]*dims[2]；顺序 x fastest，再 y，再 z（与 vtkImageData 一致）
    std::vector<std::int16_t> pixels;
    int dims[3] = {0, 0, 0};  // [width, height, sliceCount]，对应 vtk x,y,z

    // ---- 几何（DICOM LPS 坐标系，单位 mm）----
    double spacing[3] = {1.0, 1.0, 1.0};
    double origin[3] = {0.0, 0.0, 0.0};  // 第一层第一像素中心（ImagePositionPatient）
    // 方向余弦，列 j = 体素索引 j 增长方向（由 ImageOrientationPatient 构造）
    double direction[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

    // ---- 灰度 ----
    // HU = pixels[i] * slope + intercept；pixels 保持原始存储值
    double slope = 1.0;
    double intercept = 0.0;

    double windowWidth = 400.0;  // 默认值，可被 0028,1051 覆盖
    double windowLevel = 40.0;   // 默认值，可被 0028,1050 覆盖

    // ---- 元数据（key 见 dicom_series_loader.cpp 写入处）----
    std::map<std::string, std::string> metadata;
    std::string seriesUID;
    std::string modality;
    int seriesFileCount = 0;  // 序列原始文件数（多序列选择用）

    std::vector<std::string> sliceFiles;  // 已按空间位置排序的文件路径
};

enum class DicomLoadError {
    Ok = 0,
    EmptyDirectory,
    NoDicomSeries,
    NotDicomFile,
    ReadFailed,
    UnsupportedPixelType,
    MixedSeries,  // 多文件分属不同序列
};

const char* toString(DicomLoadError e);

}  // namespace cbct::dicom
