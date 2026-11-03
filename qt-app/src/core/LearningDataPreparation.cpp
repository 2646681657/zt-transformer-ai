#include "LearningDataPreparation.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>
#include <cmath>

namespace LearningPreparation {
namespace {
using SelfLearning::Sample;
QString group(const Sample &s) { return s.productId.trimmed().toCaseFolded(); }
QString hash(const Sample &s) {
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(SelfLearning::encode(s)).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}
QStringList headers() {
    return {"sample_id","snapshot_sha256","product_id","scheme_name","source","tested_on","purpose",
            "noLoadLoss_W","loadLoss_W","impedance_pct","oilTopRise_K","hvWindingRise_K","lvWindingRise_K"};
}
QJsonObject context(const Sample &s) {
    // Keep exactly aligned with learning/trainer.py FEATURES / CONTEXT_EXCLUSIONS.
    static const QStringList variable = {
        "capacity_kVA","hvRated_kV","lvRated_kV","coreDiameter_mm","coreStraight_mm",
        "stackFactor","steelThickness_mm","hvBareWidth_mm","hvBareThick_mm","lvTurns",
        "lvFoilThick_mm","lvFoilWidth_mm","mainDuctWidth_mm","refFluxDens_T","hvCopperWire","lvCopperFoil",
        "materialPrices","steelPricePerKg","useCustomSteelPrice","steelPriceGrade","performanceCriteria",
        "craftConstraints","legacyUnitLossRecordText","legacyCraftCoefRecordText"};
    auto object = SelfLearning::encode(s)["input"].toObject();
    for (const auto &key : variable) object.remove(key);
    return object;
}
QString quoted(QString text) { text.replace('"', "\"\""); return '"' + text + '"'; }
QString unprefix(QString text) { if (text.startsWith('\'')) text.remove(0,1); return text; }
struct Row { QStringList cells; int line; };
// Strict four-state RFC4180 parser, retaining physical start lines for diagnostics.
bool parse(const QString &text, QVector<Row> &rows, QString &error) {
    enum State { Start, Bare, Quoted, Closed }; State state = Start;
    QString cell; QStringList cells; int line=1, start=1;
    bool pending=false;
    auto finishCell=[&] { cells.append(cell); cell.clear(); state=Start; };
    for (qsizetype i=0; i<text.size(); ++i) {
        const QChar ch=text[i]; pending=true;
        if (state==Quoted) {
            if(ch=='"') state=Closed;
            else { cell+=ch; if(ch=='\n') ++line; }
            continue;
        }
        if (state==Closed && ch=='"') { cell+='"'; state=Quoted; continue; }
        if (ch==',') { finishCell(); continue; }
        if (ch=='\r' || ch=='\n') {
            if(ch=='\r') {
                if(i+1>=text.size() || text[i+1]!='\n') { error=QString("第%1行：CSV 裸回车无效").arg(line); return false; }
                ++i;
            }
            finishCell(); rows.append({cells,start}); cells.clear(); ++line; start=line; pending=false; continue;
        }
        if (state==Closed || (state==Bare && ch=='"')) { error=QString("第%1行：CSV 引号格式错误").arg(line); return false; }
        if(state==Start && ch=='"') state=Quoted;
        else { cell+=ch; state=Bare; }
    }
    if(state==Quoted) { error=QString("第%1行：CSV 引号未闭合").arg(start); return false; }
    if(pending) { finishCell(); rows.append({cells,start}); }
    return true;
}
}

QJsonObject inspect(const QVector<Sample> &samples) {
    QJsonArray issues, targets;
    auto issue=[&](const QString &severity,const Sample *s,const QString &message) {
        issues.append(QJsonObject{{"severity",severity},{"productId",s?s->productId:QString()},
                                 {"sampleId",s?s->id:QString()},{"message",message}});
    };
    QString error; bool valid=SelfLearning::validateDataset(samples,error);
    if(!valid) issue("error",nullptr,error);
    QMap<QString,QSet<QString>> groups;
    for(const auto &purpose : {"unassigned","training","independent_test","excluded"}) groups[purpose];
    QSet<QByteArray> contexts;
    QMap<QString,QString> roles;
    QSet<QString> ids, tests;
    for(const auto &s:samples) {
        QString rowValidation;
        if(!SelfLearning::validateDataset({s},rowValidation)) issue("error",&s,rowValidation);
        const auto product=group(s);
        groups[s.purpose].insert(product);
        if(ids.contains(s.id)) issue("error",&s,"重复 sample_id");
        ids.insert(s.id);
        const auto test=QString::fromUtf8(QJsonDocument(QJsonArray{product,s.testedOn.toString(Qt::ISODate)}).toJson(QJsonDocument::Compact));
        if(tests.contains(test)) issue("error",&s,"同一产品和 tested_on 重复");
        tests.insert(test);
        if(roles.contains(product) && roles[product]!=s.purpose) issue("error",&s,"同一产品用途不一致");
        roles[product]=s.purpose;
        if(s.purpose=="unassigned") issue("warning",&s,"用途未分配");
        for(int i=0;i<SelfLearning::MetricCount;++i)
            if(!s.measured[i]) issue("warning",&s,QString("%1 缺少实测标签").arg(SelfLearning::metrics()[i].key));
        if(s.purpose=="training") {
            contexts.insert(QJsonDocument(context(s)).toJson(QJsonDocument::Compact));
            if(contexts.size()>1) issue("error",&s,"训练样本固定物理上下文不一致，请人工分组");
        }
    }
    bool ready=valid && !samples.isEmpty() && contexts.size()==1;
    for(int i=0;i<SelfLearning::MetricCount;++i) {
        int tr=0,te=0,missing=0; QSet<QString> trGroups,teGroups;
        for(const auto &s:samples) {
            if(s.purpose=="training") { if(s.measured[i]) { ++tr; trGroups.insert(group(s)); } else ++missing; }
            if(s.purpose=="independent_test" && s.measured[i]) { ++te; teGroups.insert(group(s)); }
        }
        const auto &m=SelfLearning::metrics()[i];
        const bool targetReady=valid && contexts.size()==1 && trGroups.size()>=5;
        ready=ready && targetReady;
        targets.append(QJsonObject{{"key",m.key},{"name",m.name},{"unit",m.unit},
            {"trainingSamples",tr},{"trainingProducts",int(trGroups.size())},{"testSamples",te},
            {"testProducts",int(teGroups.size())},{"missingTrainingSamples",missing},{"ready",targetReady}});
        if(trGroups.size()<5) issue("warning",nullptr,QString("%1 有标签训练产品不足 5 个；就绪状态不代表精度").arg(m.key));
    }
    QJsonObject counts;
    for(auto it=groups.cbegin();it!=groups.cend();++it) counts[it.key()]=int(it.value().size());
    return {{"groups",counts},{"targets",targets},{"trainingContextCount",int(contexts.size())},
            {"trainingReady",ready},{"issues",issues}};
}

QByteArray csvTemplate(const QVector<Sample> &samples) {
    QString text=headers().join(',')+"\r\n";
    for(const auto &s:samples) {
        QStringList row;
        for(const auto &value : QStringList{s.id,hash(s),s.productId,s.schemeName,s.source,s.testedOn.toString(Qt::ISODate),s.purpose})
            row.append(quoted("'"+value));
        for(const auto &value:s.measured) row.append(value?QString::number(*value,'g',17):QString());
        text+=row.join(',')+"\r\n";
    }
    return QByteArray::fromHex("efbbbf")+text.toUtf8();
}

bool previewCsv(const QByteArray &bytes, const QVector<Sample> &current, CsvPreview &preview, QString &error) {
    preview={}; error.clear();
    auto fail=[&](const QString &message) {
        preview.errors.append(message); preview.originals.clear(); preview.replacements.clear();
        error=preview.errors.join('\n'); return false;
    };
    if(bytes.size()>16*1024*1024) return fail("第1行：CSV 超过 16 MiB");
    QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    QString text=decoder.decode(bytes);
    if(decoder.hasError()) return fail("第1行：CSV 不是有效 UTF-8");
    if(text.startsWith(QChar(0xfeff))) text.remove(0,1);
    QVector<Row> rows; QString parseError;
    if(!parse(text,rows,parseError)) return fail(parseError);
    if(rows.isEmpty()) return fail("第1行：缺少 CSV 表头");
    const auto required=headers(); const auto supplied=rows[0].cells;
    QMap<QString,int> columns;
    for(int i=0;i<supplied.size();++i) {
        const auto &key=supplied[i];
        if(!required.contains(key)) preview.errors.append(QString("第1行，列 %1：未知表头或单位不匹配").arg(key));
        if(columns.contains(key)) preview.errors.append(QString("第1行，列 %1：重复表头").arg(key));
        columns[key]=i;
    }
    for(const auto &key:required) if(!columns.contains(key)) preview.errors.append(QString("第1行，列 %1：缺少表头").arg(key));
    if(!preview.errors.isEmpty()) return fail("第1行：表头校验失败");
    QString validation;
    if(!SelfLearning::validateDataset(current,validation)) return fail("当前样本库无效："+validation);
    QMap<QString,int> byId;
    for(int i=0;i<current.size();++i) {
        if(byId.contains(current[i].id)) return fail("当前样本库存在重复 sample_id");
        byId[current[i].id]=i;
    }
    QVector<Sample> merged=current;
    QSet<QString> seen;
    static const QRegularExpression number("\\A[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?\\z");
    for(int n=1;n<rows.size();++n) {
        const auto &row=rows[n];
        auto rowError=[&](const QString &column,const QString &message) { preview.errors.append(QString("第%1行，列 %2：%3").arg(row.line).arg(column,message)); };
        if(row.cells.size()!=required.size()) { rowError("CSV","列数不匹配"); continue; }
        auto cell=[&](const QString &key) { return row.cells[columns[key]]; };
        const auto id=unprefix(cell("sample_id"));
        if(seen.contains(id)) { rowError("sample_id","重复样本 ID"); continue; }
        seen.insert(id);
        if(!byId.contains(id)) { rowError("sample_id","未知样本，不能创建设计"); continue; }
        const auto &original=current[byId[id]];
        if(unprefix(cell("snapshot_sha256"))!=hash(original)) { rowError("snapshot_sha256","样本快照已变化，请重新导出"); continue; }
        Sample replacement=original;
        replacement.productId=unprefix(cell("product_id")); replacement.schemeName=unprefix(cell("scheme_name"));
        replacement.source=unprefix(cell("source")); replacement.purpose=unprefix(cell("purpose"));
        const auto date=unprefix(cell("tested_on"));
        replacement.testedOn=QDate::fromString(date,Qt::ISODate);
        if(!replacement.testedOn.isValid() || date.size()!=10 || replacement.testedOn.toString(Qt::ISODate)!=date) rowError("tested_on","必须为有效 YYYY-MM-DD 日期");
        if(!QStringList{"unassigned","training","independent_test","excluded"}.contains(replacement.purpose)) rowError("purpose","用途必须为规范代码");
        for(int i=0;i<SelfLearning::MetricCount;++i) {
            const QString key=SelfLearning::metrics()[i].key; const auto value=cell(key);
            if(value.isEmpty()) { replacement.measured[i].reset(); continue; }
            bool ok=false; const double d=QLocale::c().toDouble(value,&ok);
            if(!number.match(value).hasMatch() || !ok || !std::isfinite(d)) { rowError(key,"必须为有限 C 区域数值或空值"); continue; }
            replacement.measured[i]=d;
        }
        QString singleError;
        if(!SelfLearning::validateDataset({replacement},singleError)) rowError("product_id/source/tested_on/purpose/实测列",singleError);
        merged[byId[id]]=replacement;
        preview.originals.append(original); preview.replacements.append(replacement);
    }
    if(rows.size()==1) return fail("第2行：没有样本数据");
    if(!SelfLearning::validateDataset(merged,validation)) {
        // Include CSV line references even when a conflict involves an untouched library row.
        for(int n=1;n<rows.size();++n) preview.errors.append(QString("第%1行，列 product_id/tested_on/purpose：合并样本库校验失败：%2").arg(rows[n].line).arg(validation));
    }
    if(!preview.errors.isEmpty()) { preview.originals.clear(); preview.replacements.clear(); error=preview.errors.join('\n'); return false; }
    return true;
}

bool commitCsv(const QString &path,const CsvPreview &preview,QString &error) {
    error.clear();
    if(!preview.errors.isEmpty() || preview.replacements.isEmpty() || preview.originals.size()!=preview.replacements.size()) {
        error="CSV 预览无效或包含错误，不能提交"; return false;
    }
    return SelfLearning::updateBatch(path,preview.replacements,preview.originals,error);
}
}
