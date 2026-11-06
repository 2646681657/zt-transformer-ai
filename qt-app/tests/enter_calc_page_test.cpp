#include <QDir>
#include "EnterCalcPage.h"
#include "OptimizeCalcPage.h"
#include <QComboBox>
#include <QPushButton>
#include "EmResultPanel.h"
#include "GridOptimizer.h"
#include "PrintTableWidget.h"
#include "SchemeTableWidget.h"
#include "CandidateComparisonDialog.h"
#include "SchemeCalculationSheet.h"
#include "ParamTableWidget.h"
#include "QuotePage.h"
#include "ElectromagneticEngine.h"
#include <QLabel>
#include <QDialog>
#include <QToolButton>
#include <QApplication>
#include <QMetaObject>
#include <QTableWidget>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QStandardPaths>
#include <QMessageBox>
#include <QTimer>
#include <QSettings>
#include <QTemporaryDir>
#include <QFileDialog>
#include <QPrinter>
#include <QFileInfo>
#include <QLineEdit>
#include "SchemeStore.h"
#include "StartupSchemeLibrary.h"
#include <QListWidget>
#include <QCheckBox>
#include <QDialogButtonBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    app.setApplicationName(QStringLiteral("ZTBLD-EnterCalc-Regression"));
    QTemporaryDir settingsDir;
    if (!settingsDir.isValid()) return 10;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDir.path());
    if(app.arguments().contains(QStringLiteral("--startup-schemes"))) {
        app.setApplicationName(QStringLiteral("ZTBLD-Startup-")+QFileInfo(settingsDir.path()).fileName());
        OptimizeCalcPage page;
        const QString captureDir=qEnvironmentVariable("ZTBLD_TEST_OUTPUT_DIR");
        if(!captureDir.isEmpty()) {
            QDir().mkpath(captureDir);page.resize(1600,1000);page.show();app.processEvents();
            page.grab().save(QDir(captureDir).filePath("startup-design-input.png"));
        }
        auto *choose=page.findChild<QToolButton *>("chooseStartupSchemeButton");
        auto *save=page.findChild<QToolButton *>("saveStartupSchemeButton");
        if(!choose||!save||choose->text()!=QStringLiteral("默认方案选择"))return 150;
        bool selected=false;
        QTimer::singleShot(0,&page,[&] {
            auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());
            auto *list=dialog?dialog->findChild<QListWidget *>("startupSchemeList"):nullptr;
            auto *activate=dialog?dialog->findChild<QPushButton *>("activateSelectedStartupScheme"):nullptr;
            if(!list||!activate||list->count()!=3){if(dialog)dialog->reject();return;}
            if(!captureDir.isEmpty()) {
                app.processEvents();dialog->grab().save(QDir(captureDir).filePath("startup-library.png"));
            }
            list->setCurrentRow(1);selected=true;activate->click();
        });
        choose->click();
        if(!selected||page.currentInput().capacity_kVA!=400||page.currentInput().lvFoilThick_mm!=1.0)return 151;
        auto *table=page.findChild<ParamTableWidget *>();
        CalcInput collected=page.currentInput();table->saveToInput(collected);
        auto expectedInput=page.currentInput();
        // Legacy manual criteria acquire a normalized lookup identity on display;
        // all numerical values, source modes and process inputs must stay identical.
        expectedInput.performanceCriteria.lossStandardsKey=collected.performanceCriteria.lossStandardsKey;
        if(!SchemeStore::sameInput(collected,expectedInput)) {
            const auto actual=SchemeStore::toJson(collected),expected=SchemeStore::toJson(page.currentInput());
            for(auto it=expected.begin();it!=expected.end();++it)if(it.value()!=actual[it.key()])
                qCritical()<<it.key()<<"saved"<<it.value()<<"loaded"<<actual[it.key()];
            qCritical()<<"Loading a startup scheme changed its parameter snapshot";return 152;
        }
        bool saved=false;
        QTimer::singleShot(0,&page,[&] {
            auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());
            auto *name=dialog?dialog->findChild<QLineEdit *>("startupSchemeName"):nullptr;
            auto *active=dialog?dialog->findChild<QCheckBox *>("activateStartupScheme"):nullptr;
            auto *buttons=dialog?dialog->findChild<QDialogButtonBox *>():nullptr;
            if(!name||!active||!buttons){if(dialog)dialog->reject();return;}
            name->setText("startup test snapshot");active->setChecked(true);saved=true;
            buttons->button(QDialogButtonBox::Save)->click();
        });
        save->click();if(!saved)return 153;
        OptimizeCalcPage restarted;
        if(!SchemeStore::sameInput(restarted.currentInput(),collected))return 154;
        bool third=false;
        QTimer::singleShot(0,&restarted,[&] {
            auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());
            auto *list=dialog?dialog->findChild<QListWidget *>("startupSchemeList"):nullptr;
            auto *activate=dialog?dialog->findChild<QPushButton *>("activateSelectedStartupScheme"):nullptr;
            if(!list||!activate||list->count()!=4){if(dialog)dialog->reject();return;}
            if(!list->item(3)->text().contains(QStringLiteral("启动默认"))){dialog->reject();return;}
            list->setCurrentRow(2);third=true;activate->click();
        });
        restarted.findChild<QToolButton *>("chooseStartupSchemeButton")->click();
        OptimizeCalcPage thirdStartup;
        if(!third||thirdStartup.currentInput().capacity_kVA!=630||thirdStartup.currentInput().coreStraight_mm!=136)return 155;
        QFile::remove(StartupSchemeLibrary::path());
        qInfo()<<"PASS: three presets, select/load, save/activate, reopen, exact snapshots and manual criteria";
        return 0;
    }
    if (app.arguments().contains(QStringLiteral("--worksheet-output"))) {
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        const QString outputDir=qEnvironmentVariable("ZTBLD_TEST_OUTPUT_DIR",settingsDir.path());
        QDir().mkpath(outputDir);
        const CalcInput defaults;
        if(defaults.lvConnectionBarThicknessText != QStringLiteral("6")
            || defaults.lvConnectionBarWidthText != QStringLiteral("50")) return 121;
        const auto legacyDefaults=SchemeStore::fromJson(QJsonObject{});
        if(legacyDefaults.lvConnectionBarThicknessText != QStringLiteral("6")
            || legacyDefaults.lvConnectionBarWidthText != QStringLiteral("50")) return 122;
        CalcInput customBar=defaults;
        customBar.lvConnectionBarThicknessText=QStringLiteral("8");
        customBar.lvConnectionBarWidthText=QStringLiteral("60");
        const auto reopenedBar=SchemeStore::fromJson(SchemeStore::toJson(customBar));
        if(reopenedBar.lvConnectionBarThicknessText!=QStringLiteral("8")
            || reopenedBar.lvConnectionBarWidthText!=QStringLiteral("60")) return 123;
        bool loaded=false;
        const auto inputs=SchemeStore::loadSchemes(QStringLiteral(ZTBLD_TEST_SOURCE_DIR "/tests/fixtures/default-review-scheme.json"), &loaded);
        if (!loaded || inputs.isEmpty()) return 111;
        EnterCalcPage page;
        page.setParams(inputs.first().performanceCriteria);
        CalcInput legacyBlank=inputs.first();
        legacyBlank.lvConnectionBarThicknessText.clear();
        legacyBlank.lvConnectionBarWidthText.clear();
        page.setCalcInput(legacyBlank);
        page.resize(1600,1000); page.show(); app.processEvents();
        if (!QMetaObject::invokeMethod(&page,"onRunEmCalc",Qt::DirectConnection)) return 112;
        auto *schemes=page.findChild<SchemeTableWidget *>();
        if (!schemes || schemes->rowCount()!=1) return 113;
        for (auto *button:page.findChildren<QToolButton *>())
            if (button->text()==QStringLiteral("方案确认") || button->text()==QStringLiteral("保存方案库")) return 114;
        bool emitted=false, saved=false, opened=false;
        CalcInput confirmed;
        QObject::connect(&page,&EnterCalcPage::schemeConfirmed,&page,
            [&](const TransformerParams &,const CalcInput &input,const CalcResult &){emitted=true;confirmed=input;});
        const QString library=settingsDir.filePath(QStringLiteral("edited.json"));
        QTimer::singleShot(0,&page,[&] {
            auto *dlg=qobject_cast<QDialog *>(QApplication::activeModalWidget());
            auto *sheet=dlg?dlg->findChild<SchemeCalculationSheet *>():nullptr;
            auto *save=dlg?dlg->findChild<QPushButton *>(QStringLiteral("saveSchemeLibraryButton")):nullptr;
            auto *confirm=dlg?dlg->findChild<QPushButton *>(QStringLiteral("confirmSchemeButton")):nullptr;
            QTableWidget *grid=nullptr;
            if(sheet)for(auto *table:sheet->findChildren<QTableWidget *>())if(table->columnCount()==38)grid=table;
            if(!dlg||!sheet||!save||!confirm||!grid){if(dlg)dlg->reject();return;}
            opened=true;
            if(grid->item(11,32)->text()!=QStringLiteral("6") || grid->item(11,35)->text()!=QStringLiteral("50")
                || !(grid->item(11,32)->flags() & Qt::ItemIsEditable)
                || !(grid->item(11,35)->flags() & Qt::ItemIsEditable)){dlg->reject();return;}
            grid->item(11,32)->setText(QStringLiteral("8"));
            grid->item(11,35)->setText(QStringLiteral("60"));
            grid->item(11,7)->setText(QStringLiteral("175"));
            QTimer::singleShot(0,dlg,[&] {
                auto *file=qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
                if(file){file->selectFile(library);QMetaObject::invokeMethod(file,"accept",Qt::DirectConnection);}
            });
            save->click();
            bool ok=false;const auto savedInputs=SchemeStore::loadSchemes(library,&ok);
            saved=ok && savedInputs.size()==1 && savedInputs.first().coreDiameter_mm==175.0
                && savedInputs.first().lvConnectionBarThicknessText==QStringLiteral("8")
                && savedInputs.first().lvConnectionBarWidthText==QStringLiteral("60");
            confirm->click();
        });
        QMetaObject::invokeMethod(&page,"onSchemeSelected",Qt::DirectConnection,Q_ARG(int,0));
        if(!opened||!saved||!emitted||confirmed.coreDiameter_mm!=175.0) return 115;
        if(confirmed.lvConnectionBarThicknessText!=QStringLiteral("8")
            ||confirmed.lvConnectionBarWidthText!=QStringLiteral("60")) return 124;
        auto *tabs=page.findChild<QTabBar *>();
        auto *sheet=page.findChild<SchemeCalculationSheet *>(QStringLiteral("outputCalculationSheet"));
        if(!tabs||tabs->currentIndex()!=2||!sheet) return 116;
        app.processEvents();
        QTableWidget *labelGrid=nullptr;
        for(auto *candidate:sheet->findChildren<QTableWidget *>())if(candidate->columnCount()==38)labelGrid=candidate;
        if(!labelGrid)return 125;
        for(int r=2;r<54;++r)if(labelGrid->columnSpan(r,19)!=1)return 126;
        for(const auto range: {qMakePair(8,4),qMakePair(12,2),qMakePair(21,22),qMakePair(43,2),qMakePair(45,9)}) {
            if(labelGrid->rowSpan(range.first,19)!=range.second)return 127;
            auto *block=labelGrid->cellWidget(range.first,19);
            if(!block||!block->property("worksheetLabelBlock").toBool())return 128;
        }
        for(const auto range: {qMakePair(32,3),qMakePair(41,3),qMakePair(45,2)})
            if(labelGrid->rowSpan(range.first,0)!=range.second || labelGrid->columnSpan(range.first,0)!=1)return 129;
        if(labelGrid->rowSpan(41,1)!=3||labelGrid->rowSpan(32,2)!=3
           ||labelGrid->rowSpan(32,3)!=3||labelGrid->rowSpan(48,3)!=2)return 132;
        if(labelGrid->rowSpan(6,4)!=5||labelGrid->rowSpan(11,4)!=5||labelGrid->columnSpan(25,0)!=4)return 130;
        for(int r=8;r<25;++r)if(labelGrid->columnSpan(r,0)!=(r==18?1:2))return 131;
        auto *supplement=sheet->findChild<QPushButton *>(QStringLiteral("supplementParametersButton"));
        if(!supplement||!supplement->isHidden())return 117;
        QTableWidget *grid=nullptr;
        for(auto *table:sheet->findChildren<QTableWidget *>())if(table->columnCount()==38)grid=table;
        if(!grid||grid->rowCount()!=54||grid->rowSpan(20,4)!=7||grid->columnSpan(20,4)!=15
           ||grid->editTriggers()!=QAbstractItemView::NoEditTriggers||grid->item(11,7)->text().toDouble()!=175.0)return 118;
        for(int r=0;r<54;++r)for(int c=0;c<38;++c)if(auto *w=grid->cellWidget(r,c))
            if(qobject_cast<QComboBox *>(w)||qobject_cast<QLineEdit *>(w))return 119;
        ElectromagneticEngine exportEngine;CalcResult exportResult;
        if(!exportEngine.calcElectromagnetic(confirmed,exportResult,true))return 133;
        QString exportError;
        if(!sheet->exportExcel(QDir(outputDir).filePath(QStringLiteral("linked-export-test.xlsx")),
            confirmed,confirmed.performanceCriteria,exportResult,exportError)){QFile failure(QDir(outputDir).filePath(QStringLiteral("excel-export-error.txt")));if(failure.open(QIODevice::WriteOnly))failure.write(exportError.toUtf8());return 134;}
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setPageSize(QPageSize(QPageSize::A3));
        printer.setOutputFileName(QDir(outputDir).filePath(QStringLiteral("worksheet.pdf")));
        sheet->printTo(&printer);
        if(QFileInfo(printer.outputFileName()).size()<10000)return 120;
        page.grab().save(QDir(outputDir).filePath(QStringLiteral("output-page.png")));
        qInfo()<<"PASS: popup edited save, immediate confirmation/navigation, original 54x38 merged worksheet, clean readonly output and PDF";
        return 0;
    }
    // Exercise the real startup page, then the native popup without typing AN3.
    for (bool professional : {false, true}) {
        QSettings("ZTF", "Designer").setValue("optimize/proMode", professional);
        OptimizeCalcPage startup;
        StructureConfig config;
        config.calcMode = professional ? StructureConfig::Professional : StructureConfig::Normal;
        startup.setStructureConfig(config);
        if (startup.currentConfig().calcMode != config.calcMode) return 90;
        auto *editor = startup.findChild<ParamTableWidget *>();
        if (!editor) return 91;
        CalcInput initial;
        editor->saveToInput(initial);
        const auto p = editor->getParams();
        if (!p.hasNonstandardAn3 || std::abs(p.nonstandardAn3_pct-0.7)>1e-9) {
            qCritical() << "Startup default is missing original AN3=0.7"; return 92;
        }
        ElectromagneticEngine engine;
        CalcResult r;
        if (!engine.calcElectromagnetic(initial,r,true)) return 93;
        SchemeCalculationSheet sheet(p,config,initial,r,professional);
        QTableWidget *grid = nullptr;
        for (auto *candidate:sheet.findChildren<QTableWidget *>())
            if (candidate->columnCount()==38) grid=candidate;
        auto *mode=grid?qobject_cast<QComboBox *>(grid->cellWidget(2,5)):nullptr;
        if (!mode) return 94;
        mode->setCurrentIndex(1);
        if (std::abs(sheet.inputTable()->getParams().noLoadCurrentStd_pct-0.36)>1e-9) {
            qCritical() << "Startup popup must switch directly to 0.36 without AN3 entry"; return 95;
        }
        if (!grid->item(8,14) || std::abs(grid->item(8,14)->text().toDouble()-0.36)>1e-9) return 100;
        CalcInput custom=initial; sheet.inputTable()->saveToInput(custom);
        const auto reopened=SchemeStore::fromJson(SchemeStore::toJson(custom));
        if (!reopened.hasPerformanceCriteria || !reopened.performanceCriteria.hasNonstandardAn3
                || std::abs(reopened.performanceCriteria.noLoadCurrentStd_pct-0.36)>1e-9) return 96;
        SchemeCalculationSheet restored(reopened.performanceCriteria,config,reopened,r,professional);
        if (std::abs(restored.inputTable()->getParams().noLoadCurrentStd_pct-0.36)>1e-9) return 97;
        QTimer::singleShot(0, [] {
            if (auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                if (auto *yes=box->button(QMessageBox::Yes)) yes->click();
        });
        mode->setCurrentIndex(0);
        if (std::abs(sheet.inputTable()->getParams().noLoadCurrentStd_pct-0.56)>1e-9) return 98;
        mode->setCurrentIndex(1);
        if (std::abs(sheet.inputTable()->getParams().noLoadCurrentStd_pct-0.36)>1e-9) return 99;
        qInfo() << "Startup default automatic switch and reopen passed; professional=" << professional;
    }
    QTimer modalGuard;
    QObject::connect(&modalGuard, &QTimer::timeout, &app, [] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            qWarning().noquote() << "Regression modal:" << box->windowTitle() << box->text();
            box->reject();
        }
    });
    modalGuard.start(20);
    EnterCalcPage page;
    auto *results = page.findChild<EmResultPanel *>();
    auto *schemes = page.findChild<SchemeTableWidget *>();
    auto *print = page.findChild<PrintTableWidget *>();
    if (!results || !schemes || !print) {
        qCritical() << "Calculation page widgets are missing";
        return 11;
    }

    CalcInput first;
    // Current calculation entry requires an explicit confirmed criteria snapshot.
    first.hasPerformanceCriteria = true;
    first.performanceCriteria.standardMode = TransformerParams::StandardMode::Custom;
    first.performanceCriteria.productModel = QStringLiteral("SB20-M-630/10-0.4");
    first.performanceCriteria.lossStandardsManual = true;
    page.setCalcInput(first);
    if (!QMetaObject::invokeMethod(&page, "onRunEmCalc", Qt::DirectConnection)
            || schemes->rowCount() == 0
            || qobject_cast<QTableWidget *>(results->widget(0))->rowCount() == 0) {
        qCritical() << "Could not establish a completed first calculation";
        return 12;
    }

    // The selection popup must show both out-of-limit and skipped checks after edits.
    {
        bool sawExceeded=false, sawSkipped=false, opened=false;
        QTimer::singleShot(0,&page,[&]() {
            auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if(!dialog)return;
            opened=true;
            auto *sheet=dialog->findChild<SchemeCalculationSheet *>();
            auto *summary=dialog->findChild<QLabel *>(QStringLiteral("schemeCalculationSummary"));
            QTableWidget *grid=nullptr;
            if(sheet)for(auto *candidate:sheet->findChildren<QTableWidget *>())if(candidate->columnCount()==38)grid=candidate;
            if(!grid||!summary){dialog->reject();return;}
            grid->item(8,11)->setText(QStringLiteral("100"));
            QTimer::singleShot(400,dialog,[&,dialog,grid,summary]() {
                sawExceeded=summary->text().contains(QStringLiteral("负载损耗"))
                    && summary->text().contains(QStringLiteral("超出限值"));
                grid->item(8,11)->setText(QStringLiteral("0"));
                QTimer::singleShot(400,dialog,[&,dialog,summary]() {
                    sawSkipped=summary->text().contains(QStringLiteral("未校核"))
                        && summary->text().contains(QStringLiteral("负载损耗"));
                    dialog->reject();
                });
            });
        });
        if(!QMetaObject::invokeMethod(&page,"onSchemeSelected",Qt::DirectConnection,Q_ARG(int,0))
            ||!opened||!sawExceeded||!sawSkipped) {
            qCritical()<<"Selection popup must report exceeded and skipped performance checks"; return 32;
        }
    }
    // Two candidates: confirm edits, cancel/close edits, and verify downstream output.
    {
        if(!QMetaObject::invokeMethod(&page,"onRunEmCalc",Qt::DirectConnection)||schemes->rowCount()!=2) {
            qCritical()<<"Could not prepare two independent candidates";return 33;
        }
        bool interactionOk=true;
        auto openSheet=[&](int row,const std::function<void(QDialog *,SchemeCalculationSheet *,QTableWidget *)> &action) {
            bool opened=false;
            QTimer::singleShot(0,&page,[&]() {
                auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if(!dialog){interactionOk=false;return;}
                auto *sheet=dialog->findChild<SchemeCalculationSheet *>();QTableWidget *grid=nullptr;
                if(sheet)for(auto *candidate:sheet->findChildren<QTableWidget *>())if(candidate->columnCount()==38)grid=candidate;
                if(!sheet||!grid){interactionOk=false;dialog->reject();return;}
                opened=true;action(dialog,sheet,grid);
            });
            const bool invoked=QMetaObject::invokeMethod(&page,"onSchemeSelected",Qt::DirectConnection,Q_ARG(int,row));
            return invoked&&opened&&interactionOk;
        };
        auto seamRow=[](ParamTableWidget *source) {
            for(int row=0;row<source->rowCount();++row)
                if(source->item(row,1)&&source->item(row,1)->text()==QStringLiteral("接缝数"))return row;
            return -1;
        };
        auto clickDialogButton=[&](QDialog *dialog,const QString &text) {
            for(auto *button:dialog->findChildren<QPushButton *>())
                if(button->text()==text){button->click();return;}
            interactionOk=false;dialog->reject();
        };
        if(!openSheet(0,[&](QDialog *dialog,SchemeCalculationSheet *sheet,QTableWidget *grid) {
            grid->item(11,7)->setText(QStringLiteral("175"));
            auto *button=sheet->findChild<QPushButton *>(QStringLiteral("supplementParametersButton"));
            if(!button){interactionOk=false;dialog->reject();return;}
            QTimer::singleShot(0,dialog,[&]() {
                auto *supplement=qobject_cast<QDialog *>(QApplication::activeModalWidget());
                auto *source=sheet->inputTable();const int row=seamRow(source);
                if(!supplement||row<0||source->isRowHidden(row)){interactionOk=false;if(supplement)supplement->reject();return;}
                source->item(row,2)->setText(QStringLiteral("7"));
                for(int r=0;r<source->rowCount();++r)
                    if(source->item(r,1)&&source->item(r,1)->text()==QStringLiteral("油顶层温升限值(K)"))
                        source->item(r,2)->setText(QStringLiteral("1"));
                supplement->accept();
            });
            button->click();
            clickDialogButton(dialog,QStringLiteral("确认方案"));
        })) {qCritical()<<"Could not confirm first candidate edit";return 34;}
        auto verifyFirst=[&](QDialog *dialog,SchemeCalculationSheet *sheet,QTableWidget *grid) {
            const int row=seamRow(sheet->inputTable());
            auto *summary=dialog->findChild<QLabel *>(QStringLiteral("schemeCalculationSummary"));
            if(grid->item(11,7)->text().toDouble()!=175.0||row<0
                ||sheet->inputTable()->item(row,2)->text().toInt()!=7
                ||sheet->inputTable()->getParams().oilTopTempRise_K!=1.0
                ||!summary||!summary->text().contains(QStringLiteral("油顶层温升K")))interactionOk=false;
            dialog->reject();
        };
        if(!openSheet(0,verifyFirst)){qCritical()<<"Confirmed first candidate did not persist";return 35;}
        if(!openSheet(0,[&](QDialog *dialog,SchemeCalculationSheet *sheet,QTableWidget *grid) {
            grid->item(11,7)->setText(QStringLiteral("180"));
            const int row=seamRow(sheet->inputTable());
            if(row<0){interactionOk=false;dialog->reject();return;}
            sheet->inputTable()->item(row,2)->setText(QStringLiteral("8"));
            auto *source=sheet->inputTable();
            for(int r=0;r<source->rowCount();++r)
                if(source->item(r,1)&&source->item(r,1)->text()==QStringLiteral("油顶层温升限值(K)"))
                    source->item(r,2)->setText(QStringLiteral("2"));
            clickDialogButton(dialog,QStringLiteral("取消"));
        })||!openSheet(0,verifyFirst)){qCritical()<<"Cancel failed to discard candidate edits";return 36;}
        if(!openSheet(0,[&](QDialog *dialog,SchemeCalculationSheet *,QTableWidget *grid) {
            grid->item(11,7)->setText(QStringLiteral("185"));dialog->close();
        })||!openSheet(0,verifyFirst)){qCritical()<<"Closing failed to discard candidate edits";return 37;}
        if(!openSheet(1,[&](QDialog *dialog,SchemeCalculationSheet *sheet,QTableWidget *grid) {
            const int row=seamRow(sheet->inputTable());
            if(grid->item(11,7)->text().toDouble()!=170.0||row<0
                ||sheet->inputTable()->item(row,2)->text().toInt()!=5
                ||sheet->inputTable()->getParams().oilTopTempRise_K!=60.0)interactionOk=false;
            dialog->reject();
        })) {qCritical()<<"Editing candidate one changed candidate two";return 38;}
        QuotePage quote;
        CalcInput confirmedInput;CalcResult confirmedResult;bool emitted=false;
        QObject::connect(&page,&EnterCalcPage::schemeConfirmed,&quote,
            [&](const TransformerParams &params,const CalcInput &input,const CalcResult &result) {
                emitted=true;confirmedInput=input;confirmedResult=result;quote.loadScheme(params,input,result);
            });
        if(!QMetaObject::invokeMethod(&page,"onConfirmScheme",Qt::DirectConnection)
           ||!emitted||!quote.hasScheme()||confirmedInput.coreDiameter_mm!=175.0||confirmedInput.seamCount!=7) {
            qCritical()<<"Confirmed data did not reach quote page";return 39;
        }
        PrintTableWidget expectedPrint;
        expectedPrint.loadData(ElectromagneticEngine::buildPrintOutput(confirmedInput,confirmedResult));
        bool samePrint=print->rowCount()==expectedPrint.rowCount()&&print->columnCount()==expectedPrint.columnCount();
        for(int row=0;samePrint&&row<print->rowCount();++row)for(int col=0;col<print->columnCount();++col) {
            const auto *actual=print->item(row,col);const auto *expected=expectedPrint.item(row,col);
            if((actual?actual->text():QString())!=(expected?expected->text():QString()))samePrint=false;
        }
        auto *quoteTable=quote.findChild<QTableWidget *>();bool steelMatches=false;
        if(quoteTable)for(int row=0;row<quoteTable->rowCount();++row) {
            const auto *name=quoteTable->item(row,0);const auto *quantity=quoteTable->item(row,1);
            if(name&&quantity&&name->text().contains(QStringLiteral("硅钢")))
                steelMatches=std::abs(quantity->text().toDouble()-std::round(confirmedResult.core.coreWeight_kg*1.05))<0.051;
        }
        if(!samePrint||!steelMatches){qCritical()<<"Print or quote quantities do not match confirmed candidate";return 40;}
    }
    // Real popup workflow in the isolated supported-segment build.
    {
        QSettings isolated("ZTF","Designer");
        isolated.setValue("optimize/searchDiameter",false);
        isolated.setValue("optimize/searchStraight",false);
        isolated.setValue("optimize/searchLvTurns",false);
        isolated.setValue("optimize/searchHvLayers",true);
        isolated.setValue("optimize/hvTplRange",1);
        isolated.setValue("optimize/searchMethod",int(OptimizationSettings::Exhaustive));
        isolated.sync();
        EnterCalcPage segmentPage;
        segmentPage.setParams(first.performanceCriteria);
        segmentPage.setCalcInput(first);
        auto *segmentSchemes=segmentPage.findChild<SchemeTableWidget *>();
        if(!segmentSchemes || !QMetaObject::invokeMethod(&segmentPage,"onRunEmCalc",Qt::DirectConnection)
           || segmentSchemes->rowCount()!=1) {qCritical()<<"Segment lab: cannot prepare cylinder";return 50;}
        bool edited=false;
        QTimer::singleShot(0,&segmentPage,[&] {
            auto *dlg=qobject_cast<QDialog *>(QApplication::activeModalWidget());
            auto *sheet=dlg?dlg->findChild<SchemeCalculationSheet *>():nullptr;
            auto *selector=sheet?sheet->findChild<QComboBox *>(QStringLiteral("sheetCoilForm")):nullptr;
            if(!dlg||!sheet||!selector){if(dlg)dlg->reject();return;}
            selector->setCurrentIndex(1);
            QTimer::singleShot(500,dlg,[&,dlg,sheet] {
                auto *summary=dlg->findChild<QLabel *>(QStringLiteral("schemeCalculationSummary"));
                if(!summary||!summary->text().contains(QStringLiteral("7.56"))){dlg->reject();return;}
                for(auto *button:dlg->findChildren<QPushButton *>()) if(button->text()==QStringLiteral("确认方案")) {
                    edited=true;button->click();return;
                }
                dlg->reject();
            });
        });
        if(!QMetaObject::invokeMethod(&segmentPage,"onSchemeSelected",Qt::DirectConnection,Q_ARG(int,0)) || !edited) {
            qCritical()<<"Segment lab: popup failed to recalculate or confirm";return 51;
        }
        bool confirmed=false;CalcInput snapshot;CalcResult output;
        QObject::connect(&segmentPage,&EnterCalcPage::schemeConfirmed,&segmentPage,
          [&](const TransformerParams &,const CalcInput &in,const CalcResult &r){confirmed=true;snapshot=in;output=r;});
        if(!QMetaObject::invokeMethod(&segmentPage,"onConfirmScheme",Qt::DirectConnection)
           || !confirmed || snapshot.hvCoilFormIdx!=2 || output.winding.hvCoilFormIdx!=2
           || std::abs(output.impedance.impedance_pct-7.56)>0.001) {
            qCritical()<<"Segment lab: confirmed snapshot mismatch";return 52;
        }
        const auto report=ElectromagneticEngine::buildPrintOutput(snapshot,output);
        bool found=false;for(const auto &r:report.rows)
            if(r.leftName==QStringLiteral("高压线圈型式")&&r.leftValue==QStringLiteral("分段式"))found=true;
        if(!found){qCritical()<<"Segment lab: printed type mismatch";return 53;}
        bool cancelled=false;
        QTimer::singleShot(0,&segmentPage,[&] {
            auto *dlg=qobject_cast<QDialog *>(QApplication::activeModalWidget());
            auto *sheet=dlg?dlg->findChild<SchemeCalculationSheet *>():nullptr;
            auto *selector=sheet?sheet->findChild<QComboBox *>(QStringLiteral("sheetCoilForm")):nullptr;
            if(!dlg||!selector){if(dlg)dlg->reject();return;}
            selector->setCurrentIndex(0);cancelled=true;dlg->reject();
        });
        if(!QMetaObject::invokeMethod(&segmentPage,"onSchemeSelected",Qt::DirectConnection,Q_ARG(int,0)) || !cancelled) {
            qCritical()<<"Segment lab: cancellation failed";return 54;
        }
        confirmed=false;
        QMetaObject::invokeMethod(&segmentPage,"onConfirmScheme",Qt::DirectConnection);
        if(!confirmed||snapshot.hvCoilFormIdx!=2||std::abs(output.impedance.impedance_pct-7.56)>0.001) {
            qCritical()<<"Segment lab: cancellation changed saved candidate";return 55;
        }
        QTemporaryDir storeDir;
        const QString storePath=storeDir.filePath(QStringLiteral("mixed-schemes.json"));
        if(!storeDir.isValid() || !SchemeStore::saveSchemes(storePath,{first,snapshot})) {
            qCritical()<<"Segment lab: cannot write isolated scheme file";return 56;
        }
        segmentPage.setCalcInput(first);
        bool pickerOpened=false;
        QTimer::singleShot(0,&segmentPage,[&] {
            auto *picker=qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
            if(!picker)return;
            pickerOpened=true;picker->selectFile(storePath);
            QMetaObject::invokeMethod(picker,"accept",Qt::DirectConnection);
        });
        confirmed=false;
        if(!QMetaObject::invokeMethod(&segmentPage,"onLoadSchemes",Qt::DirectConnection)
           || !pickerOpened || segmentSchemes->rowCount()!=2 || !confirmed
           || snapshot.hvCoilFormIdx!=1 || output.winding.hvCoilFormIdx!=1) {
            qCritical()<<"Segment lab: mixed scheme reload failed"<<segmentSchemes->rowCount();return 57;
        }
        segmentSchemes->markRow(1);confirmed=false;
        QMetaObject::invokeMethod(&segmentPage,"onConfirmScheme",Qt::DirectConnection);
        if(!confirmed||snapshot.hvCoilFormIdx!=2||std::abs(output.impedance.impedance_pct-7.56)>0.001) {
            qCritical()<<"Segment lab: restored segmented candidate mismatch";return 58;
        }
        {
            CalcInput invalid=snapshot; invalid.lvTurns=0;
            const QString badPath=storeDir.filePath(QStringLiteral("invalid-schemes.json"));
            if(!SchemeStore::saveSchemes(badPath,{first,invalid})) return 62;
            const int oldCount=segmentSchemes->rowCount();
            confirmed=false;
            QTimer::singleShot(0,&segmentPage,[&] {
                auto *picker=qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
                if(picker){picker->selectFile(badPath);QMetaObject::invokeMethod(picker,"accept",Qt::DirectConnection);}
            });
            QMetaObject::invokeMethod(&segmentPage,"onLoadSchemes",Qt::DirectConnection);
            if(segmentSchemes->rowCount()!=oldCount || confirmed) {
                qCritical()<<"Segment lab: invalid scheme file changed existing schemes";return 63;
            }
            segmentSchemes->markRow(1); QMetaObject::invokeMethod(&segmentPage,"onConfirmScheme",Qt::DirectConnection);
            if(!confirmed || snapshot.hvCoilFormIdx!=2) {qCritical()<<"Segment lab: old scheme lost after failed load";return 64;}
        }
        {
            GridOptimizer guarded;
            for(int scenario=0;scenario<2;++scenario) {
                CalcInput guardedInput=snapshot; StructureConfig guardedConfig;
                if(scenario==1) {guardedConfig.hvCoilStructure=StructureConfig::TwoSegCylinder; guardedInput.hvDuctHeightSide[1]=0;}
                QEventLoop guardLoop; bool guardDone=false; QString guardError; int guardEvaluated=-1;
                const auto conn=QObject::connect(&guarded,&IOptimizer::finished,&guardLoop,[&](bool,const OptimizeCandidate &,const OptimizationRunSummary &summary){
                    guardDone=true;guardError=summary.error;guardEvaluated=summary.evaluated;guardLoop.quit();
                });
                QTimer::singleShot(3000,&guardLoop,&QEventLoop::quit);
                guarded.start(first.performanceCriteria,guardedConfig,guardedInput,OptimizationSettings{});
                guardLoop.exec();QObject::disconnect(conn);
                if(!guardDone||guardError.isEmpty()||guardEvaluated!=0) {
                    qCritical()<<"Segment lab: optimizer scope guard failed"<<scenario;return 61;
                }
            }
        }
        segmentPage.setParams(snapshot.performanceCriteria);
        segmentPage.setCalcInput(snapshot);
        auto *segmentOptimizer=segmentPage.findChild<GridOptimizer *>();
        bool optimizeDone=false, allSegmented=true;int evaluated=0,accepted=0;
        QObject::connect(segmentOptimizer,&IOptimizer::candidateReady,&segmentPage,[&](const OptimizeCandidate &c){
            allSegmented = allSegmented && c.input.hvCoilFormIdx==2 && c.result.winding.hvCoilFormIdx==2;
        });
        QObject::connect(segmentOptimizer,&IOptimizer::finished,&segmentPage,[&](bool,const OptimizeCandidate &,const OptimizationRunSummary &summary){
            optimizeDone=true;evaluated=summary.evaluated;accepted=summary.accepted;
        });
        if(!segmentOptimizer || !QMetaObject::invokeMethod(&segmentPage,"onOptimizeStart",Qt::DirectConnection)) {
            qCritical()<<"Segment lab: cannot start optimizer";return 59;
        }
        QElapsedTimer optimizeTimer;optimizeTimer.start();
        while(!optimizeDone&&optimizeTimer.elapsed()<10000){app.processEvents();QThread::msleep(10);}
        if(!optimizeDone||evaluated==0||accepted==0||!allSegmented){
            qCritical()<<"Segment lab: optimization mismatch"<<optimizeDone<<evaluated<<accepted<<allSegmented;return 60;
        }
    }
    CalcInput second = first;
    {
        const int beforeCount=schemes->rowCount(); const auto beforeCost=schemes->item(0,2)->text();
        auto *open=page.findChild<QToolButton *>("openCandidateComparison");
        bool sawDialog=false; QTimer closeDialog; closeDialog.setSingleShot(true);
        QObject::connect(&closeDialog,&QTimer::timeout,&page,[&] {
            if(auto *dialog=qobject_cast<CandidateComparisonDialog *>(QApplication::activeModalWidget())) {sawDialog=true;dialog->reject();}
        });
        closeDialog.start(0); if(open) open->click(); closeDialog.stop();
        if(!open||!sawDialog||schemes->rowCount()!=beforeCount||schemes->item(0,2)->text()!=beforeCost) {
            qCritical()<<"Read-only candidate comparison entry must open without changing formal schemes"; return 31;
        }
    }
    second.coreDiameter_mm += 5.0;
    page.setCalcInput(second);
    for (int i = 0; i < results->count(); ++i) {
        auto *table = qobject_cast<QTableWidget *>(results->widget(i));
        if (!table || table->rowCount() != 0) {
            qCritical() << "Old calculation remains visible in result tab" << i;
            return 13;
        }
    }
    if (schemes->rowCount() != 0) {
        qCritical() << "Old schemes remain available after input change";
        return 14;
    }
    if (print->rowCount() == 0) {
        qCritical() << "New input's initial print sheet was not prepared";
        return 15;
    }

    auto *optimizer = page.findChild<GridOptimizer *>();
    if (!optimizer) {
        return 16;
    }
    bool finished = false;
    QObject::connect(optimizer, &IOptimizer::finished, &page,
                     [&finished](bool, const OptimizeCandidate &, const OptimizationRunSummary &) {
                         finished = true;
                     });
    if (!QMetaObject::invokeMethod(&page, "onOptimizeStart", Qt::DirectConnection)) {
        return 17;
    }
    CalcInput third = second;
    third.coreDiameter_mm += 5.0;
    page.setCalcInput(third);
    QElapsedTimer timer;
    timer.start();
    while (!finished && timer.elapsed() < 10000) {
        app.processEvents();
        QThread::msleep(10);
    }
    if (!finished || schemes->rowCount() != 0
            || qobject_cast<QTableWidget *>(results->widget(0))->rowCount() != 0) {
        qCritical() << "Old optimizer repopulated results after input change";
        return 18;
    }
    return 0;
}
