#ifndef CBCT_CLIENT_CORE_H
#define CBCT_CLIENT_CORE_H

#include <QObject>

namespace cbct {

class RecipeManager;

/// 核心门面：app 只通过 instance() 访问各子模块
class ClientCore : public QObject
{
    Q_OBJECT

public:
    static ClientCore &instance();

    ClientCore(const ClientCore &) = delete;
    ClientCore &operator=(const ClientCore &) = delete;

    /// 幂等初始化；UI 启动时调用一次
    void initialize();

    RecipeManager &recipeManager();

signals:
    void ready();

private:
    explicit ClientCore(QObject *parent = nullptr);

    RecipeManager *m_recipeManager = nullptr;
    bool m_initialized = false;
};

} // namespace cbct

#endif // CBCT_CLIENT_CORE_H
