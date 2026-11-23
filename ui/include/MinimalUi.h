#pragma once

#include <QCheckBox>
#include <QIcon>
#include <QLabel>
#include <QPoint>
#include <QWidget>

class QAction;
class QPlainTextEdit;
class QVBoxLayout;

QIcon loadUiIcon(const QString &resourcePath);

// 自绘状态标记，避免透明小窗中原生复选框的勾选颜色不可见。
class SoftCheckBox : public QCheckBox {
public:
    explicit SoftCheckBox(const QString &text, QWidget *parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool hitButton(const QPoint &position) const override;
};

// 视口上的竖向工具胶囊，顶部圆点区域用于拖动。
class FloatingToolPalette : public QWidget {
public:
    explicit FloatingToolPalette(const QString &name, QWidget *parent);
    void addActionButton(QAction *action);
    void keepInBounds();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QVBoxLayout *layout_ = nullptr;
    QPoint dragOffset_;
    bool dragging_ = false;
};

class SoftLogPanel : public QWidget {
public:
    explicit SoftLogPanel(QPlainTextEdit *log, QWidget *parent);

protected:
    void paintEvent(QPaintEvent *event) override;
};

class PopupTitleBar : public QLabel {
public:
    explicit PopupTitleBar(const QString &title, QWidget *parent);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QPoint dragOffset_;
    bool dragging_ = false;
};
