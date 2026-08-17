# -*- coding: utf-8 -*-
"""计算用数据表.xls 结构概览：sheet 列表、尺寸、合并单元格"""
import sys
import xlrd

sys.stdout.reconfigure(encoding="utf-8")

PATH = r"D:\zt-transformer\zt-transformer-ai-main\计算用数据表.xls"

wb = xlrd.open_workbook(PATH, formatting_info=False)
print(f"工作表数量: {wb.nsheets}")
for sh in wb.sheets():
    print(f"\n### Sheet: {sh.name!r}")
    print(f"    行数: {sh.nrows}  列数: {sh.ncols}")
    # 打印前 3 行预览
    for r in range(min(3, sh.nrows)):
        vals = []
        for c in range(min(15, sh.ncols)):
            v = sh.cell_value(r, c)
            if v != "":
                vals.append(f"{xlrd.colname(c)}{r+1}={v!r}")
        print(f"    行{r+1}: {' | '.join(vals) if vals else '(空)'}")
