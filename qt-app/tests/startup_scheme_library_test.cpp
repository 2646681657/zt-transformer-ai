#include "StartupSchemeLibrary.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <cstdio>

int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    QTemporaryDir directory;
    CalcInput base;
    base.hasPerformanceCriteria=true;
    base.performanceCriteria.productModel=QStringLiteral("SB20-M-630/10-0.4");
    auto state=StartupSchemeLibrary::initial(base);
    QString error;
    const auto path=directory.filePath("defaults.json");
    if(state.entries.size()!=3 || state.entries[1].input.capacity_kVA!=400
       ||state.entries[2].input.capacity_kVA!=630) return 1;
    auto input=state.entries[1].input;
    input.lvFoilThick_mm=1.25;
    auto config=state.entries[1].config;
    config.calcMode=StructureConfig::Professional;
    if(!StartupSchemeLibrary::add(state,"my default",input,config,true,error))return 2;
    const auto selected=state.selectedId;
    if(!StartupSchemeLibrary::write(path,state,error))return 3;
    StartupSchemeLibrary::State reopened;
    if(!StartupSchemeLibrary::read(path,base,reopened,error)
       ||reopened.selectedId!=selected||reopened.entries.size()!=4)return 4;
    const auto saved=reopened.entries.last();
    if(!SchemeStore::sameInput(saved.input,input)||saved.config.calcMode!=config.calcMode)return 5;
    input.lvFoilThick_mm=1.5; // Editing the live input must not change the snapshot.
    if(reopened.entries.last().input.lvFoilThick_mm!=1.25)return 6;
    if(StartupSchemeLibrary::remove(reopened,"builtin-base",error))return 7;
    if(StartupSchemeLibrary::add(reopened,"my default",input,config,false,error))return 8;
    if(!StartupSchemeLibrary::remove(reopened,selected,error)||reopened.selectedId!="builtin-base")return 9;
    QFile corrupt(path);if(!corrupt.open(QIODevice::WriteOnly))return 10;
    corrupt.write("{broken");corrupt.close();
    if(StartupSchemeLibrary::read(path,base,reopened,error)||error.isEmpty()
       ||reopened.selectedId!="builtin-base"||reopened.entries.size()!=3)return 11;
    if(StartupSchemeLibrary::write(directory.path(),state,error))return 12;
    puts("PASS: presets, complete restart snapshot, selection, protected base, corruption and write failure");
    return 0;
}
