/**
 * @file ois_types.h
 * @brief 与 ois_server 对应的数据类型（检查/序列/下载结果）
 */
#ifndef CBCT_OIS_TYPES_H
#define CBCT_OIS_TYPES_H

#include <QList>
#include <QMetaType>
#include <QString>

namespace cbct {

/// 一条检查（对应 ois query_study 返回项）
struct OisStudy {
    QString studyId;      ///< 服务端 study_id
    QString patientName;
    QString modality;
    QString studyDate;
    QString studyDesc;
};

/// 一条序列（对应 ois query_series 返回项）
struct OisSeries {
    QString seriesId;
    int seriesNum = 0;
    QString seriesDesc;
    int instanceCount = 0;
};

/// 下载结果：本地保存目录与文件数
struct OisDownloadResult {
    QString localDir;     ///< 本地目录绝对路径
    int fileCount = 0;    ///< 落盘切片数
    qint64 totalBytes = 0;
    QString patientId;
    QString studyUid;
    QString seriesUid;
};

} // namespace cbct

Q_DECLARE_METATYPE(cbct::OisStudy)
Q_DECLARE_METATYPE(QList<cbct::OisStudy>)
Q_DECLARE_METATYPE(cbct::OisSeries)
Q_DECLARE_METATYPE(QList<cbct::OisSeries>)
Q_DECLARE_METATYPE(cbct::OisDownloadResult)

#endif // CBCT_OIS_TYPES_H
