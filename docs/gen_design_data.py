# -*- coding: utf-8 -*-
"""
从 计算用数据表.xls 提取设计基础数据，生成 qt-app/resources/data/design_data.json
供程序 Adapter 层加载（替代 Excel LOOKUP 查表）。

数据来源与计算单公式对齐：
- 硅钢性能曲线  能效与硅钢性能!A34:AL120（VA/kg 列按计算单公式实际引用取值：
    18SQGD065/20R070/23R080/23RK085 共用 R 列，27RK090 用 W，27ZH100 用 AB，默认用 AD）
- 性能标准值    Sheet1!A41:F60（容量→空载电流，文本数字转 float）
- 铁芯叠积对照  Sheet1!A125:P209（参考直径→各级片宽 B..L + 轭片 M..P）
- 波纹油箱系数  Sheet1!U30:W55（波纹深→Ks/Kp）
- 线规表        Sheet1!N49:Q105（序号→裸线宽/绝缘宽/加重量%）
- 储油柜规格    Sheet1!A214:F250（原始行）
- 片散规格      Sheet1!C255:P291（中心距×片宽→单片散热面积）
"""
import json
import sys
import xlrd

sys.stdout.reconfigure(encoding="utf-8")

SRC = r"D:\zt-transformer\zt-transformer-ai-main\计算用数据表.xls"
DST = r"D:\zt-transformer\zt-transformer-ai-main\qt-app\resources\data\design_data.json"

wb = xlrd.open_workbook(SRC, formatting_info=False)
eff = wb.sheet_by_name("能效与硅钢性能")
s1 = wb.sheet_by_name("Sheet1")


def cell(sh, r, c):
    """取数值，文本型数字自动转换，空/非数值返回 None"""
    v = sh.cell_value(r, c)
    t = sh.cell_type(r, c)
    if v == "" or v is None:
        return None
    if t == xlrd.XL_CELL_NUMBER:
        return round(v, 10)
    if t == xlrd.XL_CELL_TEXT:
        try:
            return round(float(v), 10)
        except ValueError:
            return v  # 保留文本（如 'φ180'）
    return None


def col(name):
    """列字母→0基索引"""
    idx = 0
    for ch in name:
        idx = idx * 26 + (ord(ch) - 64)
    return idx - 1


# ---------- 1. 硅钢性能曲线 ----------
# 列映射（与计算单公式一致）：grade -> (磁密列, W/kg列, VA/kg列)
STEEL_MAP = {
    "18SQGD065": ("A", "B", "R"),
    "20R070": ("F", "G", "R"),
    "23R080": ("K", "L", "R"),
    "23RK085": ("P", "Q", "R"),
    "27RK090": ("U", "V", "W"),
    "27ZH100": ("Z", "AA", "AB"),
    "30Q120": ("Z", "AG", "AD"),  # 公式默认分支
}
steel = []
for grade, (cb, cw, cva) in STEEL_MAP.items():
    pts = []
    for r in range(33, 120):  # 行34..120
        b = cell(eff, r, col(cb))
        w = cell(eff, r, col(cw))
        va = cell(eff, r, col(cva))
        if b is None or w is None:
            continue
        pts.append({"b": b, "wPerKg": w, "vaPerKg": va, "index": r - 32})  # 对应 AL 列序号(行34→1)
    steel.append({"grade": grade, "points": pts})

# 接缝磁化容量（AK 列）与序号（AL 列）
seam = []
for r in range(33, 120):
    va = cell(eff, r, col("AK"))
    idx = cell(eff, r, col("AL"))
    if va is not None and idx is not None:
        seam.append({"index": int(idx), "vaPerCm2": va})

# ---------- 2. 性能标准值（容量→空载电流） ----------
perf = []
for r in range(40, 60):  # 行41..60
    cap = cell(s1, r, col("A"))
    io = cell(s1, r, col("F"))
    if cap is None or io is None:
        continue
    perf.append({"capacityKVA": cap, "noLoadCurrentPct": io})

# ---------- 3. 铁芯叠积对照表 ----------
core = []
for r in range(124, 209):  # 行125..209
    ref = cell(s1, r, col("A"))
    if ref is None:
        continue
    widths = [cell(s1, r, c) for c in range(col("B"), col("L") + 1)]  # B..L 11级片宽
    yoke = [cell(s1, r, c) for c in range(col("M"), col("P") + 1)]  # M..P 轭片基数
    core.append({
        "ref": ref,
        "widths": [0 if w is None else w for w in widths],
        "yokeBase": [0 if y is None else y for y in yoke],
    })

# ---------- 4. 波纹油箱系数 ----------
corr = []
for r in range(29, 55):  # 行30..55
    depth = cell(s1, r, col("U"))
    ks = cell(s1, r, col("V"))
    kp = cell(s1, r, col("W"))
    if depth is None or ks is None:
        continue
    corr.append({"depthMm": depth, "ks": ks, "kp": kp})

# ---------- 5. 线规表 ----------
wires = []
for r in range(48, 105):  # 行49..105
    n = cell(s1, r, col("N"))
    o = cell(s1, r, col("O"))
    p = cell(s1, r, col("P"))
    q = cell(s1, r, col("Q"))
    if o is None:
        continue
    wires.append({
        "index": int(n) if n is not None else None,
        "bareWidthMm": o,      # 裸线宽
        "insulatedWidthMm": p,  # 绝缘后宽
        "weightAddPct": q,      # 导线加重量 %
    })

# ---------- 6. 储油柜规格（原始行） ----------
conservators = []
for r in range(213, 250):  # 行214..250
    a, b = cell(s1, r, col("A")), cell(s1, r, col("B"))
    if a is None and b is None:
        continue
    conservators.append({
        "no": a,
        "diameter": b,       # 'φ180' 等文本
        "height": cell(s1, r, col("C")),
        "oilWeightKg": cell(s1, r, col("D")),
        "colE": cell(s1, r, col("E")),
        "colF": cell(s1, r, col("F")),
    })

# ---------- 7. 片散规格（中心距×片宽→面积） ----------
rad_widths = [cell(s1, 254, c) for c in range(col("C"), col("P") + 1)]
radiators = []
for r in range(255, 291):  # 行256..291
    cd = cell(s1, r, col("C"))
    if cd is None:
        continue
    areas = [cell(s1, r, c) for c in range(col("D"), col("P") + 1)]
    radiators.append({
        "centerDistMm": cd,
        "type": cell(s1, r, col("B")),  # SZ / SF
        "areasM2": [0 if a is None else a for a in areas],
    })

data = {
    "meta": {
        "source": "计算用数据表.xls",
        "note": "由 docs/gen_design_data.py 生成，列映射与 SB20 计算单公式对齐",
    },
    "siliconSteel": {"grades": steel, "seamMagnetization": seam},
    "performanceStandards": perf,
    "coreLaminations": core,
    "corrugatedTankCoefficients": corr,
    "wireSpecs": wires,
    "conservators": conservators,
    "radiators": {"widthsMm": rad_widths, "rows": radiators},
}

with open(DST, "w", encoding="utf-8") as f:
    json.dump(data, f, ensure_ascii=False, indent=2)

print(f"OK -> {DST}")
print(f"  硅钢牌号: {len(steel)} 曲线点数 {[len(g['points']) for g in steel]}")
print(f"  性能标准: {len(perf)} 项")
print(f"  铁芯对照: {len(core)} 行")
print(f"  波纹系数: {len(corr)} 项")
print(f"  线规: {len(wires)} 项")
print(f"  储油柜: {len(conservators)} 项")
print(f"  片散: {len(radiators)} 行")
