#ifndef SWPARAMETRICDRAWINGPAGE_H
#define SWPARAMETRICDRAWINGPAGE_H
// SW 参数化出图页：选择模型/工程图模板，填写尺寸参数并调用 SOLIDWORKS 自动出图

#include <QWidget>

#include "SolidWorksAutomation.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTextEdit;

class SwParametricDrawingPage : public QWidget {
    Q_OBJECT
public:
    explicit SwParametricDrawingPage(QWidget *parent = nullptr);

private slots:
    void onBrowseModelTemplate();
    void onBrowseDrawingTemplate();
    void onBrowseOutputDirectory();
    void onAddParameter();
    void onRemoveParameters();
    void onClearParameters();
    void onDetectSolidWorks();
    void onGenerate();
    void onOpenOutputDirectory();
    void onAutomationFinished(bool success, const QString &message,
                              const QStringList &outputFiles);

private:
    void setupUi();
    void addParameterRow(const QString &name = QString(), double valueMm = 1.0,
                         const QString &description = QString());
    bool buildRequest(SwDrawingRequest *request, QString *errorMessage) const;
    void updateSolidWorksStatus();
    void setBusy(bool busy);
    void loadSettings();
    void saveSettings() const;

    SolidWorksAutomation *m_automation = nullptr;
    QLineEdit *m_modelTemplateEdit = nullptr;
    QLineEdit *m_drawingTemplateEdit = nullptr;
    QLineEdit *m_outputDirectoryEdit = nullptr;
    QLineEdit *m_outputNameEdit = nullptr;
    QTableWidget *m_parameterTable = nullptr;
    QCheckBox *m_exportPdfCheck = nullptr;
    QCheckBox *m_showSolidWorksCheck = nullptr;
    QCheckBox *m_insertDimensionsCheck = nullptr;
    QLabel *m_solidWorksStatusLabel = nullptr;
    QTextEdit *m_logEdit = nullptr;
    QPushButton *m_generateButton = nullptr;
    QPushButton *m_openOutputButton = nullptr;
    QString m_lastOutputDirectory;
};

#endif // SWPARAMETRICDRAWINGPAGE_H
