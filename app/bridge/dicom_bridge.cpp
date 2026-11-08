#include "dicom_bridge.h"

#include "dicom/dicom_series_loader.h"
#include "dicom/rt_struct_loader.h"

#include "common/log_service.h"

// 引擎侧纯数据结构（VTK 轮廓叠加用）
#include "ui/view/engine/utils/RTStructureData.h"

#include <vtkImageData.h>
#include <vtkMatrix3x3.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>

#include <QFileInfo>
#include <QtConcurrent/QtConcurrentRun>
#include <QMetaObject>

#include <cstring>

DicomBridge::DicomBridge(QObject *parent)
    : QObject(parent)
{
}

void DicomBridge::startLoad(
    std::function<cbct::dicom::DicomVolumeData(cbct::dicom::DicomLoadError *)> loader)
{
    if (m_loading.exchange(true)) {
        m_lastError = QStringLiteral("已有加载任务进行中");
        emit loadFailed(m_lastError);
        return;
    }
    emit loadStarted();

    // core 解析在工作线程；VTK 转换与信号回到主线程
    QtConcurrent::run([this, loader = std::move(loader)]() {
        cbct::dicom::DicomLoadError err{};
        cbct::dicom::DicomVolumeData volume = loader(&err);

        QMetaObject::invokeMethod(
            this,
            [this, volume = std::move(volume), err]() mutable {
                m_loading = false;
                if (err != cbct::dicom::DicomLoadError::Ok) {
                    m_lastError = QString::fromUtf8(cbct::dicom::toString(err));
                    CBCT_LOG_WARN("dicom", "加载失败 err={}", m_lastError.toStdString());
                    emit loadFailed(m_lastError);
                    return;
                }
                adoptVolume(std::move(volume));
            },
            Qt::QueuedConnection);
    });
}

bool DicomBridge::loadDirectory(const QString &directory)
{
    const std::string dir = directory.toStdString();
    startLoad([dir](cbct::dicom::DicomLoadError *err) {
        return cbct::dicom::DicomSeriesLoader::loadDirectory(dir, err);
    });
    return true;
}

QVector<DicomBridge::SeriesSummary> DicomBridge::enumerateSeries(const QString &directory)
{
    QVector<SeriesSummary> out;
    auto series = cbct::dicom::DicomSeriesLoader::enumerateSeries(directory.toStdString());
    out.reserve(static_cast<int>(series.size()));
    for (const auto &s : series) {
        SeriesSummary item;
        item.seriesUID = QString::fromStdString(s.seriesUID);
        item.modality = QString::fromStdString(s.modality);
        item.description = QString::fromStdString(s.description);
        item.fileCount = s.fileCount;
        out.push_back(item);
    }
    return out;
}

bool DicomBridge::loadSeriesAt(const QString &directory, int index)
{
    const std::string dir = directory.toStdString();
    startLoad([dir, index](cbct::dicom::DicomLoadError *err) {
        auto series = cbct::dicom::DicomSeriesLoader::enumerateSeries(dir);
        if (index < 0 || index >= static_cast<int>(series.size())) {
            if (err) *err = cbct::dicom::DicomLoadError::NoDicomSeries;
            return cbct::dicom::DicomVolumeData{};
        }
        return cbct::dicom::DicomSeriesLoader::loadSeries(
            series[static_cast<size_t>(index)], err);
    });
    return true;
}

bool DicomBridge::loadFiles(const QStringList &files)
{
    if (files.isEmpty()) {
        m_lastError = QStringLiteral("未选择文件");
        emit loadFailed(m_lastError);
        return false;
    }
    std::vector<std::string> paths;
    paths.reserve(static_cast<size_t>(files.size()));
    for (const auto &f : files) paths.push_back(f.toStdString());

    startLoad([paths = std::move(paths)](cbct::dicom::DicomLoadError *err) {
        return cbct::dicom::DicomSeriesLoader::loadFiles(paths, err);
    });
    return true;
}

bool DicomBridge::loadRtStruct(const QString &filePath)
{
    auto src = cbct::dicom::RtStructLoader::load(filePath.toStdString());
    if (!src) {
        m_lastError = QStringLiteral("RTSTRUCT 解析失败");
        emit loadFailed(m_lastError);
        return false;
    }

    // core 纯结构体 → 引擎 RTStructureData
    auto data = std::make_shared<RTStructureData>();
    data->label = src->label;
    data->name = src->name;
    for (const auto &[num, sroi] : src->rois) {
        RTStructureROI roi;
        roi.roiNumber = sroi.roiNumber;
        roi.name = sroi.name;
        roi.color = sroi.color;
        roi.contours.reserve(sroi.contours.size());
        for (const auto &sc : sroi.contours) {
            RTContour c;
            c.z = sc.z;
            c.points = sc.points;
            roi.contours.push_back(std::move(c));
        }
        data->rois[num] = std::move(roi);
    }

    CBCT_LOG_INFO("dicom", "RTSTRUCT 加载成功 file={} rois={}",
                  filePath.toStdString(), data->rois.size());
    emit rtStructLoaded(data);
    return true;
}

bool DicomBridge::loadSecondaryFiles(const QStringList &files)
{
    if (files.isEmpty()) {
        m_lastError = QStringLiteral("未选择文件");
        emit loadFailed(m_lastError);
        return false;
    }
    std::vector<std::string> paths;
    paths.reserve(static_cast<size_t>(files.size()));
    for (const auto &f : files) paths.push_back(f.toStdString());

    if (m_loading.exchange(true)) {
        m_lastError = QStringLiteral("已有加载任务进行中");
        emit loadFailed(m_lastError);
        return false;
    }
    emit loadStarted();

    QtConcurrent::run([this, paths = std::move(paths)]() {
        cbct::dicom::DicomLoadError err{};
        auto volume = cbct::dicom::DicomSeriesLoader::loadFiles(paths, &err);

        QMetaObject::invokeMethod(
            this,
            [this, volume = std::move(volume), err]() mutable {
                m_loading = false;
                if (err != cbct::dicom::DicomLoadError::Ok) {
                    m_lastError = QString::fromUtf8(cbct::dicom::toString(err));
                    emit loadFailed(m_lastError);
                    return;
                }
                auto *img = toVtkImageData(volume);
                if (!img) {
                    m_lastError = QStringLiteral("转换 VTK 图像失败");
                    emit loadFailed(m_lastError);
                    return;
                }
                CBCT_LOG_INFO("dicom", "叠加图加载成功 n={} {}x{}x{}",
                              volume.dims[2], volume.dims[0], volume.dims[1], volume.dims[2]);
                emit secondaryLoaded(img);
            },
            Qt::QueuedConnection);
    });
    return true;
}

QString DicomBridge::summary() const
{
    if (m_volume.dims[0] == 0) return {};
    return QStringLiteral("%1 张 · %2×%3 · %4mm")
        .arg(m_volume.dims[2])
        .arg(m_volume.dims[0])
        .arg(m_volume.dims[1])
        .arg(m_volume.spacing[0], 0, 'f', 3);
}

bool DicomBridge::adoptVolume(cbct::dicom::DicomVolumeData &&volume)
{
    m_volume = std::move(volume);
    m_image = toVtkImageData(m_volume);
    if (!m_image) {
        m_lastError = QStringLiteral("转换 VTK 图像失败");
        emit loadFailed(m_lastError);
        return false;
    }
    CBCT_LOG_INFO("dicom", "加载成功 series={} mod={} {}x{}x{}",
                  m_volume.seriesUID, m_volume.modality, m_volume.dims[0],
                  m_volume.dims[1], m_volume.dims[2]);
    emit volumeLoaded();
    return true;
}

vtkImageData *DicomBridge::toVtkImageData(const cbct::dicom::DicomVolumeData &volume)
{
    if (volume.dims[0] <= 0 || volume.dims[1] <= 0 || volume.dims[2] <= 0 ||
        volume.pixels.empty()) {
        return nullptr;
    }

    auto *image = vtkImageData::New();
    image->SetDimensions(volume.dims[0], volume.dims[1], volume.dims[2]);
    image->SetSpacing(volume.spacing[0], volume.spacing[1], volume.spacing[2]);
    image->SetOrigin(volume.origin[0], volume.origin[1], volume.origin[2]);

    // 方向矩阵（VTK9 需 vtkMatrix3x3；行主序拷入）
    vtkNew<vtkMatrix3x3> dirMat;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            dirMat->SetElement(r, c, volume.direction[r][c]);
    image->SetDirectionMatrix(dirMat);

    // 拷贝像素（解耦 DicomVolumeData 生命周期）
    image->AllocateScalars(VTK_SHORT, 1);
    auto *scalars = image->GetPointData()->GetScalars();
    const vtkIdType count =
        static_cast<vtkIdType>(volume.dims[0]) * volume.dims[1] * volume.dims[2];
    std::memcpy(scalars->GetVoidPointer(0), volume.pixels.data(),
                static_cast<size_t>(count) * sizeof(std::int16_t));
    scalars->Modified();
    image->Modified();
    return image;
}
