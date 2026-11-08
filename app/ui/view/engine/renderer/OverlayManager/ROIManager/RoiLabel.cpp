#include "RoiLabel.h"

#include "common/ViewTypes.h"

#include <vtkRenderer.h>
#include <vtkCamera.h>
#include <vtkFollower.h>
#include <vtkVectorText.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>

// ============================================================
//  析构
// ============================================================

RoiLabel::~RoiLabel()
{
    Shutdown();
}

// ============================================================
//  生命周期
// ============================================================

void RoiLabel::Initialize(vtkRenderer* renderer)
{
    if (m_initialized || !renderer) return;
    m_renderer = renderer;
    m_initialized = true;
}

void RoiLabel::Shutdown()
{
    if (!m_initialized) return;

    for (auto& row : m_rows) {
        if (row.follower && m_renderer) {
            m_renderer->RemoveViewProp(row.follower);
        }
        row.follower = nullptr;
        row.textSrc = nullptr;
    }
    m_rows.clear();
    m_renderer = nullptr;
    m_initialized = false;
}

// ============================================================
//  内容更新
// ============================================================

void RoiLabel::SetLines(const std::vector<std::string>& lines)
{
    if (!m_initialized) return;

    const int n = static_cast<int>(lines.size());

    // 确保有足够的行对象
    EnsureRowCount(n);

    // 更新每行文字
    for (int i = 0; i < n; ++i) {
        m_rows[i].textSrc->SetText(lines[i].c_str());
        m_rows[i].textSrc->Modified();
        m_rows[i].follower->SetVisibility(true);
    }

    // 隐藏多余的行（不销毁，下次可复用）
    for (int i = n; i < static_cast<int>(m_rows.size()); ++i) {
        m_rows[i].follower->SetVisibility(false);
    }
}

void RoiLabel::SetAnchor(const std::array<double, 3>& anchor, int viewType)
{
    if (!m_initialized) return;

    // 根据视图方向确定"向下"的轴和方向
    // 各视图在屏幕上"向下"对应的世界坐标方向：
    //   Axial(0)    → Y 轴负方向 (axis=1, sign=-1)
    //   Sagittal(1) → Z 轴负方向 (axis=2, sign=-1)
    //   Coronal(2)  → Z 轴负方向 (axis=2, sign=-1)
    int    downAxis = 1;
    double downSign = -1.0;

    switch (static_cast<ViewType>(viewType)) {
    case ViewType::Axial:
        downAxis = 1; downSign = -1.0;
        break;
    case ViewType::Sagittal:
        downAxis = 2; downSign = -1.0;
        break;
    case ViewType::Coronal:
        downAxis = 2; downSign = -1.0;
        break;
    default:
        break;
    }

    // 依次设置每行的世界坐标位置
    for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
        std::array<double, 3> pos = anchor;
        // 第 i 行在第 0 行下方 i * lineSpacingMm
        pos[downAxis] += downSign * i * lineSpacingMm;
        m_rows[i].follower->SetPosition(pos[0], pos[1], pos[2]);
    }
}

void RoiLabel::SetVisible(bool visible)
{
    for (auto& row : m_rows) {
        if (row.follower) {
            row.follower->SetVisibility(visible);
        }
    }
}

// ============================================================
//  私有方法
// ============================================================

void RoiLabel::EnsureRowCount(int n)
{
    if (!m_renderer) return;

    const int current = static_cast<int>(m_rows.size());
    for (int i = current; i < n; ++i) {
        LabelRow row;

        // 文字数据源
        row.textSrc = vtkSmartPointer<vtkVectorText>::New();
        row.textSrc->SetText("");

        // Mapper
        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(row.textSrc->GetOutputPort());

        // Follower（始终朝向相机）
        row.follower = vtkSmartPointer<vtkFollower>::New();
        row.follower->SetMapper(mapper);
        row.follower->SetScale(scale, scale, scale);
        row.follower->SetCamera(m_renderer->GetActiveCamera());
        row.follower->GetProperty()->SetColor(1.0, 1.0, 0.0);  // 黄色
        row.follower->SetVisibility(false);  // 初始隐藏，SetLines 时再显示

        m_renderer->AddViewProp(row.follower);
        m_rows.push_back(std::move(row));
    }
}