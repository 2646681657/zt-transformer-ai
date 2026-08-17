# -*- coding: utf-8 -*-
"""完整导出 计算用数据表.xls 的关键区域（供公式翻译对拍）"""
import sys
import xlrd

sys.stdout.reconfigure(encoding="utf-8")

PATH = r"D:\zt-transformer\zt-transformer-ai-main\计算用数据表.xls"
wb = xlrd.open_workbook(PATH, formatting_info=False)


def col_letter(idx):
    s = ""
    idx += 1
    while idx:
        idx, r = divmod(idx - 1, 26)
        s = chr(65 + r) + s
    return s


def dump_sheet(name, row_range=None, col_range=None):
    sh = wb.sheet_by_name(name)
    print("=" * 90)
    print(f"Sheet: {name}  ({sh.nrows}行 × {sh.ncols}列)")
    print("=" * 90)
    r0, r1 = row_range or (0, sh.nrows)
    c0, c1 = col_range or (0, sh.ncols)
    for r in range(r0, min(r1, sh.nrows)):
        for c in range(c0, min(c1, sh.ncols)):
            v = sh.cell_value(r, c)
            if v == "" or v is None:
                continue
            t = sh.cell_type(r, c)
            ts = {0: "E", 1: "S", 2: "N", 3: "D", 4: "B", 5: "E", 6: "E"}[t]
            print(f"{col_letter(c)}{r+1}\t{ts}\t{v}")


# 1. 能效与硅钢性能：全表（硅钢曲线 + 能效标准 + 波纹系数）
dump_sheet("能效与硅钢性能")

# 2. Sheet1 关键区域
print()
dump_sheet("Sheet1", row_range=(30, 60))    # 性能标准值 A41:F60
print()
dump_sheet("Sheet1", row_range=(46, 110))   # 线规表 N49:Q105
print()
dump_sheet("Sheet1", row_range=(122, 212))  # 铁芯直径→片宽/叠厚 A125:L209
print()
dump_sheet("Sheet1", row_range=(211, 250))  # 储油柜规格 A214:D250
print()
dump_sheet("Sheet1", row_range=(252, 291))  # 片散规格 C255:P291
