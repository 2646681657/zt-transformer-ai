#include "CandidateComparisonDialog.h"
#include "LearningCandidateComparison.h"
#include "LearningModelReview.h"
#include "SelfLearningDataset.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QTableWidget>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <cmath>

namespace {
QJsonObject read(const QString &path) {
    if(path.isEmpty()) return {}; QFile file(path);
    if(!file.open(QIODevice::ReadOnly)||file.size()>16*1024*1024) return {};
    QJsonParseError error; const auto doc=QJsonDocument::fromJson(file.readAll(),&error);
    return error.error==QJsonParseError::NoError&&doc.isObject()?doc.object():QJsonObject{};
}
QString sha(const QString &path) {
    if(path.isEmpty()) return {}; QFile file(path);
    if(!file.open(QIODevice::ReadOnly)||file.size()>16*1024*1024) return {};
    return QString::fromLatin1(QCryptographicHash::hash(file.readAll(),QCryptographicHash::Sha256).toHex());
}
QString identity(const QString &path) {
    if(path.isEmpty()) return {}; const QFileInfo info(path);
    return QDir::cleanPath(info.canonicalFilePath().isEmpty()?info.absoluteFilePath():info.canonicalFilePath()).toCaseFolded();
}
QLabel *note(const QString &text,QWidget *parent) {auto *label=new QLabel(text,parent);label->setWordWrap(true);label->setTextFormat(Qt::PlainText);return label;}
QString number(QJsonValue value) {return value.isDouble()?QString::number(value.toDouble(),'g',12):QStringLiteral("—");}
QString reason(QString text) {
    if(text.contains("Missing model target")) return QStringLiteral("目标未训练：")+text;
    if(text.contains("outside training bounds")) return QStringLiteral("超出模型训练范围：")+text;
    if(text.contains("Fixed physical context")) return QStringLiteral("固定物理参数不匹配：")+text;
    if(text.contains("Unsupported structure")) return QStringLiteral("结构不在模型适用范围：")+text;
    return text;
}
}

CandidateComparisonDialog::CandidateComparisonDialog(const QVector<OptimizeCandidate> &candidates,const QString &directory,QWidget *parent)
    :QDialog(parent),m_directory(directory)
{
    setObjectName("candidateComparisonDialog"); setWindowTitle(QStringLiteral("DNN旁路对比 · 不干预正式寻优")); resize(1220,780); setModal(true);
    m_input=LearningComparison::makeInput(candidates,m_snapshotError);
    auto *layout=new QVBoxLayout(this);
    layout->addWidget(note(QStringLiteral("本次冻结当前可见方案%1个，按原列表顺序对比；隐藏/未保留方案不纳入，不代表全部搜索空间。此窗口不改正式排名、筛选、合格判定、报价或打印。DNN残差修正仍依赖原公式，不是寻优加速或精度证明。").arg(candidates.size()),this));
    auto *choose=new QHBoxLayout; m_models=new QComboBox(this);m_models->setObjectName("candidateComparisonModel"); choose->addWidget(m_models,1);
    m_reload=new QPushButton(QStringLiteral("刷新模型和运行环境"),this);m_reload->setAutoDefault(false);choose->addWidget(m_reload);layout->addLayout(choose);
    m_experimental=new QCheckBox(QStringLiteral("允许仅作实验/测试对比（未审核结果不能当作工程结论）"),this);m_experimental->setObjectName("candidateExperimentalConsent");layout->addWidget(m_experimental);
    m_badge=note(QString(),this);m_badge->setObjectName("candidateComparisonModelStatus");layout->addWidget(m_badge);
    auto *actions=new QHBoxLayout; m_start=new QPushButton(QStringLiteral("开始批量旁路对比"),this);m_start->setObjectName("candidateComparisonStart");m_start->setAutoDefault(false);
    m_cancel=new QPushButton(QStringLiteral("取消本次对比"),this);m_cancel->setObjectName("candidateComparisonCancel");m_cancel->setAutoDefault(false);
    m_progress=new QProgressBar(this);m_progress->setRange(0,100);actions->addWidget(m_start);actions->addWidget(m_cancel);actions->addWidget(m_progress,1);layout->addLayout(actions);
    m_values=new QTableWidget(0,8,this);m_values->setObjectName("candidateComparisonValues");
    m_values->setHorizontalHeaderLabels({QStringLiteral("原列表序"),QStringLiteral("方案ID"),QStringLiteral("原材料成本(元)"),QStringLiteral("指标/单位"),QStringLiteral("原公式值"),QStringLiteral("DNN独立修正值"),QStringLiteral("修正值－原值"),QStringLiteral("推理适用性/原因（非合格判定）")});
    m_values->setSortingEnabled(false);m_values->setEditTriggers(QAbstractItemView::NoEditTriggers);m_values->setAlternatingRowColors(true);m_values->verticalHeader()->hide();
    m_values->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);m_values->horizontalHeader()->setStretchLastSection(true);layout->addWidget(m_values,1);
    m_status=note(QString(),this);m_status->setObjectName("candidateComparisonStatus");layout->addWidget(m_status);
    m_record=new QLineEdit(this);m_record->setObjectName("candidateComparisonRecord");m_record->setReadOnly(true);m_record->setPlaceholderText(QStringLiteral("完成并通过完整性核验后显示本地记录路径；失败/取消不发布成功记录。"));layout->addWidget(m_record);
    auto *close=new QDialogButtonBox(QDialogButtonBox::Close,this);layout->addWidget(close);connect(close,&QDialogButtonBox::rejected,this,&CandidateComparisonDialog::reject);
    connect(m_start,&QPushButton::clicked,this,&CandidateComparisonDialog::start);connect(m_cancel,&QPushButton::clicked,this,&CandidateComparisonDialog::cancel);
    connect(m_models,&QComboBox::currentIndexChanged,this,&CandidateComparisonDialog::selectModel);connect(m_experimental,&QCheckBox::toggled,this,&CandidateComparisonDialog::refresh);
    connect(m_reload,&QPushButton::clicked,this,&CandidateComparisonDialog::reloadModels);
    connect(&m_process,&QProcess::readyReadStandardOutput,this,&CandidateComparisonDialog::consume);
    connect(&m_process,&QProcess::readyReadStandardError,this,[this]{m_stderr=(m_stderr+m_process.readAllStandardError()).right(32768);});
    connect(&m_process,&QProcess::finished,this,&CandidateComparisonDialog::finish);
    connect(&m_process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart){m_stderr+=m_process.errorString().toUtf8();finish(-1,QProcess::CrashExit);}});
    auto *watch=new QTimer(this);watch->setInterval(1000);connect(watch,&QTimer::timeout,this,[this]{
        if(m_startedModel.isEmpty()||m_invalidated) return;
        QString error;const bool approved=!m_startedApproved||(identity(LearningReview::activeModel(m_directory+"/review",error))==identity(m_startedModel)&&sha(m_directory+"/review/registry/registry.json")==m_registryHash);
        if(sha(m_startedModel)!=m_modelHash||sha(QFileInfo(m_startedModel).dir().filePath("report.json"))!=m_trainingReportHash||!approved) invalidate(QStringLiteral("模型、报告或审核状态已变化，修正展示已清空；请刷新并重新比较。"));
    });watch->start();reloadModels();render();
    if(!m_snapshotError.isEmpty()) m_status->setText(m_snapshotError);refresh();
}
CandidateComparisonDialog::~CandidateComparisonDialog() {
    if(m_process.state()!=QProcess::NotRunning){m_process.disconnect(this);m_process.kill();m_process.waitForFinished(1500);}
    if(m_running&&!m_job.isEmpty()){QString error;SelfLearning::writeJson(m_job+"/job.json",{{"status","cancelled"},{"usage","independent-comparison-only"},{"diagnostics","Dialog destroyed before completion"}},error);}
}
bool CandidateComparisonDialog::isBusy() const {return m_running||m_process.state()!=QProcess::NotRunning;}
void CandidateComparisonDialog::setRuntime(const QString &python,const QString &script) {if(isBusy())return;m_python=python;m_script=script;refresh();}
void CandidateComparisonDialog::reloadModels() {
    if(isBusy())return;m_report={};m_record->clear();m_startedModel.clear();m_models->clear();
    auto runtime=read(m_directory+"/runtime.json");if(runtime.isEmpty())runtime=read(QCoreApplication::applicationDirPath()+"/learning/runtime.json");
    m_python=runtime["python"].toString();m_script=runtime["script"].toString();
    QString error;const auto active=LearningReview::activeModel(m_directory+"/review",error);QStringList seen;
    if(!active.isEmpty()){m_models->addItem(QStringLiteral("[已审核默认·仅独立展示] ")+read(active)["modelVersion"].toString(),active);m_models->setItemData(0,true,Qt::UserRole+1);seen<<identity(active);}
    const QDir jobs(m_directory+"/jobs");for(const auto &job:jobs.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Time)) {
        const auto path=job.filePath()+"/model.json";const auto model=read(path),report=read(job.filePath()+"/report.json");
        if(seen.contains(identity(path))||read(job.filePath()+"/job.json")["status"]!=QJsonValue("completed")||model["format"]!=QJsonValue("ZTBLD-ResidualModel")||model["status"]!=QJsonValue("experimental")||model["productionEligible"]!=QJsonValue(false)||!model["testOnly"].isBool()||model["modelVersion"].toString().isEmpty()||report["status"]!=QJsonValue("completed")||report["modelVersion"]!=model["modelVersion"]||report["testOnly"]!=model["testOnly"])continue;
        m_models->addItem((model["testOnly"].toBool()?QStringLiteral("[测试模型·禁止工程结论] "):QStringLiteral("[未审核实验模型] "))+model["modelVersion"].toString(),path);m_models->setItemData(m_models->count()-1,false,Qt::UserRole+1);seen<<identity(path);
    }
    selectModel();
}
void CandidateComparisonDialog::selectModel() {
    if(isBusy())return;m_report={};m_startedModel.clear();m_record->clear();m_invalidated=false;render();
    m_status->setText(m_models->count()?QStringLiteral("独立对比不会改变正式方案；请核对模型版本和实验标识后开始。"):
        QStringLiteral("尚无可用模型。请先到自学习管理中配置环境、训练模型；已有原公式值仍完整保留。"));refresh();
}
void CandidateComparisonDialog::refresh() {
    const bool busy=isBusy(),hasModel=!m_models->currentData().toString().isEmpty(),approved=m_models->currentData(Qt::UserRole+1).toBool();
    m_start->setEnabled(!busy&&!m_input.isEmpty()&&hasModel&&QFileInfo(m_python).isFile()&&QFileInfo(m_script).isFile()&&(approved||m_experimental->isChecked()));
    m_cancel->setEnabled(busy);m_models->setEnabled(!busy);m_reload->setEnabled(!busy);m_experimental->setEnabled(!busy);
    m_badge->setText(!hasModel?QStringLiteral("无可用模型；不会补0或伪造修正结果。"):
        approved?QStringLiteral("已审核默认模型也仅用于独立修正展示，不替代正式计算或约束判定。"):
        QStringLiteral("未审核实验/测试模型。必须明确勾选实验对比；输出永久保留实验/测试标识，不代表精度通过。"));
    if(hasModel&&(!QFileInfo(m_python).isFile()||!QFileInfo(m_script).isFile()))m_badge->setText(m_badge->text()+QStringLiteral(" 本地运行环境未配置，请先在自学习管理配置。"));
}
void CandidateComparisonDialog::render() {
    m_values->setRowCount(0);const auto originals=m_input["candidates"].toArray(),results=m_report["candidates"].toArray();
    for(int index=0;index<originals.size();++index){const auto original=originals[index].toObject();const auto baseline=original["sample"].toObject()["baseline"].toObject();const auto result=index<results.size()?results[index].toObject():QJsonObject{};
        const auto predictions=result["predictions"].toObject();
        for(const auto &metric:SelfLearning::metrics()){const QJsonValue prediction=predictions.value(metric.key);const bool available=!m_report.isEmpty()&&prediction.isDouble();const auto failure=result["failures"].toObject()[metric.key].toString();
            const QStringList fields{QString::number(original["originalOrder"].toInt()),QString::number(original["schemeId"].toInt()),number(original["materialCost"]),QString::fromUtf8(metric.name)+" ("+metric.unit+")",number(baseline[metric.key]),available?number(prediction):QStringLiteral("—"),available?QString::number(prediction.toDouble()-baseline[metric.key].toDouble(),'g',10):QStringLiteral("—"),available?(m_report["testOnly"].toBool()?QStringLiteral("仅测试预测"):m_startedApproved?QStringLiteral("已审核·独立展示"):QStringLiteral("未审核实验预测")):failure.isEmpty()?QStringLiteral("尚未独立比较"):reason(failure)};
            const int row=m_values->rowCount();m_values->insertRow(row);for(int col=0;col<fields.size();++col){auto *item=new QTableWidgetItem(fields[col]);item->setToolTip(fields[col]);m_values->setItem(row,col,item);}
        }
    }
}
void CandidateComparisonDialog::start() {
    if(!m_start->isEnabled())return;m_report={};m_record->clear();render();m_invalidated=false;m_cancelled=false;m_closeAfterCancel=false;
    m_startedModel=m_models->currentData().toString();m_startedApproved=m_models->currentData(Qt::UserRole+1).toBool();
    QString error;if(m_startedApproved&&identity(LearningReview::activeModel(m_directory+"/review",error))!=identity(m_startedModel)){m_status->setText(QStringLiteral("默认模型审核已失效，请刷新模型：")+error);m_startedModel.clear();return;}
    m_modelHash=sha(m_startedModel);m_trainingReportHash=sha(QFileInfo(m_startedModel).dir().filePath("report.json"));m_registryHash=sha(m_directory+"/review/registry/registry.json");m_version=read(m_startedModel)["modelVersion"].toString();
    if(m_modelHash.isEmpty()||m_trainingReportHash.isEmpty()){m_status->setText(QStringLiteral("模型或训练报告无法读取，未启动。"));m_startedModel.clear();return;}
    m_job=m_directory+"/comparisons/"+QUuid::createUuid().toString(QUuid::WithoutBraces);m_outputDirectory=m_job+"/"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    if(!SelfLearning::writeJson(m_job+"/input.json",m_input,error)||!SelfLearning::writeJson(m_job+"/job.json",{{"status","running"},{"usage","independent-comparison-only"},{"modelPath",m_startedModel},{"modelSha256",m_modelHash},{"reportSha256",m_trainingReportHash},{"approvedDefault",m_startedApproved},{"registrySha256",m_registryHash}},error)){m_status->setText(error);return;}
    ++m_generation;m_stdout.clear();m_stderr.clear();m_progress->setValue(0);m_running=true;
    auto env=QProcessEnvironment::systemEnvironment();env.insert("PYTHONIOENCODING","utf-8");env.insert("PYTHONUTF8","1");m_process.setProcessEnvironment(env);
    m_process.setProgram(m_python);m_process.setArguments({"-I","-u",m_script,"predict-batch","--model",m_startedModel,"--input",m_job+"/input.json","--output",m_outputDirectory,"--cancel-file",m_job+"/cancel"});m_process.start();m_status->setText(QStringLiteral("正在后台逐目标推理；原公式、候选顺序和成本保持不变。"));refresh();
}
void CandidateComparisonDialog::consume() {
    m_stdout+=m_process.readAllStandardOutput();if(m_stdout.size()>1024*1024){m_stderr+="Backend protocol too long";m_stdout.clear();m_process.kill();return;}
    qsizetype pos;while((pos=m_stdout.indexOf('\n'))>=0){const auto object=QJsonDocument::fromJson(m_stdout.left(pos)).object();m_stdout.remove(0,pos+1);if(object["event"]==QJsonValue("error"))m_stderr+=object["message"].toString().toUtf8();if(object["event"]==QJsonValue("progress")){m_progress->setValue(qBound(0,object["percent"].toInt(),99));m_status->setText(object["message"].toString());}}
}
void CandidateComparisonDialog::finish(int code,QProcess::ExitStatus status) {
    if(!m_running)return;consume();m_stderr=(m_stderr+m_process.readAllStandardError()).right(32768);m_running=false;
    const auto report=read(m_outputDirectory+"/comparison.json");QString error;
    bool ok=!m_cancelled&&!m_invalidated&&code==0&&status==QProcess::NormalExit;
    if(ok)ok=LearningComparison::validateReport(m_input,report,m_startedModel,m_job+"/input.json",error);
    if(ok)ok=sha(m_startedModel)==m_modelHash&&sha(QFileInfo(m_startedModel).dir().filePath("report.json"))==m_trainingReportHash;
    if(ok&&m_startedApproved)ok=identity(LearningReview::activeModel(m_directory+"/review",error))==identity(m_startedModel)&&sha(m_directory+"/review/registry/registry.json")==m_registryHash;
    QString saveError;const bool saved=SelfLearning::writeJson(m_job+"/job.json",{{"status",ok?"completed":m_cancelled?"cancelled":"failed"},{"usage","independent-comparison-only"},{"modelPath",m_startedModel},{"modelVersion",m_version},{"modelSha256",m_modelHash},{"reportSha256",m_trainingReportHash},{"approvedDefault",m_startedApproved},{"registrySha256",m_registryHash},{"comparisonPath",m_outputDirectory+"/comparison.json"},{"diagnostics",error+QString::fromUtf8(m_stderr)}},saveError);
    if(ok&&saved){m_report=report;m_progress->setValue(100);m_record->setText(m_outputDirectory+"/comparison.json");m_status->setText(QStringLiteral("旁路对比完成并已保存：%1个候选。空值旁有失败原因；不是精度验证，不影响原方案。模型版本：%2").arg(m_input["candidates"].toArray().size()).arg(m_version));}
    else{m_report={};m_record->clear();m_status->setText(!saved?saveError:m_invalidated?QStringLiteral("模型/审核变化，本次结果作废，未发布成功记录。"):m_cancelled?QStringLiteral("对比已取消，未发布成功记录；原公式值保留。"):QStringLiteral("对比失败或完整性核验不通过：%1 %2").arg(error,QString::fromUtf8(m_stderr)));}
    render();refresh();if(m_closeAfterCancel)QDialog::reject();
}
void CandidateComparisonDialog::cancel() {
    if(!isBusy()||m_cancelled)return;m_cancelled=true;QSaveFile marker(m_job+"/cancel");if(marker.open(QIODevice::WriteOnly)){marker.write("cancel");marker.commit();}
    const auto generation=m_generation;QTimer::singleShot(5000,this,[this,generation]{if(isBusy()&&m_generation==generation&&m_cancelled)m_process.kill();});m_status->setText(QStringLiteral("正在取消本次独立推理，不影响正式寻优。"));
}
void CandidateComparisonDialog::invalidate(const QString &reason) {m_invalidated=true;m_report={};m_record->clear();render();m_status->setText(reason);if(isBusy())cancel();}
void CandidateComparisonDialog::reject() {if(isBusy()){m_closeAfterCancel=true;cancel();return;}QDialog::reject();}
