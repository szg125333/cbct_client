/**
 * @file contour_list_popup.h
 * @brief RTSTRUCT 轮廓列表弹窗：每行 = 色块 + ROI 名 + 显隐 checkbox
 *
 * 移植自 DicomImageStudy/UI/Dialog/ContourListPopup；只依赖 Qt + ROIDisplayInfo，
 * ROI 显隐通过信号交回 MainWindow → ThreeViewPanel（UI 层不碰引擎内部）。
 */
#ifndef CONTOUR_LIST_POPUP_H
#define CONTOUR_LIST_POPUP_H

#include <QWidget>
#include <map>
#include <vector>

#include "common/ROIDisplayInfo.h"

class QCheckBox;
class QVBoxLayout;
class QScrollArea;

class ContourListPopup : public QWidget
{
    Q_OBJECT

public:
    explicit ContourListPopup(QWidget *parent = nullptr);

    /// 刷新 ROI 列表（会清空重建）
    void setRoiList(const std::vector<ROIDisplayInfo> &roiList);
    void clear();

signals:
    void roiVisibilityChanged(int roiNumber, bool visible);
    void allVisibilityChanged(bool visible);

protected:
    void changeEvent(QEvent *event) override;  // 失活自动隐藏

private:
    void buildUi();
    QWidget *createRoiItem(const ROIDisplayInfo &roi);

    QVBoxLayout *m_listLayout = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    std::map<int, QCheckBox *> m_checkboxes;
};

#endif  // CONTOUR_LIST_POPUP_H
