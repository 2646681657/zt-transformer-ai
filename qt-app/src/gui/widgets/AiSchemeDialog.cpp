#include "AiSchemeDialog.h"
#include "CloudLlmClient.h"
#include "LlmClient.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QHash>

namespace {
// AI 可推荐的设计变量：JSON 键、合法范围、中文说明、取值/写回函数
struct FieldSpec {
    const char *jsonKey;
    double minVal;
    double maxVal;
    const char *label;
};
const QVector<FieldSpec> kFields = {
    {"capacity_kVA",    30, 4000, "容量（kVA）"},
    {"hvRated_kV",       3,   40, "高压额定（kV）"},
    {"lvRated_kV",     0.1,    1, "低压额定（kV）"},
    {"coreDiameter_mm", 80,  400, "铁芯直径（mm）"},
    {"coreStraight_mm", 40,  300, "铁芯直线长（mm）"},
    {"ellipseAngle_deg", 5,   45, "椭圆角（°）"},
    {"stackFactor",    0.9, 0.99, "叠片系数"},
    {"hvBareWidth_mm", 0.5,   10, "高压裸线宽（mm）"},
    {"hvBareThick_mm", 0.5,   10, "高压裸线厚（mm）"},
    {"lvTurns",          5,  500, "低压匝数"},
    {"lvFoilThick_mm", 0.1,    3, "低压箔厚（mm）"},
    {"lvFoilWidth_mm",  20,  500, "低压箔宽（mm）"},
};
} // namespace

AiSchemeDialog::AiSchemeDialog(const CalcInput &base, QWidget *parent)
    : QDialog(parent)
    , m_base(base)
{
    setWindowTitle(QStringLiteral("AI 方案助手"));
    setModal(true);
    resize(520, 420);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto *intro = new QLabel(QStringLiteral(
        "用一句话描述设计需求，AI 解析为设计变量并填入参数表："), this);
    intro->setStyleSheet("color: #e0e6ed; font-size: 12px;");
    layout->addWidget(intro);

    m_inputEdit = new QTextEdit(this);
    m_inputEdit->setPlaceholderText(QStringLiteral(
        "例：1000kVA 干式变压器，10kV 进线，低压 0.4kV，"
        "低损耗取向硅钢，铁芯直径 220mm 左右"));
    m_inputEdit->setStyleSheet(
        "QTextEdit { background: #22262e; color: #e0e6ed;"
        " border: 1px solid #3a4050; border-radius: 4px; padding: 6px; }");
    layout->addWidget(m_inputEdit, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #8a9bb0; font-size: 11px;");
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);
    m_submitBtn = new QPushButton(QStringLiteral("AI 解析"), this);
    m_submitBtn->setCursor(Qt::PointingHandCursor);
    m_submitBtn->setStyleSheet(
        "QPushButton { background: #00bcd4; color: #1a1d23; font-size: 12px;"
        " padding: 8px 24px; border: none; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background: #4dd0e1; }"
        "QPushButton:disabled { background: #45505e; color: #8a9bb0; }");
    m_applyBtn = new QPushButton(QStringLiteral("应用到参数表"), this);
    m_applyBtn->setEnabled(false);
    m_applyBtn->setCursor(Qt::PointingHandCursor);
    m_applyBtn->setStyleSheet(
        "QPushButton { background: #4caf50; color: #1a1d23; font-size: 12px;"
        " padding: 8px 24px; border: none; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background: #81c784; }"
        "QPushButton:disabled { background: #45505e; color: #8a9bb0; }");
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), this);
    cancelBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #8a9bb0; font-size: 12px;"
        " padding: 8px 16px; border: 1px solid #3a4050; border-radius: 4px; }"
        "QPushButton:hover { color: #e0e6ed; }");
    btnLayout->addWidget(m_submitBtn);
    btnLayout->addWidget(m_applyBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    connect(m_submitBtn, &QPushButton::clicked, this, &AiSchemeDialog::onSubmit);
    connect(m_applyBtn, &QPushButton::clicked, this, [this]() { accept(); });
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void AiSchemeDialog::onSubmit()
{
    const QString text = m_inputEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("请先输入设计需求描述"));
        return;
    }

    const LlmConfig cfg = LlmConfig::load();
    if (!cfg.enabled) {
        QMessageBox::warning(this, QStringLiteral("AI 未启用"),
            QStringLiteral("请先在「程序工具 → 系统设置 → AI 配置」中启用 AI 功能并配置密钥"));
        return;
    }

    m_submitBtn->setEnabled(false);
    m_submitBtn->setText(QStringLiteral("解析中…"));
    m_statusLabel->setText(QStringLiteral("正在请求大模型，请稍候…"));
    m_applyBtn->setEnabled(false);

    // 构造消息：system 约束输出格式 + user 携带需求与当前基准值
    QVector<LlmMessage> msgs;
    msgs.append({QStringLiteral("system"), QStringLiteral(
        "你是变压器电磁设计助手。根据用户的设计需求，推荐干式变压器的设计变量。"
        "只输出一个 JSON 对象，不要输出任何解释文字、markdown 代码块标记。"
        "JSON 字段及合法范围：capacity_kVA(30-4000), hvRated_kV(3-40), "
        "lvRated_kV(0.1-1), coreDiameter_mm(80-400), coreStraight_mm(40-300), "
        "ellipseAngle_deg(5-45), stackFactor(0.9-0.99), hvBareWidth_mm(0.5-10), "
        "hvBareThick_mm(0.5-10), lvTurns(5-500), lvFoilThick_mm(0.1-3), "
        "lvFoilWidth_mm(20-500)。"
        "只包含用户需求中明确或隐含的变量；没有依据的变量不要输出。"
        "所有数值必须在合法范围内。")});
    QString userMsg = QStringLiteral("设计需求：%1\n").arg(text);
    userMsg += QStringLiteral("当前基准值（未提及的变量将保持不变）：");
    userMsg += QStringLiteral("capacity_kVA=%1, hvRated_kV=%2, lvRated_kV=%3, "
                              "coreDiameter_mm=%4")
                  .arg(m_base.capacity_kVA).arg(m_base.hvRated_kV)
                  .arg(m_base.lvRated_kV).arg(m_base.coreDiameter_mm);
    msgs.append({QStringLiteral("user"), userMsg});

    m_client = new CloudLlmClient(cfg, this);
    connect(m_client, &LlmClient::finished, this, &AiSchemeDialog::onLlmFinished);
    connect(m_client, &LlmClient::failed, this, &AiSchemeDialog::onLlmFailed);
    m_client->chat(msgs, 512);
}

void AiSchemeDialog::onLlmFinished(const QString &reply)
{
    m_client->deleteLater();
    m_client = nullptr;
    m_submitBtn->setEnabled(true);
    m_submitBtn->setText(QStringLiteral("AI 解析"));

    if (!parseReply(reply)) {
        m_statusLabel->setText(QStringLiteral(
            "AI 返回格式无法解析，请调整描述后重试"));
    }
}

void AiSchemeDialog::onLlmFailed(const QString &error)
{
    m_client->deleteLater();
    m_client = nullptr;
    m_submitBtn->setEnabled(true);
    m_submitBtn->setText(QStringLiteral("AI 解析"));
    m_statusLabel->setText(error);
}

bool AiSchemeDialog::parseReply(const QString &reply)
{
    // 剥离可能包裹的 markdown 代码块标记
    QString json = reply.trimmed();
    if (json.startsWith(QLatin1String("```"))) {
        const int first = json.indexOf(QLatin1Char('\n'));
        const int last = json.lastIndexOf(QLatin1String("```"));
        if (first >= 0 && last > first) {
            json = json.mid(first + 1, last - first - 1).trimmed();
        }
    }
    // 容错：截取第一个 { 到最后一个 }
    const int lbrace = json.indexOf(QLatin1Char('{'));
    const int rbrace = json.lastIndexOf(QLatin1Char('}'));
    if (lbrace < 0 || rbrace <= lbrace) {
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(
        json.mid(lbrace, rbrace - lbrace + 1).toUtf8());
    if (!doc.isObject()) {
        return false;
    }

    // 合并：范围校验 + 写回（未推荐字段保持 base 值）
    m_result = m_base;
    QStringList changes;
    const QJsonObject obj = doc.object();
    for (const auto &f : kFields) {
        const QString key = QLatin1String(f.jsonKey);
        if (!obj.contains(key)) {
            continue;
        }
        const double v = obj.value(key).toDouble();
        if (v < f.minVal || v > f.maxVal) {
            continue;  // 超范围：丢弃该建议，保持当前值
        }
        if (key == QLatin1String("capacity_kVA"))      m_result.capacity_kVA = v;
        else if (key == QLatin1String("hvRated_kV"))   m_result.hvRated_kV = v;
        else if (key == QLatin1String("lvRated_kV"))   m_result.lvRated_kV = v;
        else if (key == QLatin1String("coreDiameter_mm")) m_result.coreDiameter_mm = v;
        else if (key == QLatin1String("coreStraight_mm")) m_result.coreStraight_mm = v;
        else if (key == QLatin1String("ellipseAngle_deg")) m_result.ellipseAngle_deg = v;
        else if (key == QLatin1String("stackFactor"))  m_result.stackFactor = v;
        else if (key == QLatin1String("hvBareWidth_mm")) m_result.hvBareWidth_mm = v;
        else if (key == QLatin1String("hvBareThick_mm")) m_result.hvBareThick_mm = v;
        else if (key == QLatin1String("lvTurns"))      m_result.lvTurns = qRound(v);
        else if (key == QLatin1String("lvFoilThick_mm")) m_result.lvFoilThick_mm = v;
        else if (key == QLatin1String("lvFoilWidth_mm")) m_result.lvFoilWidth_mm = v;
        changes.append(QStringLiteral("%1 → %2")
                           .arg(QString::fromUtf8(f.label)).arg(v));
    }

    if (changes.isEmpty()) {
        return false;
    }

    m_statusLabel->setText(QStringLiteral("AI 建议修改以下变量：\n%1")
                               .arg(changes.join(QStringLiteral("\n"))));
    m_applyBtn->setEnabled(true);
    return true;
}
