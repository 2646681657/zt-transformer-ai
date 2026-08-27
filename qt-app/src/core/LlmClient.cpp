#include "LlmClient.h"

#include <QSettings>

LlmConfig LlmConfig::load()
{
    LlmConfig c;
    QSettings s(QStringLiteral("ZTF"), QStringLiteral("Designer"));
    c.enabled = s.value(QStringLiteral("llm/enabled"), false).toBool();
    c.provider = s.value(QStringLiteral("llm/provider"),
                          QStringLiteral("cloud")).toString();
    c.baseUrl = s.value(QStringLiteral("llm/baseUrl"),
                        defaultCloudBaseUrl()).toString();
    c.apiKey = s.value(QStringLiteral("llm/apiKey")).toString();
    c.model = s.value(QStringLiteral("llm/model"), defaultModel()).toString();
    c.timeoutSec = s.value(QStringLiteral("llm/timeoutSec"), 60).toInt();
    c.temperature = s.value(QStringLiteral("llm/temperature"), 0.3).toDouble();
    return c;
}

bool LlmConfig::save() const
{
    QSettings s(QStringLiteral("ZTF"), QStringLiteral("Designer"));
    s.setValue(QStringLiteral("llm/enabled"), enabled);
    s.setValue(QStringLiteral("llm/provider"), provider);
    s.setValue(QStringLiteral("llm/baseUrl"), baseUrl);
    s.setValue(QStringLiteral("llm/apiKey"), apiKey);
    s.setValue(QStringLiteral("llm/model"), model);
    s.setValue(QStringLiteral("llm/timeoutSec"), timeoutSec);
    s.setValue(QStringLiteral("llm/temperature"), temperature);
    s.sync();
    return s.status() == QSettings::NoError;
}
