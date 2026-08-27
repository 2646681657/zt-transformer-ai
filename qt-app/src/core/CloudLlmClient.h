#ifndef CLOUDLLMCLIENT_H
#define CLOUDLLMCLIENT_H
// 云端大模型客户端（OpenAI 兼容协议 /chat/completions）
// 适用于阿里云百炼、DeepSeek、GLM 等兼容服务商（过渡验证）
// 本地 Ollama 部署后由 LocalLlmClient 替换，接口不变

#include "LlmClient.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonArray>

class CloudLlmClient : public LlmClient {
    Q_OBJECT
public:
    explicit CloudLlmClient(const LlmConfig &config,
                            QObject *parent = nullptr);

    // LlmClient 接口
    void chat(const QVector<LlmMessage> &messages,
              int maxTokens = 2048) override;
    void cancel() override;

private:
    // 解析响应 JSON，成功返回回复文本
    QString parseReply(const QByteArray &body, QString *error) const;

    LlmConfig m_config;
    QNetworkAccessManager m_nam;
    QNetworkReply *m_reply = nullptr;
    QStringList m_sslErrors;  // 本次请求收集的 SSL 错误（诊断用）
};

#endif // CLOUDLLMCLIENT_H
