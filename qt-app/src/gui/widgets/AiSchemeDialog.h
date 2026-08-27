#ifndef AISCHEMEDIALOG_H
#define AISCHEMEDIALOG_H
// AI 方案助手对话框：自然语言 → 大模型解析 → 设计变量（JSON）→ 校验 → 应用
// 原则：AI 只推荐宏观设计变量（容量/电压/铁芯/导线规格等），
// 工艺细节保持当前值不动；所有数值最终由引擎计算，AI 不产出计算结果

#include <QDialog>
#include "CalcInput.h"

class QTextEdit;
class QLabel;
class QPushButton;
class CloudLlmClient;

class AiSchemeDialog : public QDialog {
    Q_OBJECT
public:
    // base：当前设计变量（AI 未提到的字段保持该值）
    explicit AiSchemeDialog(const CalcInput &base, QWidget *parent = nullptr);

    // 解析成功后的设计变量（仅在 accept() 后有效）
    CalcInput resultInput() const { return m_result; }

private slots:
    void onSubmit();
    void onLlmFinished(const QString &reply);
    void onLlmFailed(const QString &error);

private:
    void buildPrompt() const;
    bool parseReply(const QString &reply);
    // 校验解析结果范围，非法字段回退 base 值；返回字段修改说明列表
    QStringList validateAndMerge(const CalcInput &ai);

    CalcInput m_base;
    CalcInput m_result;
    QTextEdit *m_inputEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_submitBtn = nullptr;
    QPushButton *m_applyBtn = nullptr;
    CloudLlmClient *m_client = nullptr;
};

#endif // AISCHEMEDIALOG_H
