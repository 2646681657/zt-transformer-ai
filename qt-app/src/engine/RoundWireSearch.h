#ifndef ROUNDWIRESEARCH_H
#define ROUNDWIRESEARCH_H

#include "IOptimizer.h"
#include "DesignDatabase.h"
#include <algorithm>

// GUI线程准备当前运行的表内直径快照。配置与启动共用，不能静默取近似档。
inline void prepareRoundWireSearch(OptimizationSettings &settings, const CalcInput &base)
{
    settings.hvRoundWireDiameters.clear();
    settings.hvRoundWireSelectionError.clear();
    if (!settings.searchHvRoundWire) return;
    const auto fail = [&settings](const QString &error) { settings.hvRoundWireSelectionError = error; };
    if (!base.isRoundHighVoltageWire()) {
        fail(QStringLiteral("当前为扁线，不能启用圆线规格寻优，请取消圆线参与或回设计输入选择有效圆线规格。"));
        return;
    }
    if (settings.hvRoundWireRange < 0 || settings.hvRoundWireRange > 5) {
        fail(QStringLiteral("圆线搜索范围须为0至5档。"));
        return;
    }
    auto &db = DesignDatabase::instance();
    if (!db.isLoaded() && !db.load()) {
        fail(QStringLiteral("圆线规格表加载失败：") + db.lastError());
        return;
    }
    WireSpec spec;
    // roundWireSpec同时检查整个表的完整、升序、唯一、增厚及重量数据。
    if (!db.roundWireSpec(base.hvBareWidth_mm, spec)) {
        fail(QStringLiteral("当前圆线规格未收录或规格表无效，不能生成寻优候选；不插值、不向下取档。"));
        return;
    }
    const auto &rows = db.wireSpecs();
    int center = -1;
    for (int i = 0; i < rows.size(); ++i)
        if (rows[i].bareWidthMm == base.hvBareWidth_mm) { center = i; break; }
    if (center < 0) {
        fail(QStringLiteral("圆线基准未精确匹配规格表。"));
        return;
    }
    const int first = std::max(0, center - settings.hvRoundWireRange);
    const int last = std::min(int(rows.size()) - 1, center + settings.hvRoundWireRange);
    for (int i = first; i <= last; ++i)
        settings.hvRoundWireDiameters.append(rows[i].bareWidthMm);
}

#endif
