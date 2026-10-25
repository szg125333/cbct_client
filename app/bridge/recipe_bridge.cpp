#include "bridge/recipe_bridge.h"

#include "client_core.h"
#include "common/log_service.h"
#include "recipe/recipe_manager.h"

#include <algorithm>

RecipeBridge::RecipeBridge(QObject *parent)
    : QObject(parent)
{
    reloadFromCore();

    connect(&cbct::ClientCore::instance().recipeManager(),
            &cbct::RecipeManager::recipesChanged,
            this,
            &RecipeBridge::reloadFromCore);
}

void RecipeBridge::refresh()
{
    reloadFromCore();
}

QStringList RecipeBridge::names() const
{
    QStringList list;
    list.reserve(m_recipes.size());
    for (const cbct::Recipe &r : m_recipes) {
        list << r.name;
    }
    return list;
}

QString RecipeBridge::idAt(int displayIndex) const
{
    if (displayIndex < 0 || displayIndex >= m_recipes.size()) {
        return QString();
    }
    return m_recipes.at(displayIndex).id;
}

int RecipeBridge::indexOfId(const QString &id) const
{
    for (int i = 0; i < m_recipes.size(); ++i) {
        if (m_recipes.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

QString RecipeBridge::summaryOf(const QString &id) const
{
    for (const cbct::Recipe &r : m_recipes) {
        if (r.id == id) {
            return QStringLiteral("kV: %1    mA: %2    时间: %3 ms")
                .arg(r.hvg.tubeVoltageKv, 0, 'f', 1)
                .arg(r.hvg.tubeCurrentMa, 0, 'f', 1)
                .arg(r.hvg.exposureTimeMs);
        }
    }
    return QStringLiteral("参数：kV: --    mA: --    时间: --");
}

bool RecipeBridge::addRecipe(const cbct::Recipe &recipe, QString *error)
{
    cbct::ClientCore::instance().initialize();
    CBCT_LOG_INFO("bridge", "新增配方 name={}", recipe.name.toStdString());
    if (!cbct::ClientCore::instance().recipeManager().add(recipe)) {
        CBCT_LOG_ERROR("bridge", "新增配方失败 name={}", recipe.name.toStdString());
        if (error) {
            *error = QStringLiteral("保存配方失败");
        }
        return false;
    }
    return true;
}

void RecipeBridge::reloadFromCore()
{
    cbct::ClientCore::instance().initialize();
    m_recipes = cbct::ClientCore::instance().recipeManager().all();

    std::sort(m_recipes.begin(), m_recipes.end(),
              [](const cbct::Recipe &a, const cbct::Recipe &b) {
                  return a.name.localeAwareCompare(b.name) < 0;
              });

    CBCT_LOG_DEBUG("bridge", "配方列表刷新 count={}", static_cast<int>(m_recipes.size()));
    emit recipesUpdated();
}
