#include "client_core.h"

#include "recipe/recipe_manager.h"

namespace cbct {

ClientCore &ClientCore::instance()
{
    static ClientCore core;
    return core;
}

void ClientCore::initialize()
{
    if (m_initialized) {
        return;
    }
    m_recipeManager->load();
    m_initialized = true;
    emit ready();
}

RecipeManager &ClientCore::recipeManager()
{
    return *m_recipeManager;
}

ClientCore::ClientCore(QObject *parent)
    : QObject(parent)
    , m_recipeManager(new RecipeManager(this))
{
}

} // namespace cbct
