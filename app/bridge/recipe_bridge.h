#ifndef RECIPE_BRIDGE_H
#define RECIPE_BRIDGE_H

#include "recipe/recipe_params.h"

#include <QObject>
#include <QStringList>

/// app ↔ core 配方适配层：UI 只依赖本类与 ClientCore 公共 API
class RecipeBridge : public QObject
{
    Q_OBJECT

public:
    explicit RecipeBridge(QObject *parent = nullptr);

    void refresh();

    QStringList names() const;
    QString idAt(int displayIndex) const;
    int indexOfId(const QString &id) const;
    QString summaryOf(const QString &id) const;

    bool addRecipe(const cbct::Recipe &recipe, QString *error = nullptr);

signals:
    void recipesUpdated();

private:
    void reloadFromCore();

    QList<cbct::Recipe> m_recipes;
};

#endif // RECIPE_BRIDGE_H
