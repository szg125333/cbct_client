#include "rt_struct_loader.h"

#include <gdcmAttribute.h>
#include <gdcmDataElement.h>
#include <gdcmDataSet.h>
#include <gdcmItem.h>
#include <gdcmReader.h>
#include <gdcmSequenceOfItems.h>
#include <gdcmSmartPointer.h>

#include <sstream>

#include "common/log_service.h"

namespace cbct::dicom {
namespace {

// "r\g\b" → 归一化 RGB（0~255 → 0~1）
std::array<double, 3> parseColor(const std::string& colorStr) {
    std::array<double, 3> color = {1.0, 0.0, 0.0};
    std::istringstream iss(colorStr);
    std::string token;
    int idx = 0;
    while (std::getline(iss, token, '\\') && idx < 3) {
        try {
            color[idx] = std::stod(token) / 255.0;
        } catch (...) {
        }
        ++idx;
    }
    return color;
}

std::string trimRight(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
    return s;
}

}  // namespace

std::shared_ptr<RtStructureSet> RtStructLoader::load(const std::string& filePath) {
    gdcm::Reader reader;
    reader.SetFileName(filePath.c_str());
    if (!reader.Read()) {
        CBCT_LOG_WARN("dicom", "RTSTRUCT 读取失败: {}", filePath);
        return nullptr;
    }

    const gdcm::DataSet& ds = reader.GetFile().GetDataSet();

    // (3006,0020) Structure Set ROI Sequence / (3006,0039) ROI Contour Sequence
    const gdcm::Tag tagSSROISeq(0x3006, 0x0020);
    const gdcm::Tag tagROIContourSeq(0x3006, 0x0039);
    if (!ds.FindDataElement(tagSSROISeq) || !ds.FindDataElement(tagROIContourSeq)) {
        CBCT_LOG_WARN("dicom", "RTSTRUCT 缺少 ROI 序列: {}", filePath);
        return nullptr;
    }

    auto result = std::make_shared<RtStructureSet>();

    if (ds.FindDataElement(gdcm::Tag(0x3006, 0x0002))) {
        gdcm::Attribute<0x3006, 0x0002> label;
        label.SetFromDataSet(ds);
        result->label = trimRight(label.GetValue());
    }
    if (ds.FindDataElement(gdcm::Tag(0x3006, 0x0004))) {
        gdcm::Attribute<0x3006, 0x0004> atname;
        atname.SetFromDataSet(ds);
        result->name = trimRight(atname.GetValue());
    }

    // ---- Structure Set ROI Sequence：ROI 编号 + 名称 ----
    gdcm::SmartPointer<gdcm::SequenceOfItems> ssroiSQ =
        ds.GetDataElement(tagSSROISeq).GetValueAsSQ();
    if (!ssroiSQ || ssroiSQ->GetNumberOfItems() == 0) {
        CBCT_LOG_WARN("dicom", "RTSTRUCT ROI 序列为空: {}", filePath);
        return nullptr;
    }

    for (unsigned int i = 0; i < ssroiSQ->GetNumberOfItems(); ++i) {
        const gdcm::DataSet& nestedDS = ssroiSQ->GetItem(i + 1).GetNestedDataSet();

        gdcm::Attribute<0x3006, 0x0022> roiNumAttr;
        roiNumAttr.SetFromDataSet(nestedDS);
        gdcm::Attribute<0x3006, 0x0026> roiNameAttr;
        roiNameAttr.SetFromDataSet(nestedDS);

        RtStructureRoi roi;
        roi.roiNumber = roiNumAttr.GetValue();
        roi.name = trimRight(roiNameAttr.GetValue());
        result->rois[roi.roiNumber] = roi;
    }

    // ---- ROI Contour Sequence：颜色 + 轮廓点 ----
    gdcm::SmartPointer<gdcm::SequenceOfItems> roiContourSQ =
        ds.GetDataElement(tagROIContourSeq).GetValueAsSQ();
    if (!roiContourSQ || roiContourSQ->GetNumberOfItems() == 0) {
        CBCT_LOG_INFO("dicom", "RTSTRUCT 无轮廓数据 rois={}", result->rois.size());
        return result;
    }

    const gdcm::Tag tagColor(0x3006, 0x002A);
    const gdcm::Tag tagContourSeq(0x3006, 0x0040);
    const gdcm::Tag tagGeomType(0x3006, 0x0042);
    const gdcm::Tag tagNumPts(0x3006, 0x0046);
    const gdcm::Tag tagContourData(0x3006, 0x0050);

    for (unsigned int i = 0; i < roiContourSQ->GetNumberOfItems(); ++i) {
        const gdcm::DataSet& nestedDS = roiContourSQ->GetItem(i + 1).GetNestedDataSet();

        gdcm::Attribute<0x3006, 0x0084> refROINum;
        refROINum.SetFromDataSet(nestedDS);
        auto it = result->rois.find(refROINum.GetValue());
        if (it == result->rois.end()) continue;
        RtStructureRoi& roi = it->second;

        if (nestedDS.FindDataElement(tagColor)) {
            const gdcm::DataElement& colorDE = nestedDS.GetDataElement(tagColor);
            if (colorDE.GetByteValue()) {
                roi.color = parseColor(std::string(
                    colorDE.GetByteValue()->GetPointer(),
                    colorDE.GetByteValue()->GetLength()));
            }
        }

        if (!nestedDS.FindDataElement(tagContourSeq)) continue;
        gdcm::SmartPointer<gdcm::SequenceOfItems> contourSQ =
            nestedDS.GetDataElement(tagContourSeq).GetValueAsSQ();
        if (!contourSQ) continue;

        for (unsigned int c = 0; c < contourSQ->GetNumberOfItems(); ++c) {
            const gdcm::DataSet& contourDS = contourSQ->GetItem(c + 1).GetNestedDataSet();

            // 只接受 CLOSED_PLANAR
            if (contourDS.FindDataElement(tagGeomType)) {
                const gdcm::DataElement& geomDE = contourDS.GetDataElement(tagGeomType);
                if (geomDE.GetByteValue()) {
                    std::string geomType(geomDE.GetByteValue()->GetPointer(),
                                         geomDE.GetByteValue()->GetLength());
                    if (trimRight(geomType) != "CLOSED_PLANAR") continue;
                }
            }

            gdcm::Attribute<0x3006, 0x0046> numPtsAttr;
            numPtsAttr.SetFromDataSet(contourDS);
            const int numPts = numPtsAttr.GetValue();
            if (numPts <= 0) continue;

            if (!contourDS.FindDataElement(tagContourData)) continue;
            gdcm::Attribute<0x3006, 0x0050> contourDataAttr;
            contourDataAttr.SetFromDataElement(contourDS.GetDataElement(tagContourData));
            const double* values = contourDataAttr.GetValues();
            const unsigned int numValues = contourDataAttr.GetNumberOfValues();
            if (!values || numValues < static_cast<unsigned int>(numPts) * 3) continue;

            RtContour contour;
            contour.points.resize(numPts);
            for (int p = 0; p < numPts; ++p) {
                contour.points[p][0] = values[p * 3 + 0];
                contour.points[p][1] = values[p * 3 + 1];
                contour.points[p][2] = values[p * 3 + 2];
            }
            contour.z = contour.points[0][2];
            roi.contours.push_back(std::move(contour));
        }

        CBCT_LOG_INFO("dicom", "RTSTRUCT ROI \"{}\" (num={}): {} 条轮廓",
                      roi.name, roi.roiNumber, roi.contours.size());
    }

    return result;
}

}  // namespace cbct::dicom
