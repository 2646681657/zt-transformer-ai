#include "LearningPreparationPanel.h"
#include "LearningWorkspace.h"
#include "LearningDataPreparation.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QComboBox>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QDebug>
#include <QDateEdit>
#include <QLineEdit>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QTimer>
#include <QFile>
#include <QFontDatabase>
#include <QTabWidget>
int main(int argc, char **argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); QTemporaryDir dir; QString error; int failures = 0;
    auto check = [&](bool ok, const char *why) { if (!ok) { ++failures; qCritical() << why; } };
    const auto path = dir.filePath("samples.json");
    LearningPreparationPanel panel(path);
    panel.setDataset({}, true);
    check(panel.findChild<QPushButton *>("assignProductPurpose") != nullptr, "Product grouping must have an explicit persistent action");
    auto *assign = panel.findChild<QPushButton *>("assignProductPurpose");
    check(assign && !assign->isEnabled(), "No product means no usage assignment");
    auto *overview = panel.findChild<QTableWidget *>("readinessOverview");
    check(overview && overview->rowCount() == SelfLearning::MetricCount, "All six target readiness counts must be visible");
    check(panel.findChild<QPushButton *>("exportMeasurementCsv") && !panel.findChild<QPushButton *>("exportMeasurementCsv")->isEnabled(), "No saved samples means no fill template");
    SelfLearning::Sample a; check(SelfLearning::makeSample(CalcInput{}, a, error), "Real engine fixture must calculate");
    a.productId="000123"; a.schemeName="fixture"; a.source="temporary UI fixture"; a.measured[0]=a.baseline[0]+1;
    auto b=a; b.id+="-later"; b.testedOn=b.testedOn.addDays(1);
    check(SelfLearning::save(path,{a,b},error),"Unassigned group fixture saves without losing labels");
    panel.setDataset({a,b},true);
    auto *products=panel.findChild<QComboBox *>("usageProducts"); auto *purpose=panel.findChild<QComboBox *>("productPurpose");
    check(products && products->count()==1,"Repeated tests of one product must be one usage group");
    if (products && purpose && assign) {
        purpose->setCurrentIndex(purpose->findData("independent_test")); assign->click();
        QVector<SelfLearning::Sample> saved;
        check(SelfLearning::load(path,saved,error) && saved.size()==2 && saved[0].purpose=="independent_test" && saved[1].purpose=="independent_test", "One group action must reserve every date of a product");
        panel.setDataset(saved,true);
        overview=panel.findChild<QTableWidget *>("readinessOverview");
        check(overview && overview->item(0,2)->text()=="0" && overview->item(0,4)->text()=="1", "Reserved products count once as tests and never as training products");
        panel.setBeforeMutation([]{return false;});
        purpose->setCurrentIndex(purpose->findData("training")); assign->click();
        check(SelfLearning::load(path,saved,error) && saved[0].purpose=="independent_test", "Unsaved sample/task guard must prevent hidden purpose changes");
    }
    panel.setDataset({},false);
    check(assign && !assign->isEnabled(),"An unhealthy library cannot mutate usage");
    {
        LearningWorkspace editor(path); editor.setScheme(CalcInput{},"Repeated product fixture");
        editor.findChild<QPushButton *>("newSample")->click();
        editor.findChild<QLineEdit *>("productId")->setText("000123");
        editor.findChild<QDateEdit *>()->setDate(QDate::currentDate().addDays(3));
        editor.findChild<QPushButton *>("saveSample")->click();
        QVector<SelfLearning::Sample> saved;
        check(SelfLearning::load(path,saved,error) && saved.size()==3 && saved.last().purpose=="independent_test",
              "A new test date must inherit the existing product reservation rather than fail or become training");
    }
    {
        const auto csvStore=dir.filePath("csv-store.json"), csvPath=dir.filePath("filled.csv");
        auto sample=a; sample.measured[0]=530; sample.productId="CSV-FIXTURE";
        check(SelfLearning::save(csvStore,{sample},error),"CSV dialog fixture persists");
        auto bytes=LearningPreparation::csvTemplate({sample}); bytes.replace(",530,",",552,");
        QFile csv(csvPath); check(csv.open(QIODevice::WriteOnly)&&csv.write(bytes)==bytes.size(),"Filled temporary CSV writes"); csv.close();
        LearningPreparationPanel importer(csvStore); importer.setDataset({sample},true);
        const auto runImport=[&](bool expectedValid) {
            bool picked=false,previewed=false;
            QTimer::singleShot(0,[&] {
                auto *picker=qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
                if(!picker) return; picked=true; picker->selectFile(csvPath); QMetaObject::invokeMethod(picker,"accept",Qt::DirectConnection);
                QTimer::singleShot(0,[&] {
                    auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget()); if(!dialog) return;
                    auto *values=dialog->findChild<QTableWidget *>("measurementImportPreview"); auto *buttons=dialog->findChild<QDialogButtonBox *>();
                    if(!values||!buttons) {dialog->reject();return;}
                    previewed=true; auto *save=buttons->button(QDialogButtonBox::Save);
                    check(save->isEnabled()==expectedValid,"Import preview Save must reflect whole-batch validity");
                    if(expectedValid) {check(values->rowCount()==1 && values->item(0,6)->text().contains("552"),"Preview must expose the exact measurement change before commit"); save->click();}
                    else buttons->button(QDialogButtonBox::Cancel)->click();
                });
            });
            QTimer watchdog; watchdog.setSingleShot(true); watchdog.setInterval(3000);
            QObject::connect(&watchdog,&QTimer::timeout,&importer,[] {if(auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget())) dialog->reject();}); watchdog.start();
            importer.findChild<QPushButton *>("importMeasurementCsv")->click(); watchdog.stop();
            check(picked&&previewed,"Real file-picker to preview flow must complete");
        };
        runImport(true); QVector<SelfLearning::Sample> saved;
        check(SelfLearning::load(csvStore,saved,error)&&saved.size()==1&&saved[0].measured[0]==552,"Confirmed CSV preview must persist only the intended real sample");
        runImport(false);
        check(SelfLearning::load(csvStore,saved,error)&&saved[0].measured[0]==552,"Reusing stale exported CSV must disable saving and preserve current values");
    }
    const auto capture=qEnvironmentVariable("ZTBLD_PREPARATION_IMAGE");
    if(!capture.isEmpty()) {
        const int font=QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc"); if(font>=0) app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).first(),10));
        QFile style(":/styles/ztf_theme.qss"); if(style.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(style.readAll()));
        QTabWidget tabs; auto *workspace=new LearningWorkspace(path,&tabs); tabs.addTab(workspace,QStringLiteral("学习样本")); tabs.addTab(workspace->preparationPage(),QStringLiteral("数据准备")); tabs.addTab(workspace->trainingPage(),QStringLiteral("训练与模型")); tabs.setCurrentIndex(1); tabs.resize(1150,850); tabs.show(); app.processEvents(); check(tabs.grab().save(capture),"Data preparation screenshot must save");
    }
    qInfo()<<"preparation UI failures:"<<failures; return failures?1:0;
}
