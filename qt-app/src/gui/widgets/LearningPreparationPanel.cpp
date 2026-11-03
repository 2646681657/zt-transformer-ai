#include "LearningPreparationPanel.h"
#include "LearningDataPreparation.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QMap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QLabel *note(const QString &text,QWidget *parent) {
    auto *label=new QLabel(text,parent); label->setWordWrap(true); label->setTextFormat(Qt::PlainText); return label;
}
QTableWidget *table(const QStringList &headers,QWidget *parent) {
    auto *t=new QTableWidget(0,headers.size(),parent); t->setHorizontalHeaderLabels(headers);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers); t->setAlternatingRowColors(true);
    t->verticalHeader()->hide(); t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setStretchLastSection(true); return t;
}
void row(QTableWidget *t,const QStringList &values) {
    const auto r=t->rowCount(); t->insertRow(r);
    for(int col=0;col<values.size();++col) {auto *item=new QTableWidgetItem(values[col]); item->setToolTip(values[col]); t->setItem(r,col,item);}
}
QString value(const std::optional<double> &v) {return v?QString::number(*v,'g',17):QStringLiteral("未测");}
QString change(const QString &old,const QString &now) {return old==now?now:old+QStringLiteral(" → ")+now;}
}
LearningPreparationPanel::LearningPreparationPanel(QString path,QWidget *parent):QWidget(parent),m_path(std::move(path))
{
    auto *root=new QVBoxLayout(this); auto *scroll=new QScrollArea(this); scroll->setWidgetResizable(true); root->addWidget(scroll);
    auto *body=new QWidget(scroll); scroll->setWidget(body); auto *layout=new QVBoxLayout(body);
    layout->addWidget(note(QStringLiteral("先在“学习样本”建立并保存设计样本 → 按产品划分用途 → 补录或批量导入实测 → 检查准备情况 → 进入“训练与模型”。"),body));
    auto *group=new QGroupBox(QStringLiteral("产品用途分区（同产品所有日期一起设置）"),body); auto *gl=new QVBoxLayout(group); auto *choices=new QHBoxLayout;
    m_products=new QComboBox(group); m_products->setObjectName("usageProducts"); m_purpose=new QComboBox(group); m_purpose->setObjectName("productPurpose");
    for(const auto &purpose:QStringList{"unassigned","training","independent_test","excluded"}) m_purpose->addItem(SelfLearning::purposeName(purpose),purpose);
    m_assign=new QPushButton(QStringLiteral("按产品设置用途"),group); m_assign->setObjectName("assignProductPurpose"); m_assign->setAutoDefault(false);
    choices->addWidget(m_products,2); choices->addWidget(m_purpose,1); choices->addWidget(m_assign); gl->addLayout(choices);
    gl->addWidget(note(QStringLiteral("旧样本默认待分区，不会自动训练。保留测试和暂不使用的数据不进入训练。将训练产品改成测试用途并不能变成新的独立产品，后台仍检查历史模型的产品分组。"),group)); layout->addWidget(group);
    auto *csv=new QGroupBox(QStringLiteral("批量填写实测（Excel可打开的CSV模板）"),body); auto *cl=new QVBoxLayout(csv); auto *buttons=new QHBoxLayout;
    m_export=new QPushButton(QStringLiteral("导出已保存样本的实测填写模板"),csv); m_export->setObjectName("exportMeasurementCsv");
    m_import=new QPushButton(QStringLiteral("导入CSV并预览核对"),csv); m_import->setObjectName("importMeasurementCsv");
    m_export->setAutoDefault(false); m_import->setAutoDefault(false); buttons->addWidget(m_export); buttons->addWidget(m_import); cl->addLayout(buttons);
    cl->addWidget(note(QStringLiteral("模板仅更新已有设计样本。保留sample_id和snapshot_sha256；用途代码：training / independent_test / excluded / unassigned。未测留空；损耗W、阻抗%、温升K，不自动换算kW。文本开头单引号保护Excel编号及防止公式执行，导入还原一层。先预览，全部有效并确认后才整批保存。"),csv)); layout->addWidget(csv);
    auto *ready=new QGroupBox(QStringLiteral("训练准备概览（运行门槛，不是精度证明）"),body); auto *rl=new QVBoxLayout(ready);
    m_summary=note(QString(),ready); m_summary->setObjectName("dataReadinessSummary"); rl->addWidget(m_summary);
    m_overview=table({QStringLiteral("目标/单位"),QStringLiteral("训练标签数"),QStringLiteral("训练产品数"),QStringLiteral("保留测试标签数"),QStringLiteral("测试产品数"),QStringLiteral("训练缺测数"),QStringLiteral("运行准备")},ready);
    m_overview->setObjectName("readinessOverview"); m_overview->setMinimumHeight(225); rl->addWidget(m_overview);
    rl->addWidget(note(QStringLiteral("每目标至少5个有标签的独立训练产品，仅表示可尝试分组训练，不代表满足企业误差要求。固定物理上下文不同需分开准备，不得混合套用。独立测试数量仍需企业确认。"),ready)); layout->addWidget(ready);
    auto *quality=new QGroupBox(QStringLiteral("数据质量检查（只提示，不自动删除或补0）"),body); auto *ql=new QVBoxLayout(quality);
    m_issues=table({QStringLiteral("级别"),QStringLiteral("产品"),QStringLiteral("样本ID"),QStringLiteral("问题/建议")},quality); m_issues->setObjectName("learningDataIssues"); m_issues->setMinimumHeight(190); ql->addWidget(m_issues); layout->addWidget(quality);
    m_feedback=note(QString(),body); m_feedback->setObjectName("dataPreparationFeedback"); layout->addWidget(m_feedback); layout->addStretch();
    connect(m_products,&QComboBox::currentIndexChanged,this,&LearningPreparationPanel::selectProduct);
    connect(m_assign,&QPushButton::clicked,this,&LearningPreparationPanel::assignPurpose);
    connect(m_export,&QPushButton::clicked,this,&LearningPreparationPanel::exportCsv);
    connect(m_import,&QPushButton::clicked,this,&LearningPreparationPanel::importCsv); setDataset({},true);
}
void LearningPreparationPanel::feedback(const QString &text,bool error) {m_feedback->setText(text); m_feedback->setStyleSheet(error?"color:#a12622;":"color:#1b6547;");}
void LearningPreparationPanel::setDataset(const QVector<SelfLearning::Sample> &samples,bool healthy)
{
    m_healthy=healthy; m_samples=healthy?samples:QVector<SelfLearning::Sample>{}; const auto previous=m_products->currentData().toString(); QSignalBlocker block(m_products); m_products->clear();
    QMap<QString,QVector<SelfLearning::Sample>> groups; for(const auto &s:m_samples) groups[s.productId.trimmed().toCaseFolded()].append(s);
    for(auto it=groups.begin();it!=groups.end();++it) m_products->addItem(QStringLiteral("%1 · %2 · %3条试验").arg(it.value().first().productId,SelfLearning::purposeName(it.value().first().purpose)).arg(it.value().size()),it.key());
    const int selected=m_products->findData(previous); if(selected>=0) m_products->setCurrentIndex(selected); selectProduct();
    m_products->setEnabled(healthy&&!m_samples.isEmpty()); m_export->setEnabled(healthy&&!m_samples.isEmpty()); m_import->setEnabled(healthy&&!m_samples.isEmpty());
    const auto report=LearningPreparation::inspect(m_samples), counts=report["groups"].toObject();
    m_summary->setText(healthy?QStringLiteral("独立产品数：训练%1，保留测试%2，待分区%3，暂不使用%4；训练固定物理上下文%5种。下表按实际标签计数，不用重复试验凑产品数。")
        .arg(counts["training"].toInt()).arg(counts["independent_test"].toInt()).arg(counts["unassigned"].toInt()).arg(counts["excluded"].toInt()).arg(report["trainingContextCount"].toInt()):QStringLiteral("样本库读取失败，禁止分区和导入导出；原文件保留，请先重新读取或修复。"));
    m_overview->setRowCount(0);
    for(const auto &item:report["targets"].toArray()) {const auto t=item.toObject(); row(m_overview,{t["name"].toString()+" ("+t["unit"].toString()+")",QString::number(t["trainingSamples"].toInt()),QString::number(t["trainingProducts"].toInt()),QString::number(t["testSamples"].toInt()),QString::number(t["testProducts"].toInt()),QString::number(t["missingTrainingSamples"].toInt()),report["trainingContextCount"].toInt()>1?QStringLiteral("须分开物理上下文"):t["ready"].toBool()?QStringLiteral("可尝试训练，待验证"):QStringLiteral("训练产品标签不足")});}
    m_issues->setRowCount(0); for(const auto &item:report["issues"].toArray()) {const auto issue=item.toObject(); const auto severity=issue["severity"].toString(); row(m_issues,{severity=="error"?QStringLiteral("错误"):severity=="warning"?QStringLiteral("注意"):QStringLiteral("提示"),issue["productId"].toString(),issue["sampleId"].toString(),issue["message"].toString()});}
}
void LearningPreparationPanel::selectProduct()
{
    const auto product=m_products->currentData().toString(); const bool available=m_healthy&&!product.isEmpty(); m_assign->setEnabled(available); m_purpose->setEnabled(available);
    for(const auto &s:m_samples) if(s.productId.trimmed().toCaseFolded()==product) {m_purpose->setCurrentIndex(m_purpose->findData(s.purpose)); break;}
}
void LearningPreparationPanel::assignPurpose()
{
    if(!m_assign->isEnabled() || (m_guard&&!m_guard())) return;
    const auto product=m_products->currentData().toString(); QVector<SelfLearning::Sample> originals; for(const auto &s:m_samples) if(s.productId.trimmed().toCaseFolded()==product) originals.append(s);
    QString error; if(!SelfLearning::assignProductPurpose(m_path,product,m_purpose->currentData().toString(),originals,error)) {feedback(error,true); return;}
    emit datasetChanged(); QVector<SelfLearning::Sample> fresh;
    if(SelfLearning::load(m_path,fresh,error)) setDataset(fresh,true); else {setDataset({},false); feedback(error,true); return;}
    feedback(QStringLiteral("同产品的%1条试验已一起设置用途；实测、设计快照和原公式未改变。").arg(originals.size()));
}
void LearningPreparationPanel::exportCsv()
{
    if(!m_export->isEnabled()) return; QVector<SelfLearning::Sample> saved; QString error;
    if(!SelfLearning::load(m_path,saved,error)) {feedback(error,true);return;}
    const auto path=QFileDialog::getSaveFileName(this,QStringLiteral("导出实测填写模板"),"learning_measurements.csv","CSV (*.csv)"); if(path.isEmpty()) return;
    if(QFileInfo(path).absoluteFilePath().compare(QFileInfo(m_path).absoluteFilePath(),Qt::CaseInsensitive)==0) {feedback(QStringLiteral("模板不能覆盖内部样本库。"),true);return;}
    QSaveFile file(path); file.setDirectWriteFallback(false); const auto bytes=LearningPreparation::csvTemplate(saved);
    if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) {feedback(file.errorString(),true);return;}
    feedback(QStringLiteral("已导出%1个已保存样本；未保存修改不在模板中。填完后导入并预览核对。").arg(saved.size()));
}
void LearningPreparationPanel::importCsv()
{
    if(!m_import->isEnabled() || (m_guard&&!m_guard())) return;
    const auto path=QFileDialog::getOpenFileName(this,QStringLiteral("选择填写后的实测CSV"),QString(),"CSV (*.csv)"); if(path.isEmpty()) return;
    QFile file(path); if(!file.open(QIODevice::ReadOnly)) {feedback(file.errorString(),true);return;}
    if(file.size()>16*1024*1024) {feedback(QStringLiteral("CSV超过16MB，请分批导入。"),true);return;}
    QVector<SelfLearning::Sample> saved; QString error; if(!SelfLearning::load(m_path,saved,error)) {feedback(error,true);return;}
    LearningPreparation::CsvPreview preview; const bool parsed=LearningPreparation::previewCsv(file.readAll(),saved,preview,error);
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("CSV导入预览（尚未写入）")); dialog.resize(1080,650); auto *layout=new QVBoxLayout(&dialog);
    layout->addWidget(note(QStringLiteral("固定映射：sample_id=已有样本；snapshot_sha256=原样本摘要；product_id=产品；source=来源；tested_on=日期；purpose=用途。六项实测按表头单位读取。箭头表示将修改的值。"),&dialog));
    auto *values=table({"sample_id",QStringLiteral("产品编号"),QStringLiteral("方案"),QStringLiteral("试验日期"),QStringLiteral("来源"),QStringLiteral("用途"),"空载损耗(W)","负载损耗(W)","阻抗(%)","油顶温升(K)","高压温升(K)","低压温升(K)"},&dialog); values->setObjectName("measurementImportPreview");
    for(int i=0;i<preview.replacements.size()&&i<preview.originals.size();++i) {const auto &s=preview.replacements[i]; const auto &old=preview.originals[i]; QStringList fields{s.id,change(old.productId,s.productId),change(old.schemeName,s.schemeName),change(old.testedOn.toString(Qt::ISODate),s.testedOn.toString(Qt::ISODate)),change(old.source,s.source),change(SelfLearning::purposeName(old.purpose),SelfLearning::purposeName(s.purpose))}; for(int k=0;k<SelfLearning::MetricCount;++k) fields<<change(value(old.measured[k]),value(s.measured[k])); row(values,fields);}
    layout->addWidget(values,1); auto *diagnostics=new QPlainTextEdit(&dialog); diagnostics->setReadOnly(true); diagnostics->setMaximumHeight(130); QStringList errors=preview.errors; if(!parsed&&!error.isEmpty()) errors.prepend(error);
    diagnostics->setPlainText(errors.isEmpty()?QStringLiteral("%1行有效，未写入。确认后整批更新；任一行失效或原库已变化则全部拒绝，不覆盖其他样本。").arg(preview.replacements.size()):errors.join("\n")); layout->addWidget(diagnostics);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog); buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("确认整批保存")); buttons->button(QDialogButtonBox::Save)->setEnabled(parsed&&errors.isEmpty()&&!preview.replacements.isEmpty()); layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept); connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted || (m_guard&&!m_guard())) return;
    if(!LearningPreparation::commitCsv(m_path,preview,error)) {feedback(error,true);return;}
    emit datasetChanged(); QVector<SelfLearning::Sample> fresh;
    if(SelfLearning::load(m_path,fresh,error)) {setDataset(fresh,true);feedback(QStringLiteral("%1行已整批保存；无自动删除、补0或单位换算。").arg(preview.replacements.size()));} else {setDataset({},false);feedback(error,true);}
}
