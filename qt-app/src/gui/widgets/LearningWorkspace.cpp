#include "LearningWorkspace.h"
#include "LearningTrainingPanel.h"
#include "LearningPreparationPanel.h"
#include "LearningDataPreparation.h"
#include <QScrollArea>
#include <QDateEdit>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QJsonArray>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>
#include <limits>

using namespace SelfLearning;
namespace {
QLabel *note(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}
QPushButton *action(const QString &text, const char *name, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setObjectName(QString::fromLatin1(name));
    button->setAutoDefault(false);
    return button;
}
void readonlyTable(QTableWidget *table, const QStringList &headers)
{
    table->setColumnCount(headers.size()); table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->hide(); table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true); table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
}
}

LearningWorkspace::LearningWorkspace(const QString &storagePath, QWidget *parent)
    : QWidget(parent), m_path(storagePath)
{
    const QString disabledButtons = QStringLiteral("QPushButton:disabled { color:#8A958F; background:#F1F3F2; border-color:#DDE3DF; }");
    setStyleSheet(disabledButtons);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(note(QStringLiteral("流程：选择已有方案 → 新建样本 → 填写产品编号及实测值 → 保存到本地。未测项目留空，不填0。"), this));
    auto *buttons = new QHBoxLayout;
    auto *pick = action(QStringLiteral("选择方案"), "chooseScheme", this);
    m_new = action(QStringLiteral("从所选方案新建"), "newSample", this);
    auto *import = action(QStringLiteral("导入样本JSON"), "importSamples", this);
    auto *exportAll = action(QStringLiteral("导出样本库"), "exportSamples", this);
    auto *refresh = action(QStringLiteral("重新读取"), "reloadSamples", this);
    for (auto *b : {pick, m_new, import, exportAll, refresh}) buttons->addWidget(b);
    layout->addLayout(buttons);
    m_schemeLabel = note(QStringLiteral("尚未选择方案；仅接受已支持完整计算的结构。"), this);
    layout->addWidget(m_schemeLabel);
    m_list = new QTableWidget(this); m_list->setObjectName("sampleList");
    readonlyTable(m_list, {QStringLiteral("产品/试验编号"), QStringLiteral("方案"), QStringLiteral("试验日期"), QStringLiteral("实测项目数"),QStringLiteral("产品用途")});
    m_list->setMaximumHeight(145);
    layout->addWidget(m_list);
    auto *group = new QGroupBox(QStringLiteral("当前样本（留空指标不进入该目标的训练）"), this);
    auto *editor = new QVBoxLayout(group);
    auto *form = new QFormLayout;
    m_product = new QLineEdit(group); m_product->setObjectName("productId");
    m_product->setPlaceholderText(QStringLiteral("唯一产品编号；同一产品不要换编号重复录入"));
    m_source = new QLineEdit(group); m_source->setObjectName("sampleSource");
    m_source->setPlaceholderText(QStringLiteral("试验报告编号/测量条件与来源"));
    m_date = new QDateEdit(QDate::currentDate(), group); m_date->setCalendarPopup(true);
    m_date->setDisplayFormat("yyyy-MM-dd");
    form->addRow(QStringLiteral("产品/试验编号 *"), m_product);
    form->addRow(QStringLiteral("数据来源（有实测时必填）"), m_source);
    form->addRow(QStringLiteral("试验日期 *"), m_date); editor->addLayout(form);
    m_values = new QTableWidget(MetricCount, 4, group);
    m_values->setObjectName("learningMetrics");
    readonlyTable(m_values, {QStringLiteral("指标"), QStringLiteral("原公式值"), QStringLiteral("实测值（可留空）"), QStringLiteral("DNN修正值")});
    for (int i = 0; i < MetricCount; ++i) {
        m_values->setItem(i, 0, new QTableWidgetItem(QString::fromUtf8(metrics()[i].name) + " (" + metrics()[i].unit + ")"));
        m_values->setItem(i, 1, new QTableWidgetItem("—"));
        m_values->setItem(i, 3, new QTableWidgetItem(QStringLiteral("尚无模型")));
        m_measured[i] = new QLineEdit(group);
        m_measured[i]->setObjectName(QStringLiteral("measured_%1").arg(i));
        m_measured[i]->setPlaceholderText(QStringLiteral("未测留空"));
        m_values->setCellWidget(i, 2, m_measured[i]);
        connect(m_measured[i], &QLineEdit::textChanged, this, [this] { if (!m_loading) { m_dirty = true; refreshState(); } });
    }
    m_values->setMinimumHeight(210); editor->addWidget(m_values);
    auto *saveRow = new QHBoxLayout;
    m_save = action(QStringLiteral("保存当前样本"), "saveSample", group);
    m_template = action(QStringLiteral("导出当前样本/填写模板"), "exportTemplate", group);
    saveRow->addWidget(m_save); saveRow->addWidget(m_template); saveRow->addStretch(); editor->addLayout(saveRow);
    layout->addWidget(group, 1);
    m_feedback = note(QString(), this); layout->addWidget(m_feedback);
    layout->addWidget(note(QStringLiteral("独立本地存储：%1\n旧版实测对比数据不会自动变成训练样本，请核对后在此录入。").arg(m_path), this));
    m_preparation = new LearningPreparationPanel(m_path,this); m_preparation->hide();
    m_preparation->setBeforeMutation([this] { return confirmDiscard(); });
    connect(m_preparation,&LearningPreparationPanel::datasetChanged,this,[this] {
        m_hasSample=false; m_dirty=false; m_original={}; reload();
        feedback(QStringLiteral("数据准备已更新。请在样本列表重新选取记录；训练只使用已分区的训练产品。"));
    });

    m_trainingPage = new QWidget(this);
    m_trainingPage->setStyleSheet(disabledButtons);
    m_trainingPage->hide(); // Reparented into the outer dialog's third tab.
    auto *trainingRoot = new QVBoxLayout(m_trainingPage);
    auto *scroll = new QScrollArea(m_trainingPage); scroll->setWidgetResizable(true);
    auto *body = new QWidget(scroll); scroll->setWidget(body); trainingRoot->addWidget(scroll);
    auto *training = new QVBoxLayout(body);
    training->addWidget(note(QStringLiteral("DNN残差学习：学习“实测值 − 原公式值”，原公式结果始终独立保留。"), m_trainingPage));
    m_trainingStatus = note(QString(), m_trainingPage); m_trainingStatus->setObjectName("trainingStatus");
    training->addWidget(m_trainingStatus);
    m_counts = new QTableWidget(MetricCount, 3, m_trainingPage);
    m_counts->setFixedHeight(210);
    readonlyTable(m_counts, {QStringLiteral("训练目标"), QStringLiteral("训练标签 / 独立产品"), QStringLiteral("运行准备（非精度）")});
    training->addWidget(m_counts);
    auto *jobs = new QHBoxLayout;
    m_exportTraining = action(QStringLiteral("导出训练交接包"), "exportTraining", m_trainingPage);
    jobs->addWidget(m_exportTraining);
    training->addLayout(jobs);
    m_trainingPanel = new LearningTrainingPanel(QFileInfo(m_path).absolutePath(), body);
    m_trainingPanel->setDatasetSource(m_path);
    training->addWidget(m_trainingPanel);
    connect(m_trainingPanel, &LearningTrainingPanel::predictionInvalidated, this, [this] {
        for (int i = 0; i < MetricCount; ++i) { m_values->item(i, 3)->setText(QStringLiteral("尚未独立预测")); m_values->item(i, 3)->setToolTip(QString()); }
    });
    connect(m_trainingPanel, &LearningTrainingPanel::predictionReady, this, [this](const QJsonObject &values, bool testOnly, const QString &version, bool approved) {
        for (int i = 0; i < MetricCount; ++i) {
            const auto v = values.value(metrics()[i].key);
            m_values->item(i, 3)->setText(v.isDouble() ? QString::number(v.toDouble(), 'g', 10)
                + (testOnly ? QStringLiteral("（测试）") : approved ? QStringLiteral("（已审核独立展示）") : QStringLiteral("（独立验证）")) : QStringLiteral("该目标未训练"));
            m_values->item(i, 3)->setToolTip(QStringLiteral("模型版本：%1；%2；不改变原公式或寻优").arg(version, approved ? QStringLiteral("已审核默认独立展示") : QStringLiteral("未审核实验预测")));
        }
    });
    m_trainingFeedback = note(QString(), m_trainingPage);
    training->addWidget(m_trainingFeedback);
    training->addWidget(note(QStringLiteral("本地CPU训练与独立验证；有标签不代表数据覆盖范围或精度已满足上线要求。模型不会自动启用，也不参与寻优。\n范围：油浸式、箔绕、叠片硅钢、椭圆心柱/T形轭、双绕组、多层圆筒式、50Hz/75℃。数据默认保存在本地，不上传。"), m_trainingPage));
    training->addStretch();

    connect(pick, &QPushButton::clicked, this, &LearningWorkspace::chooseScheme);
    connect(m_new, &QPushButton::clicked, this, &LearningWorkspace::newSample);
    connect(m_save, &QPushButton::clicked, this, &LearningWorkspace::saveSample);
    connect(m_template, &QPushButton::clicked, this, [this] { exportJson(false, true); });
    connect(exportAll, &QPushButton::clicked, this, [this] { exportJson(false, false); });
    connect(m_exportTraining, &QPushButton::clicked, this, [this] { exportJson(true, false); });
    connect(import, &QPushButton::clicked, this, &LearningWorkspace::importJson);
    connect(refresh, &QPushButton::clicked, this, [this] { if (confirmDiscard()) { m_hasSample = false; reload(); } });
    connect(m_product, &QLineEdit::textChanged, this, [this] { if (!m_loading) m_dirty = true; refreshState(); });
    connect(m_source, &QLineEdit::textChanged, this, [this] { if (!m_loading) { m_dirty = true; refreshState(); } });
    connect(m_date, &QDateEdit::dateChanged, this, [this] { if (!m_loading) { m_dirty = true; refreshState(); } });
    connect(m_list, &QTableWidget::currentCellChanged, this, [this](int row, int, int previous, int) {
        if (row < 0 || row >= m_samples.size()) return;
        if (!confirmDiscard()) { QSignalBlocker block(m_list); m_list->setCurrentCell(previous, 0); return; }
        m_original = encode(m_samples[row]); showSample(m_samples[row]);
    });
    reload();
}
QWidget *LearningWorkspace::preparationPage() const { return m_preparation; }

void LearningWorkspace::feedback(const QString &text, bool error)
{
    m_feedback->setText(text);
    m_feedback->setStyleSheet(error ? "color:#a12622;" : "color:#1b6547;");
    m_trainingFeedback->setText(text);
    m_trainingFeedback->setStyleSheet(m_feedback->styleSheet());
}

bool LearningWorkspace::confirmDiscard()
{
    if (m_trainingPanel && m_trainingPanel->isBusy()) {
        if (QMessageBox::question(this, QStringLiteral("训练任务进行中"), QStringLiteral("是否取消本地任务？取消完成后可再次关闭窗口。")) == QMessageBox::Yes)
            m_trainingPanel->requestCancel();
        return false;
    }
    if (!m_hasSample || !m_dirty) return true;
    if (QMessageBox::question(this, QStringLiteral("未保存的学习样本"),
            QStringLiteral("当前样本的编辑尚未保存，是否放弃这些修改？"), QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Cancel) != QMessageBox::Discard) return false;
    // Discard really restores the last saved/draft snapshot even if the next
    // file picker is cancelled or a replacement scheme fails calculation.
    showSample(m_sample); return true;
}

void LearningWorkspace::setScheme(const CalcInput &input, const QString &name)
{
    // Selection only changes the source for the NEXT draft, never silently edits an existing one.
    m_schemeInput = input; m_schemeName = name; m_hasScheme = true;
    m_schemeLabel->setText(QStringLiteral("新建来源：%1（更换方案不会改动当前样本，须点击“从所选方案新建”）").arg(name));
    refreshState();
}

void LearningWorkspace::refreshState()
{
    m_new->setEnabled(m_hasScheme && m_healthy);
    m_save->setEnabled(m_hasSample && m_healthy && !m_product->text().trimmed().isEmpty());
    m_template->setEnabled(m_hasSample && !m_original.isEmpty() && !m_dirty);
    m_product->setEnabled(m_hasSample); m_source->setEnabled(m_hasSample); m_date->setEnabled(m_hasSample);
    for (auto *edit : m_measured) edit->setEnabled(m_hasSample);
    const auto &prepared = m_readiness;
    const auto targets = prepared["targets"].toArray();
    bool any = false;
    for (int i = 0; i < MetricCount; ++i) {
        const auto target=i<targets.size()?targets[i].toObject():QJsonObject{};
        any |= target["trainingSamples"].toInt()>0;
        m_counts->setItem(i, 0, new QTableWidgetItem(QString::fromUtf8(metrics()[i].name)));
        m_counts->setItem(i, 1, new QTableWidgetItem(QStringLiteral("%1 / %2").arg(target["trainingSamples"].toInt()).arg(target["trainingProducts"].toInt())));
        m_counts->setItem(i, 2, new QTableWidgetItem(target["ready"].toBool()?QStringLiteral("可尝试训练，需验证"):QStringLiteral("先分区并补训练标签")));
    }
    m_exportTraining->setEnabled(m_healthy && any);
    m_trainingStatus->setText(m_healthy ? QStringLiteral("%1个已保存样本；先在“数据准备”分区。仅训练产品进入训练，保留测试不进入；没有标签的目标不训练。").arg(m_samples.size())
        : QStringLiteral("样本库读取失败，禁止训练；请先修复文件再重新读取。"));
}

void LearningWorkspace::reload()
{
    QString error;
    QVector<Sample> loaded;
    m_healthy = load(m_path, loaded, error);
    if (!m_healthy) {
        m_readiness=LearningPreparation::inspect({});
        if(m_trainingPanel) m_trainingPanel->setDataset({}); if(m_preparation) m_preparation->setDataset({},false);
        feedback(QStringLiteral("读取失败：%1").arg(error), true); refreshState(); return;
    }
    m_samples = loaded;
    m_readiness=LearningPreparation::inspect(m_samples);
    if(m_trainingPanel) m_trainingPanel->setDataset(m_samples); if(m_preparation) m_preparation->setDataset(m_samples,true);
    QSignalBlocker block(m_list);
    m_list->setRowCount(m_samples.size());
    for (int row = 0; row < m_samples.size(); ++row) {
        const auto &s = m_samples[row]; int n = 0; for (const auto &v : s.measured) if (v) ++n;
        const QStringList values{s.productId, s.schemeName, s.testedOn.toString(Qt::ISODate), QString::number(n),purposeName(s.purpose)};
        for (int col = 0; col < values.size(); ++col) m_list->setItem(row, col, new QTableWidgetItem(values[col]));
    }
    if (!m_hasSample) {
        m_loading = true; m_product->clear(); m_source->clear();
        for (int i = 0; i < MetricCount; ++i) { m_measured[i]->clear(); m_values->item(i, 1)->setText("—"); m_values->item(i, 3)->setText(QStringLiteral("尚未独立预测")); m_values->item(i, 3)->setToolTip(QString()); }
        if (m_trainingPanel) m_trainingPanel->setPreviewSample({});
        m_loading = false; m_dirty = false; m_original = {};
    }
    refreshState();
}

void LearningWorkspace::showSample(const Sample &s)
{
    m_loading = true; m_sample = s; m_hasSample = true;
    m_product->setText(s.productId); m_source->setText(s.source); m_date->setDate(s.testedOn);
    for (int i = 0; i < MetricCount; ++i) {
        m_values->item(i, 1)->setText(QString::number(s.baseline[i], 'g', 12));
        m_values->item(i, 3)->setText(QStringLiteral("尚未独立预测"));
        m_values->item(i, 3)->setToolTip(QString());
        m_measured[i]->setText(s.measured[i] ? QString::number(*s.measured[i], 'g', std::numeric_limits<double>::max_digits10) : QString());
    }
    if (m_trainingPanel) m_trainingPanel->setPreviewSample(encode(s));
    m_loading = false; m_dirty = false; refreshState();
}

void LearningWorkspace::newSample()
{
    if (!m_hasScheme || !confirmDiscard()) return;
    Sample sample; QString error;
    if (!makeSample(m_schemeInput, sample, error)) { feedback(error, true); return; }
    sample.schemeName = m_schemeName; m_original = {};
    showSample(sample);
    feedback(QStringLiteral("已建立未保存草稿；请填写产品编号。没有实测数据也可先保存。"));
}

void LearningWorkspace::saveSample()
{
    if (!m_hasSample || !m_healthy) return;
    auto updated = m_sample; updated.productId = m_product->text().trimmed();
    updated.source = m_source->text().trimmed(); updated.testedOn = m_date->date();
    // A new test of an already partitioned product belongs to that product's
    // existing group. The transaction still rejects concurrent group changes.
    if(m_original.isEmpty()) for(const auto &saved:m_samples)
        if(saved.productId.trimmed().toCaseFolded()==updated.productId.toCaseFolded()) {updated.purpose=saved.purpose;break;}
    for (int i = 0; i < MetricCount; ++i) {
        const auto text = m_measured[i]->text().trimmed();
        if (text.isEmpty()) { updated.measured[i].reset(); continue; }
        bool ok = false; const double v = QLocale::c().toDouble(text, &ok);
        if (!ok || !std::isfinite(v)) { feedback(QStringLiteral("%1须填有效数值，未测请留空。").arg(QString::fromUtf8(metrics()[i].name)), true); return; }
        updated.measured[i] = v;
    }
    QString error;
    if (!updateSample(m_path, updated, m_original, error)) { feedback(error, true); return; }
    m_original = encode(updated); showSample(updated); reload();
    feedback(QStringLiteral("样本已保存；用途：%1。同产品新增试验继承已有用途，未测指标保持为空。").arg(purposeName(updated.purpose)));
}

void LearningWorkspace::exportJson(bool training, bool selected)
{
    QString error; QVector<Sample> saved;
    if (!load(m_path, saved, error)) { feedback(error, true); return; }
    if (selected) {
        if (m_dirty) { feedback(QStringLiteral("请先保存当前修改，再导出填写模板。"), true); return; }
        QVector<Sample> one; for (const auto &s : saved) if (s.id == m_sample.id) one.append(s);
        if (one.isEmpty()) { feedback(QStringLiteral("当前样本尚未保存。"), true); return; }
        saved = one;
    }
    QJsonObject json;
    if (training) { if (!trainingBundle(saved, json, error)) { feedback(error, true); return; } }
    else json = document(saved);
    const auto path = QFileDialog::getSaveFileName(this, QStringLiteral("导出本地JSON（只导出已保存数据）"),
        training ? "training_bundle.json" : "learning_samples.json", "JSON (*.json)");
    if (path.isEmpty()) return;
    if (QFileInfo(path).absoluteFilePath().compare(QFileInfo(m_path).absoluteFilePath(), Qt::CaseInsensitive) == 0) {
        feedback(QStringLiteral("导出文件不能覆盖内部样本库，请另选文件名。"), true); return;
    }
    if (!writeJson(path, json, error)) { feedback(error, true); return; }
    feedback(QStringLiteral("已导出：%1；导出不表示训练完成。修改同ID样本请在界面加载后保存，JSON导入不会自动覆盖原样本。").arg(path));
}

void LearningWorkspace::importJson()
{
    if (!confirmDiscard()) return;
    const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("导入学习样本"), QString(), "JSON (*.json)");
    if (path.isEmpty()) return;
    QFile file(path); QString error;
    if (!file.open(QIODevice::ReadOnly)) { feedback(file.errorString(), true); return; }
    if (file.size() > 16 * 1024 * 1024) { feedback(QStringLiteral("文件超过16MB，请分批导入。"), true); return; }
    if (!importDataset(m_path, file.readAll(), error)) { feedback(error, true); return; }
    m_hasSample = false; reload(); feedback(QStringLiteral("导入完成；完全相同的重复样本已跳过。"));
}
