#ifndef AIANALYSISDIALOG_H
#define AIANALYSISDIALOG_H
// AI 分析对话框：接收引擎输出的汇总数据文本，请求 LLM 生成解读/对比
// 用途一：计算结果解读（优化 Tab「AI 解读」）
// 用途二：方案对比评价（方案选择 Tab「AI 对比」）
// 原则：LLM 只基于传入的引擎汇总数据做解读，不产出计算数值

#include <QDialog>

class QTextEdit;
class QLabel;
class QPushButton;
class CloudLlmClient;

class AiAnalysisDialog : public QDialog {
    Q_OBJECT
public:
    // title：窗口标题；taskDesc：分析任务描述（写入 system 提示词）；
    // dataText：引擎输出的汇总数据（键值对文本）
    explicit AiAnalysisDialog(const QString &title, const QString &taskDesc,
                              const QString &dataText, QWidget *parent = nullptr);

private slots:
    void onSubmit();
    void onLlmFinished(const QString &reply);
    void onLlmFailed(const QString &error);

private:
    QString m_taskDesc;
    QString m_dataText;
    QTextEdit *m_outputEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_submitBtn = nullptr;
    CloudLlmClient *m_client = nullptr;
};

#endif // AIANALYSISDIALOG_H
