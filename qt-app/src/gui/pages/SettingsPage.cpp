#include "SettingsPage.h"
#include "QuoteCalculator.h"
#include "CloudLlmClient.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QSettings>
#include <QPrinterInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QCheckBox>
#include <QScrollArea>
#include <QFrame>

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    loadSettings();
}

void SettingsPage::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 工具栏
    auto *toolbar = new QWidget(this);
    toolbar->setFixedHeight(34);
    toolbar->setStyleSheet("background: #2a2f38; border-bottom: 1px solid #3a4050;");
    auto *toolLayout = new QHBoxLayout(toolbar);
    toolLayout->setContentsMargins(8, 4, 8, 4);
    auto *pageLabel = new QLabel(QStringLiteral("系统设置"), toolbar);
    pageLabel->setStyleSheet("color: #4dd0e1; font-size: 12px; font-weight: bold;");
    toolLayout->addWidget(pageLabel);
    toolLayout->addStretch();
    mainLayout->addWidget(toolbar);

    // 内容区
    auto *content = new QWidget(this);
    content->setStyleSheet("background: #1a1d23;");
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(16, 16, 16, 16);
    contentLayout->setSpacing(16);

    // 样式工厂
    const QString groupStyle =
        "QGroupBox { color: #c0c8d0; font-size: 12px; border: 1px solid #3a4050;"
        " border-radius: 4px; margin-top: 10px; padding-top: 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }";
    const QString spinStyle =
        "QSpinBox, QDoubleSpinBox { background: #22262e; color: #e0e6ed;"
        " border: 1px solid #3a4050; border-radius: 4px; padding: 4px 6px; }"
        "QSpinBox:focus, QDoubleSpinBox:focus { border: 1px solid #00bcd4; }";
    const QString comboStyle =
        "QComboBox { background: #22262e; color: #e0e6ed;"
        " border: 1px solid #3a4050; border-radius: 4px; padding: 4px 8px; }"
        "QComboBox:focus { border: 1px solid #00bcd4; }"
        "QComboBox QAbstractItemView { background: #22262e; color: #e0e6ed;"
        " selection-background-color: rgba(0,188,212,0.3); }";

    // ---- 计算精度 ----
    auto *calcGroup = new QGroupBox(QStringLiteral("计算精度"), content);
    calcGroup->setStyleSheet(groupStyle);
    auto *calcForm = new QFormLayout(calcGroup);
    calcForm->setContentsMargins(12, 8, 12, 12);
    calcForm->setSpacing(10);

    m_precisionSpin = new QSpinBox(calcGroup);
    m_precisionSpin->setRange(0, 6);
    m_precisionSpin->setValue(3);
    m_precisionSpin->setStyleSheet(spinStyle);
    calcForm->addRow(QStringLiteral("结果小数位数："), m_precisionSpin);

    // 默认计算模式（与参数设置页 Ribbon 共用偏好；参数页切换模式时也会即时更新）
    m_calcModeCombo = new QComboBox(calcGroup);
    m_calcModeCombo->setStyleSheet(comboStyle);
    m_calcModeCombo->addItem(QStringLiteral("正常模式"), false);
    m_calcModeCombo->addItem(QStringLiteral("专业模式"), true);
    calcForm->addRow(QStringLiteral("默认计算模式："), m_calcModeCombo);

    auto *calcHint = new QLabel(QStringLiteral(
        "影响电磁计算结果在表格中的显示精度（0-6 位）；"
        "默认计算模式在下次进入参数设置页时生效"), calcGroup);
    calcHint->setStyleSheet("color: #8a9bb0; font-size: 11px;");
    calcForm->addRow(QString(), calcHint);
    contentLayout->addWidget(calcGroup);

    // ---- 默认打印机 ----
    auto *printerGroup = new QGroupBox(QStringLiteral("默认打印机"), content);
    printerGroup->setStyleSheet(groupStyle);
    auto *printerForm = new QFormLayout(printerGroup);
    printerForm->setContentsMargins(12, 8, 12, 12);
    printerForm->setSpacing(10);

    m_printerCombo = new QComboBox(printerGroup);
    m_printerCombo->setStyleSheet(comboStyle);
    m_printerCombo->addItem(QStringLiteral("（使用系统默认打印机）"), QString());
    const QStringList printers = QPrinterInfo::availablePrinterNames();
    for (const QString &name : printers) {
        m_printerCombo->addItem(name, name);
    }
    printerForm->addRow(QStringLiteral("打印机："), m_printerCombo);

    auto *printerHint = new QLabel(QStringLiteral(
        "打印计算单时优先使用此打印机，留空则使用系统默认"), printerGroup);
    printerHint->setStyleSheet("color: #8a9bb0; font-size: 11px;");
    printerForm->addRow(QString(), printerHint);
    contentLayout->addWidget(printerGroup);

    // ---- 报价参数 ----
    auto *quoteGroup = new QGroupBox(QStringLiteral("报价参数"), content);
    quoteGroup->setStyleSheet(groupStyle);
    auto *quoteLayout = new QVBoxLayout(quoteGroup);
    quoteLayout->setContentsMargins(12, 8, 12, 12);
    quoteLayout->setSpacing(10);

    // 材料单价
    auto *materialForm = new QFormLayout();
    materialForm->setSpacing(8);
    auto makePriceSpin = [this, spinStyle](double min, double max, double step) {
        auto *s = new QDoubleSpinBox(this);
        s->setRange(min, max);
        s->setSingleStep(step);
        s->setDecimals(2);
        s->setStyleSheet(spinStyle);
        return s;
    };

    m_steelPrice = makePriceSpin(0.0, 99999.0, 1.0);
    m_cuPrice = makePriceSpin(0.0, 99999.0, 1.0);
    m_oilPrice = makePriceSpin(0.0, 99999.0, 1.0);
    m_tankPrice = makePriceSpin(0.0, 99999.0, 1.0);

    materialForm->addRow(QStringLiteral("硅钢片单价（元/kg）："), m_steelPrice);
    materialForm->addRow(QStringLiteral("铜导线基价（元/kg）："), m_cuPrice);
    materialForm->addRow(QStringLiteral("绝缘油单价（元/kg）："), m_oilPrice);
    materialForm->addRow(QStringLiteral("油箱钢材单价（元/kg）："), m_tankPrice);
    quoteLayout->addLayout(materialForm);

    // 费用系数
    auto *feeForm = new QFormLayout();
    feeForm->setSpacing(8);
    auto makePctSpin = [this, spinStyle](double max) {
        auto *s = new QDoubleSpinBox(this);
        s->setRange(0.0, max);
        s->setSingleStep(0.5);
        s->setDecimals(2);
        s->setSuffix(QStringLiteral(" %"));
        s->setStyleSheet(spinStyle);
        return s;
    };

    m_purchasedPartsPct = makePctSpin(100.0);
    m_laborPct = makePctSpin(100.0);
    m_managementPct = makePctSpin(100.0);
    m_profitPct = makePctSpin(100.0);
    m_taxPct = makePctSpin(100.0);
    m_miscCost = makePriceSpin(0.0, 999999.0, 50.0);
    m_miscCost->setSuffix(QStringLiteral(" 元"));
    m_miscCost->setSingleStep(50.0);

    feeForm->addRow(QStringLiteral("外购件系数："), m_purchasedPartsPct);
    feeForm->addRow(QStringLiteral("人工系数："), m_laborPct);
    feeForm->addRow(QStringLiteral("管理费系数："), m_managementPct);
    feeForm->addRow(QStringLiteral("利润率："), m_profitPct);
    feeForm->addRow(QStringLiteral("增值税率："), m_taxPct);
    feeForm->addRow(QStringLiteral("其他固定费用："), m_miscCost);
    quoteLayout->addLayout(feeForm);

    contentLayout->addWidget(quoteGroup);

    // ---- AI 配置 ----
    auto *llmGroup = new QGroupBox(QStringLiteral("AI 配置（大语言模型）"), content);
    llmGroup->setStyleSheet(groupStyle);
    auto *llmForm = new QFormLayout(llmGroup);
    llmForm->setContentsMargins(12, 8, 12, 12);
    llmForm->setSpacing(10);

    const QString editStyle =
        "QLineEdit { background: #22262e; color: #e0e6ed;"
        " border: 1px solid #3a4050; border-radius: 4px; padding: 4px 6px; }"
        "QLineEdit:focus { border: 1px solid #00bcd4; }";

    m_llmEnabled = new QCheckBox(QStringLiteral("启用 AI 功能"), llmGroup);
    m_llmEnabled->setStyleSheet("QCheckBox { color: #e0e6ed; font-size: 12px; }");
    llmForm->addRow(m_llmEnabled);

    m_llmKeyEdit = new QLineEdit(llmGroup);
    m_llmKeyEdit->setPlaceholderText(QStringLiteral("sk- 开头的 API 密钥"));
    m_llmKeyEdit->setEchoMode(QLineEdit::Password);
    m_llmKeyEdit->setStyleSheet(editStyle);
    llmForm->addRow(QStringLiteral("API 密钥："), m_llmKeyEdit);

    m_llmModelEdit = new QLineEdit(llmGroup);
    m_llmModelEdit->setPlaceholderText(LlmConfig::defaultModel());
    m_llmModelEdit->setStyleSheet(editStyle);
    llmForm->addRow(QStringLiteral("模型名："), m_llmModelEdit);

    m_llmUrlEdit = new QLineEdit(llmGroup);
    m_llmUrlEdit->setPlaceholderText(LlmConfig::defaultCloudBaseUrl());
    m_llmUrlEdit->setStyleSheet(editStyle);
    llmForm->addRow(QStringLiteral("接口地址："), m_llmUrlEdit);

    m_llmTestBtn = new QPushButton(QStringLiteral("测试连接"), llmGroup);
    m_llmTestBtn->setCursor(Qt::PointingHandCursor);
    m_llmTestBtn->setStyleSheet(
        "QPushButton { background: #00bcd4; color: #1a1d23; font-size: 12px;"
        " padding: 6px 16px; border: none; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background: #4dd0e1; }"
        "QPushButton:disabled { background: #45505e; color: #8a9bb0; }");
    llmForm->addRow(QString(), m_llmTestBtn);

    auto *llmHint = new QLabel(QStringLiteral(
        "云端过渡验证（OpenAI 兼容协议）。关闭开关即离线模式，AI 功能整体停用；"
        "密钥仅保存在本机用户目录，不会入库"), llmGroup);
    llmHint->setWordWrap(true);
    llmHint->setStyleSheet("color: #8a9bb0; font-size: 11px;");
    llmForm->addRow(QString(), llmHint);
    contentLayout->addWidget(llmGroup);


    // ---- 操作按钮 ----
    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    auto makeBtn = [](const QString &text, const QString &color = "#00bcd4") {
        auto *btn = new QPushButton(text);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            QString("QPushButton { background: %1; color: #1a1d23; font-size: 12px;"
                    " padding: 8px 20px; border: none; border-radius: 4px; font-weight: bold; }"
                    "QPushButton:hover { background: %2; }")
                .arg(color, color == "#00bcd4" ? "#4dd0e1" :
                            color == "#ff9800" ? "#ffb74d" : "#81c784"));
        return btn;
    };

    auto *saveBtn = makeBtn(QStringLiteral("保存设置"), "#4caf50");
    auto *resetQuoteBtn = makeBtn(QStringLiteral("重置报价参数"), "#ff9800");
    auto *restoreBtn = makeBtn(QStringLiteral("恢复默认"), "#ef5350");
    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(resetQuoteBtn);
    btnLayout->addWidget(restoreBtn);
    btnLayout->addStretch();
    contentLayout->addLayout(btnLayout);

    // 状态标签
    m_statusLabel = new QLabel(content);
    m_statusLabel->setStyleSheet("color: #8a9bb0; font-size: 11px;");
    contentLayout->addWidget(m_statusLabel);

    connect(saveBtn, &QPushButton::clicked, this, &SettingsPage::onSave);
    connect(resetQuoteBtn, &QPushButton::clicked, this, &SettingsPage::onResetQuote);
    connect(restoreBtn, &QPushButton::clicked, this, &SettingsPage::onRestoreDefaults);
    connect(m_llmTestBtn, &QPushButton::clicked, this, &SettingsPage::onTestLlm);

    contentLayout->addStretch();

    // 包一层滚动区域：窗口过矮时滚动查看，不再压缩变形
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: #1a1d23; }"
                          "QScrollBar:vertical { background: #22262e; width: 8px; }"
                          "QScrollBar::handle:vertical { background: #45505e;"
                          " border-radius: 4px; min-height: 30px; }"
                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
                          " height: 0px; }");
    content->setAutoFillBackground(true);
    scroll->setWidget(content);
    mainLayout->addWidget(scroll, 1);
}

void SettingsPage::loadSettings()
{
    QSettings s("ZTF", "Designer");

    // 计算精度
    m_precisionSpin->setValue(s.value("calc/precision", 3).toInt());

    // 默认计算模式（与参数设置页共用 optimize/proMode）
    const bool proMode = s.value("optimize/proMode", false).toBool();
    m_calcModeCombo->setCurrentIndex(proMode ? 1 : 0);

    // 默认打印机
    const QString printer = s.value("print/defaultPrinter").toString();
    int idx = m_printerCombo->findData(printer);
    m_printerCombo->setCurrentIndex(idx >= 0 ? idx : 0);

    // 报价参数：优先从持久化文件读取，不存在则用默认
    bool ok = false;
    QuoteParams p = QuoteParams::loadFromFile(QuoteCalculator::defaultParamsPath(), &ok);
    if (!ok) p = QuoteParams();

    m_steelPrice->setValue(p.steelPrice);
    m_cuPrice->setValue(p.cuPrice);
    m_oilPrice->setValue(p.oilPrice);
    m_tankPrice->setValue(p.tankPrice);
    m_purchasedPartsPct->setValue(p.purchasedParts_pct);
    m_laborPct->setValue(p.labor_pct);
    m_managementPct->setValue(p.management_pct);
    m_profitPct->setValue(p.profit_pct);
    m_taxPct->setValue(p.tax_pct);
    m_miscCost->setValue(p.miscCost);

    m_statusLabel->setText(QStringLiteral("设置已加载"));

    // AI 配置
    const LlmConfig llm = LlmConfig::load();
    m_llmEnabled->setChecked(llm.enabled);
    m_llmKeyEdit->setText(llm.apiKey);
    m_llmModelEdit->setText(llm.model);
    m_llmUrlEdit->setText(llm.baseUrl);
}

void SettingsPage::onSave()
{
    QSettings s("ZTF", "Designer");
    s.setValue("calc/precision", m_precisionSpin->value());
    s.setValue("print/defaultPrinter", m_printerCombo->currentData().toString());
    // 默认计算模式（写入后下次进入参数设置页生效）
    s.setValue("optimize/proMode", m_calcModeCombo->currentData().toBool());

    // 保存报价参数到持久化文件
    QuoteParams p;
    p.steelPrice = m_steelPrice->value();
    p.cuPrice = m_cuPrice->value();
    p.oilPrice = m_oilPrice->value();
    p.tankPrice = m_tankPrice->value();
    p.purchasedParts_pct = m_purchasedPartsPct->value();
    p.labor_pct = m_laborPct->value();
    p.management_pct = m_managementPct->value();
    p.profit_pct = m_profitPct->value();
    p.tax_pct = m_taxPct->value();
    p.miscCost = m_miscCost->value();

    const QString path = QuoteCalculator::defaultParamsPath();
    bool quoteOk = QuoteParams::saveToFile(path, p);

    // AI 配置（留空字段回落默认值）
    LlmConfig llm;
    llm.enabled = m_llmEnabled->isChecked();
    llm.provider = QStringLiteral("cloud");
    llm.apiKey = m_llmKeyEdit->text().trimmed();
    llm.model = m_llmModelEdit->text().trimmed().isEmpty()
                    ? LlmConfig::defaultModel()
                    : m_llmModelEdit->text().trimmed();
    llm.baseUrl = m_llmUrlEdit->text().trimmed().isEmpty()
                      ? LlmConfig::defaultCloudBaseUrl()
                      : m_llmUrlEdit->text().trimmed();
    const bool llmOk = llm.save();

    if (quoteOk && llmOk) {
        m_statusLabel->setText(QStringLiteral("设置已保存"));
        m_statusLabel->setStyleSheet("color: #4caf50; font-size: 11px;");
        QMessageBox::information(this, QStringLiteral("保存成功"),
            QStringLiteral("设置已保存"));
    } else if (quoteOk && !llmOk) {
        m_statusLabel->setText(QStringLiteral("AI 配置保存失败，其余设置已保存"));
        m_statusLabel->setStyleSheet("color: #ef5350; font-size: 11px;");
        QMessageBox::warning(this, QStringLiteral("部分保存失败"),
            QStringLiteral("AI 配置写入失败，其余设置已保存"));
    } else {
        m_statusLabel->setText(QStringLiteral("报价参数保存失败，其余设置已保存"));
        m_statusLabel->setStyleSheet("color: #ef5350; font-size: 11px;");
        QMessageBox::warning(this, QStringLiteral("部分保存失败"),
            QStringLiteral("报价参数文件写入失败，其余设置已保存"));
    }
}

void SettingsPage::onResetQuote()
{
    auto ret = QMessageBox::question(this, QStringLiteral("确认重置"),
        QStringLiteral("将报价参数重置为默认值，确定继续吗？"),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    QuoteParams p;
    m_steelPrice->setValue(p.steelPrice);
    m_cuPrice->setValue(p.cuPrice);
    m_oilPrice->setValue(p.oilPrice);
    m_tankPrice->setValue(p.tankPrice);
    m_purchasedPartsPct->setValue(p.purchasedParts_pct);
    m_laborPct->setValue(p.labor_pct);
    m_managementPct->setValue(p.management_pct);
    m_profitPct->setValue(p.profit_pct);
    m_taxPct->setValue(p.tax_pct);
    m_miscCost->setValue(p.miscCost);

    m_statusLabel->setText(QStringLiteral("报价参数已重置为默认值（点击保存生效）"));
}

void SettingsPage::onRestoreDefaults()
{
    auto ret = QMessageBox::question(this, QStringLiteral("确认恢复默认"),
        QStringLiteral("将所有设置恢复为默认值，确定继续吗？"),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    m_precisionSpin->setValue(3);
    m_printerCombo->setCurrentIndex(0);
    m_calcModeCombo->setCurrentIndex(0);

    m_llmEnabled->setChecked(false);
    m_llmKeyEdit->clear();
    m_llmModelEdit->clear();
    m_llmUrlEdit->clear();

    QuoteParams p;
    m_steelPrice->setValue(p.steelPrice);
    m_cuPrice->setValue(p.cuPrice);
    m_oilPrice->setValue(p.oilPrice);
    m_tankPrice->setValue(p.tankPrice);
    m_purchasedPartsPct->setValue(p.purchasedParts_pct);
    m_laborPct->setValue(p.labor_pct);
    m_managementPct->setValue(p.management_pct);
    m_profitPct->setValue(p.profit_pct);
    m_taxPct->setValue(p.tax_pct);
    m_miscCost->setValue(p.miscCost);

    m_statusLabel->setText(QStringLiteral("已恢复默认设置（点击保存生效）"));
}

void SettingsPage::onTestLlm()
{
    // 用表单当前值即时测试（无需先保存）
    LlmConfig cfg;
    cfg.enabled = true;  // 测试时临时启用
    cfg.provider = QStringLiteral("cloud");
    cfg.apiKey = m_llmKeyEdit->text().trimmed();
    cfg.model = m_llmModelEdit->text().trimmed().isEmpty()
                    ? LlmConfig::defaultModel()
                    : m_llmModelEdit->text().trimmed();
    cfg.baseUrl = m_llmUrlEdit->text().trimmed().isEmpty()
                      ? LlmConfig::defaultCloudBaseUrl()
                      : m_llmUrlEdit->text().trimmed();

    if (cfg.apiKey.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("缺少密钥"),
            QStringLiteral("请先填写 API 密钥再测试连接"));
        return;
    }

    m_llmTestBtn->setEnabled(false);
    m_llmTestBtn->setText(QStringLiteral("测试中…"));

    // 客户端生命周期：用完即毁，parent 设为本页
    auto *client = new CloudLlmClient(cfg, this);
    QVector<LlmMessage> msgs;
    msgs.append({QStringLiteral("user"),
                 QStringLiteral("请只回复四个字：连接成功")});

    connect(client, &LlmClient::finished, this, [this, client](const QString &reply) {
        client->deleteLater();
        m_llmTestBtn->setEnabled(true);
        m_llmTestBtn->setText(QStringLiteral("测试连接"));
        QMessageBox::information(this, QStringLiteral("测试成功"),
            QStringLiteral("模型已应答：%1").arg(reply));
    });
    connect(client, &LlmClient::failed, this, [this, client](const QString &error) {
        client->deleteLater();
        m_llmTestBtn->setEnabled(true);
        m_llmTestBtn->setText(QStringLiteral("测试连接"));
        QMessageBox::warning(this, QStringLiteral("测试失败"), error);
    });

    client->chat(msgs, 32);
}
