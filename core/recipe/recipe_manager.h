/**
 * @file recipe_manager.h
 * @brief 配方列表管理：内存 CRUD + recipes.json 持久化
 *
 * 存储路径由 paths::recipeStoragePath() 提供（见 app_paths）。
 * 线程：主线程使用；recipesChanged 通知 UI 刷新下拉框。
 */
#ifndef CBCT_RECIPE_MANAGER_H
#define CBCT_RECIPE_MANAGER_H

#include "recipe/recipe_params.h"

#include <QList>
#include <QObject>
#include <QString>

namespace cbct {

/**
 * @class RecipeManager
 * @brief 持有全部配方；add/update/remove 成功后自动写盘并发信号
 */
class RecipeManager : public QObject
{
    Q_OBJECT

public:
    explicit RecipeManager(QObject *parent = nullptr);

    QString storagePath() const;           ///< 当前 recipes.json 完整路径
    void setStoragePath(const QString &path); ///< 覆盖默认路径（测试用）

    bool load();   ///< 从磁盘加载；无文件则种入示例并保存
    bool save() const; ///< 立即写盘

    QList<Recipe> all() const;              ///< 全部配方副本
    Recipe findById(const QString &id) const; ///< 按 id 查找；找不到返回默认构造 Recipe
    bool contains(const QString &id) const;

    bool add(const Recipe &recipe);      ///< id 不能为空且不能重复
    bool update(const Recipe &recipe);   ///< 按 id 替换
    bool remove(const QString &id);      ///< 按 id 删除

signals:
    /// 列表变化（增删改、load 后）；UI 重填下拉框
    void recipesChanged();

private:
    void seedDefaults(); ///< 写入 3 条示例配方（成人头部/胸部低剂/四肢）
    bool persist() const; ///< 组 JSON 并 JsonFileIo::write

    QString m_storagePath;  ///< recipes.json 路径
    QList<Recipe> m_recipes;///< 内存中的配方表
};

} // namespace cbct

#endif // CBCT_RECIPE_MANAGER_H
