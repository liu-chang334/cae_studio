#include "MinimalUi.h"

#include <QAction>
#include <QGraphicsDropShadowEffect>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QSvgRenderer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

QIcon loadUiIcon(const QString &resourcePath) {
    // 显式渲染 SVG，多尺寸图标不依赖外部 SVG 图标引擎插件。
    QSvgRenderer renderer(resourcePath);
    QIcon icon;
    for (const int edge : {20, 40, 60}) {
        QPixmap pixmap(edge, edge);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

SoftCheckBox::SoftCheckBox(const QString &text, QWidget *parent) : QCheckBox(text, parent) {
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
}

QSize SoftCheckBox::sizeHint() const {
    return QSize(fontMetrics().horizontalAdvance(text()) + 28, std::max(24, fontMetrics().height() + 4));
}

QSize SoftCheckBox::minimumSizeHint() const {
    return sizeHint();
}

bool SoftCheckBox::hitButton(const QPoint &position) const {
    return rect().contains(position);
}

void SoftCheckBox::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF indicator(1.0, (height() - 16.0) * 0.5, 16.0, 16.0);
    const bool checked = isChecked();
    const bool enabled = isEnabled();
    const QColor accent = enabled ? QColor("#568e98") : QColor("#a5b8bd");

    painter.setPen(QPen(checked ? accent : QColor(enabled ? "#a9c1c8" : "#c8d5d9"), 1.2));
    painter.setBrush(checked ? accent : QColor(255, 255, 255, 220));
    painter.drawRoundedRect(indicator, 4, 4);
    if (checked) {
        QPainterPath tick;
        tick.moveTo(indicator.left() + 3.7, indicator.top() + 8.0);
        tick.lineTo(indicator.left() + 6.7, indicator.top() + 11.0);
        tick.lineTo(indicator.left() + 12.3, indicator.top() + 5.0);
        painter.setPen(QPen(Qt::white, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(tick);
    }
    if (hasFocus()) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor("#739ca6"), 1, Qt::DotLine));
        painter.drawRoundedRect(indicator.adjusted(-1, -1, 1, 1), 5, 5);
    }
    painter.setPen(QColor(enabled ? "#647e88" : "#9bafb6"));
    painter.drawText(QRect(26, 0, width() - 26, height()), Qt::AlignLeft | Qt::AlignVCenter, text());
}

FloatingToolPalette::FloatingToolPalette(const QString &name, QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAccessibleName(name);
    setToolTip(name + QStringLiteral(" · 拖动顶部圆点移动"));
    setFixedSize(34, 24);
    setCursor(Qt::OpenHandCursor);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(18);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(45, 73, 83, 28));
    setGraphicsEffect(shadow);

    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(3, 18, 3, 6);
    layout_->setSpacing(3);
}

void FloatingToolPalette::addActionButton(QAction *action) {
    auto *button = new QToolButton(this);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setIconSize(QSize(20, 20));
    button->setFixedSize(28, 28);
    button->setCursor(Qt::PointingHandCursor);
    button->setAccessibleName(action->text());
    button->setStyleSheet(QStringLiteral(
        "QToolButton { border: none; border-radius: 9px; background: transparent; }"
        "QToolButton:hover { background: rgba(219, 235, 236, 170); }"
        "QToolButton:checked { background: rgba(196, 224, 226, 205); }"
        "QToolButton:pressed { background: rgba(178, 209, 214, 210); }"
        "QToolButton:disabled { background: transparent; }"
        "QToolButton:focus { border: 1px solid rgba(78, 135, 143, 120); }"));
    layout_->addWidget(button);
    const int count = layout_->count();
    setFixedHeight(18 + 6 + count * 28 + (count - 1) * 3);
}

void FloatingToolPalette::keepInBounds() {
    if (!parentWidget())
        return;
    // 为底部日志保留空间，窗口缩小时将工具条收回可见范围。
    const int maxX = std::max(12, parentWidget()->width() - width() - 12);
    const int maxY = std::max(12, parentWidget()->height() - height() - 156);
    move(std::clamp(x(), 12, maxX), std::clamp(y(), 12, maxY));
}

void FloatingToolPalette::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QLinearGradient wash(0, 0, width(), height());
    wash.setColorAt(0.0, QColor(255, 255, 255, 244));
    wash.setColorAt(1.0, QColor(233, 243, 244, 218));
    painter.setBrush(wash);
    painter.setPen(QPen(QColor(213, 227, 230, 165), 1));
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 11, 11);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(133, 157, 165, 145));
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < 3; ++column)
            painter.drawEllipse(QPointF(width() * 0.5 - 5 + column * 5, 6.5 + row * 4), 1.0, 1.0);
}

void FloatingToolPalette::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && event->position().y() < 18) {
        dragging_ = true;
        dragOffset_ = event->position().toPoint();
        setCursor(Qt::ClosedHandCursor);
        raise();
        event->accept();
        return;
    }
    // 工具条空白处也拦截点击，避免穿透到网格拾取。
    event->accept();
}

void FloatingToolPalette::mouseMoveEvent(QMouseEvent *event) {
    if (dragging_) {
        move(parentWidget()->mapFromGlobal(event->globalPosition().toPoint() - dragOffset_));
        keepInBounds();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void FloatingToolPalette::mouseReleaseEvent(QMouseEvent *event) {
    if (dragging_ && event->button() == Qt::LeftButton) {
        dragging_ = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    event->accept();
}

SoftLogPanel::SoftLogPanel(QPlainTextEdit *log, QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 30, 28, 14);
    auto *caption = new QLabel(QStringLiteral("日志"), this);
    caption->setStyleSheet(QStringLiteral("color: #8a9da4; font-size: 10px; background: transparent;"));
    layout->addWidget(caption);
    layout->addWidget(log, 1);
}

void SoftLogPanel::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    QLinearGradient fade(0, 0, 0, height());
    fade.setColorAt(0.0, QColor(246, 250, 251, 0));
    fade.setColorAt(0.24, QColor(246, 250, 251, 130));
    fade.setColorAt(0.65, QColor(240, 247, 248, 230));
    fade.setColorAt(1.0, QColor(235, 244, 246, 247));
    painter.fillRect(rect(), fade);
}

PopupTitleBar::PopupTitleBar(const QString &title, QWidget *parent) : QLabel(title, parent) {
    setCursor(Qt::OpenHandCursor);
}

void PopupTitleBar::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        dragOffset_ = event->globalPosition().toPoint() - window()->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QLabel::mousePressEvent(event);
}

void PopupTitleBar::mouseMoveEvent(QMouseEvent *event) {
    if (dragging_) {
        window()->move(event->globalPosition().toPoint() - dragOffset_);
        event->accept();
        return;
    }
    QLabel::mouseMoveEvent(event);
}

void PopupTitleBar::mouseReleaseEvent(QMouseEvent *event) {
    if (dragging_ && event->button() == Qt::LeftButton) {
        dragging_ = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QLabel::mouseReleaseEvent(event);
}
