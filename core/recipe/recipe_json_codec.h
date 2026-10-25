#ifndef CBCT_RECIPE_JSON_CODEC_H
#define CBCT_RECIPE_JSON_CODEC_H

#include "recipe/recipe_params.h"

#include <QJsonObject>

namespace cbct {

/// Recipe 与 JSON 互转
class RecipeJsonCodec
{
public:
    static QJsonObject toJson(const Recipe &recipe);
    static Recipe fromJson(const QJsonObject &object, bool *ok = nullptr);
};

} // namespace cbct

#endif // CBCT_RECIPE_JSON_CODEC_H
