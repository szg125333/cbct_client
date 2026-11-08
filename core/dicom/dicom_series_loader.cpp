#include "dicom_series_loader.h"

#include <gdcmAttribute.h>
#include <gdcmImageHelper.h>
#include <gdcmImageReader.h>
#include <gdcmReader.h>
#include <gdcmScanner.h>
#include <gdcmStringFilter.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <map>

#include "common/log_service.h"

namespace fs = std::filesystem;

namespace cbct::dicom {

const char* toString(DicomLoadError e) {
    switch (e) {
        case DicomLoadError::Ok: return "ok";
        case DicomLoadError::EmptyDirectory: return "empty directory";
        case DicomLoadError::NoDicomSeries: return "no DICOM series found";
        case DicomLoadError::NotDicomFile: return "not a DICOM file";
        case DicomLoadError::ReadFailed: return "read failed";
        case DicomLoadError::UnsupportedPixelType: return "unsupported pixel type";
        case DicomLoadError::MixedSeries: return "files belong to different series";
    }
    return "unknown error";
}

namespace {

constexpr const char* kTagModality[] = {"0008", "0060"};
constexpr const char* kTagSeriesUID[] = {"0020", "000e"};
constexpr const char* kTagStudyUID[] = {"0020", "000d"};
constexpr const char* kTagSeriesDesc[] = {"0008", "103e"};
constexpr const char* kTagIPP[] = {"0020", "0032"};
constexpr const char* kTagIOP[] = {"0020", "0037"};
constexpr const char* kTagPixelSpacing[] = {"0028", "0030"};
constexpr const char* kTagSliceThickness[] = {"0018", "0050"};
constexpr const char* kTagRows[] = {"0028", "0010"};
constexpr const char* kTagCols[] = {"0028", "0011"};
constexpr const char* kTagSlope[] = {"0028", "1053"};
constexpr const char* kTagIntercept[] = {"0028", "1052"};
constexpr const char* kTagWC[] = {"0028", "1050"};
constexpr const char* kTagWW[] = {"0028", "1051"};

bool isRtModality(const std::string& m) {
    return m == "RTSTRUCT" || m == "RTPLAN" || m == "RTDOSE" || m == "REG";
}

std::string trimDs(std::string s) {
    // DICOM DS 值可能带尾随空格/反斜杠分隔的多值，单值场景先取第一段
    auto back = s.find('\\');
    if (back != std::string::npos) s = s.substr(0, back);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\r' || s.back() == '\n'))
        s.pop_back();
    while (!s.empty() && (s.front() == ' ' || s.front() == '\r')) s.erase(s.begin());
    return s;
}

std::vector<double> parseDsMulti(const std::string& raw) {
    std::vector<double> out;
    size_t pos = 0;
    while (pos <= raw.size()) {
        auto sep = raw.find('\\', pos);
        std::string tok = raw.substr(pos, sep == std::string::npos ? std::string::npos
                                                                  : sep - pos);
        tok = trimDs(tok);
        if (!tok.empty()) {
            try {
                out.push_back(std::stod(tok));
            } catch (...) {
            }
        }
        if (sep == std::string::npos) break;
        pos = sep + 1;
    }
    return out;
}

/// 每文件的空间读取结果（排序用）
struct SliceSpatial {
    std::string file;
    double ipp[3] = {0, 0, 0};
    double iop[6] = {1, 0, 0, 0, 1, 0};
    bool hasIpp = false;
    bool hasIop = false;
    int instanceNumber = 0;
};

bool readSliceSpatial(const std::string& path, SliceSpatial& out) {
    gdcm::Reader r;
    r.SetFileName(path.c_str());
    if (!r.Read()) return false;

    gdcm::StringFilter sf;
    sf.SetFile(r.GetFile());

    auto grab = [&](const char* g, const char* c) -> std::string {
        return sf.ToString(gdcm::Tag(static_cast<uint16_t>(std::stoul(g, nullptr, 16)),
                                     static_cast<uint16_t>(std::stoul(c, nullptr, 16))));
    };

    out.file = path;
    std::string ipp = grab(kTagIPP[0], kTagIPP[1]);
    if (!ipp.empty()) {
        auto v = parseDsMulti(ipp);
        if (v.size() >= 3) {
            out.ipp[0] = v[0];
            out.ipp[1] = v[1];
            out.ipp[2] = v[2];
            out.hasIpp = true;
        }
    }
    std::string iop = grab(kTagIOP[0], kTagIOP[1]);
    if (!iop.empty()) {
        auto v = parseDsMulti(iop);
        if (v.size() >= 6) {
            for (int i = 0; i < 6; ++i) out.iop[i] = v[i];
            out.hasIop = true;
        }
    }
    std::string inst = grab("0020", "0013");
    if (!inst.empty()) {
        try {
            out.instanceNumber = std::stoi(trimDs(inst));
        } catch (...) {
        }
    }
    return true;
}

/// 层排序：有 IPP 时按层法向（IOP 行×列）投影排序，否则按 InstanceNumber/文件名
void sortSlices(std::vector<SliceSpatial>& slices) {
    if (slices.empty()) return;

    // 层法向 = rowCos × colCos
    double n[3] = {0, 0, 0};
    const double* iop = slices.front().iop;
    n[0] = iop[1] * iop[5] - iop[2] * iop[4];
    n[1] = iop[2] * iop[3] - iop[0] * iop[5];
    n[2] = iop[0] * iop[4] - iop[1] * iop[3];

    bool anyIpp = false;
    for (const auto& s : slices)
        if (s.hasIpp) {
            anyIpp = true;
            break;
        }

    if (anyIpp) {
        std::stable_sort(slices.begin(), slices.end(),
                         [&](const SliceSpatial& a, const SliceSpatial& b) {
                             double pa = a.ipp[0] * n[0] + a.ipp[1] * n[1] + a.ipp[2] * n[2];
                             double pb = b.ipp[0] * n[0] + b.ipp[1] * n[1] + b.ipp[2] * n[2];
                             if (std::abs(pa - pb) > 1e-6) return pa < pb;
                             return a.instanceNumber < b.instanceNumber;
                         });
    } else {
        std::stable_sort(slices.begin(), slices.end(),
                         [](const SliceSpatial& a, const SliceSpatial& b) {
                             if (a.instanceNumber != b.instanceNumber)
                                 return a.instanceNumber < b.instanceNumber;
                             return a.file < b.file;
                         });
    }
}

}  // namespace

// ---------------------------------------------------------------------------
//  枚举
// ---------------------------------------------------------------------------

std::vector<DicomSeriesLoader::SeriesInfo> DicomSeriesLoader::enumerateSeries(
    const std::string& directory) {
    std::vector<SeriesInfo> result;

    std::error_code ec;
    if (!fs::is_directory(directory, ec)) {
        CBCT_LOG_WARN("dicom", "不是目录或不存在: {}", directory);
        return result;
    }

    gdcm::Directory dir;
    dir.Load(directory.c_str());
    auto names = dir.GetFilenames();
    if (names.empty()) {
        CBCT_LOG_WARN("dicom", "目录无文件: {}", directory);
        return result;
    }

    gdcm::Scanner scanner;
    scanner.AddTag(gdcm::Tag(0x0008, 0x0060));  // Modality
    scanner.AddTag(gdcm::Tag(0x0020, 0x000e));  // SeriesInstanceUID
    scanner.AddTag(gdcm::Tag(0x0008, 0x103e));  // SeriesDescription
    if (!scanner.Scan(names)) {
        CBCT_LOG_WARN("dicom", "Scanner 扫描失败: {}", directory);
        return result;
    }

    // 按 SeriesUID 分组（保留扫描顺序）
    std::map<std::string, SeriesInfo> byUid;
    std::vector<std::string> order;

    for (const auto& path : scanner.GetFilenames()) {
        const char* uid = scanner.GetValue(path.c_str(), gdcm::Tag(0x0020, 0x000e));
        const char* mod = scanner.GetValue(path.c_str(), gdcm::Tag(0x0008, 0x0060));
        std::string uidS = uid ? uid : "";
        std::string modS = mod ? mod : "";
        // 去尾随空格（CS 值）
        while (!modS.empty() && modS.back() == ' ') modS.pop_back();
        while (!uidS.empty() && uidS.back() == ' ') uidS.pop_back();

        if (uidS.empty() || isRtModality(modS)) continue;

        auto it = byUid.find(uidS);
        if (it == byUid.end()) {
            SeriesInfo info;
            info.seriesUID = uidS;
            info.modality = modS;
            const char* desc = scanner.GetValue(path.c_str(), gdcm::Tag(0x0008, 0x103e));
            if (desc) {
                info.description = desc;
                while (!info.description.empty() && info.description.back() == ' ')
                    info.description.pop_back();
            }
            it = byUid.emplace(uidS, std::move(info)).first;
            order.push_back(uidS);
        }
        it->second.files.emplace_back(path);
        it->second.fileCount++;
    }

    for (auto& uid : order) {
        auto& info = byUid[uid];

        // 空间排序
        std::vector<SliceSpatial> spatial;
        spatial.reserve(info.files.size());
        for (auto& f : info.files) {
            SliceSpatial s;
            if (readSliceSpatial(f, s)) spatial.push_back(std::move(s));
        }
        sortSlices(spatial);
        info.files.clear();
        info.files.reserve(spatial.size());
        for (auto& s : spatial) info.files.push_back(std::move(s.file));

        result.push_back(std::move(info));
    }

    // 文件多的在前，方便取最大
    std::stable_sort(result.begin(), result.end(),
                     [](const SeriesInfo& a, const SeriesInfo& b) {
                         return a.fileCount > b.fileCount;
                     });
    return result;
}

// ---------------------------------------------------------------------------
//  读取
// ---------------------------------------------------------------------------

DicomVolumeData DicomSeriesLoader::loadDirectory(const std::string& directory,
                                                 DicomLoadError* err) {
    if (err) *err = DicomLoadError::Ok;

    std::error_code ec;
    if (!fs::is_directory(directory, ec)) {
        if (err) *err = DicomLoadError::EmptyDirectory;
        return {};
    }

    auto series = enumerateSeries(directory);
    if (series.empty()) {
        if (err) *err = DicomLoadError::NoDicomSeries;
        return {};
    }
    // enumerateSeries 已按文件数降序
    CBCT_LOG_INFO("dicom", "目录 {} 共 {} 个序列，取最大（{} 张, modality={}）",
                  directory, series.size(), series.front().fileCount,
                  series.front().modality);
    return loadSeries(series.front(), err);
}

DicomVolumeData DicomSeriesLoader::loadSeries(const SeriesInfo& series,
                                              DicomLoadError* err) {
    if (err) *err = DicomLoadError::Ok;
    if (series.files.empty()) {
        if (err) *err = DicomLoadError::NoDicomSeries;
        return {};
    }
    return loadSortedFiles(series.files, err);
}

DicomVolumeData DicomSeriesLoader::loadFiles(const std::vector<std::string>& files,
                                             DicomLoadError* err) {
    if (err) *err = DicomLoadError::Ok;
    if (files.empty()) {
        if (err) *err = DicomLoadError::EmptyDirectory;
        return {};
    }
    if (files.size() == 1) {
        return loadSortedFiles(files, err);
    }

    // 多文件：必须同 SeriesInstanceUID
    gdcm::StringFilter sf;
    std::string uid;
    for (size_t i = 0; i < files.size(); ++i) {
        gdcm::Reader r;
        r.SetFileName(files[i].c_str());
        if (!r.Read()) {
            if (err) *err = DicomLoadError::NotDicomFile;
            return {};
        }
        sf.SetFile(r.GetFile());
        std::string u = sf.ToString(gdcm::Tag(0x0020, 0x000e));
        while (!u.empty() && u.back() == ' ') u.pop_back();
        if (i == 0) {
            uid = u;
        } else if (u != uid) {
            CBCT_LOG_WARN("dicom", "多选文件分属不同序列，拒绝拼接");
            if (err) *err = DicomLoadError::MixedSeries;
            return {};
        }
    }

    // 空间排序后读取
    std::vector<SliceSpatial> spatial;
    spatial.reserve(files.size());
    for (auto& f : files) {
        SliceSpatial s;
        if (readSliceSpatial(f, s)) spatial.push_back(std::move(s));
    }
    sortSlices(spatial);
    std::vector<std::string> sorted;
    sorted.reserve(spatial.size());
    for (auto& s : spatial) sorted.push_back(std::move(s.file));
    return loadSortedFiles(sorted, err);
}

bool DicomSeriesLoader::isRtStruct(const std::string& path) {
    gdcm::Reader r;
    r.SetFileName(path.c_str());
    if (!r.Read()) return false;
    gdcm::StringFilter sf;
    sf.SetFile(r.GetFile());
    std::string mod = sf.ToString(gdcm::Tag(0x0008, 0x0060));
    while (!mod.empty() && mod.back() == ' ') mod.pop_back();
    return mod == "RTSTRUCT";
}

DicomVolumeData DicomSeriesLoader::loadSortedFiles(const std::vector<std::string>& files,
                                                   DicomLoadError* err) {
    DicomVolumeData vol;
    if (files.empty()) {
        if (err) *err = DicomLoadError::NoDicomSeries;
        return vol;
    }

    // ---- 逐文件读像素（第一层取元数据/几何）----
    int width = 0, height = 0;
    std::vector<std::int16_t> firstBuffer;
    std::vector<std::vector<std::int16_t>> perSlice;
    perSlice.reserve(files.size());

    double spacing[3] = {1, 1, 1};
    double origin[3] = {0, 0, 0};
    double direction[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    double slope = 1.0, intercept = 0.0;
    double wc = 40.0, ww = 400.0;
    std::map<std::string, std::string> meta;
    std::string seriesUID, modality;

    for (size_t zi = 0; zi < files.size(); ++zi) {
        gdcm::ImageReader ir;
        ir.SetFileName(files[zi].c_str());
        if (!ir.Read()) {
            CBCT_LOG_WARN("dicom", "读取失败: {}", files[zi]);
            if (err) *err = DicomLoadError::ReadFailed;
            return {};
        }
        gdcm::Image& img = ir.GetImage();
        std::vector<char> rawBuf(img.GetBufferLength());
        if (rawBuf.empty() || !img.GetBuffer(rawBuf.data())) {
            CBCT_LOG_WARN("dicom", "读像素缓冲失败: {}", files[zi]);
            if (err) *err = DicomLoadError::ReadFailed;
            return {};
        }

        const unsigned dims[3] = {img.GetDimensions()[0], img.GetDimensions()[1],
                                  img.GetDimensions()[2]};
        if (dims[2] != 1) {
            CBCT_LOG_WARN("dicom", "非单层文件（z={}），跳过不支持: {}", dims[2],
                          files[zi]);
            if (err) *err = DicomLoadError::UnsupportedPixelType;
            return {};
        }
        if (width == 0) {
            width = static_cast<int>(dims[0]);
            height = static_cast<int>(dims[1]);
        } else if (width != static_cast<int>(dims[0]) ||
                   height != static_cast<int>(dims[1])) {
            CBCT_LOG_WARN("dicom", "序列内尺寸不一致: {}", files[zi]);
            if (err) *err = DicomLoadError::ReadFailed;
            return {};
        }

        // 像素类型检查：仅支持 8/16 位有符号或无符号 → 统一转 int16
        const gdcm::PixelFormat& pf = img.GetPixelFormat();
        const size_t npix = static_cast<size_t>(width) * height;
        const char* buf = rawBuf.data();
        std::vector<std::int16_t> slice(npix);

        if (pf.GetScalarType() == gdcm::PixelFormat::INT16) {
            std::memcpy(slice.data(), buf, npix * 2);
        } else if (pf.GetScalarType() == gdcm::PixelFormat::UINT16) {
            const auto* p = reinterpret_cast<const std::uint16_t*>(buf);
            for (size_t i = 0; i < npix; ++i)
                slice[i] = static_cast<std::int16_t>(p[i]);  // CT 常规范围安全
        } else if (pf.GetScalarType() == gdcm::PixelFormat::INT8) {
            const auto* p = reinterpret_cast<const std::int8_t*>(buf);
            for (size_t i = 0; i < npix; ++i) slice[i] = p[i];
        } else if (pf.GetScalarType() == gdcm::PixelFormat::UINT8) {
            const auto* p = reinterpret_cast<const std::uint8_t*>(buf);
            for (size_t i = 0; i < npix; ++i) slice[i] = p[i];
        } else {
            CBCT_LOG_WARN("dicom", "不支持像素类型 {} : {}",
                          static_cast<int>(pf.GetScalarType()), files[zi]);
            if (err) *err = DicomLoadError::UnsupportedPixelType;
            return {};
        }

        if (zi == 0) {
            firstBuffer = slice;

            // 几何
            const double* sp = img.GetSpacing();
            spacing[0] = sp[0];
            spacing[1] = sp[1];
            spacing[2] = sp[2] > 0 ? sp[2] : 1.0;

            const double* org = img.GetOrigin();
            origin[0] = org[0];
            origin[1] = org[1];
            origin[2] = org[2];

            // GDCM 方向余弦为 6 值 [rowX,rowY,rowZ,colX,colY,colZ]（LPS），
            // 3x3 方向矩阵列 = 索引轴 x/y/z 的方向（z 列 = 行×列叉积）
            const double* dircos = img.GetDirectionCosines();
            if (dircos) {
                direction[0][0] = dircos[0];
                direction[1][0] = dircos[1];
                direction[2][0] = dircos[2];
                direction[0][1] = dircos[3];
                direction[1][1] = dircos[4];
                direction[2][1] = dircos[5];
                direction[0][2] = dircos[1] * dircos[5] - dircos[2] * dircos[4];
                direction[1][2] = dircos[2] * dircos[3] - dircos[0] * dircos[5];
                direction[2][2] = dircos[0] * dircos[4] - dircos[1] * dircos[3];
            }

            // 灰度与窗位（DS 多值取第一个）
            {
                auto rs = gdcm::ImageHelper::GetRescaleInterceptSlopeValue(ir.GetFile());
                if (rs.size() >= 2) {
                    intercept = rs[0];
                    slope = rs[1];
                }
            }

            gdcm::StringFilter sf;
            sf.SetFile(ir.GetFile());
            auto firstDs = [&](const char* g, const char* c, double def) {
                std::string s = sf.ToString(
                    gdcm::Tag(static_cast<uint16_t>(std::stoul(g, nullptr, 16)),
                              static_cast<uint16_t>(std::stoul(c, nullptr, 16))));
                auto v = parseDsMulti(s);
                return v.empty() ? def : v.front();
            };
            wc = firstDs("0028", "1050", wc);
            ww = firstDs("0028", "1051", ww);

            // spacing 第三层：IPP 差（多层时），此处单层文件稍后统一算
            meta["PatientName"] = sf.ToString(gdcm::Tag(0x0010, 0x0010));
            meta["PatientID"] = sf.ToString(gdcm::Tag(0x0010, 0x0020));
            meta["PatientBirthDate"] = sf.ToString(gdcm::Tag(0x0010, 0x0030));
            meta["PatientSex"] = sf.ToString(gdcm::Tag(0x0010, 0x0040));
            meta["PatientAge"] = sf.ToString(gdcm::Tag(0x0010, 0x1010));
            meta["StudyDate"] = sf.ToString(gdcm::Tag(0x0008, 0x0020));
            meta["StudyTime"] = sf.ToString(gdcm::Tag(0x0008, 0x0030));
            meta["StudyDescription"] = sf.ToString(gdcm::Tag(0x0008, 0x1030));
            meta["SeriesDescription"] = sf.ToString(gdcm::Tag(0x0008, 0x103e));
            meta["StudyInstanceUID"] = sf.ToString(gdcm::Tag(0x0020, 0x000d));
            meta["Manufacturer"] = sf.ToString(gdcm::Tag(0x0008, 0x0070));
            meta["ManufacturerModel"] = sf.ToString(gdcm::Tag(0x0008, 0x1090));
            meta["InstitutionName"] = sf.ToString(gdcm::Tag(0x0008, 0x0080));
            meta["BodyPartExamined"] = sf.ToString(gdcm::Tag(0x0018, 0x0015));
            meta["ProtocolName"] = sf.ToString(gdcm::Tag(0x0018, 0x1030));
            meta["SliceThickness"] = sf.ToString(gdcm::Tag(0x0018, 0x0050));
            meta["KVP"] = sf.ToString(gdcm::Tag(0x0018, 0x0060));
            meta["TubeCurrent"] = sf.ToString(gdcm::Tag(0x0018, 0x1150));
            meta["ExposureTime"] = sf.ToString(gdcm::Tag(0x0018, 0x1151));
            meta["ConvolutionKernel"] = sf.ToString(gdcm::Tag(0x0018, 0x1210));
            meta["PatientPosition"] = sf.ToString(gdcm::Tag(0x0018, 0x5100));
            meta["Rows"] = sf.ToString(gdcm::Tag(0x0028, 0x0010));
            meta["Columns"] = sf.ToString(gdcm::Tag(0x0028, 0x0011));
            meta["PixelSpacing"] = sf.ToString(gdcm::Tag(0x0028, 0x0030));
            meta["ImageType"] = sf.ToString(gdcm::Tag(0x0008, 0x0008));
            meta["SeriesNumber"] = sf.ToString(gdcm::Tag(0x0020, 0x0011));
            meta["ImageOrientationPatient"] = sf.ToString(gdcm::Tag(0x0020, 0x0037));
            meta["ImagePositionPatient"] = sf.ToString(gdcm::Tag(0x0020, 0x0032));

            seriesUID = sf.ToString(gdcm::Tag(0x0020, 0x000e));
            while (!seriesUID.empty() && seriesUID.back() == ' ') seriesUID.pop_back();
            modality = sf.ToString(gdcm::Tag(0x0008, 0x0060));
            while (!modality.empty() && modality.back() == ' ') modality.pop_back();
            meta["SeriesInstanceUID"] = seriesUID;
            meta["Modality"] = modality;
        }

        perSlice.push_back(std::move(slice));
    }

    // ---- 组装 3D（x fastest, y, z）----
    const int depth = static_cast<int>(perSlice.size());
    vol.dims[0] = width;
    vol.dims[1] = height;
    vol.dims[2] = depth;
    vol.pixels.resize(static_cast<size_t>(width) * height * depth);
    for (int z = 0; z < depth; ++z) {
        std::memcpy(vol.pixels.data() + static_cast<size_t>(z) * width * height,
                    perSlice[z].data(),
                    static_cast<size_t>(width) * height * sizeof(std::int16_t));
    }

    // 层间距：IPP 沿法向的差（>1 层时）
    if (depth > 1) {
        std::vector<SliceSpatial> spatial;
        spatial.reserve(files.size());
        for (auto& f : files) {
            SliceSpatial s;
            if (readSliceSpatial(f, s)) spatial.push_back(std::move(s));
        }
        if (spatial.size() == static_cast<size_t>(depth) && spatial.front().hasIpp &&
            spatial.back().hasIpp) {
            const double* iop = spatial.front().iop;
            double n[3] = {iop[1] * iop[5] - iop[2] * iop[4],
                           iop[2] * iop[3] - iop[0] * iop[5],
                           iop[0] * iop[4] - iop[1] * iop[3]};
            double dz = std::abs((spatial.back().ipp[0] - spatial.front().ipp[0]) * n[0] +
                                 (spatial.back().ipp[1] - spatial.front().ipp[1]) * n[1] +
                                 (spatial.back().ipp[2] - spatial.front().ipp[2]) * n[2]) /
                        (depth - 1);
            if (dz > 1e-6) spacing[2] = dz;
        }
    }

    vol.spacing[0] = spacing[0];
    vol.spacing[1] = spacing[1];
    vol.spacing[2] = spacing[2];
    vol.origin[0] = origin[0];
    vol.origin[1] = origin[1];
    vol.origin[2] = origin[2];
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) vol.direction[r][c] = direction[r][c];
    vol.slope = slope;
    vol.intercept = intercept;
    vol.windowWidth = ww;
    vol.windowLevel = wc;
    vol.metadata = std::move(meta);
    vol.seriesUID = seriesUID;
    vol.modality = modality;
    vol.seriesFileCount = depth;
    vol.sliceFiles = files;

    if (err) *err = DicomLoadError::Ok;
    CBCT_LOG_INFO("dicom", "加载完成 {}x{}x{} spacing=({:.4f},{:.4f},{:.4f}) "
                  "origin=({:.2f},{:.2f},{:.2f}) slope={} intercept={} ww={} wc={}",
                  width, height, depth, vol.spacing[0], vol.spacing[1], vol.spacing[2],
                  vol.origin[0], vol.origin[1], vol.origin[2], vol.slope, vol.intercept,
                  vol.windowWidth, vol.windowLevel);
    (void)firstBuffer;
    return vol;
}

}  // namespace cbct::dicom
