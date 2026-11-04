#ifndef STARTUPSCHEMELIBRARY_H
#define STARTUPSCHEMELIBRARY_H
#include "SchemeStore.h"
#include <QSaveFile>
#include <QSet>
#include <QUuid>

namespace StartupSchemeLibrary {
struct Entry { QString id,name; CalcInput input; StructureConfig config; bool builtin=false; };
struct State { QVector<Entry> entries; QString selectedId=QStringLiteral("builtin-base"); };
inline QString path() { return SchemeStore::storePath(QStringLiteral("startup_schemes.json")); }
inline State initial(const CalcInput &base) {
    State state;
    state.entries.append({"builtin-base",QStringLiteral("内置基准方案｜SB20-630kVA"),base,{},true});
    QFile resource(QStringLiteral(":/templates/startup_schemes.json"));
    if(resource.open(QIODevice::ReadOnly)) {
        for(const auto &value:QJsonDocument::fromJson(resource.readAll()).array()) {
            const auto o=value.toObject();
            state.entries.append({o["id"].toString(),o["name"].toString(),SchemeStore::fromJson(o["input"].toObject()),{},true});
        }
    }
    return state;
}
inline QJsonObject encode(const StructureConfig &c) {
    return {{"category",int(c.category)},{"process",int(c.windingProcess)},
        {"mode",int(c.calcMode)},{"core",int(c.coreType)},{"shape",int(c.coreShape)},
        {"yoke",int(c.yokeShape)},{"winding",int(c.windingForm)},{"coil",int(c.hvCoilStructure)}};
}
inline bool valid(const State &state,QString &error) {
    QSet<QString> ids,names;
    for(const auto &e:state.entries) {
        const auto c=encode(e.config);
        bool configValid=true;
        for(const auto &key:{"category","process","mode","yoke","winding","coil"})
            configValid &= c[key].toInt()>=0 && c[key].toInt()<=1;
        configValid &= c["core"].toInt()>=0 && c["core"].toInt()<=2
            && c["shape"].toInt()>=0 && c["shape"].toInt()<=4;
        if(e.id.isEmpty()||e.name.trimmed().isEmpty()||ids.contains(e.id)||names.contains(e.name)
           ||!e.input.hasPerformanceCriteria||!configValid
           ||e.config.coreShape!=e.input.coreShape||e.config.yokeShape!=e.input.yokeShape
           ||int(e.config.hvCoilStructure)+1!=e.input.hvCoilFormIdx) {
            error=QStringLiteral("默认方案内容不完整或选型不一致，请重新保存。");return false;
        }
        ids.insert(e.id);names.insert(e.name);
    }
    if(!ids.contains("builtin-base")||!ids.contains(state.selectedId)) {
        error=QStringLiteral("启动默认方案不存在。");return false;
    }
    return true;
}
inline bool write(const QString &fileName,const State &state,QString &error) {
    error.clear();if(!valid(state,error))return false;
    QJsonArray entries;
    for(const auto &e:state.entries)entries.append(QJsonObject{{"id",e.id},{"name",e.name},
        {"input",SchemeStore::toJson(e.input)},{"config",encode(e.config)},{"builtin",e.builtin}});
    QSaveFile file(fileName);
    if(!file.open(QIODevice::WriteOnly)){error=file.errorString();return false;}
    const auto data=QJsonDocument(QJsonObject{{"format","ZTBLD-StartupSchemes"},{"version",1},
        {"selectedId",state.selectedId},{"entries",entries}}).toJson();
    if(file.write(data)!=data.size()||!file.commit()){error=file.errorString();return false;}
    return true;
}
inline bool read(const QString &fileName,const CalcInput &base,State &state,QString &error) {
    state=initial(base);error.clear();
    if(!QFile::exists(fileName))return true;
    QFile file(fileName);if(!file.open(QIODevice::ReadOnly)){error=file.errorString();return false;}
    QJsonParseError parse;const auto doc=QJsonDocument::fromJson(file.readAll(),&parse);const auto root=doc.object();
    if(parse.error!=QJsonParseError::NoError||root["format"].toString()!="ZTBLD-StartupSchemes"
       ||root["version"].toInt()!=1||!root["entries"].isArray()) {
        error=QStringLiteral("默认方案库无法读取，已恢复内置基准方案；原文件保留。");return false;
    }
    State loaded;loaded.selectedId=root["selectedId"].toString();
    for(const auto &v:root["entries"].toArray()) {
        const auto o=v.toObject();const auto c=o["config"].toObject();StructureConfig config;
        const auto fallback=encode(config);
        for(auto it=fallback.begin();it!=fallback.end();++it)if(!c[it.key()].isDouble()) {
            error=QStringLiteral("默认方案的结构字段缺失或损坏，已恢复内置基准方案。");return false;
        }
        config.category=StructureConfig::TransformerCategory(c["category"].toInt(-1));
        config.windingProcess=StructureConfig::WindingProcess(c["process"].toInt(-1));
        config.calcMode=StructureConfig::CalcMode(c["mode"].toInt(-1));
        config.coreType=StructureConfig::CoreType(c["core"].toInt(-1));
        config.coreShape=StructureConfig::CoreShape(c["shape"].toInt(-1));
        config.yokeShape=StructureConfig::YokeShape(c["yoke"].toInt(-1));
        config.windingForm=StructureConfig::WindingForm(c["winding"].toInt(-1));
        config.hvCoilStructure=StructureConfig::HvCoilStructure(c["coil"].toInt(-1));
        loaded.entries.append({o["id"].toString(),o["name"].toString(),SchemeStore::fromJson(o["input"].toObject()),config,o["builtin"].toBool()});
    }
    if(!valid(loaded,error))return false;
    state=loaded;return true;
}
inline bool add(State &state,const QString &name,const CalcInput &input,const StructureConfig &config,bool active,QString &error) {
    error.clear();for(const auto &e:state.entries)if(e.name==name.trimmed()){error=QStringLiteral("该名称已存在，请使用其他名称。");return false;}
    auto next=state;const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    next.entries.append({id,name.trimmed(),input,config,false});if(active)next.selectedId=id;
    if(!valid(next,error))return false;state=next;return true;
}
inline bool remove(State &state,const QString &id,QString &error) {
    error.clear();for(int i=0;i<state.entries.size();++i)if(state.entries[i].id==id) {
        if(state.entries[i].builtin){error=QStringLiteral("内置方案保留用于恢复，不能删除。");return false;}
        state.entries.removeAt(i);if(state.selectedId==id)state.selectedId="builtin-base";return true;
    }
    error=QStringLiteral("方案不存在。");return false;
}
}
#endif
