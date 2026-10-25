/**
 * @file recipe_dialog.h
 * @brief 「新增配方」对话框：Tab 分模块编辑高压/探测器/龙门架/重建参数
 */
#ifndef RECIPE_DIALOG_H
#define RECIPE_DIALOG_H

#include "recipe/recipe_params.h"

#include <QDialog>

namespace Ui {
class RecipeDialog;
}

/**
 * @class RecipeDialog
 * @brief 保存后 recipe() 返回填好的 Recipe（id 为新 UUID）
 *
 * Tab 页与 Recipe 子结构对应：
 *   基本信息 → Recipe::name / description
 *   高压发生器 → HvgParams
 *   平板探测器 → DetectorParams
 *   龙门架 → GantryParams
 *   重建算法 → ReconParams（暂无独立 recon_panel 类，逻辑在此对话框）
 */
class RecipeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RecipeDialog(QWidget *parent = nullptr);
    ~RecipeDialog() override;

    /// 点「保存」并 accept 后调用；含用户填写的全部参数
    cbct::Recipe recipe() const;

private slots:
    void onAccept(); ///< 校验名称非空 → collect → accept

private:
    void setupSpinRanges();  ///< 设置各 SpinBox 合理上下限与默认值
    cbct::Recipe collectRecipe() const; ///< UI → 结构体

    Ui::RecipeDialog *ui;
    cbct::Recipe m_recipe; ///< 最终生成的配方（accept 时填充）
};

#endif // RECIPE_DIALOG_H
