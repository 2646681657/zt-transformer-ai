#ifndef SEARCHCOVERAGESETTINGS_H
#define SEARCHCOVERAGESETTINGS_H

#include "IOptimizer.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QTableWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <optional>

namespace SearchCoverageSettings {
// 修改局部副本；取消不写QSettings，也不改变外层循环参数。
inline std::optional<OptimizationSettings> edit(QWidget *parent, const OptimizationSettings &current,
                                               const CalcInput &base)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("搜索覆盖与企业工艺条件"));
    dlg.resize(920, 670);
    QVBoxLayout layout(&dlg);
    auto *hint = new QLabel(QStringLiteral("增强覆盖：最低成本方案必留，先覆盖可行牌号，再选择不同尺寸区域。展示最低成本5000个；全部可行数量仍统计。\n"
        "扩展只处理触边方向的局部点，不穷举扩大后的包围盒；空硬边界不扩展该维度。圆线清单与牌号清单不自动扩大。"), &dlg);
    hint->setWordWrap(true);
    layout.addWidget(hint);
    QFormLayout options;
    auto *centers = new QSpinBox(&dlg);
    centers->setRange(3, 12);
    centers->setValue(current.centerLimit);
    centers->setEnabled(current.enhancedCoverage);
    options.addRow(QStringLiteral("最大细搜中心（增强模式）："), centers);
    auto *expand = new QCheckBox(QStringLiteral("允许在明确硬边界内按触边方向扩展"), &dlg);
    expand->setChecked(current.expandCoverage);
    expand->setEnabled(current.enhancedCoverage);
    options.addRow(expand);
    auto *rounds = new QSpinBox(&dlg);
    rounds->setRange(1, 5);
    rounds->setValue(current.maxExpansionRounds);
    rounds->setEnabled(current.enhancedCoverage && expand->isChecked());
    options.addRow(QStringLiteral("最大扩展轮数："), rounds);
    layout.addLayout(&options);

    auto *bounds = new QTableWidget(9, 4, &dlg);
    bounds->setHorizontalHeaderLabels({QStringLiteral("数值变量"), QStringLiteral("当前初始范围"),
        QStringLiteral("绝对硬下界（可空）"), QStringLiteral("绝对硬上界（可空）")});
    bounds->verticalHeader()->hide();
    bounds->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    bounds->setEditTriggers(QAbstractItemView::NoEditTriggers);
    const QStringList names{QStringLiteral("铁芯直径 mm"), QStringLiteral("直线段长 mm"),
        QStringLiteral("低压匝数"), QStringLiteral("高压总层数 W12"), QStringLiteral("低压箔厚 mm"),
        QStringLiteral("低压箔宽 mm"), QStringLiteral("高压裸线宽 mm"), QStringLiteral("高压裸线厚 mm"),
        QStringLiteral("主油道宽 mm")};
    const double bases[]{base.coreDiameter_mm, base.coreStraight_mm, double(base.lvTurns),
        double(base.hvTurnsPerLayer), base.lvFoilThick_mm, base.lvFoilWidth_mm,
        base.hvBareWidth_mm, base.hvBareThick_mm, base.mainDuctWidth_mm};
    const double steps[]{current.diaStep_mm, current.straightStep_mm, 1, 1,
        current.lvFoilThickStep_mm, current.lvFoilWidthStep_mm, current.hvBareWidthStep_mm,
        current.hvBareThickStep_mm, current.mainDuctStep_mm};
    const int radii[]{current.diameterRadius(), current.straightRadius(), current.lvTurnsRadius(),
        current.hvLayersRadius(), current.lvFoilThickRadius(), current.lvFoilWidthRadius(),
        current.hvBareWidthRadius(), current.hvBareThickRadius(), current.searchMainDuct ? current.mainDuctRange : 0};
    std::array<QLineEdit *, 9> lower{}, upper{};
    for (int i = 0; i < 9; ++i) {
        bounds->setItem(i, 0, new QTableWidgetItem(names[i]));
        const double span = radii[i] > 0 ? radii[i] * steps[i] : 0.0;
        bounds->setItem(i, 1, new QTableWidgetItem(QStringLiteral("%1 ～ %2%3")
            .arg(bases[i] - span, 0, 'g', 12).arg(bases[i] + span, 0, 'g', 12)
            .arg(radii[i] > 0 ? QString() : QStringLiteral("（固定）"))));
        lower[i] = new QLineEdit(current.hardMinimumTexts[i], &dlg);
        upper[i] = new QLineEdit(current.hardMaximumTexts[i], &dlg);
        lower[i]->setPlaceholderText(QStringLiteral("空：不扩展"));
        upper[i]->setPlaceholderText(QStringLiteral("空：不扩展"));
        bounds->setCellWidget(i, 2, lower[i]);
        bounds->setCellWidget(i, 3, upper[i]);
    }
    const auto enableExpansion = [=]() {
        rounds->setEnabled(current.enhancedCoverage && expand->isChecked());
        for (int i = 0; i < 9; ++i) {
            // 保留的旧边界允许修正/清空；是否参与扩展由搜索开关和范围决定，不能靠锁编辑代替。
            const bool active = current.enhancedCoverage && expand->isChecked();
            lower[i]->setEnabled(active);
            upper[i]->setEnabled(active);
        }
    };
    QObject::connect(expand, &QCheckBox::toggled, &dlg, enableExpansion);
    enableExpansion();
    layout.addWidget(bounds, 1);
    auto *craft = new QCheckBox(QStringLiteral("校核企业认可的主油道宽最小值（未启用即未校核）"), &dlg);
    craft->setChecked(current.craftConstraints.enabled);
    layout.addWidget(craft);
    QFormLayout craftForm;
    auto *minimum = new QLineEdit(current.craftConstraints.minimumMainDuct_mm > 0
        ? QString::number(current.craftConstraints.minimumMainDuct_mm, 'g', 17) : QString(), &dlg);
    minimum->setPlaceholderText(QStringLiteral("企业认可的有限正数，不自动设定"));
    auto *source = new QLineEdit(current.craftConstraints.source, &dlg);
    source->setPlaceholderText(QStringLiteral("例如企业工艺文件编号／确认人，启用时必填"));
    craftForm.addRow(QStringLiteral("主油道宽最小值 mm："), minimum);
    craftForm.addRow(QStringLiteral("依据／来源："), source);
    layout.addLayout(&craftForm);
    const auto enableCraft = [=]() { minimum->setEnabled(craft->isChecked()); source->setEnabled(craft->isChecked()); };
    QObject::connect(craft, &QCheckBox::toggled, &dlg, enableCraft);
    enableCraft();
    auto *note = new QLabel(QStringLiteral("软件正数保护不等于绝缘安全校核。这里仅增加企业主油道最小值；扁线／箔库存规格及其他制造条件仍需企业资料。"), &dlg);
    note->setWordWrap(true);
    layout.addWidget(note);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout.addWidget(buttons);
    OptimizationSettings result = current;
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, [&]() {
        result.centerLimit = centers->value();
        result.expandCoverage = expand->isChecked();
        result.maxExpansionRounds = rounds->value();
        for (int i = 0; i < 9; ++i) {
            result.hardMinimumTexts[i] = lower[i]->text();
            result.hardMaximumTexts[i] = upper[i]->text();
        }
        result.craftConstraints.enabled = craft->isChecked();
        bool valid = false;
        result.craftConstraints.minimumMainDuct_mm = minimum->text().trimmed().toDouble(&valid);
        if (!valid) result.craftConstraints.minimumMainDuct_mm = craft->isChecked()
            ? std::numeric_limits<double>::quiet_NaN() : 0.0;
        result.craftConstraints.source = source->text().trimmed();
        const auto error = result.validationError(base);
        if (!error.isEmpty()) { QMessageBox::warning(&dlg, QStringLiteral("设置无效"), error); return; }
        dlg.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return std::nullopt;
    return result;
}
}
#endif
