#ifndef TRANSFORMERTYPEDIALOG_H
#define TRANSFORMERTYPEDIALOG_H
// 变压器类型选择对话框（新建计算时选择大类和绕组工艺）

#include <QDialog>
#include "StructureConfig.h"

class QButtonGroup;

// 变压器大类和绕组工艺选择对话框
class TransformerTypeDialog : public QDialog {
    Q_OBJECT
public:
    explicit TransformerTypeDialog(QWidget *parent = nullptr);
    StructureConfig::TransformerCategory selectedCategory() const { return m_category; }
    StructureConfig::WindingProcess selectedProcess() const { return m_process; }

private:
    StructureConfig::TransformerCategory m_category = StructureConfig::OilImmersed;
    StructureConfig::WindingProcess m_process = StructureConfig::FoilWound;
    QButtonGroup *m_categoryGroup;
    QButtonGroup *m_processGroup;
};

#endif // TRANSFORMERTYPEDIALOG_H
