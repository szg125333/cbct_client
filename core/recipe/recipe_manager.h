#ifndef CBCT_RECIPE_MANAGER_H
#define CBCT_RECIPE_MANAGER_H

#include "recipe/recipe_params.h"

#include <QList>
#include <QObject>
#include <QString>

namespace cbct {

/// 配方列表管理：内存 CRUD + JSON 持久化
class RecipeManager : public QObject
{
    Q_OBJECT

public:
    explicit RecipeManager(QObject *parent = nullptr);

    QString storagePath() const;
    void setStoragePath(const QString &path);

    bool load();
    bool save() const;

    QList<Recipe> all() const;
    Recipe findById(const QString &id) const;
    bool contains(const QString &id) const;

    bool add(const Recipe &recipe);
    bool update(const Recipe &recipe);
    bool remove(const QString &id);

signals:
    void recipesChanged();

private:
    void seedDefaults();
    bool persist() const;

    QString m_storagePath;
    QList<Recipe> m_recipes;
};

} // namespace cbct

#endif // CBCT_RECIPE_MANAGER_H
