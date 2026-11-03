#include "LearningDataPreparation.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QLockFile>
#include <QTemporaryDir>
#include <QDebug>
#include <QCryptographicHash>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int failures = 0;
    auto check = [&](bool ok, const char *why) { if (!ok) { qCritical() << why; ++failures; } };
    using namespace SelfLearning;
    using namespace LearningPreparation;
    QString error;
    Sample s;
    check(makeSample(CalcInput{}, s, error), "fixture");
    s.id = "00012345678901234567890"; s.productId = "001";
    s.schemeName = "=公式,\"中文\"\r\n下一行"; s.source = "'原文";
    s.testedOn = QDate(2026,10,23); s.purpose = "training";
    for (auto &m : s.measured) m = 12.345678901234567;
    s.measured[5].reset();
    CsvPreview p;
    const auto csv = csvTemplate({s});
    const auto digest=QCryptographicHash::hash(QJsonDocument(encode(s)).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex();
    check(csv.contains(digest),"snapshot is SHA256 of entire compact encoded sample");
    check(csv.startsWith(QByteArray::fromHex("efbbbf")), "Excel BOM");
    check(csv.contains("'00012345678901234567890") && csv.contains("'=公式") && csv.contains("''原文"), "all text cells protected reversibly");
    check(previewCsv(csv, {s}, p, error), "quoted multiline Chinese round trip");
    check(p.replacements.size() == 1 && encode(p.replacements[0]) == encode(s), "precision, IDs, apostrophes, null round trip");
    auto lf=csv; lf.replace("\r\n","\n");
    check(previewCsv(lf,{s},p,error),"LF records supported");
    auto noBom=csv.mid(3); check(previewCsv(noBom,{s},p,error),"UTF8 without BOM supported");
    auto reject = [&](QByteArray bytes, QVector<Sample> current, const char *why) {
        check(!previewCsv(bytes,current,p,error) && !error.isEmpty() && !p.errors.isEmpty()
              && p.replacements.isEmpty() && p.originals.isEmpty(), why);
    };
    reject(QByteArray("\xff"), {s}, "invalid UTF8");
    reject(csv+QByteArray::fromHex("e4b8"),{s},"truncated UTF8 must not be accepted as buffered text");
    reject(QByteArray(16*1024*1024+1,'x'), {s}, "oversize");
    auto bad = csv; bad.replace("noLoadLoss_W", "noLoadLoss_kW"); reject(bad,{s},"units exact");
    bad = csv; bad.replace("sample_id", "source"); reject(bad,{s},"duplicate missing headers");
    bad = csv; bad.replace("'00012345678901234567890", "'unknown"); reject(bad,{s},"unknown IDs");
    auto stale = s; stale.source += "modified"; reject(csv,{stale},"snapshot includes metadata");
    stale = s; stale.input.lvTurns += 1;
    Sample recalculated;
    check(makeSample(stale.input,recalculated,error),"changed input baseline fixture");
    stale.baseline=recalculated.baseline;
    reject(csv,{stale},"snapshot includes inputs");
    stale = s; stale.measured[0] = 99; reject(csv,{stale},"snapshot includes labels");
    stale = s; stale.purpose = "excluded"; reject(csv,{stale},"snapshot includes role");
    bad = csv; bad.replace("12.345678901234567", "NaN"); reject(bad,{s},"nonfinite numeric");
    for (auto value : {"1,2", "1e999", "=1+1", "-1", "12junk"}) {
        bad = csv; bad.replace("12.345678901234567", value); reject(bad,{s},"bad numbers");
    }
    for (auto value : {"\" 12\"", "\"12 \"", "\"12\n\"", "\"12\r\n\"", "\"\t12\""}) {
        bad=csv; bad.replace("12.345678901234567",value);
        reject(bad,{s},"quoted numeric whitespace must be rejected including final newline");
    }
    reject(csvTemplate({s,s}),{s},"repeated CSV ID");
    reject(csv,{s,s},"duplicate current library cannot drop rows");
    reject(csv + "\"unterminated",{s},"malformed quote");
    bad=csv; bad.replace("\"'001\"", "\"'001\"junk"); reject(bad,{s},"characters after closing quote");
    bad=csv; bad.replace("'2026-10-23", "'2026-02-30"); reject(bad,{s},"invalid calendar date");
    auto other = s; other.id = "second"; other.testedOn = other.testedOn.addDays(1);
    other.productId=" 001 ";
    other.purpose = "independent_test";
    reject(csv,{s,other},"merged group roles checked");
    other.purpose = "training";
    bad=csv; bad.replace("'training","'independent_test");
    reject(bad,{s,other},"single-row purpose edit cannot split untouched product group");
    check(!inspect({s,other})["trainingReady"].toBool()
          && inspect({s,other})["groups"].toObject()["training"].toInt()==1,"two rows are one canonical product");
    QVector<Sample> samples;
    for (int i=0;i<5;++i) { auto row=s; row.id=QString::number(i); row.productId=QString::number(i); for(auto &m:row.measured)m=20; samples.append(row); }
    auto report=inspect(samples);
    check(report["trainingReady"].toBool() && report["trainingContextCount"].toInt()==1,"five complete products ready");
    check(report["groups"].toObject()["training"].toInt()==5,"group count");
    auto variable=samples; variable[0].input.lvTurns+=1;
    check(inspect(variable)["trainingContextCount"].toInt()==1,"whitelisted feature does not split context");
    auto unassigned=s; unassigned.purpose="unassigned";
    check(!inspect({unassigned})["trainingReady"].toBool(),"unassigned never trains");
    check(!inspect({})["trainingReady"].toBool(),"empty library not ready");
    auto repeated=samples;
    for(int i=0;i<5;++i) { auto row=samples[0]; row.id="repeat"+QString::number(i); row.testedOn=row.testedOn.addDays(i+1); repeated.append(row); }
    report=inspect(repeated);
    check(report["targets"].toArray()[0].toObject()["trainingSamples"].toInt()==10
          && report["targets"].toArray()[0].toObject()["trainingProducts"].toInt()==5,"unique products distinct from row counts");
    samples[0].measured[0].reset(); report=inspect(samples);
    check(!report["trainingReady"].toBool() && report["targets"].toArray()[0].toObject()["missingTrainingSamples"].toInt()==1,"missing not zero");
    samples[0].measured[0]=20; samples[0].input.waveDepth_mm+=1;
    check(makeSample(samples[0].input,recalculated,error),"physical context recalculated fixture");
    samples[0].baseline=recalculated.baseline;
    report=inspect(samples);
    check(!report["trainingReady"].toBool() && report["trainingContextCount"].toInt()==2,"physical context blocks");
    bool contextError=false;
    for(const auto &value:report["issues"].toArray()) { const auto issue=value.toObject();
        contextError |= issue["severity"]=="error" && issue["message"].toString().contains("上下文"); }
    check(contextError,"context conflict is an error, not advisory");
    samples[0]=samples[1]; check(!inspect(samples)["trainingReady"].toBool(),"duplicate invalid blocks");
    QTemporaryDir dir; const auto path=dir.filePath("samples.json");
    other=s; other.id="other"; other.productId="other";
    check(save(path,{s,other},error),"save fixture");
    check(previewCsv(csv,{s,other},p,error),"subset preview");
    auto editedCsv=csv; editedCsv.replace("''原文","'changed");
    check(previewCsv(editedCsv,{s,other},p,error) && p.replacements.size()==1
        && p.replacements[0].source=="changed" && s.source=="'原文","preview edits metadata without overwriting current snapshot");
    check(commitCsv(path,p,error),"commit delegates batch");
    QVector<Sample> loaded; check(load(path,loaded,error) && loaded.size()==2 && loaded[0].source=="changed","subset preserves other rows");
    check(!commitCsv(path,p,error),"stale commit rejected");
    if(loaded.size()==2) {
        CsvPreview batch;
        check(previewCsv(csvTemplate(loaded),loaded,batch,error),"two-row preview");
        auto changed=loaded; changed[1].source="concurrent second-row edit";
        check(save(path,changed,error),"concurrent writer fixture");
        if(batch.replacements.size()==2) batch.replacements[0].source="must not be written";
        check(!commitCsv(path,batch,error),"stale second row aborts whole batch");
        QVector<Sample> after; check(load(path,after,error) && after.size()==2
            && after[0].source=="changed" && after[1].source=="concurrent second-row edit","stale batch writes no first-row prefix");
        loaded=after;
    }
    check(previewCsv(csvTemplate(loaded),loaded,p,error),"refresh snapshot");
    QFile beforeFile(path); check(beforeFile.open(QIODevice::ReadOnly),"read saved bytes");
    const auto before=beforeFile.readAll(); beforeFile.close();
    QLockFile lock(path+".lock"); check(lock.tryLock(),"hold library lock");
    check(!commitCsv(path,p,error),"lock honored"); lock.unlock();
    reject(csvTemplate({s,other})+"broken,row\r\n",{s,other},"partial invalid clears entire preview");
    check(!commitCsv(path,p,error),"failed preview cannot commit");
    QFile afterFile(path); check(afterFile.open(QIODevice::ReadOnly) && afterFile.readAll()==before,"invalid preview and lock preserve bytes");
    qInfo()<<"data preparation failures:"<<failures;
    return failures ? 1 : 0;
}
