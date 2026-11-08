#pragma once
// 双图像混合控制器（移植自 DicomImageStudy/Controller/ImageBlend）
// 纯 VTK 算子，属于渲染引擎层；core/dicom 只负责解析，不碰混合。

#include <vtkImageData.h>
#include <vtkImageBlend.h>
#include <vtkSmartPointer.h>

/**
 * @brief CT/CBCT 透明混合
 *
 * output = (1 - ratio) * primary + ratio * secondary
 * ratio = 0 → 纯 CT；ratio = 1 → 纯 CBCT
 *
 * 前置条件：两图 dimensions / spacing / origin 一致
 * （不一致时 SetSecondaryImage 拒绝并记日志，对齐原版；
 *  重采样到 CT 网格留给二期，不引 ITK）
 */
class ImageBlend {
public:
    ImageBlend();
    void SetPrimaryImage(vtkImageData *image);
    void SetSecondaryImage(vtkImageData *image);
    void SetBlendRatio(double ratio);

    double GetBlendRatio() const { return m_ratio; }
    bool IsReady() const;

    /// 混合结果（无 secondary / ratio 为端点时直接返回对应源图）
    vtkImageData *GetBlendedImage();

private:
    void updateBlend();

    vtkSmartPointer<vtkImageData> m_primary;
    vtkSmartPointer<vtkImageData> m_secondary;
    vtkSmartPointer<vtkImageBlend> m_blender;
    double m_ratio = 0.5;
    bool m_dirty = true;
};