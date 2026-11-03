#pragma once

#include <QFrame>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QPainter>
#include <QPointer>
#include <QSizePolicy>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <cmath>

// Display-only schematic. All dimensions and annotations come from the sheet.
namespace tank_dimensions_detail {
class Sketch final : public QWidget {
public:
    explicit Sketch(QWidget* parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("tankSketch"));
        setAccessibleName(QStringLiteral("油箱平面与波纹壁截面简图"));
        setMinimumSize(360, 190);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setValues(const QMap<QString, QString>& values)
    {
        values_ = values;
        setAccessibleDescription(annotations().join(QStringLiteral("；")));
        update();
    }

    void setCalculationStatus(const QString& status)
    {
        stale_ = status.contains(QStringLiteral("pending"), Qt::CaseInsensitive)
            || status.contains(QStringLiteral("error"), Qt::CaseInsensitive)
            || status.contains(QStringLiteral("fail"), Qt::CaseInsensitive)
            || status.contains(QStringLiteral("stale"), Qt::CaseInsensitive)
            || status.contains(QStringLiteral("待"))
            || status.contains(QStringLiteral("计算中"))
            || status.contains(QStringLiteral("失败"))
            || status.contains(QStringLiteral("错误"))
            || status.contains(QStringLiteral("未更新"));
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), Qt::white);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(Qt::black, 1.2));
        QFont font = p.font();
        font.setPointSize(9);
        p.setFont(font);

        const qreal half = width() / 2.0;
        text(p, QRectF(0, 0, half, 20), QStringLiteral("平面 (mm)"));
        text(p, QRectF(half, 0, half, 20), QStringLiteral("波纹壁截面 (mm)"));

        double length = 0, width = 0, height = 0;
        const bool valid = positive(QStringLiteral("H26"), length)
            && positive(QStringLiteral("F25"), width)
            && positive(QStringLiteral("J27"), height);
        if (valid) {
            // Normalize first to avoid overflow for finite but extreme input.
            const double maximum = std::max({length, width, height});
            const qreal l = length / maximum;
            const qreal w = width / maximum;
            const qreal h = height / maximum;
            const qreal availableWidth = half - 66;
            const qreal availableHeight = std::max(qreal(35), this->height() - qreal(119));
            const qreal scale = std::min(availableWidth / l,
                                        availableHeight / std::max(w, h));
            const qreal base = 48 + availableHeight;
            const QRectF plan(18 + (availableWidth - l * scale) / 2,
                              base - w * scale, l * scale, w * scale);
            p.drawRect(plan);
            // Original K22:S27 uses a wall section with one outward rectangle,
            // not an overall L x H elevation or a row of corrugation stripes.
            // Preserve the source annotation positions. Depth is drawn with
            // readable schematic exaggeration, not a fabrication scale.
            double rimHeight=0,topOil=0,waveHeight=0,depth=0;
            const bool sectionValid=nonnegative(QStringLiteral("Q22"),rimHeight)
                &&nonnegative(QStringLiteral("Q26"),topOil)
                &&positive(QStringLiteral("S23"),waveHeight)
                &&positive(QStringLiteral("Q25"),depth)
                &&std::abs(rimHeight+waveHeight+topOil-height)<=std::max(0.1,height*0.000001);
            if(sectionValid){
                const qreal wall=half+58,projection=half-110,top=36,bottom=base;
                const qreal sectionHeight=bottom-top;
                // Like the workbook, enlarge short annotation bands so their
                // numerals stay legible; this section is intentionally unscaled.
                const qreal upperBand=rimHeight>0?std::clamp(sectionHeight*(rimHeight/height),qreal(20),sectionHeight/3):0;
                const qreal lowerBand=topOil>0?std::clamp(sectionHeight*(topOil/height),qreal(20),sectionHeight/3):0;
                const qreal waveTop=top+upperBand;
                const qreal waveBottom=bottom-lowerBand;
                p.drawLine(QPointF(wall,top),QPointF(wall,bottom));
                p.drawLine(QPointF(wall-18,top),QPointF(wall+projection*.45,top));
                p.drawRect(QRectF(wall,waveTop,projection,waveBottom-waveTop));
                // The Q26 annotation band remains open on its right side.
                p.drawLine(QPointF(wall-18,bottom),QPointF(wall+projection*.4,bottom));
                const auto compact=[](double value){return QString::number(value,'g',10);};
                text(p,QRectF(half+1,(top+bottom)/2-10,52,20),compact(height));
                text(p,QRectF(wall+projection+3,(waveTop+waveBottom)/2-10,48,20),compact(waveHeight));
                text(p,QRectF(wall+4,(top+waveTop)/2-10,48,20),compact(rimHeight));
                text(p,QRectF(wall,(waveTop+waveBottom)/2-10,projection,20),compact(depth));
                text(p,QRectF(wall,(waveBottom+bottom)/2-10,projection*.4,20),compact(topOil));
            }else{
                text(p,QRectF(half+4,34,half-8,availableHeight),QStringLiteral("截面尺寸待核对"));
            }
            horizontalDimension(p, plan, dimension(length));
            verticalDimension(p, plan, dimension(width));
        } else {
            text(p, QRectF(8, 32, this->width() - 16, this->height() - 100),
                 QStringLiteral("尺寸待核对，暂不绘制几何\n请提供有效的长、宽、高 (mm)"));
        }

        const qreal bottom = this->height() - 61;
        const auto notes=annotations();
        text(p, QRectF(2, bottom, half - 4, 19), notes[0]);
        text(p, QRectF(half, bottom, half - 2, 19), notes[1]);
        text(p, QRectF(2, bottom + 19, half - 4, 19), notes[2]);
        text(p, QRectF(half, bottom + 19, half - 2, 19), notes[3]);
        text(p, QRectF(0, this->height() - 21, this->width(), 20),
             stale_ ? QStringLiteral("旧值示意（待更新）· 非制造图")
                    : QStringLiteral("尺寸示意 · 非制造图"));
    }

private:
    QStringList annotations() const
    {
        return {annotation("Q22","箱沿高"),annotation("S23","波纹高"),
            annotation("Q25","波纹深"),annotation("Q26","箱顶油空")};
    }
    bool nonnegative(const QString& address,double& number) const
    {
        bool ok=false;number=values_.value(address).trimmed().toDouble(&ok);
        return ok&&std::isfinite(number)&&number>=0;
    }
    bool positive(const QString& address, double& number) const
    {
        QString value = values_.value(address).trimmed();
        if (value.endsWith(QStringLiteral("mm"), Qt::CaseInsensitive))
            value.chop(2);
        else if (value.endsWith(QStringLiteral("毫米")))
            value.chop(2);
        bool ok = false;
        number = value.trimmed().toDouble(&ok);
        return ok && std::isfinite(number) && number > 0;
    }

    static QString dimension(double number)
    {
        return QString::number(number, 'g', 10) + QStringLiteral(" mm");
    }

    QString annotation(const char* address, const char* caption) const
    {
        const QString key = QString::fromLatin1(address);
        double number = 0;
        const QString supplied = values_.value(key).trimmed();
        return QString::fromUtf8(caption) + QStringLiteral(": ")
            + (positive(key, number) ? dimension(number)
                                    : (supplied.isEmpty() ? QStringLiteral("待核对") : supplied));
    }

    static void text(QPainter& p, const QRectF& area, const QString& value)
    {
        p.drawText(area, Qt::AlignCenter | Qt::TextWordWrap, value);
    }

    static void arrow(QPainter& p, const QPointF& from, const QPointF& to)
    {
        p.drawLine(from, to);
        const QPointF delta = to - from;
        const qreal magnitude = std::hypot(delta.x(), delta.y());
        if (magnitude < 1)
            return;
        const QPointF along = delta / magnitude;
        const QPointF across(-along.y(), along.x());
        const qreal head = std::min(qreal(4), magnitude / 3);
        p.drawLine(from, from + along * head + across * (head / 2));
        p.drawLine(from, from + along * head - across * (head / 2));
        p.drawLine(to, to - along * head + across * (head / 2));
        p.drawLine(to, to - along * head - across * (head / 2));
    }

    static void horizontalDimension(QPainter& p, const QRectF& box, const QString& value)
    {
        const qreal y = box.top() - 7;
        p.drawLine(box.topLeft(), QPointF(box.left(), y - 3));
        p.drawLine(box.topRight(), QPointF(box.right(), y - 3));
        arrow(p, QPointF(box.left(), y), QPointF(box.right(), y));
        text(p, QRectF(box.center().x() - 72, y - 23, 144, 20), value);
    }

    static void verticalDimension(QPainter& p, const QRectF& box, const QString& value)
    {
        const qreal x = box.right() + 7;
        p.drawLine(box.topRight(), QPointF(x + 3, box.top()));
        p.drawLine(box.bottomRight(), QPointF(x + 3, box.bottom()));
        arrow(p, QPointF(x, box.top()), QPointF(x, box.bottom()));
        p.save();
        p.translate(x + 3, box.center().y());
        p.rotate(-90);
        text(p, QRectF(-72, 0, 144, 20), value);
        p.restore();
    }

    QMap<QString, QString> values_;
    bool stale_ = false;
};
} // namespace tank_dimensions_detail

class TankDimensionsPanel : public QFrame {
public:
    explicit TankDimensionsPanel(QWidget* parent = nullptr) : QFrame(parent)
    {
        setObjectName(QStringLiteral("tankDimensionsPanel"));
        setFrameShape(QFrame::NoFrame);
        setStyleSheet(QStringLiteral(
            "QFrame#tankDimensionsPanel { background: white; border: 2px solid black; }"
            "QFrame#tankDimensionsPanel QLabel { color: black; background: transparent; border: none; }"));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(10, 7, 10, 7);
        root->setSpacing(4);

        auto* title = new QLabel(QStringLiteral("油箱尺寸及简图"), this);
        title->setObjectName(QStringLiteral("tankDimensionsTitle"));
        title->setAlignment(Qt::AlignCenter);
        QFont titleFont = title->font();
        titleFont.setBold(true);
        titleFont.setPointSize(12);
        title->setFont(titleFont);
        root->addWidget(title);

        auto* summary = new QGridLayout;
        summary->setVerticalSpacing(1);
        const QStringList headings{QStringLiteral("长(mm)"), QStringLiteral("宽(mm)"), QStringLiteral("高(mm)")};
        const QStringList addresses{QStringLiteral("H26"), QStringLiteral("F25"), QStringLiteral("J27")};
        const QStringList names{QStringLiteral("tankLengthValue"), QStringLiteral("tankWidthValue"), QStringLiteral("tankHeightValue")};
        for (int column = 0; column < 3; ++column) {
            auto* heading = new QLabel(headings[column], this);
            heading->setAlignment(Qt::AlignCenter);
            summary->addWidget(heading, 0, column);
            auto* value = new QLabel(QStringLiteral("待核对"), this);
            value->setObjectName(names[column]);
            value->setAlignment(Qt::AlignCenter);
            value->setMinimumWidth(0);
            value->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            value->setTextFormat(Qt::PlainText);
            summary->addWidget(value, 1, column);
            summary->setColumnStretch(column, 1);
            summaryLabels_.insert(addresses[column], value);
        }
        root->addLayout(summary);

        auto* middle = new QHBoxLayout;
        middle->setSpacing(8);
        auto* construction = new QGridLayout;
        construction->setHorizontalSpacing(5);
        construction->setVerticalSpacing(3);
        const QStringList axes{QStringLiteral("宽向 (mm)"), QStringLiteral("长向 (mm)"), QStringLiteral("高向 (mm)")};
        for (int column = 0; column < 3; ++column) {
            auto* heading = new QLabel(axes[column], this);
            heading->setAlignment(Qt::AlignCenter);
            construction->addWidget(heading, 0, column);
            construction->setColumnStretch(column, 1);
        }
        addCell(construction, 1, 0, "F23", "器身尺寸");
        addCell(construction, 1, 1, "H23", "两倍中心距");
        addCell(construction, 1, 2, "J23", "箱底油空");
        addCell(construction, 2, 0, "F24", "侧净空");
        addCell(construction, 2, 1, "H24", "绕组辐向");
        addCell(construction, 2, 2, "J24", "铁芯尺寸项");
        addCell(construction, 3, 0, "F25", "油箱宽");
        addCell(construction, 3, 1, "H25", "端净空");
        addCell(construction, 3, 2, "J25", "低压轴向高");
        addCell(construction, 4, 1, "H26", "油箱长");
        addCell(construction, 4, 2, "J26", "垫脚高");
        addCell(construction, 5, 2, "J27", "油箱高");
        middle->addLayout(construction, 1);
        sketch_ = new tank_dimensions_detail::Sketch(this);
        middle->addWidget(sketch_, 1);
        root->addLayout(middle, 1);

        auto* details = new QGridLayout;
        details->setHorizontalSpacing(8);
        details->setVerticalSpacing(2);
        addCell(details, 0, 0, "F27", "储油柜规格");
        addCell(details, 0, 1, "M21", "油箱型式");
        for (int column = 0; column < 2; ++column)
            details->setColumnStretch(column, 1);
        root->addLayout(details);

        status_ = new QLabel(this);
        status_->setObjectName(QStringLiteral("tankCalculationStatus"));
        status_->setTextFormat(Qt::PlainText);
        status_->setWordWrap(true);
        status_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        status_->hide();
        root->addWidget(status_);
    }

    void setValues(const QMap<QString, QString>& values)
    {
        for (auto it = cells_.begin(); it != cells_.end(); ++it) {
            const QString value = values.value(it.key());
            it.value().label->setText(value.isEmpty() ? QStringLiteral("—") : value);
            it.value().label->setToolTip(value);
        }
        for (auto it = summaryLabels_.begin(); it != summaryLabels_.end(); ++it) {
            QString value = values.value(it.key()).trimmed();
            // Keep the source's numeric formatting (e.g. 1323.0); units live above.
            if (value.endsWith(QStringLiteral("mm"), Qt::CaseInsensitive)
                || value.endsWith(QStringLiteral("毫米"))) {
                value.chop(2);
                value = value.trimmed();
            }
            it.value()->setText(value.isEmpty() ? QStringLiteral("待核对") : value);
            it.value()->setToolTip(value);
        }
        sketch_->setValues(values);
    }

    // Pass a dedicated mirrored editor; the panel never changes its value or model.
    void installEditor(const QString& address, QWidget* editor)
    {
        auto it = cells_.find(address);
        if (it == cells_.end() || !editor || editor == it.value().label)
            return;
        Cell& cell = it.value();
        if (cell.editor == editor)
            return;
        // Replacing/moving an editor must not destroy either supplied widget.
        for (auto other = cells_.begin(); other != cells_.end(); ++other) {
            if (other.value().editor == editor) {
                other.value().slot->removeWidget(editor);
                other.value().editor.clear();
                other.value().label->show();
            }
        }
        if (cell.editor) {
            cell.slot->removeWidget(cell.editor.data());
            cell.editor->hide();
        }
        cell.label->hide();
        cell.slot->addWidget(editor);
        cell.editor = editor;
        editor->show();
    }

    void setCalculationStatus(const QString& status)
    {
        status_->setText(status.simplified());
        status_->setVisible(!status.trimmed().isEmpty());
        sketch_->setCalculationStatus(status);
    }

private:
    struct Cell {
        QLabel* label = nullptr;
        QVBoxLayout* slot = nullptr;
        QPointer<QWidget> editor;
    };

    void addCell(QGridLayout* grid, int row, int column,
                 const char* address, const char* caption)
    {
        auto* container = new QWidget(this);
        container->setMinimumWidth(0);
        container->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        auto* layout = new QVBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        auto* heading = new QLabel(QString::fromUtf8(caption), container);
        heading->setAlignment(Qt::AlignCenter);
        layout->addWidget(heading);
        auto* value = new QLabel(QStringLiteral("—"), container);
        const QString key = QString::fromLatin1(address);
        value->setObjectName(QStringLiteral("tankCell_") + key);
        value->setTextFormat(Qt::PlainText);
        value->setAlignment(Qt::AlignCenter);
        value->setWordWrap(true);
        value->setMinimumWidth(0);
        layout->addWidget(value);
        grid->addWidget(container, row, column);
        cells_.insert(key, Cell{value, layout, {}});
    }

    QMap<QString, Cell> cells_;
    QMap<QString, QLabel*> summaryLabels_;
    tank_dimensions_detail::Sketch* sketch_ = nullptr;
    QLabel* status_ = nullptr;
};
