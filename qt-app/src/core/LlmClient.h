#ifndef LLMCLIENT_H
#define LLMCLIENT_H
// 大模型客户端抽象接口（Core 层）
// 统一云端（OpenAI 兼容协议）与本地（Ollama）实现的入口
// 原则：LLM 只做参数推荐/结果解读，数值计算一律由引擎完成

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QObject>

// 一轮对话消息
struct LlmMessage {
    QString role;    // "system" / "user" / "assistant"
    QString content;
};

// LLM 配置（持久化到用户 AppData，密钥绝不入库）
struct LlmConfig {
    bool enabled = false;      // AI 功能总开关（离线模式）
    QString provider;          // "cloud"（过渡验证）/ "local"（Ollama）
    QString baseUrl;           // OpenAI 兼容端点，含 /compatible-mode/v1
    QString apiKey;            // sk- 开头密钥，本地存储
    QString model;             // 模型名，如 qwen3.6-27b
    int timeoutSec = 60;       // 请求超时（秒）
    double temperature = 0.3;  // 低温度：结构化输出更稳定

    // 默认云端端点（阿里云百炼业务空间，OpenAI 兼容协议）
    static QString defaultCloudBaseUrl() {
        return QStringLiteral(
            "https://dashscope.aliyuncs.com/compatible-mode/v1");
    }
    static QString defaultModel() { return QStringLiteral("qwen3.6-27b"); }

    // 从 QSettings 读取，不存在则给默认值
    static LlmConfig load();
    // 写回 QSettings
    bool save() const;
};

// LLM 抽象客户端：异步请求，信号返回结果
class LlmClient : public QObject {
    Q_OBJECT
public:
    explicit LlmClient(QObject *parent = nullptr) : QObject(parent) {}
    ~LlmClient() override = default;

    // 发起一次对话请求（非流式）。messages 含 system/user 等角色
    // maxTokens 限制输出长度；请求完成或失败后发出信号
    virtual void chat(const QVector<LlmMessage> &messages,
                      int maxTokens = 2048) = 0;

    // 取消进行中的请求
    virtual void cancel() = 0;

signals:
    // 成功：返回模型回复文本
    void finished(const QString &reply);
    // 失败：错误说明（网络/鉴权/解析）
    void failed(const QString &error);
};

#endif // LLMCLIENT_H
