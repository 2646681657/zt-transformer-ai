#include "CloudLlmClient.h"

#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QSslSocket>
#include <QNetworkProxy>

CloudLlmClient::CloudLlmClient(const LlmConfig &config, QObject *parent)
    : LlmClient(parent)
    , m_config(config)
{
    // LLM 端点均为国内服务，直连不走系统代理
    // （本地代理对部分域名的分流规则会导致连接被关闭）
    m_nam.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
}

void CloudLlmClient::chat(const QVector<LlmMessage> &messages, int maxTokens)
{
    // 参数校验
    if (!m_config.enabled) {
        emit failed(QStringLiteral("AI 功能未启用（离线模式）"));
        return;
    }
    if (m_config.apiKey.isEmpty()) {
        emit failed(QStringLiteral("未配置 API 密钥，请先在系统设置中配置"));
        return;
    }
    if (m_reply) {
        emit failed(QStringLiteral("已有请求进行中，请稍候"));
        return;
    }

    // 组装 OpenAI 兼容请求体
    QJsonObject body;
    body.insert(QStringLiteral("model"), m_config.model);
    QJsonArray msgs;
    for (const auto &m : messages) {
        QJsonObject o;
        o.insert(QStringLiteral("role"), m.role);
        o.insert(QStringLiteral("content"), m.content);
        msgs.append(o);
    }
    body.insert(QStringLiteral("messages"), msgs);
    body.insert(QStringLiteral("max_tokens"), maxTokens);
    body.insert(QStringLiteral("temperature"), m_config.temperature);

    const QString url = m_config.baseUrl + QStringLiteral("/chat/completions");
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json; charset=utf-8"));
    req.setRawHeader(QByteArrayLiteral("Authorization"),
                     QStringLiteral("Bearer %1").arg(m_config.apiKey)
                         .toUtf8());
    req.setTransferTimeout(m_config.timeoutSec * 1000);

    // UTF-8 字节流发送，避免中文乱码
    const QByteArray payload =
        QJsonDocument(body).toJson(QJsonDocument::Compact);
    m_reply = m_nam.post(req, payload);
    m_sslErrors.clear();

    // 收集 SSL 错误细节（诊断用）
    connect(m_reply, &QNetworkReply::sslErrors, this, [this](const QList<QSslError> &errors) {
        for (const auto &e : errors) {
            m_sslErrors.append(e.errorString());
        }
    });

    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QString detail = QStringLiteral("网络错误：%1").arg(reply->errorString());
            // 附带诊断信息：TLS 后端与 SSL 错误
            detail += QStringLiteral("\nTLS 后端：%1")
                          .arg(QSslSocket::activeBackend());
            if (!m_sslErrors.isEmpty()) {
                detail += QStringLiteral("\nSSL 细节：%1").arg(m_sslErrors.join(QStringLiteral("; ")));
            }
            emit failed(detail);
            return;
        }
        QString err;
        const QString text = parseReply(reply->readAll(), &err);
        if (!err.isEmpty()) {
            emit failed(err);
            return;
        }
        emit finished(text);
    });
}

void CloudLlmClient::cancel()
{
    if (m_reply) {
        m_reply->abort();
    }
}

QString CloudLlmClient::parseReply(const QByteArray &body, QString *error) const
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        if (error) *error = QStringLiteral("响应不是有效 JSON");
        return {};
    }
    const QJsonObject root = doc.object();

    // OpenAI 兼容错误结构：{"error": {"message": ...}}
    if (root.contains(QStringLiteral("error"))) {
        const QString msg = root.value(QStringLiteral("error"))
                                .toObject()
                                .value(QStringLiteral("message"))
                                .toString();
        if (error) *error = QStringLiteral("服务端错误：%1").arg(msg);
        return {};
    }

    const QString content = root.value(QStringLiteral("choices"))
                                .toArray()
                                .at(0)
                                .toObject()
                                .value(QStringLiteral("message"))
                                .toObject()
                                .value(QStringLiteral("content"))
                                .toString();
    if (content.isEmpty()) {
        if (error) *error = QStringLiteral("响应中无回复内容");
        return {};
    }
    return content;
}
