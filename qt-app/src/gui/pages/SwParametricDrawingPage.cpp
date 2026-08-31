#include "SwParametricDrawingPage.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

QString groupStyle()
{
    return QStringLiteral(
        "QGroupBox { color: #c0c8d0; font-size: 12px; border: 1px solid #3a4050;"
        " border-radius: 4px; margin-top: 10px; padding-top: 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
}

QPushButton *makeActionButton(const QString &text, const QString &color, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(
        QStringLiteral(
            "QPushButton { background: %1; color: #1a1d23; font-size: 12px;"
            " padding: 7px 16px; border: none; border-radius: 4px; font-weight: bold; }"
            "QPushButton:hover { background: %2; }"
            "QPushButton:disabled { background: #3a4050; color: #777f89; }")
            .arg(color, color == QStringLiteral("#00bcd4")
                            ? QStringLiteral("#4dd0e1")
                            : QStringLiteral("#81c784")));
    return button;
}

} // namespace

SwParametricDrawingPage::SwParametricDrawingPage(QWidget *parent)
    : QWidget(parent), m_automation(new SolidWorksAutomation(this))
{
    setupUi();
    loadSettings();
    updateSolidWorksStatus();

    connect(m_automation, &SolidWorksAutomation::logMessage, this,
            [this](const QString &message) {
        m_logEdit->append(message.toHtmlEscaped().replace(QLatin1Char('\n'),
                                                          QStringLiteral("<br>")));
    });
    connect(m_automation, &SolidWorksAutomation::finished,
            this, &SwParametricDrawingPage::onAutomationFinished);
}

void SwParametricDrawingPage::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto *toolbar = new QWidget(this);
    toolbar->setFixedHeight(34);
    toolbar->setStyleSheet(QStringLiteral(
        "background: #2a2f38; border-bottom: 1px solid #3a4050;"));
    auto *toolLayout = new QHBoxLayout(toolbar);
    toolLayout->setContentsMargins(8, 4, 8, 4);

    auto *pageLabel = new QLabel(QStringLiteral("SW 参数化出图"), toolbar);
    pageLabel->setStyleSheet(
        QStringLiteral("color: #4dd0e1; font-size: 12px; font-weight: bold;"));
    toolLayout->addWidget(pageLabel);
    toolLayout->addStretch();

    m_solidWorksStatusLabel = new QLabel(toolbar);
    toolLayout->addWidget(m_solidWorksStatusLabel);

    auto *detectButton = new QPushButton(QStringLiteral("重新检测"), toolbar);
    detectButton->setCursor(Qt::PointingHandCursor);
    detectButton->setFixedHeight(24);
    toolLayout->addWidget(detectButton);
    connect(detectButton, &QPushButton::clicked,
            this, &SwParametricDrawingPage::onDetectSolidWorks);

    mainLayout->addWidget(toolbar);

    auto *content = new QWidget(this);
    content->setStyleSheet(QStringLiteral("background: #1a1d23;"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(14, 10, 14, 12);
    contentLayout->setSpacing(10);

    auto *hintLabel = new QLabel(QStringLiteral(
        "选择带有可驱动尺寸的 SOLIDWORKS 零件/装配体模板和工程图模板，"
        "填写完整尺寸名后可自动生成参数化模型、SLDDRW 工程图和 PDF。"), content);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet(QStringLiteral(
        "color: #8a9bb0; background: #22262e; border-left: 3px solid #00bcd4;"
        " padding: 7px 10px; font-size: 11px;"));
    contentLayout->addWidget(hintLabel);

    auto *fileGroup = new QGroupBox(QStringLiteral("模板与输出"), content);
    fileGroup->setStyleSheet(groupStyle());
    auto *fileForm = new QFormLayout(fileGroup);
    fileForm->setContentsMargins(12, 8, 12, 10);
    fileForm->setHorizontalSpacing(10);
    fileForm->setVerticalSpacing(7);

    auto makePathRow = [fileGroup](QLineEdit **edit, const QString &buttonText) {
        auto *row = new QWidget(fileGroup);
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);
        *edit = new QLineEdit(row);
        auto *button = new QPushButton(buttonText, row);
        button->setFixedWidth(72);
        layout->addWidget(*edit, 1);
        layout->addWidget(button);
        return qMakePair(row, button);
    };

    auto modelRow = makePathRow(&m_modelTemplateEdit, QStringLiteral("浏览…"));
    m_modelTemplateEdit->setPlaceholderText(
        QStringLiteral("选择 .SLDPRT 或 .SLDASM 参数化模型模板"));
    fileForm->addRow(QStringLiteral("模型模板："), modelRow.first);
    connect(modelRow.second, &QPushButton::clicked,
            this, &SwParametricDrawingPage::onBrowseModelTemplate);

    auto drawingRow = makePathRow(&m_drawingTemplateEdit, QStringLiteral("浏览…"));
    m_drawingTemplateEdit->setPlaceholderText(
        QStringLiteral("选择 .DRWDOT 工程图模板"));
    fileForm->addRow(QStringLiteral("工程图模板："), drawingRow.first);
    connect(drawingRow.second, &QPushButton::clicked,
            this, &SwParametricDrawingPage::onBrowseDrawingTemplate);

    auto outputRow = makePathRow(&m_outputDirectoryEdit, QStringLiteral("浏览…"));
    m_outputDirectoryEdit->setPlaceholderText(QStringLiteral("选择出图文件保存目录"));
    fileForm->addRow(QStringLiteral("输出目录："), outputRow.first);
    connect(outputRow.second, &QPushButton::clicked,
            this, &SwParametricDrawingPage::onBrowseOutputDirectory);

    m_outputNameEdit = new QLineEdit(fileGroup);
    m_outputNameEdit->setPlaceholderText(QStringLiteral("例如：S11-1000-10"));
    fileForm->addRow(QStringLiteral("输出名称："), m_outputNameEdit);
    contentLayout->addWidget(fileGroup);

    auto *parameterGroup = new QGroupBox(QStringLiteral("模型尺寸参数（单位：mm）"), content);
    parameterGroup->setStyleSheet(groupStyle());
    auto *parameterLayout = new QVBoxLayout(parameterGroup);
    parameterLayout->setContentsMargins(12, 8, 12, 10);
    parameterLayout->setSpacing(6);

    auto *parameterHint = new QLabel(QStringLiteral(
        "尺寸名必须与模板完全一致，例如 D1@Sketch1；可在 SOLIDWORKS 中点击尺寸查看完整名称。"),
        parameterGroup);
    parameterHint->setWordWrap(true);
    parameterHint->setStyleSheet(QStringLiteral("color: #8a9bb0; font-size: 11px;"));
    parameterLayout->addWidget(parameterHint);

    m_parameterTable = new QTableWidget(0, 3, parameterGroup);
    m_parameterTable->setHorizontalHeaderLabels({
        QStringLiteral("SOLIDWORKS 尺寸名"),
        QStringLiteral("数值（mm）"),
        QStringLiteral("说明")
    });
    m_parameterTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_parameterTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_parameterTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_parameterTable->verticalHeader()->setVisible(false);
    m_parameterTable->setAlternatingRowColors(true);
    m_parameterTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_parameterTable->setMinimumHeight(150);
    parameterLayout->addWidget(m_parameterTable, 1);

    auto *parameterButtons = new QHBoxLayout();
    auto *addButton = new QPushButton(QStringLiteral("添加参数"), parameterGroup);
    auto *removeButton = new QPushButton(QStringLiteral("删除选中"), parameterGroup);
    auto *clearButton = new QPushButton(QStringLiteral("清空参数"), parameterGroup);
    parameterButtons->addWidget(addButton);
    parameterButtons->addWidget(removeButton);
    parameterButtons->addWidget(clearButton);
    parameterButtons->addStretch();
    parameterLayout->addLayout(parameterButtons);

    connect(addButton, &QPushButton::clicked,
            this, &SwParametricDrawingPage::onAddParameter);
    connect(removeButton, &QPushButton::clicked,
            this, &SwParametricDrawingPage::onRemoveParameters);
    connect(clearButton, &QPushButton::clicked,
            this, &SwParametricDrawingPage::onClearParameters);

    addParameterRow();
    contentLayout->addWidget(parameterGroup, 1);

    auto *optionRow = new QHBoxLayout();
    m_exportPdfCheck = new QCheckBox(QStringLiteral("同时导出 PDF"), content);
    m_exportPdfCheck->setChecked(true);
    m_showSolidWorksCheck = new QCheckBox(QStringLiteral("显示 SOLIDWORKS 窗口"), content);
    m_showSolidWorksCheck->setChecked(true);
    m_insertDimensionsCheck = new QCheckBox(QStringLiteral("自动导入模型尺寸"), content);
    m_insertDimensionsCheck->setChecked(true);
    optionRow->addWidget(m_exportPdfCheck);
    optionRow->addWidget(m_showSolidWorksCheck);
    optionRow->addWidget(m_insertDimensionsCheck);
    optionRow->addStretch();
    contentLayout->addLayout(optionRow);

    auto *actionRow = new QHBoxLayout();
    m_generateButton = makeActionButton(QStringLiteral("开始参数化出图"),
                                        QStringLiteral("#00bcd4"), content);
    m_openOutputButton = makeActionButton(QStringLiteral("打开输出目录"),
                                          QStringLiteral("#4caf50"), content);
    m_openOutputButton->setEnabled(false);
    actionRow->addWidget(m_generateButton);
    actionRow->addWidget(m_openOutputButton);
    actionRow->addStretch();
    contentLayout->addLayout(actionRow);

    connect(m_generateButton, &QPushButton::clicked,
            this, &SwParametricDrawingPage::onGenerate);
    connect(m_openOutputButton, &QPushButton::clicked,
            this, &SwParametricDrawingPage::onOpenOutputDirectory);

    m_logEdit = new QTextEdit(content);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(105);
    m_logEdit->setPlaceholderText(QStringLiteral("任务日志将在这里显示……"));
    contentLayout->addWidget(m_logEdit);

    mainLayout->addWidget(content, 1);
}

void SwParametricDrawingPage::addParameterRow(const QString &name, double valueMm,
                                               const QString &description)
{
    const int row = m_parameterTable->rowCount();
    m_parameterTable->insertRow(row);
    m_parameterTable->setItem(row, 0, new QTableWidgetItem(name));
    m_parameterTable->setItem(row, 2, new QTableWidgetItem(description));

    auto *valueSpin = new QDoubleSpinBox(m_parameterTable);
    valueSpin->setRange(0.001, 1000000.0);
    valueSpin->setDecimals(3);
    valueSpin->setSingleStep(1.0);
    valueSpin->setValue(valueMm);
    valueSpin->setSuffix(QStringLiteral(" mm"));
    m_parameterTable->setCellWidget(row, 1, valueSpin);
}

void SwParametricDrawingPage::onBrowseModelTemplate()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 SOLIDWORKS 模型模板"),
        QFileInfo(m_modelTemplateEdit->text()).absolutePath(),
        QStringLiteral("SOLIDWORKS 模型 (*.SLDPRT *.sldprt *.SLDASM *.sldasm)"));
    if (!path.isEmpty()) {
        m_modelTemplateEdit->setText(QDir::toNativeSeparators(path));
    }
}

void SwParametricDrawingPage::onBrowseDrawingTemplate()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 SOLIDWORKS 工程图模板"),
        QFileInfo(m_drawingTemplateEdit->text()).absolutePath(),
        QStringLiteral("SOLIDWORKS 工程图模板 (*.DRWDOT *.drwdot)"));
    if (!path.isEmpty()) {
        m_drawingTemplateEdit->setText(QDir::toNativeSeparators(path));
    }
}

void SwParametricDrawingPage::onBrowseOutputDirectory()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择出图文件保存目录"),
        m_outputDirectoryEdit->text());
    if (!path.isEmpty()) {
        m_outputDirectoryEdit->setText(QDir::toNativeSeparators(path));
    }
}

void SwParametricDrawingPage::onAddParameter()
{
    addParameterRow();
    m_parameterTable->setCurrentCell(m_parameterTable->rowCount() - 1, 0);
    m_parameterTable->editItem(m_parameterTable->currentItem());
}

void SwParametricDrawingPage::onRemoveParameters()
{
    QList<int> rows;
    const QModelIndexList selectedRows = m_parameterTable->selectionModel()->selectedRows();
    for (const QModelIndex &index : selectedRows) {
        rows.append(index.row());
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
        m_parameterTable->removeRow(row);
    }
    if (m_parameterTable->rowCount() == 0) {
        addParameterRow();
    }
}

void SwParametricDrawingPage::onClearParameters()
{
    m_parameterTable->setRowCount(0);
    addParameterRow();
}

void SwParametricDrawingPage::onDetectSolidWorks()
{
    updateSolidWorksStatus();
    if (SolidWorksAutomation::isSolidWorksRegistered()) {
        QMessageBox::information(this, QStringLiteral("检测结果"),
                                 QStringLiteral("已检测到 SOLIDWORKS COM 接口。"));
    } else {
        QMessageBox::warning(this, QStringLiteral("检测结果"),
                             QStringLiteral("未检测到 SOLIDWORKS。"
                                            "请安装并激活 SOLIDWORKS 桌面版后重试。"));
    }
}

bool SwParametricDrawingPage::buildRequest(SwDrawingRequest *request,
                                            QString *errorMessage) const
{
    const QString modelPath = QDir::fromNativeSeparators(
        m_modelTemplateEdit->text().trimmed());
    const QString drawingPath = QDir::fromNativeSeparators(
        m_drawingTemplateEdit->text().trimmed());
    const QString outputDirectory = QDir::fromNativeSeparators(
        m_outputDirectoryEdit->text().trimmed());
    const QString outputName = m_outputNameEdit->text().trimmed();

    const QString modelSuffix = QFileInfo(modelPath).suffix().toLower();
    if (!QFileInfo::exists(modelPath)
        || (modelSuffix != QStringLiteral("sldprt")
            && modelSuffix != QStringLiteral("sldasm"))) {
        *errorMessage = QStringLiteral("请选择有效的 .SLDPRT 或 .SLDASM 模型模板。");
        return false;
    }
    if (!QFileInfo::exists(drawingPath)
        || QFileInfo(drawingPath).suffix().compare(
               QStringLiteral("drwdot"), Qt::CaseInsensitive) != 0) {
        *errorMessage = QStringLiteral("请选择有效的 .DRWDOT 工程图模板。");
        return false;
    }
    if (outputDirectory.isEmpty()) {
        *errorMessage = QStringLiteral("请选择输出目录。");
        return false;
    }
    if (!QDir().mkpath(outputDirectory)) {
        *errorMessage = QStringLiteral("无法创建或访问输出目录。");
        return false;
    }
    if (outputName.isEmpty()) {
        *errorMessage = QStringLiteral("请输入输出文件名。");
        return false;
    }
    static const QRegularExpression invalidName(QStringLiteral(R"([<>:"/\\|?*])"));
    if (outputName.contains(invalidName)) {
        *errorMessage = QStringLiteral("输出文件名包含 Windows 不允许的字符。");
        return false;
    }

    QList<SwDimensionParameter> parameters;
    for (int row = 0; row < m_parameterTable->rowCount(); ++row) {
        QTableWidgetItem *nameItem = m_parameterTable->item(row, 0);
        const QString name = nameItem ? nameItem->text().trimmed() : QString();
        if (name.isEmpty()) {
            continue;
        }

        auto *valueSpin =
            qobject_cast<QDoubleSpinBox *>(m_parameterTable->cellWidget(row, 1));
        QTableWidgetItem *descriptionItem = m_parameterTable->item(row, 2);
        parameters.append({
            name,
            valueSpin ? valueSpin->value() : 0.0,
            descriptionItem ? descriptionItem->text().trimmed() : QString()
        });
    }
    if (parameters.isEmpty()) {
        *errorMessage = QStringLiteral("请至少填写一个 SOLIDWORKS 尺寸参数。");
        return false;
    }

    request->modelTemplatePath = QFileInfo(modelPath).absoluteFilePath();
    request->drawingTemplatePath = QFileInfo(drawingPath).absoluteFilePath();
    request->outputDirectory = QDir(outputDirectory).absolutePath();
    request->outputBaseName = outputName;
    request->parameters = parameters;
    request->exportPdf = m_exportPdfCheck->isChecked();
    request->solidWorksVisible = m_showSolidWorksCheck->isChecked();
    request->insertModelDimensions = m_insertDimensionsCheck->isChecked();
    return true;
}

void SwParametricDrawingPage::onGenerate()
{
    if (!SolidWorksAutomation::isSolidWorksRegistered()) {
        updateSolidWorksStatus();
        QMessageBox::warning(
            this, QStringLiteral("未检测到 SOLIDWORKS"),
            QStringLiteral("此功能需要本机安装并激活 SOLIDWORKS 桌面版。"
                           "安装后点击“重新检测”再执行出图。"));
        return;
    }

    SwDrawingRequest request;
    QString errorMessage;
    if (!buildRequest(&request, &errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("参数不完整"), errorMessage);
        return;
    }

    saveSettings();
    m_lastOutputDirectory = request.outputDirectory;
    m_logEdit->clear();
    setBusy(true);
    m_automation->generate(request);
}

void SwParametricDrawingPage::onOpenOutputDirectory()
{
    if (!m_lastOutputDirectory.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_lastOutputDirectory));
    }
}

void SwParametricDrawingPage::onAutomationFinished(bool success, const QString &message,
                                                    const QStringList &outputFiles)
{
    setBusy(false);
    if (success) {
        m_openOutputButton->setEnabled(true);
        const QString detail = outputFiles.isEmpty()
            ? message
            : message + QStringLiteral("\n\n生成文件：\n") + outputFiles.join(QLatin1Char('\n'));
        QMessageBox::information(this, QStringLiteral("出图完成"), detail);
    } else {
        QMessageBox::critical(this, QStringLiteral("出图失败"), message);
    }
}

void SwParametricDrawingPage::updateSolidWorksStatus()
{
    if (SolidWorksAutomation::isSolidWorksRegistered()) {
        m_solidWorksStatusLabel->setText(QStringLiteral("● 已检测到 SOLIDWORKS"));
        m_solidWorksStatusLabel->setStyleSheet(
            QStringLiteral("color: #81c784; font-size: 11px; padding-right: 6px;"));
    } else {
        m_solidWorksStatusLabel->setText(QStringLiteral("● 未检测到 SOLIDWORKS"));
        m_solidWorksStatusLabel->setStyleSheet(
            QStringLiteral("color: #ef5350; font-size: 11px; padding-right: 6px;"));
    }
}

void SwParametricDrawingPage::setBusy(bool busy)
{
    m_generateButton->setEnabled(!busy);
    m_generateButton->setText(busy ? QStringLiteral("正在生成……")
                                   : QStringLiteral("开始参数化出图"));
}

void SwParametricDrawingPage::loadSettings()
{
    QSettings settings(QStringLiteral("ZTF"), QStringLiteral("Designer"));
    m_modelTemplateEdit->setText(
        settings.value(QStringLiteral("solidworks/modelTemplate")).toString());
    m_drawingTemplateEdit->setText(
        settings.value(QStringLiteral("solidworks/drawingTemplate")).toString());

    QString outputDirectory =
        settings.value(QStringLiteral("solidworks/outputDirectory")).toString();
    if (outputDirectory.isEmpty()) {
        outputDirectory = QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation) + QStringLiteral("/ZTF-Drawings");
    }
    m_outputDirectoryEdit->setText(QDir::toNativeSeparators(outputDirectory));
    m_outputNameEdit->setText(
        settings.value(QStringLiteral("solidworks/outputName"),
                       QStringLiteral("ZTF-Design")).toString());
    m_exportPdfCheck->setChecked(
        settings.value(QStringLiteral("solidworks/exportPdf"), true).toBool());
    m_showSolidWorksCheck->setChecked(
        settings.value(QStringLiteral("solidworks/showWindow"), true).toBool());
    m_insertDimensionsCheck->setChecked(
        settings.value(QStringLiteral("solidworks/insertDimensions"), true).toBool());
}

void SwParametricDrawingPage::saveSettings() const
{
    QSettings settings(QStringLiteral("ZTF"), QStringLiteral("Designer"));
    settings.setValue(QStringLiteral("solidworks/modelTemplate"),
                      m_modelTemplateEdit->text());
    settings.setValue(QStringLiteral("solidworks/drawingTemplate"),
                      m_drawingTemplateEdit->text());
    settings.setValue(QStringLiteral("solidworks/outputDirectory"),
                      m_outputDirectoryEdit->text());
    settings.setValue(QStringLiteral("solidworks/outputName"),
                      m_outputNameEdit->text());
    settings.setValue(QStringLiteral("solidworks/exportPdf"),
                      m_exportPdfCheck->isChecked());
    settings.setValue(QStringLiteral("solidworks/showWindow"),
                      m_showSolidWorksCheck->isChecked());
    settings.setValue(QStringLiteral("solidworks/insertDimensions"),
                      m_insertDimensionsCheck->isChecked());
}
