#include "LearningCandidateComparison.h"
#include "SelfLearningDataset.h"
#include "SchemeStore.h"
#include "ElectromagneticEngine.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QUuid>
#include <functional>
#include <limits>

static void write(const QString &path, const QJsonObject &o) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(o).toJson()) < 0) qFatal("fixture write");
}
static QString sha(const QString &path) {
    QFile f(path); if (!f.open(QIODevice::ReadOnly)) qFatal("fixture read");
    return QString::fromLatin1(QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex());
}
// Hand-authored metadata only: these fixtures prove binding/validation,
// never trained-model provenance, inference quality or production accuracy.
struct Fixture {
    QTemporaryDir dir;
    QString model = dir.path()+"/model.json", sibling = dir.path()+"/report.json", path = dir.path()+"/input.json";
    QJsonObject input, m, r, report;
    explicit Fixture(QJsonObject in) : input(in) {
        const auto sample = in["candidates"].toArray().first().toObject()["sample"].toObject();
        m = {{"format","ZTBLD-ResidualModel"},{"version",1},{"status","experimental"},
             {"modelVersion","temporary-test-version"},{"scope",sample["scope"]},
             {"engineVersion",sample["engineVersion"]},{"testOnly",true},{"productionEligible",false}};
        r = {{"status","completed"},{"modelVersion",m["modelVersion"]},{"modelPath",model},
             {"testOnly",true},{"productionEligible",false}};
        write(model,m); write(sibling,r); write(path,input);
        QJsonArray rows;
        for (const auto &v : in["candidates"].toArray()) {
            auto row=v.toObject(); const auto s=row.take("sample").toObject();
            row["baseline"]=s["baseline"];
            row["predictions"]=QJsonObject{{"noLoadLoss_W",500},{"loadLoss_W",5000},{"impedance_pct",4},
                {"oilTopRise_K",0},{"hvWindingRise_K",QJsonValue::Null},{"lvWindingRise_K",30}};
            row["failures"]=QJsonObject{{"hvWindingRise_K","target not trained"}};
            rows.append(row);
        }
        report={{"format","ZTBLD-CandidateComparison"},{"version",1},{"status","completed"},
            {"usage","independent-comparison-only"},{"runId",input["runId"]},{"inputPath",path},
            {"inputSha256",sha(path)},{"modelPath",model},{"modelSha256",sha(model)},
            {"reportSha256",sha(sibling)},{"modelVersion",m["modelVersion"]},{"scope",m["scope"]},
            {"engineVersion",m["engineVersion"]},{"testOnly",true},{"productionEligible",false},
            {"candidates",rows},{"counts",QJsonObject{{"candidates",rows.size()},
                {"successfulTargets",rows.size()*5},{"failedTargets",rows.size()}}}};
    }
};
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    int failures=0;
    QString error;
    auto check=[&](bool ok,const char *why){if(!ok){qCritical()<<why<<"validator error:"<<error; ++failures;}};
    OptimizeCandidate first;
    ElectromagneticEngine engine;
    check(engine.calcElectromagnetic(first.input,first.result) && first.result.valid,"real engine fixture");
    first.scheme=makeScheme(19,first.input,first.result);
    // Different from recomputation: the snapshot must use the saved original.
    first.result.core.noLoadLoss_W=531.125;
    first.result.thermal.oilTopRise_K=0;
    first.scheme.costBasis="original frozen cost basis";
    first.scheme.costCuFeOil=1; // Must not replace actual result.cost.materialCost.
    auto second=first; second.scheme.schemeIdx=3; second.result.cost.materialCost+=12.5;
    const QVector<OptimizeCandidate> originals{first,second};
    const auto before=SchemeStore::toJson(first.input);
    auto input=LearningComparison::makeInput(originals,error);
    check(!input.isEmpty() && error.isEmpty(),"valid saved candidates must produce snapshot (RED with stub)");
    if(input.isEmpty()) return 1;
    const auto rows=input["candidates"].toArray();
    check(rows.size()==2 && rows[0].toObject()["candidateId"]==QJsonValue("scheme-19")
          && rows[1].toObject()["schemeId"]==QJsonValue(3)
          && rows[1].toObject()["originalOrder"]==QJsonValue(2),"preserve original order and scheme IDs");
    const auto row=rows[0].toObject(), sample=row["sample"].toObject();
    check(row["materialCost"]==QJsonValue(first.result.cost.materialCost)
          && row["costBasis"]==QJsonValue(first.scheme.costBasis)
          && sample["baseline"].toObject()["noLoadLoss_W"]==QJsonValue(531.125)
          && sample["baseline"].toObject()["oilTopRise_K"]==QJsonValue(0),"copy actual saved metrics and cost exactly");
    check(sample["input"].toObject()==before && sample["id"]==row["candidateId"]
          && sample["productId"]==row["candidateId"] && sample["source"]==QJsonValue("optimization-candidate")
          && sample["purpose"]==QJsonValue("unassigned")
          && sample["testedOn"]==QJsonValue(QDate::currentDate().toString(Qt::ISODate))
          && !QUuid(input["runId"].toString()).isNull(),"sample metadata and original input");
    for(const auto &v:sample["measured"].toObject()) check(v.isNull(),"no measured labels invented");
    check(SchemeStore::toJson(originals[0].input)==before && originals[0].result.core.noLoadLoss_W==531.125
          && originals[0].scheme.costCuFeOil==1 && originals[1].scheme.schemeIdx==3,"source objects remain unchanged");
    check(LearningComparison::makeInput(originals,error)["runId"]!=input["runId"],"fresh run identity");
    auto rejects=[&](OptimizeCandidate c){return LearningComparison::makeInput({c},error).isEmpty() && !error.isEmpty();};
    auto bad=first; bad.result.valid=false; check(rejects(bad),"reject invalid actual result");
    bad=first; bad.scheme.schemeIdx=0; check(rejects(bad),"reject nonpositive scheme ID");
    check(LearningComparison::makeInput({first,first},error).isEmpty(),"reject duplicate scheme IDs");
    check(LearningComparison::makeInput({},error).isEmpty(),"reject empty snapshots");
    for(double value:{-1.,0.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        bad=first; bad.result.impedance.impedance_pct=value; check(rejects(bad),"reject invalid positive baseline");
    }
    bad=first; bad.result.thermal.lvWindingRise_K=-1; check(rejects(bad),"reject negative temperature");
    bad=first; bad.result.cost.materialCost=std::numeric_limits<double>::infinity(); check(rejects(bad),"reject nonfinite cost");
    bad=first; bad.input.coreDiameter_mm=std::numeric_limits<double>::quiet_NaN(); check(rejects(bad),"reject serialized null from nonfinite input");
    bad=first; bad.input.coreDiameter_mm=1000001; check(rejects(bad),"enforce existing numeric import bound");
    bad=first; bad.input.hvCoilFormIdx=2;
    check(!LearningComparison::makeInput({bad},error).isEmpty(),"retain unsupported model physical context for backend failures");
    bad=first; bad.scheme.costBasis=QString(16*1024*1024,'x'); check(rejects(bad),"reject oversized snapshot without clipping");
    Fixture f(input);
    auto valid=[&](){return LearningComparison::validateReport(f.input,f.report,f.model,f.path,error);};
    check(valid() && error.isEmpty(),"accept bound partial report and corrected zero temperature");
    const auto good=f.report;
    const std::initializer_list<const char*> bound={"format","version","usage","status","runId","inputSha256",
        "modelSha256","reportSha256","modelVersion","scope","engineVersion","testOnly","productionEligible","inputPath","modelPath"};
    for(auto key:bound){ f.report=good; f.report[key]="tampered"; check(!valid() && !error.isEmpty(),key); }
    for(auto key:{"candidateId","schemeId","originalOrder","materialCost","costBasis","baseline"}) {
        f.report=good; auto a=f.report["candidates"].toArray(); auto c=a[0].toObject(); c[key]="tampered"; a[0]=c; f.report["candidates"]=a;
        check(!valid(),key);
    }
    auto mutateRow=[&](std::function<void(QJsonObject&)> mutate){f.report=good; auto a=good["candidates"].toArray();
        auto c=a[0].toObject(); mutate(c); a[0]=c; f.report["candidates"]=a; check(!valid(),"reject invalid target coverage/value/reason");};
    for(const auto &value:{QJsonValue(0),QJsonValue(-1),QJsonValue("123"),QJsonValue(true),QJsonValue(QJsonValue::Null)})
        mutateRow([&](QJsonObject &c){auto p=c["predictions"].toObject();p["loadLoss_W"]=value;c["predictions"]=p;});
    mutateRow([](QJsonObject &c){auto p=c["predictions"].toObject();p.remove("loadLoss_W");c["predictions"]=p;});
    mutateRow([](QJsonObject &c){auto p=c["predictions"].toObject();p["unknown"]=1;c["predictions"]=p;});
    mutateRow([](QJsonObject &c){c["failures"]=QJsonObject{};});
    mutateRow([](QJsonObject &c){c["failures"]=QJsonObject{{"hvWindingRise_K","  "}};});
    mutateRow([](QJsonObject &c){c["failures"]=QJsonObject{{"hvWindingRise_K","failed"},{"oilTopRise_K","unexpected"}};});
    f.report=good; auto reversed=good["candidates"].toArray(); auto tmp=reversed[0]; reversed[0]=reversed[1]; reversed[1]=tmp;
    f.report["candidates"]=reversed; check(!valid(),"reject reordered results");
    f.report=good; f.report["counts"]=QJsonObject{{"candidates",2},{"successfulTargets",12},{"failedTargets",0}};
    check(!valid(),"reject incorrect summary counts");
    f.report=good; auto altered=input; altered["runId"]="another"; write(f.path,altered);
    f.report["inputSha256"]=sha(f.path); check(!valid(),"hash alone cannot bind different parsed input"); write(f.path,input);
    f.report=good; QFile file(f.path); check(file.open(QIODevice::Append),"fixture append");file.write(" ");file.close();
    check(!valid(),"bind exact file bytes even if parsed object matches");write(f.path,input);
    f.report=good; auto m=f.m; m["scope"]="unknown"; write(f.model,m); f.report["modelSha256"]=sha(f.model);f.report["scope"]="unknown";
    check(!valid(),"matching unknown scope must fail closed");write(f.model,f.m);
    f.report=good; auto r=f.r; r["status"]="running";write(f.sibling,r); f.report["reportSha256"]=sha(f.sibling);
    check(!valid(),"require completed sibling report");write(f.sibling,f.r);
    f.report=good; f.report["testOnly"]=false;check(!valid(),"test-only model cannot be elevated");
#ifdef Q_OS_WIN
    f.report=good;f.report["modelPath"]=f.model.toUpper().replace('/','\\');
    f.report["inputPath"]=f.path.toUpper().replace('/','\\');check(valid(),"Windows paths compare case insensitively with native separators");
#endif
    // Even self-consistent hashes must not turn malformed input into a valid snapshot.
    auto rejectInput=[&](std::function<void(QJsonObject&)> mutate){
        f.input=input; f.report=good; mutate(f.input); write(f.path,f.input);
        f.report["inputSha256"]=sha(f.path); f.report["runId"]=f.input["runId"];
        check(!valid(),"reject malformed self-consistent input snapshot");
        f.input=input; write(f.path,input); f.report=good;
    };
    rejectInput([](QJsonObject &o){o["runId"]="{"+o["runId"].toString()+"}";});
    for(auto field:{"unknown","coreDiameter_mm","hvDuctWidthSide"}) {
        rejectInput([&](QJsonObject &o){auto a=o["candidates"].toArray(); auto c=a[0].toObject();
            auto s=c["sample"].toObject();auto i=s["input"].toObject(); i[field]=QJsonValue::Null;
            s["input"]=i;c["sample"]=s;a[0]=c;o["candidates"]=a;});
    }
    rejectInput([](QJsonObject &o){auto a=o["candidates"].toArray();auto c=a[0].toObject();
        auto s=c["sample"].toObject();auto measured=s["measured"].toObject();measured["loadLoss_W"]=12;
        s["measured"]=measured;c["sample"]=s;a[0]=c;o["candidates"]=a;});
    // A row may fail all targets. It must survive with complete, honest counts.
    f.report=good; auto allFailed=good["candidates"].toArray();
    for(qsizetype n=0;n<allFailed.size();++n) {
        auto c=allFailed[n].toObject();QJsonObject predictions,reasons;
        for(const auto &metric:SelfLearning::metrics()) {predictions[metric.key]=QJsonValue::Null;reasons[metric.key]="unsupported context";}
        c["predictions"]=predictions;c["failures"]=reasons;allFailed[n]=c;
    }
    f.report["candidates"]=allFailed;f.report["counts"]=QJsonObject{{"candidates",2},{"successfulTargets",0},{"failedTargets",12}};
    check(valid(),"retain entirely failed candidates");
    f.report=good;auto missing=good["candidates"].toArray();missing.removeLast();f.report["candidates"]=missing;
    check(!valid(),"reject silently omitted candidates");
    for(const auto *key:{"modelVersion","testOnly","productionEligible","modelPath"}) {
        auto sibling=f.r;sibling[key]="tampered";write(f.sibling,sibling);
        f.report=good;f.report["reportSha256"]=sha(f.sibling);check(!valid(),"reject inconsistent sibling metadata despite fresh hash");
        write(f.sibling,f.r);
    }
    f.report=good;auto model=f.m;model["modelVersion"]="changed after launch";write(f.model,model);
    check(!valid(),"reject changed model bytes");write(f.model,f.m);
    f.report=good;auto sibling=f.r;sibling["extra"]="changed after launch";write(f.sibling,sibling);
    check(!valid(),"reject changed sibling report bytes");write(f.sibling,f.r);
    f.report=good;model=f.m;model["testOnly"]=false;write(f.model,model);
    sibling=f.r;sibling["testOnly"]=false;write(f.sibling,sibling);
    f.report["testOnly"]=false;f.report["modelSha256"]=sha(f.model);f.report["reportSha256"]=sha(f.sibling);
    check(valid(),"accept completed experimental non-test metadata while remaining production-ineligible");
    write(f.model,f.m);write(f.sibling,f.r);
    f.report=good;
    check(valid(),"valid report remains usable after failed validations");
    qInfo()<<"candidate comparison failures:"<<failures;
    return failures?1:0;
}
