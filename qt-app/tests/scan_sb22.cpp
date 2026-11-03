// 临时扫描工具：在设计变量空间搜索满足 SB22-M/630 全部约束的组合
// 约束（GB 20052-2024 1级能效 630kVA：空载460/负载4460/总损耗4920，偏差+10%）：
//   空载≤506  负载≤4906  总损耗≤5412  阻抗[6.24,7.62]  油顶≤60  绕组≤65  空载电流≤0.78%
#include <QCoreApplication>
#include <QtGlobal>
#include "ElectromagneticEngine.h"
#include "CalcInput.h"
#include "CalcResult.h"
#include <cstdio>
#include <algorithm>

struct Hit {
    double D, S, sf, hw, ht, lt;
    double noLoad, load, total, z, oil, hv, lv, i0;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const double NOLOAD_MAX = 506.0, LOAD_MAX = 4906.0, TOTAL_MAX = 5412.0;
    const double Z_MIN = 6.24, Z_MAX = 7.62;
    const double OIL_MAX = 60.0, WIND_MAX = 65.0, I0_MAX = 0.78;

    QVector<Hit> hits;
    int tried = 0, calcOk = 0;

    const double Ds[]  = {170, 175, 180, 185, 190};
    const double Ss[]  = {103, 108, 113, 118};
    const double SFs[] = {0.95, 0.96, 0.97};
    const double HWs[] = {2.05, 2.24, 2.44, 2.65};
    const double HTs[] = {5.52, 6.0, 6.5};
    const double LTs[] = {1.35, 1.5, 1.6, 1.8};

    ElectromagneticEngine engine;
    for (double D : Ds)
    for (double S : Ss)
    for (double sf : SFs)
    for (double hw : HWs)
    for (double ht : HTs)
    for (double lt : LTs) {
        CalcInput in;   // 默认即 SB20-M-630-10 基准
        in.coreDiameter_mm = D;
        in.coreStraight_mm = S;
        in.stackFactor = sf;
        in.hvBareWidth_mm = hw;
        in.hvBareThick_mm = ht;
        in.lvFoilThick_mm = lt;
        ++tried;
        CalcResult r;
        if (!engine.calcElectromagnetic(in, r)) continue;
        ++calcOk;
        const double total = r.core.noLoadLoss_W + r.winding.loadLoss_W;
        if (r.core.noLoadLoss_W > NOLOAD_MAX) continue;
        if (r.winding.loadLoss_W > LOAD_MAX) continue;
        if (total > TOTAL_MAX) continue;
        if (r.impedance.impedance_pct < Z_MIN || r.impedance.impedance_pct > Z_MAX) continue;
        if (r.thermal.oilTopRise_K > OIL_MAX) continue;
        if (r.thermal.hvWindingRise_K > WIND_MAX || r.thermal.lvWindingRise_K > WIND_MAX) continue;
        if (r.core.noLoadCurrent_pct > I0_MAX) continue;
        hits.append({D, S, sf, hw, ht, lt, r.core.noLoadLoss_W, r.winding.loadLoss_W,
                     total, r.impedance.impedance_pct, r.thermal.oilTopRise_K,
                     r.thermal.hvWindingRise_K, r.thermal.lvWindingRise_K,
                     r.core.noLoadCurrent_pct});
    }

    std::sort(hits.begin(), hits.end(), [](const Hit &a, const Hit &b) { return a.total < b.total; });
    std::printf("尝试 %d 组合，计算成功 %d，满足 SB22-M/630 全部约束 %d 组\n",
                tried, calcOk, (int)hits.size());
    const int show = std::min(15, (int)hits.size());
    for (int i = 0; i < show; ++i) {
        const Hit &h = hits[i];
        std::printf("[%02d] D=%.0f 直线=%.0f 叠系=%.2f 高压线=%.2fx%.2f 低压箔厚=%.2f | "
                     "空载 %.0f 负载 %.0f 总 %.0f 阻抗 %.2f 油顶 %.1f 高绕 %.1f 低绕 %.1f I0 %.2f\n",
                     i + 1, h.D, h.S, h.sf, h.hw, h.ht, h.lt,
                     h.noLoad, h.load, h.total, h.z, h.oil, h.hv, h.lv, h.i0);
    }
    return hits.isEmpty() ? 1 : 0;
}
