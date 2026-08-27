#include "AiAnalysisDialog.h"
#include "CloudLlmClient.h"
#include "LlmClient.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QMessageBox>

AiAnalysisDialog::AiAnalysisDialog(const QString &title, const QString &taskDesc,
                                   const QString &dataText, QWidget *parent)
    : QDialog(parent)
    , m_taskDesc(taskDesc)
    , m_dataText(dataText)
{
    setWindowTitle(title);
    setModal(true);
    resize(560, 480);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    m_outputEdit = new QTextEdit(this);
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setPlaceholderText(QStringLiteral(
        "点击「开始分析」后，AI 基于引擎计算结果生成分析内容"));
    m_outputEdit->setStyleSheet(
        "QTextEdit { background: #22262e; color: #e0e6ed;"
        " border: 1px solid #3a4050; border-radius: 4px; padding: 6px; }");
    layout->addWidget(m_outputEdit, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #8a9bb0; font-size: 11px;");
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    auto *btnLayout = new QHBoxLayout();
    m_submitBtn = new QPushButton(QStringLiteral("开始分析"), this);
    m_submitBtn->setCursor(Qt::PointingHandCursor);
    m_submitBtn->setStyleSheet(
        "QPushButton { background: #00bcd4; color: #1a1d23; font-size: 12px;"
        " padding: 8px 24px; border: none; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background: #4dd0e1; }"
        "QPushButton:disabled { background: #45505e; color: #8a9bb0; }");
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    closeBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #8a9bb0; font-size: 12px;"
        " padding: 8px 16px; border: 1px solid #3a4050; border-radius: 4px; }"
        "QPushButton:hover { color: #e0e6ed; }");
    btnLayout->addWidget(m_submitBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);

    connect(m_submitBtn, &QPushButton::clicked, this, &AiAnalysisDialog::onSubmit);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void AiAnalysisDialog::onSubmit()
{
    const LlmConfig cfg = LlmConfig::load();
    if (!cfg.enabled) {
        QMessageBox::warning(this, QStringLiteral("AI 未启用"),
            QStringLiteral("请先在「程序工具 → 系统设置 → AI 配置」中启用 AI 功能并配置密钥"));
        return;
    }

    m_submitBtn->setEnabled(false);
    m_submitBtn->setText(QStringLiteral("分析中…"));
    m_statusLabel->setText(QStringLiteral("正在请求大模型，请稍候…"));

    QVector<LlmMessage> msgs;
    msgs.append({QStringLiteral("system"), QStringLiteral(
        "你是变压器电磁设计专家。只基于用户提供的数据进行分析，"
        "不得编造数据中不存在的数值。用中文分点输出，简明扼要。") + m_taskDesc});
    msgs.append({QStringLiteral("user"), m_dataText});

    m_client = new CloudLlmClient(cfg, this);
    connect(m_client, &LlmClient::finished, this, &AiAnalysisDialog::onLlmFinished);
    connect(m_client, &LlmClient::failed, this, &AiAnalysisDialog::onLlmFailed);
    m_client->chat(msgs, 1024);
}

void AiAnalysisDialog::onLlmFinished(const QString &reply)
{
    m_client->deleteLater();
    m_client = nullptr;
    m_submitBtn->setEnabled(true);
    m_submitBtn->setText(QStringLiteral("开始分析"));
    m_statusLabel->setText(QString());
    m_outputEdit->setPlainText(reply.trimmed());
}

void AiAnalysisDialog::onLlmFailed(const QString &error)
{
    m_client->deleteLater();
    m_client = nullptr;
    m_submitBtn->setEnabled(true);
    m_submitBtn->setText(QStringLiteral("开始分析"));
    m_statusLabel->setText(error);
}
