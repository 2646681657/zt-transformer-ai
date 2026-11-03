#include "CandidateComparisonDialog.h"
#include "ElectromagneticEngine.h"
#include "SchemeStore.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QDebug>
#include "SelfLearningDataset.h"
#include <QCheckBox>
#include <QLineEdit>
#include <QElapsedTimer>
#include <QThread>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QFontDatabase>
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    if(!qEnvironmentVariable("ZTBLD_COMPARISON_SCREENSHOT").isEmpty()) {
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
        app.setFont(QFont(QStringLiteral("Microsoft YaHei"),10));
    }
    QTemporaryDir dir; int failures=0;
    auto check=[&](bool ok,const char *why){if(!ok){++failures;qCritical()<<why;}};
    OptimizeCandidate a; ElectromagneticEngine engine; check(engine.calcElectromagnetic(a.input,a.result)&&a.result.valid,"Original engine must provide a real candidate");
    a.scheme=makeScheme(7,a.input,a.result); auto b=a; b.scheme=makeScheme(3,b.input,b.result);
    const auto inputBefore=SchemeStore::toJson(a.input); const double costBefore=a.result.cost.materialCost;
    CandidateComparisonDialog empty({},dir.path());
    auto *startEmpty=empty.findChild<QPushButton *>("candidateComparisonStart");
    check(startEmpty&&!startEmpty->isEnabled(),"No candidates must block comparison launch");
    CandidateComparisonDialog dialog({a,b},dir.path());
    auto *values=dialog.findChild<QTableWidget *>("candidateComparisonValues");
    check(values&&values->rowCount()==12,"Both candidates and all six original metrics must remain visible without a model");
    check(dialog.findChild<QComboBox *>("candidateComparisonModel")!=nullptr,"Model version must be explicitly visible");
    auto *start=dialog.findChild<QPushButton *>("candidateComparisonStart");
    check(start&&!start->isEnabled(),"Without a valid model/runtime the comparison must not start");
    if(values&&values->rowCount()==12) check(values->item(0,1)->text()=="7"&&values->item(6,1)->text()=="3","Original list order must not become a DNN ranking");
    dialog.reject();
    check(SchemeStore::toJson(a.input)==inputBefore&&a.result.cost.materialCost==costBefore&&a.scheme.schemeIdx==7,"Opening and closing cannot mutate original formal candidate");
    if(app.arguments().contains("--integration")) {
        const auto python=qEnvironmentVariable("ZTBLD_TRAINING_PYTHON"),script=qEnvironmentVariable("ZTBLD_TRAINER_SCRIPT");
        if(python.isEmpty()||script.isEmpty()) return 77;
        QVector<SelfLearning::Sample> samples;QString error;
        for(int i=0;i<10;++i){SelfLearning::Sample sample;check(SelfLearning::makeSample(a.input,sample,error),"Generate actual engine fixture");sample.productId=QString("SYNTHETIC-%1").arg(i);sample.source="SYNTHETIC TEST ONLY";sample.purpose="training";sample.measured[0]=sample.baseline[0]+6;samples.append(sample);}
        QJsonObject bundle;check(SelfLearning::trainingBundle(samples,bundle,error),"Freeze synthetic training bundle");
        check(SelfLearning::writeJson(dir.filePath("bundle.json"),bundle,error),"Persist isolated training fixture");
        const auto job=dir.filePath("jobs/test-model");QProcess train;train.setProgram(python);train.setArguments({"-I","-u",script,"train","--bundle",dir.filePath("bundle.json"),"--output",job,"--epochs","2","--cancel-file",dir.filePath("unused"),"--test-only"});train.start();
        check(train.waitForFinished(60000)&&train.exitCode()==0,"Train actual temporary CPU model");if(train.exitCode()!=0)qCritical()<<train.readAllStandardOutput()<<train.readAllStandardError();
        check(SelfLearning::writeJson(job+"/job.json",{{"status","completed"}},error),"Publish isolated test-only training job");
        CandidateComparisonDialog live({a,b},dir.path());live.setRuntime(python,script);
        auto *run=live.findChild<QPushButton *>("candidateComparisonStart");auto *consent=live.findChild<QCheckBox *>("candidateExperimentalConsent");
        check(run&&!run->isEnabled(),"Experimental model requires explicit consent");consent->setChecked(true);check(run->isEnabled(),"Explicit temporary model comparison can start");
        auto wait=[&]{QElapsedTimer timer;timer.start();while(live.isBusy()&&timer.elapsed()<30000){app.processEvents();QThread::msleep(10);}return !live.isBusy();};
        run->click();check(wait(),"CPU batch prediction finishes");auto *record=live.findChild<QLineEdit *>("candidateComparisonRecord");
        check(record&&!record->text().isEmpty()&&QFile::exists(record->text()),"Real GUI backend roundtrip publishes verified comparison");
        auto *table=live.findChild<QTableWidget *>("candidateComparisonValues");
        check(table&&table->item(0,5)->text()!=QStringLiteral("—")&&table->item(0,7)->text()==QStringLiteral("仅测试预测"),"Trained target is shown with permanent test badge");
        check(table&&table->item(1,5)->text()==QStringLiteral("—")&&!table->item(1,7)->text().isEmpty(),"Untrained targets retain explicit failure instead of zero");
        if(record->text().isEmpty())qCritical()<<live.findChild<QLabel *>("candidateComparisonStatus")->text();
        const auto shot=qEnvironmentVariable("ZTBLD_COMPARISON_SCREENSHOT");if(!shot.isEmpty()){live.show();app.processEvents();check(live.grab().save(shot),"Save comparison UI visual check");}
        run->click();live.findChild<QPushButton *>("candidateComparisonCancel")->click();check(wait()&&record->text().isEmpty()&&table->item(0,5)->text()==QStringLiteral("—"),"Cancellation clears preceding predictions and publishes no success path");
        run->click();check(wait()&&!record->text().isEmpty(),"Comparison can rerun after cancellation");
        QFile model(job+"/model.json");check(model.open(QIODevice::Append),"Open isolated model tamper fixture");model.write(" ");model.close();
        QElapsedTimer timer;timer.start();while(timer.elapsed()<1400){app.processEvents();QThread::msleep(10);}
        check(record->text().isEmpty()&&table->item(0,5)->text()==QStringLiteral("—"),"Changed model invalidates displayed correction");
        check(SchemeStore::toJson(a.input)==inputBefore&&a.result.cost.materialCost==costBefore&&a.scheme.schemeIdx==7,"Real CPU comparison cannot mutate formal candidates");
    }
    qInfo()<<"candidate comparison UI failures:"<<failures; return failures?1:0;
}
