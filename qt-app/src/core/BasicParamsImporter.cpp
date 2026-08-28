#include "BasicParamsImporter.h"
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QRegularExpression>
#include <QDateTime>
#include <QStringConverter>

// 参数名匹配规则：关键词（含于单元格文本即命中）+ 排除词（命中则跳过）
struct ParamRule {
    const char *keys;        // 多个关键词用 | 分隔（任一包含即命中）
    const char *excludes;    // 排除关键词（| 分隔）
};

// 文本解码：UTF-8 BOM → UTF-8 严格校验 → 本地 ANSI（Excel 另存 CSV 常为 GBK）
static QString decodeBytes(const QByteArray &raw)
{
    if (raw.startsWith("\xEF\xBB\xBF"))
        return QString::fromUtf8(raw.constData() + 3, raw.size() - 3);
    QStringDecoder utf8(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
    const QString strict = utf8.decode(raw);   // EncodedData 隐式转 QString
    if (utf8.hasError())
        return QString::fromLocal8Bit(raw.constData(), raw.size());
    return strict;
}

double BasicParamsImporter::extractNumber(const QString &text, bool *ok)
{
    // 匹配第一段数字（整数或小数，允许前置正负号）
    static const QRegularExpression re(
        QStringLiteral("[-+]?\\d+(?:\\.\\d+)?"));
    const auto m = re.match(text);
    if (!m.hasMatch()) {
        if (ok) *ok = false;
        return 0.0;
    }
    if (ok) *ok = true;
    return m.capturedView(0).toDouble();
}

QPair<int, QStringList> BasicParamsImporter::parseRows(const QStringList &rows,
                                                       TransformerParams &params, CalcInput &input)
{
    int importedCount = 0;
    QStringList details;

    // 单元格文本是否命中规则（包含任一关键词且不含排除词）
    const auto hits = [](const QString &cell, const ParamRule &rule) {
        const QStringList excludes = QString::fromUtf8(rule.excludes)
                                         .split(QLatin1Char('|'), Qt::SkipEmptyParts);
        for (const auto &ex : excludes) {
            if (cell.contains(ex, Qt::CaseInsensitive))
                return false;
        }
        const QStringList keys = QString::fromUtf8(rule.keys)
                                     .split(QLatin1Char('|'), Qt::SkipEmptyParts);
        for (const auto &k : keys) {
            if (cell.contains(k, Qt::CaseInsensitive))
                return true;
        }
        return false;
    };

    for (const QString &line : rows) {
        // 兼容逗号/制表符分隔；空行跳过
        QStringList cells;
        if (line.contains(QLatin1Char('\t')))
            cells = line.split(QLatin1Char('\t'));
        else
            cells = line.split(QLatin1Char(','));
        if (cells.isEmpty())
            continue;

        // 在本行各单元格中找参数名（首个命中关键词的非空格），值取其右侧第一个非空格
        for (int c = 0; c < cells.size(); ++c) {
            const QString nameCell = cells[c].trimmed();
            if (nameCell.isEmpty() || nameCell.size() < 2)
                continue;

            // 依次尝试规则表（先具体后泛化，避免「高压线圈温升」被「高压电压」抢先）
            bool matched = false;
            const struct { ParamRule rule; } rules[] = {
                { { "高压线圈温升|高压绕组温升", "标准值" } },
                { { "低压线圈温升|低压绕组温升", "标准值" } },
                { { "油顶层温升|油面温升|顶层油温", "" } },
                { { "空载电流", "偏差|允差" } },
                { { "阻抗电压|短路阻抗", "偏差|允差|最小" } },
                { { "空载损耗", "偏差|允差" } },
                { { "负载损耗", "偏差|允差" } },
                { { "总损耗", "偏差|允差" } },
                { { "高压额定电压|高压电压|额定高压", "低压|分接|最高" } },
                { { "低压额定电压|低压电压|额定低压", "分接|最高" } },
                { { "容量|额定容量", "阻抗|损耗|电流" } },
                { { "联结组别|联接组别|连接组标|联结组标", "" } },
                { { "频率", "" } },
                { { "海拔", "" } },
                { { "环境温度|最高环境", "" } },
                { { "型号|产品型号", "前缀" } },
            };
            for (int r = 0; r < int(sizeof(rules) / sizeof(rules[0])); ++r) {
                if (!hits(nameCell, rules[r].rule))
                    continue;
                // 值 = 右侧第一个非空单元格
                QString valueCell;
                for (int v = c + 1; v < cells.size(); ++v) {
                    if (!cells[v].trimmed().isEmpty()) {
                        valueCell = cells[v].trimmed();
                        break;
                    }
                }
                if (valueCell.isEmpty())
                    break;

                bool applied = false;
                QString detail;
                switch (r) {
                case 0: { bool ok=false; const double v=extractNumber(valueCell,&ok);
                    if (ok && v>0) { params.hvCoilTempRise_K=v; detail=QStringLiteral("高压线圈温升限值 = %1 K").arg(v); applied=true; } break; }
                case 1: { bool ok=false; const double v=extractNumber(valueCell,&ok);
                    if (ok && v>0) { params.lvCoilTempRise_K=v; detail=QStringLiteral("低压线圈温升限值 = %1 K").arg(v); applied=true; } break; }
                case 2: { bool ok=false; const double v=extractNumber(valueCell,&ok);
                    if (ok && v>0) { params.oilTopTempRise_K=v; detail=QStringLiteral("油顶层温升限值 = %1 K").arg(v); applied=true; } break; }
                case 3: { bool ok=false; const double v=extractNumber(valueCell,&ok);
                    if (ok && v>0) { params.noLoadCurrentStd_pct=v; detail=QStringLiteral("空载电流标准值 = %1 %").arg(v); applied=true; } break; }
                case 4: { bool ok=false; const double v=extractNumber(valueCell,&ok);
                    if (ok && v>0) { params.impedanceVoltageStd_pct=v; detail=QStringLiteral("阻抗电压标准值 = %1 %").arg(v); applied=true; } break; }
                case 5: { bool ok=false; const double v=extractNumber(valueCell,&ok);
                    if (ok && v>0) { params.noLoadLossStd_W=v; detail=QStringLiteral("空载损耗标准值 = %1 W").arg(v); applied=true; } break; }
                case 6: { bool ok=false; const double v=extractNumber(valueCell,&ok);
                    if (ok && v>0) { params.loadLossStd_W=v; detail=QStringLiteral("负载损耗标准值 = %1 W").arg(v); applied=true; } break; }
                case 7: { bool ok=false; const double v=extractNumber(valueCell,&ok);
                    if (ok && v>0) { params.totalLossStd_W=v; detail=QStringLiteral("总损耗标准值 = %1 W").arg(v); applied=true; } break; }
                case 8: { bool ok=false; const double v=extractNumber(valueCell,&ok);
                    if (ok && v>0) { params.hvRatedVoltage_kV=v; input.hvRated_kV=v; detail=QStringLiteral("高压额定电压 = %1 kV").arg(v); applied=true; } break; }
                case 9: { bool ok=false; const double v=extractNumber(valueCell,&ok);
                    if (ok && v>0) { params.lvRatedVoltage_kV=v; input.lvRated_kV=v; detail=QStringLiteral("低压额定电压 = %1 kV").arg(v); applied=true; } break; }
                case 10: { bool ok=false; const double v=extractNumber(valueCell,&ok);
                    if (ok && v>0) { params.capacity_kVA=v; input.capacity_kVA=v; detail=QStringLiteral("容量 = %1 kVA").arg(v); applied=true; } break; }
                case 11: {
                    // 联结组别（Dyn11 / Yyn0 等字母数字组合）
                    static const QRegularExpression cgRe(QStringLiteral("[DYdy]{1,2}[ynYNzd]{1,3}\\d{1,2}"));
                    const auto m = cgRe.match(valueCell);
                    if (m.hasMatch()) { params.connectionGroup=m.captured(0).toUpper();
                        detail=QStringLiteral("联结组别 = %1").arg(params.connectionGroup); applied=true; } break; }
                case 12: { bool ok=false; const double v=extractNumber(valueCell,&ok);
                    if (ok && v>0) { params.frequency_Hz=int(v); detail=QStringLiteral("频率 = %1 Hz").arg(int(v)); applied=true; } break; }
                case 13: { bool ok=false; const double v=extractNumber(valueCell,&ok);
                    if (ok && v>=0) { params.maxAltitude_m=v; detail=QStringLiteral("最高海拔 = %1 m").arg(v); applied=true; } break; }
                case 14: { bool ok=false; const double v=extractNumber(valueCell,&ok);
                    if (ok) { params.maxAmbientTemp_C=v; detail=QStringLiteral("最高环境温度 = %1 ℃").arg(v); applied=true; } break; }
                case 15: {
                    if (valueCell.size() <= 32) { params.productModel=valueCell;
                        detail=QStringLiteral("产品型号 = %1").arg(valueCell); applied=true; } break; }
                }
                if (applied) {
                    details << detail;
                    ++importedCount;
                }
                matched = true;
                break;   // 一格只应用一条规则
            }
            if (matched)
                break;   // 一行只取第一个命中的参数名（表格通常一参数一行）
        }
    }
    return {importedCount, details};
}

QString BasicParamsImporter::excelToCsv(const QString &excelPath, QString *error)
{
    // PowerShell + Excel COM：第一个工作表转 UTF-8 CSV（62 = xlCSVUTF8）。
    // 脚本写入临时 .ps1 文件执行（UTF-8 BOM），路径嵌入脚本内容，
    // 避免 -Command 命令行传参的编码与引号转义问题
    const QString stamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    // 统一转为反斜杠：Excel COM 的 Open/SaveAs 不接受正斜杠路径（QDir 默认输出正斜杠）
    const QString csvPath = QDir::toNativeSeparators(
        QDir::temp().filePath(QStringLiteral("ztf_import_%1.csv").arg(stamp)));
    const QString ps1Path = QDir::toNativeSeparators(
        QDir::temp().filePath(QStringLiteral("ztf_import_%1.ps1").arg(stamp)));
    // 路径中的单引号转义（PowerShell 单引号字符串内 '' 表示一个 '）
    const QString escExcel = QString(QDir::toNativeSeparators(excelPath))
                                 .replace(QLatin1Char('\''), QLatin1String("''"));
    const QString escCsv = QString(csvPath).replace(QLatin1Char('\''), QLatin1String("''"));
    const QString script = QStringLiteral(
        "$ErrorActionPreference='Stop';"
        "[Console]::OutputEncoding=[Text.Encoding]::UTF8;"
        "$excel=New-Object -ComObject Excel.Application;"
        "$excel.Visible=$false;"
        "try{"
        "$wb=$excel.Workbooks.Open('%1');"
        "$wb.Worksheets.Item(1).SaveAs('%2',62);"
        "$wb.Close($false)}"
        "finally{$excel.Quit();"
        "[Runtime.InteropServices.Marshal]::ReleaseComObject($excel)|Out-Null}")
        .arg(escExcel)
        .arg(escCsv);

    {
        QFile ps1(ps1Path);
        if (!ps1.open(QIODevice::WriteOnly)) {
            if (error) *error = QStringLiteral("无法写入临时脚本：%1").arg(ps1Path);
            return {};
        }
        ps1.write("\xEF\xBB\xBF");   // UTF-8 BOM：Windows PowerShell 5.1 依此识别编码
        ps1.write(script.toUtf8());
    }

    QProcess proc;
    proc.start(QStringLiteral("powershell"), {QStringLiteral("-NoProfile"),
                QStringLiteral("-NonInteractive"),
                QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                QStringLiteral("-File"), ps1Path});
    const bool finished = proc.waitForStarted(5000) && proc.waitForFinished(60000);
    QFile::remove(ps1Path);
    if (!finished) {
        if (error) *error = QStringLiteral("Excel 转换超时或无法启动 PowerShell");
        return {};
    }
    if (proc.exitCode() != 0 || !QFile::exists(csvPath)) {
        // 脚本内已设 [Console]::OutputEncoding=UTF8，Excel COM 错误为 UTF-8；
        // PowerShell 自身错误（脚本未执行到该行前）为本地码页——decodeBytes 兼容两者
        const QString errText = decodeBytes(proc.readAllStandardError());
        if (error) *error = QStringLiteral("Excel 转换失败：%1\n\n"
                                           "常见原因：\n"
                                           "1. 该文件正在 Excel 中打开——请先关闭后重试\n"
                                           "2. 本机未安装 Office——请在 Excel 中将参数表"
                                           "另存为 CSV 后导入")
                             .arg(errText.left(300).simplified());
        QFile::remove(csvPath);
        return {};
    }
    return csvPath;
}

bool BasicParamsImporter::importFromFile(const QString &path, TransformerParams &params,
                                         CalcInput &input, QString *report)
{
    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();

    QString csvPath = path;
    QString tmpCsv;
    if (suffix != QStringLiteral("csv")) {
        QString err;
        tmpCsv = excelToCsv(path, &err);
        if (tmpCsv.isEmpty()) {
            if (report) *report = err;
            return false;
        }
        csvPath = tmpCsv;
    }

    QFile f(csvPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (report) *report = QStringLiteral("无法打开文件：%1").arg(csvPath);
        if (!tmpCsv.isEmpty()) QFile::remove(tmpCsv);
        return false;
    }
    const QString text = decodeBytes(f.readAll());
    f.close();
    if (!tmpCsv.isEmpty())
        QFile::remove(tmpCsv);

    const QStringList rows = text.split(QLatin1Char('\n'));
    const auto result = parseRows(rows, params, input);

    if (report) {
        report->clear();
        if (result.first > 0) {
            report->append(QStringLiteral("成功导入 %1 项参数:\n\n%2")
                               .arg(result.first)
                               .arg(result.second.join(QLatin1Char('\n'))));
        } else {
            report->append(QStringLiteral("未识别到任何参数。\n\n"
                "支持识别：容量、额定电压、联结组别、频率、海拔、空载/负载/总损耗、"
                "阻抗电压、空载电流、温升限值、型号等。\n"
                "请确认文件为「参数名 + 参数值」的表格格式。"));
        }
    }
    return result.first > 0;
}
