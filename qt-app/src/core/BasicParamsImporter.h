#ifndef BASICPARAMSIMPORTER_H
#define BASICPARAMSIMPORTER_H
// 基础技术参数表导入（需求附件1）：从 Excel/CSV 文件提取变压器额定参数
// 解析策略：通用「参数名关键词 → 同行右邻单元格值」扫描，不依赖固定模板；
// Excel（xlsx/xlsm/xls）通过 PowerShell COM 转为 UTF-8 CSV 后统一解析，
// 未安装 Office 的环境可直接导入 CSV 文件

#include <QString>
#include <QStringList>
#include "TransformerParams.h"
#include "CalcInput.h"

class BasicParamsImporter {
public:
    // 从文件导入：成功识别到至少一项参数返回 true
    // report 返回逐项导入明细（参数名 = 值）与未识别项提示
    static bool importFromFile(const QString &path, TransformerParams &params,
                               CalcInput &input, QString *report = nullptr);

private:
    // 解析文本表格行集合（CSV 兼容逗号/制表符分隔），提取参数写入 params/input
    // 返回 (成功项数, 明细行列表)
    static QPair<int, QStringList> parseRows(const QStringList &rows,
                                             TransformerParams &params, CalcInput &input);
    // 数值提取：取文本中第一段数字（支持小数/负号），失败返回 ok=false
    static double extractNumber(const QString &text, bool *ok = nullptr);
    // Excel → CSV（PowerShell COM，本机需装 Office）；成功返回临时 CSV 路径
    static QString excelToCsv(const QString &excelPath, QString *error = nullptr);
};

#endif // BASICPARAMSIMPORTER_H
