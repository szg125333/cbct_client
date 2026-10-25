/**
 * @file recipe_bridge.h
 * @brief app ↔ core 配方适配层：UI 只拿显示名/id/摘要，不遍历 core 结构细节
 */
#ifndef RECIPE_BRIDGE_H
#define RECIPE_BRIDGE_H

#include "recipe/recipe_params.h"

#include <QObject>
#include <QStringList>

/**
 * @class RecipeBridge
 * @brief 监听 RecipeManager::recipesChanged，缓存排序后的列表供下拉框使用
 */
class RecipeBridge : public QObject
{
    Q_OBJECT

public:
    explicit RecipeBridge(QObject *parent = nullptr);

    void refresh(); ///< 强制从 ClientCore 重读

    QStringList names() const;          ///< 显示名（已按名称排序）
    QString idAt(int displayIndex) const; ///< 与 names() 下标对齐的配方 id
    int indexOfId(const QString &id) const;
    QString summaryOf(const QString &id) const; ///< 「kV: … mA: … 时间: …」文案

    bool addRecipe(const cbct::Recipe &recipe, QString *error = nullptr);

signals:
    void recipesUpdated(); ///< 列表已刷新，UI 应重填控件

private:
    void reloadFromCore(); ///< all() + 按 name localeAwareCompare 排序

    QList<cbct::Recipe> m_recipes; ///< 本地缓存（已排序）
};

#endif // RECIPE_BRIDGE_H
