#include "recipe/recipe_manager.h"

#include "common/app_paths.h"
#include "common/json_file_io.h"
#include "recipe/recipe_json_codec.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace cbct {

namespace {
const int kStorageVersion = 1;
const char kVersionKey[] = "version";
const char kRecipesKey[] = "recipes";
}

RecipeManager::RecipeManager(QObject *parent)
    : QObject(parent)
{
    m_storagePath = paths::recipeStoragePath();
}

QString RecipeManager::storagePath() const
{
    return m_storagePath;
}

void RecipeManager::setStoragePath(const QString &path)
{
    if (m_storagePath != path) {
        m_storagePath = path;
    }
}

bool RecipeManager::load()
{
    m_recipes.clear();

    QJsonObject root;
    QString error;
    if (!JsonFileIo::read(m_storagePath, root, &error)) {
        // 文件不存在等：写入内置示例后重试
        seedDefaults();
        const bool saved = persist();
        emit recipesChanged();
        return saved;
    }

    const QJsonArray arr = root.value(QLatin1String(kRecipesKey)).toArray();
    for (const QJsonValue &value : arr) {
        bool ok = false;
        const Recipe recipe = RecipeJsonCodec::fromJson(value.toObject(), &ok);
        if (ok) {
            m_recipes.append(recipe);
        }
    }

    if (m_recipes.isEmpty()) {
        seedDefaults();
        persist();
    }

    emit recipesChanged();
    return true;
}

bool RecipeManager::save() const
{
    return persist();
}

QList<Recipe> RecipeManager::all() const
{
    return m_recipes;
}

Recipe RecipeManager::findById(const QString &id) const
{
    for (const Recipe &r : m_recipes) {
        if (r.id == id) {
            return r;
        }
    }
    return Recipe{};
}

bool RecipeManager::contains(const QString &id) const
{
    for (const Recipe &r : m_recipes) {
        if (r.id == id) {
            return true;
        }
    }
    return false;
}

bool RecipeManager::add(const Recipe &recipe)
{
    if (recipe.id.isEmpty() || recipe.name.isEmpty() || contains(recipe.id)) {
        return false;
    }
    m_recipes.append(recipe);
    if (!persist()) {
        m_recipes.removeLast();
        return false;
    }
    emit recipesChanged();
    return true;
}

bool RecipeManager::update(const Recipe &recipe)
{
    if (recipe.id.isEmpty()) {
        return false;
    }
    for (int i = 0; i < m_recipes.size(); ++i) {
        if (m_recipes.at(i).id == recipe.id) {
            const Recipe old = m_recipes.at(i);
            m_recipes[i] = recipe;
            if (!persist()) {
                m_recipes[i] = old;
                return false;
            }
            emit recipesChanged();
            return true;
        }
    }
    return false;
}

bool RecipeManager::remove(const QString &id)
{
    for (int i = 0; i < m_recipes.size(); ++i) {
        if (m_recipes.at(i).id == id) {
            const Recipe old = m_recipes.takeAt(i);
            if (!persist()) {
                m_recipes.insert(i, old);
                return false;
            }
            emit recipesChanged();
            return true;
        }
    }
    return false;
}

void RecipeManager::seedDefaults()
{
    m_recipes.clear();

    Recipe head;
    head.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    head.name = QStringLiteral("成人头部 CBCT");
    head.description = QStringLiteral("颅脑常规扫描");
    head.hvg.tubeVoltageKv = 100.0;
    head.hvg.tubeCurrentMa = 30.0;
    head.hvg.exposureTimeMs = 800;
    head.gantry.rotationSpeedDegS = 18.0;
    head.gantry.totalFrames = 720;
    m_recipes.append(head);

    Recipe chest;
    chest.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    chest.name = QStringLiteral("胸部低剂");
    chest.description = QStringLiteral("低剂量胸部");
    chest.hvg.tubeVoltageKv = 80.0;
    chest.hvg.tubeCurrentMa = 10.0;
    chest.hvg.exposureTimeMs = 400;
    chest.gantry.rotationSpeedDegS = 25.0;
    chest.gantry.totalFrames = 480;
    chest.recon.algorithm = QStringLiteral("FDK");
    m_recipes.append(chest);

    Recipe limbs;
    limbs.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    limbs.name = QStringLiteral("四肢");
    limbs.description = QStringLiteral("四肢骨关节");
    limbs.hvg.tubeVoltageKv = 70.0;
    limbs.hvg.tubeCurrentMa = 15.0;
    limbs.hvg.exposureTimeMs = 300;
    limbs.gantry.rotationSpeedDegS = 30.0;
    limbs.gantry.totalFrames = 360;
    m_recipes.append(limbs);
}

bool RecipeManager::persist() const
{
    QJsonArray arr;
    for (const Recipe &r : m_recipes) {
        arr.append(RecipeJsonCodec::toJson(r));
    }

    QJsonObject root;
    root[QLatin1String(kVersionKey)] = kStorageVersion;
    root[QLatin1String(kRecipesKey)] = arr;

    QString error;
    if (!JsonFileIo::write(m_storagePath, root, &error)) {
        return false;
    }
    return true;
}

} // namespace cbct
