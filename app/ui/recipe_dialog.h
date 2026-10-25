#ifndef RECIPE_DIALOG_H
#define RECIPE_DIALOG_H

#include "recipe/recipe_params.h"

#include <QDialog>

namespace Ui {
class RecipeDialog;
}

/// 新增配方对话框（分模块：基本信息 / 高压 / 探测器 / 龙门架 / 重建）
class RecipeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RecipeDialog(QWidget *parent = nullptr);
    ~RecipeDialog() override;

    cbct::Recipe recipe() const;

private slots:
    void onAccept();

private:
    void setupSpinRanges();
    cbct::Recipe collectRecipe() const;

    Ui::RecipeDialog *ui;
    cbct::Recipe m_recipe;
};

#endif // RECIPE_DIALOG_H
