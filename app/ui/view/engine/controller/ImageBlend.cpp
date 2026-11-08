#include "ImageBlend.h"

#include <algorithm>
#include <cmath>

#include <vtkImageCast.h>

#include "common/log_service.h"

ImageBlend::ImageBlend()
{
    m_blender = vtkSmartPointer<vtkImageBlend>::New();
    m_blender->SetBlendModeToNormal();
}

void ImageBlend::SetPrimaryImage(vtkImageData *image)
{
    if (!image) return;
    m_primary = image;
    m_dirty = true;
}

void ImageBlend::SetSecondaryImage(vtkImageData *image)
{
    if (!image) return;

    // 与原版一致：两图必须同网格，否则拒绝（重采样留二期）
    if (m_primary) {
        int dimsPri[3], dimsSec[3];
        m_primary->GetDimensions(dimsPri);
        image->GetDimensions(dimsSec);
        if (dimsPri[0] != dimsSec[0] || dimsPri[1] != dimsSec[1] || dimsPri[2] != dimsSec[2]) {
            CBCT_LOG_WARN("ui", "混合图像尺寸不一致 CT={}x{}x{} 叠加={}x{}x{}，需先重采样",
                          dimsPri[0], dimsPri[1], dimsPri[2],
                          dimsSec[0], dimsSec[1], dimsSec[2]);
            return;
        }
    }

    m_secondary = image;
    m_dirty = true;
}

void ImageBlend::SetBlendRatio(double ratio)
{
    ratio = std::clamp(ratio, 0.0, 1.0);
    if (std::abs(ratio - m_ratio) < 1e-6) return;
    m_ratio = ratio;
    m_dirty = true;
}

bool ImageBlend::IsReady() const
{
    return m_primary != nullptr && m_secondary != nullptr;
}

vtkImageData *ImageBlend::GetBlendedImage()
{
    if (m_dirty) {
        updateBlend();
        m_dirty = false;
    }

    if (!m_secondary) return m_primary;
    if (m_ratio < 1e-6) return m_primary;
    if (m_ratio > 1.0 - 1e-6) return m_secondary;
    return m_blender->GetOutput();
}

void ImageBlend::updateBlend()
{
    if (!m_primary || !m_secondary) return;
    if (m_ratio < 1e-6 || m_ratio > 1.0 - 1e-6) return;

    // vtkImageBlend 要求同标量类型，统一转 double
    auto castPrimary = vtkSmartPointer<vtkImageCast>::New();
    castPrimary->SetInputData(m_primary);
    castPrimary->SetOutputScalarTypeToDouble();
    castPrimary->Update();

    auto castSecondary = vtkSmartPointer<vtkImageCast>::New();
    castSecondary->SetInputData(m_secondary);
    castSecondary->SetOutputScalarTypeToDouble();
    castSecondary->Update();

    m_blender->RemoveAllInputs();
    m_blender->AddInputConnection(castPrimary->GetOutputPort());
    m_blender->AddInputConnection(castSecondary->GetOutputPort());
    m_blender->SetOpacity(0, 1.0 - m_ratio);
    m_blender->SetOpacity(1, m_ratio);
    m_blender->Update();
}