#include "DataManagementPage.h"
#include "DesignDatabase.h"
#include "QuoteCalculator.h"
#include "SchemeStore.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>

DataManagementPage::DataManagementPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    refreshStats();
}

void DataManagementPage::setupUi()
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
    auto *pageLabel = new QLabel(QStringLiteral("数据管理"), toolbar);
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

    // 数据统计
    m_statsLabel = new QLabel(content);
    m_statsLabel->setStyleSheet("color: #c0c8d0; font-size: 12px; padding: 8px 12px;"
                               " background: #22262e; border-radius: 4px;");
    contentLayout->addWidget(m_statsLabel);

    // 按钮样式工厂
    auto makeBtn = [](const QString &text, const QString &color = "#00bcd4") {
        auto *btn = new QPushButton(text);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            QString("QPushButton { background: %1; color: #1a1d23; font-size: 12px;"
                    " padding: 8px 20px; border: none; border-radius: 4px; font-weight: bold; }"
                    "QPushButton:hover { background: %2; }")
                .arg(color, color == "#00bcd4" ? "#4dd0e1" : "#ef9a9a"));
        btn->setFixedWidth(200);
        return btn;
    };

    // ---- 设计数据导出 ----
    auto *exportGroup = new QGroupBox(QStringLiteral("设计数据导出"), content);
    exportGroup->setStyleSheet(
        "QGroupBox { color: #c0c8d0; font-size: 12px; border: 1px solid #3a4050;"
        " border-radius: 4px; margin-top: 10px; padding-top: 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    auto *exportLayout = new QHBoxLayout(exportGroup);
    exportLayout->setContentsMargins(12, 8, 12, 12);
    exportLayout->setSpacing(12);

    auto *exportJsonBtn = makeBtn(QStringLiteral("导出为 JSON"));
    auto *exportCsvBtn = makeBtn(QStringLiteral("导出为 CSV"));
    auto *exportHint = new QLabel(QStringLiteral(
        "将硅钢曲线、性能标准、铁芯叠积、线规表、波纹油箱系数导出为文件"));
    exportHint->setStyleSheet("color: #8a9bb0; font-size: 11px;");
    exportHint->setWordWrap(true);

    exportLayout->addWidget(exportJsonBtn);
    exportLayout->addWidget(exportCsvBtn);
    exportLayout->addWidget(exportHint, 1);
    contentLayout->addWidget(exportGroup);

    connect(exportJsonBtn, &QPushButton::clicked, this, &DataManagementPage::onExportJson);
    connect(exportCsvBtn, &QPushButton::clicked, this, &DataManagementPage::onExportCsv);

    // ---- 设计数据导入 ----
    auto *importGroup = new QGroupBox(QStringLiteral("设计数据导入"), content);
    importGroup->setStyleSheet(exportGroup->styleSheet());
    auto *importLayout = new QHBoxLayout(importGroup);
    importLayout->setContentsMargins(12, 8, 12, 12);
    importLayout->setSpacing(12);

    auto *importBtn = makeBtn(QStringLiteral("从 JSON 导入"), "#ff9800");
    auto *importHint = new QLabel(QStringLiteral(
        "从外部 JSON 文件导入设计数据，替换当前运行时数据（不影响内置资源）"));
    importHint->setStyleSheet("color: #8a9bb0; font-size: 11px;");
    importHint->setWordWrap(true);

    importLayout->addWidget(importBtn);
    importLayout->addWidget(importHint, 1);
    contentLayout->addWidget(importGroup);

    connect(importBtn, &QPushButton::clicked, this, &DataManagementPage::onImportJson);

    // ---- 用户数据备份/恢复 ----
    auto *backupGroup = new QGroupBox(QStringLiteral("用户数据备份/恢复"), content);
    backupGroup->setStyleSheet(exportGroup->styleSheet());
    auto *backupLayout = new QHBoxLayout(backupGroup);
    backupLayout->setContentsMargins(12, 8, 12, 12);
    backupLayout->setSpacing(12);

    auto *backupBtn = makeBtn(QStringLiteral("备份数据"), "#4caf50");
    auto *restoreBtn = makeBtn(QStringLiteral("恢复数据"), "#2196f3");
    auto *backupHint = new QLabel(QStringLiteral(
        "备份/恢复报价参数和方案库文件到指定目录"));
    backupHint->setStyleSheet("color: #8a9bb0; font-size: 11px;");
    backupHint->setWordWrap(true);

    backupLayout->addWidget(backupBtn);
    backupLayout->addWidget(restoreBtn);
    backupLayout->addWidget(backupHint, 1);
    contentLayout->addWidget(backupGroup);

    connect(backupBtn, &QPushButton::clicked, this, &DataManagementPage::onBackupUserData);
    connect(restoreBtn, &QPushButton::clicked, this, &DataManagementPage::onRestoreUserData);

    contentLayout->addStretch();
    mainLayout->addWidget(content, 1);
}

void DataManagementPage::refreshStats()
{
    auto &db = DesignDatabase::instance();
    if (!db.isLoaded()) {
        db.load();
    }

    int gradeCount = db.steelCurves().size();
    int steelPoints = 0;
    for (const auto &c : db.steelCurves()) {
        steelPoints += c.points.size();
    }

    m_statsLabel->setText(
        QStringLiteral("当前设计数据：硅钢牌号 %1 个（%2 条曲线点）　"
                       "性能标准 %3 条　铁芯叠积 %4 行　"
                       "波纹油箱系数 %5 条　线规 %6 条")
            .arg(gradeCount).arg(steelPoints)
            .arg(db.perfStandards().size())
            .arg(db.coreRows().size())
            .arg(db.corrCoefs().size())
            .arg(db.wireSpecs().size()));
}

void DataManagementPage::onExportJson()
{
    auto &db = DesignDatabase::instance();
    if (!db.isLoaded()) {
        db.load();
    }

    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出设计数据"),
        QStringLiteral("design_data.json"),
        QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                              QStringLiteral("无法写入文件: %1").arg(path));
        return;
    }
    file.write(db.exportToJson().toJson(QJsonDocument::Indented));
    file.close();

    QMessageBox::information(this, QStringLiteral("导出成功"),
                              QStringLiteral("设计数据已导出到\n%1").arg(path));
}

void DataManagementPage::onExportCsv()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择 CSV 导出目录"));
    if (dir.isEmpty()) return;

    auto &db = DesignDatabase::instance();
    if (!db.isLoaded()) {
        db.load();
    }

    QStringList categories = {"steel", "perf", "core", "corrugated", "wire"};
    QStringList names = {
        QStringLiteral("硅钢性能曲线"), QStringLiteral("性能标准"),
        QStringLiteral("铁芯叠积表"), QStringLiteral("波纹油箱系数"),
        QStringLiteral("线规表")};

    int count = 0;
    for (int i = 0; i < categories.size(); ++i) {
        const QString csv = db.exportToCsv(categories[i]);
        const QString path = dir + "/" + categories[i] + ".csv";
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(csv.toUtf8());
            file.close();
            ++count;
        }
    }

    QMessageBox::information(this, QStringLiteral("导出成功"),
                              QStringLiteral("已导出 %1 个 CSV 文件到\n%2").arg(count).arg(dir));
}

void DataManagementPage::onImportJson()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入设计数据"),
        QString(), QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty()) return;

    auto ret = QMessageBox::question(this, QStringLiteral("确认导入"),
        QStringLiteral("导入将替换当前运行时设计数据，确定继续吗？"),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    auto &db = DesignDatabase::instance();
    if (db.loadFromExternal(path)) {
        refreshStats();
        QMessageBox::information(this, QStringLiteral("导入成功"),
                                  QStringLiteral("设计数据已从\n%1\n导入并更新").arg(path));
    } else {
        QMessageBox::warning(this, QStringLiteral("导入失败"), db.lastError());
    }
}

void DataManagementPage::onBackupUserData()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择备份目录"));
    if (dir.isEmpty()) return;

    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString backupDir = dir + "/ztf_backup_" + stamp;
    QDir().mkpath(backupDir);

    int count = 0;

    // 备份报价参数
    const QString quotePath = QuoteCalculator::defaultParamsPath();
    if (QFile::exists(quotePath)) {
        if (QFile::copy(quotePath, backupDir + "/quote_params.json")) {
            ++count;
        }
    }

    // 备份方案库（用户上次保存的方案文件）
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir appDir(appData);
    const QStringList schemeFiles = appDir.entryList({"*.json"}, QDir::Files);
    for (const QString &f : schemeFiles) {
        if (f == "quote_params.json") continue;
        if (QFile::copy(appDir.absoluteFilePath(f), backupDir + "/" + f)) {
            ++count;
        }
    }

    QMessageBox::information(this, QStringLiteral("备份完成"),
                              QStringLiteral("已备份 %1 个文件到\n%2").arg(count).arg(backupDir));
}

void DataManagementPage::onRestoreUserData()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择恢复目录（含备份文件）"));
    if (dir.isEmpty()) return;

    auto ret = QMessageBox::question(this, QStringLiteral("确认恢复"),
        QStringLiteral("恢复将覆盖当前用户数据，确定继续吗？"),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);

    QDir backupDir(dir);
    const QStringList files = backupDir.entryList({"*.json"}, QDir::Files);
    int count = 0;
    for (const QString &f : files) {
        const QString src = backupDir.absoluteFilePath(f);
        const QString dst = appData + "/" + f;
        // 先删除旧文件再复制
        QFile::remove(dst);
        if (QFile::copy(src, dst)) {
            ++count;
        }
    }

    QMessageBox::information(this, QStringLiteral("恢复完成"),
                              QStringLiteral("已恢复 %1 个文件到\n%2").arg(count).arg(appData));
}
