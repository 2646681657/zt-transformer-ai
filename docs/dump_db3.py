# -*- coding: utf-8 -*-
"""查看各区域表头行，确认列映射"""
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


def dump_rows(sheet_name, rows):
    sh = wb.sheet_by_name(sheet_name)
    print(f"### {sheet_name} 行 {rows}")
    for r in rows:
        if r >= sh.nrows:
            continue
        for c in range(sh.ncols):
            v = sh.cell_value(r, c)
            if v != "" and v is not None:
                print(f"  {col_letter(c)}{r+1} = {v!r}")
        print("  ---")


# 能效与硅钢性能：曲线表头行（确认列-牌号映射）
dump_rows("能效与硅钢性能", [31, 32, 33])
# Sheet1: 线规表头 48-50行、储油柜表头 213-215、片散表头 254-257
dump_rows("Sheet1", [48, 49, 213, 214, 254, 255, 256])
# Sheet1: 铁芯表 A125 附近几行完整行（看列数）
sh = wb.sheet_by_name("Sheet1")
for r in [124, 125, 174, 175]:
    vals = []
    for c in range(min(sh.ncols, 40)):
        v = sh.cell_value(r, c)
        vals.append(f"{col_letter(c)}={v!r}" if v != "" else f"{col_letter(c)}=-")
    print(f"Sheet1 行{r+1}: {' '.join(vals)}")
