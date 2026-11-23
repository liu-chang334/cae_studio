#include "MainWindow.h"
#include "AbaqusInpReader.h"
#include "AnalysisProblemBuilder.h"
#include "DisplacementResultIO.h"
#include "LinearStaticSolver.h"
#include "MeshViewport.h"
#include "MinimalUi.h"
#include "vtkBuilder.h"

#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QFutureWatcher>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QPlainTextEdit>
#include <QScreen>
#include <QSignalBlocker>
#include <QStringList>
#include <QTime>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <utility>

namespace {

const char *analysisKeyword(AnalysisType type) {
    return type == AnalysisType::ExplicitDynamics ? "*Dynamic, Explicit" : "*Static";
}

}  // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));
    setupLogWidget();
    setupCentral();
    setupResultPopup();
    setupToolPalettes();
    updateActionStates();
    setMinimumSize(680, 500);
    resize(1180, 800);
    appendLog(tr("Ctrl+O 选择工作目录 · Ctrl+0 适应视图 · F5 求解 · Ctrl+R 云图"));
}

void MainWindow::setupLogWidget() {
    logWidget_ = new QPlainTextEdit(this);
    logWidget_->setReadOnly(true);
    logWidget_->setMaximumBlockCount(1200);
    logWidget_->setFont(QFont(QStringLiteral("Consolas"), 9));
    logWidget_->setFrameStyle(QFrame::NoFrame);
    QPalette palette = logWidget_->palette();
    palette.setColor(QPalette::Base, Qt::transparent);
    logWidget_->setPalette(palette);
    logWidget_->viewport()->setAutoFillBackground(false);
    logWidget_->setStyleSheet(
        QStringLiteral("QPlainTextEdit { background: transparent; color: #657e88; border: none; selection-background-color: #d5e7e9; }"
                       "QScrollBar:vertical { background: transparent; width: 5px; margin: 0; }"
                       "QScrollBar::handle:vertical { background: rgba(130, 157, 166, 100); border-radius: 2px; min-height: 20px; }"
                       "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                       "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"));
}

void MainWindow::appendLog(const QString &message) {
    const QString prefix = QStringLiteral("[%1] ").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")));
    QString entry = message;
    entry.replace(QLatin1Char('\n'), QStringLiteral("\n") + prefix);
    logWidget_->appendPlainText(prefix + entry);
}

void MainWindow::setupCentral() {
    viewport_ = new MeshViewport(this);
    setCentralWidget(viewport_);
    viewport_->installEventFilter(this);
    logPanel_ = new SoftLogPanel(logWidget_, viewport_);

    connect(viewport_, &MeshViewport::elementPicked, this, &MainWindow::showElementInfo);
    connect(viewport_, &MeshViewport::nodePicked, this, &MainWindow::showNodeInfo);
    connect(viewport_, &MeshViewport::selectionCleared, this, [this]() { appendLog(tr("已取消选择")); });
}

void MainWindow::setupResultPopup() {
    resultDialog_ = new QDialog(this, Qt::Tool | Qt::FramelessWindowHint);
    resultDialog_->setAttribute(Qt::WA_TranslucentBackground);
    resultDialog_->setWindowTitle(tr("位移云图"));
    resultDialog_->setFixedWidth(292);

    auto *outer = new QVBoxLayout(resultDialog_);
    outer->setContentsMargins(12, 12, 12, 12);
    auto *card = new QFrame(resultDialog_);
    card->setObjectName(QStringLiteral("ResultCard"));
    card->setStyleSheet(QStringLiteral(
        "QFrame#ResultCard { border: 1px solid rgba(211, 226, 229, 210); border-radius: 18px;"
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 rgba(255,255,255,252), stop:1 rgba(236,246,247,247)); }"
        "QLabel { background: transparent; border: none; color: #5a737e; }"
        "QComboBox { color: #436775; background: rgba(255,255,255,210); border: 1px solid #d8e6e9;"
        "border-radius: 9px; padding: 7px 10px; min-width: 100px; }"
        "QComboBox QAbstractItemView { color: #436775; background: #f7fbfb; selection-background-color: #dbecee; }"
        "QCheckBox { color: #647e88; spacing: 8px; background: transparent; }"
        "QToolButton { border: none; border-radius: 8px; background: transparent; }"
        "QToolButton:hover { background: #e4eef0; }"));
    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(22);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(45, 73, 83, 30));
    card->setGraphicsEffect(shadow);
    outer->addWidget(card);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 14, 18, 20);
    layout->setSpacing(14);

    auto *heading = new QHBoxLayout();
    auto *title = new PopupTitleBar(tr("位移云图"), card);
    title->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 600; color: #3b606c;"));
    auto *close = new QToolButton(card);
    close->setIcon(loadUiIcon(QStringLiteral(":/icons/close.svg")));
    close->setIconSize(QSize(18, 18));
    close->setFixedSize(26, 26);
    close->setToolTip(tr("关闭设置 · Esc"));
    close->setAccessibleName(tr("关闭云图设置"));
    heading->addWidget(title, 1);
    heading->addWidget(close);
    layout->addLayout(heading);
    connect(close, &QToolButton::clicked, resultDialog_, &QDialog::hide);

    auto *subtitle = new QLabel(tr("节点结果 · 全局坐标系"), card);
    subtitle->setStyleSheet(QStringLiteral("color: #91a3aa; font-size: 10px;"));
    layout->addWidget(subtitle);
    componentCombo_ = new QComboBox(card);
    componentCombo_->addItems({tr("总位移 |U|"), tr("U1 · X 方向"), tr("U2 · Y 方向"), tr("U3 · Z 方向")});
    componentCombo_->setPlaceholderText(tr("选择位移分量"));
    componentCombo_->setCurrentIndex(-1);
    auto *form = new QFormLayout();
    form->addRow(tr("分量"), componentCombo_);
    layout->addLayout(form);
    contourCheck_ = new SoftCheckBox(tr("显示云图"), card);
    legendCheck_ = new SoftCheckBox(tr("显示色标"), card);
    contourCheck_->setChecked(true);
    legendCheck_->setChecked(true);
    layout->addWidget(contourCheck_);
    layout->addWidget(legendCheck_);
    connect(componentCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::updateResultDisplay);
    connect(contourCheck_, &QCheckBox::toggled, this, &MainWindow::updateResultDisplay);
    connect(legendCheck_, &QCheckBox::toggled, this, &MainWindow::updateResultDisplay);
}

void MainWindow::setupToolPalettes() {
    // 目录操作置于首个工具条顶部，快捷键继续保留。
    openAction_ = new QAction(loadUiIcon(QStringLiteral(":/icons/folder-open.svg")), tr("打开工作目录"), this);
    openAction_->setToolTip(tr("打开工作目录 · Ctrl+O"));
    openAction_->setShortcut(QKeySequence::Open);
    addAction(openAction_);
    connect(openAction_, &QAction::triggered, this, &MainWindow::onOpenFile);
    auto *fitAction = new QAction(tr("适应窗口"), this);
    fitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    addAction(fitAction);
    connect(fitAction, &QAction::triggered, viewport_, &MeshViewport::resetCamera);

    pickElementAction_ = new QAction(loadUiIcon(QStringLiteral(":/icons/select-element.svg")), tr("选取单元"), this);
    pickNodeAction_ = new QAction(loadUiIcon(QStringLiteral(":/icons/select-node.svg")), tr("选取节点"), this);
    pickElementAction_->setToolTip(tr("选取单元 · 前处理"));
    pickNodeAction_->setToolTip(tr("选取节点 · 前处理"));
    pickElementAction_->setCheckable(true);
    pickNodeAction_->setCheckable(true);
    auto *selection = new QActionGroup(this);
    selection->setExclusive(true);
    selection->addAction(pickElementAction_);
    selection->addAction(pickNodeAction_);
    pickElementAction_->setChecked(true);
    connect(pickElementAction_, &QAction::triggered, this, [this]() {
        switchWorkspace(Workspace::Preprocess);
        viewport_->setNodePickingEnabled(false);
    });
    connect(pickNodeAction_, &QAction::triggered, this, [this]() {
        switchWorkspace(Workspace::Preprocess);
        viewport_->setNodePickingEnabled(true);
    });

    solveAction_ = new QAction(loadUiIcon(QStringLiteral(":/icons/solve.svg")), tr("求解"), this);
    solveAction_->setShortcut(QKeySequence(QStringLiteral("F5")));
    addAction(solveAction_);
    connect(solveAction_, &QAction::triggered, this, &MainWindow::onSolve);
    viewResultsAction_ = new QAction(loadUiIcon(QStringLiteral(":/icons/contour.svg")), tr("显示云图"), this);
    viewResultsAction_->setCheckable(true);
    viewResultsAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    viewResultsAction_->setToolTip(tr("显示云图 · 选择位移分量 · Ctrl+R"));
    addAction(viewResultsAction_);
    connect(viewResultsAction_, &QAction::triggered, this, &MainWindow::showResultOptions);

    selectionPalette_ = new FloatingToolPalette(tr("工作目录与前处理"), viewport_);
    selectionPalette_->addActionButton(openAction_);
    selectionPalette_->addActionButton(pickElementAction_);
    selectionPalette_->addActionButton(pickNodeAction_);
    selectionPalette_->move(24, 72);
    analysisPalette_ = new FloatingToolPalette(tr("求解与后处理"), viewport_);
    analysisPalette_->addActionButton(solveAction_);
    analysisPalette_->addActionButton(viewResultsAction_);
    analysisPalette_->move(70, 72);
    layoutOverlays();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == viewport_ && event->type() == QEvent::Resize)
        layoutOverlays();
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::layoutOverlays() {
    if (logPanel_) {
        const int viewportHeight = viewport_->height();
        const int height = std::min(156, viewportHeight / 3);
        logPanel_->setGeometry(0, viewportHeight - height, viewport_->width(), height);
        if (viewportHeight > 0)
            viewport_->setLegendBottomInset(static_cast<double>(height) / viewportHeight);
        logPanel_->raise();
    }
    // 等视口获得实际尺寸后再定位，避免构造期间把两个工具条挤在同一处。
    if (viewport_->width() < 300 || viewport_->height() < 300)
        return;
    if (!palettesPlaced_ && selectionPalette_ && analysisPalette_) {
        selectionPalette_->move(24, 72);
        analysisPalette_->move(70, 72);
        palettesPlaced_ = true;
    }
    for (auto *palette : {selectionPalette_, analysisPalette_}) {
        if (palette) {
            palette->keepInBounds();
            palette->raise();
        }
    }
}

void MainWindow::showElementInfo(MeshId elementId) {
    const auto &elements = currentModel_.mesh.elements;
    const auto it =
        std::find_if(elements.begin(), elements.end(), [elementId](const MeshElement &element) { return element.id == elementId; });
    if (it == elements.end())
        return;
    QStringList nodeIds;
    for (MeshId id : it->nodeIds)
        nodeIds.append(QString::number(static_cast<qlonglong>(id)));
    appendLog(tr("单元 %1 · 节点 [%2]").arg(static_cast<qlonglong>(elementId)).arg(nodeIds.join(", ")));
}

void MainWindow::showNodeInfo(MeshId nodeId) {
    const auto &nodes = currentModel_.mesh.nodes;
    const auto it = std::find_if(nodes.begin(), nodes.end(), [nodeId](const MeshNode &node) { return node.id == nodeId; });
    if (it == nodes.end())
        return;
    const auto &xyz = it->coordinates;
    appendLog(tr("节点 %1 · X %2 · Y %3 · Z %4")
                  .arg(static_cast<qlonglong>(nodeId))
                  .arg(xyz[0], 0, 'g', 10)
                  .arg(xyz[1], 0, 'g', 10)
                  .arg(xyz[2], 0, 'g', 10));
}

bool MainWindow::switchWorkspace(Workspace workspace) {
    if (workspace == Workspace::Postprocess) {
        if (solving_ || !loadSavedResults())
            return false;
    } else {
        resultDialog_->hide();
        if (workspace_ == Workspace::Postprocess && modelGrid_)
            viewport_->setMesh(modelGrid_, false, false);
        resultGrid_ = nullptr;
    }
    workspace_ = workspace;
    updateActionStates();
    updateResultDisplay();
    return true;
}

void MainWindow::updateActionStates() {
    const bool hasModel = modelGrid_.GetPointer() != nullptr && !workingDirectory_.isEmpty();
    const bool hasResultFile = !resultFilePath_.isEmpty() && QFileInfo(resultFilePath_).isFile();
    openAction_->setEnabled(!solving_);
    solveAction_->setEnabled(hasModel && !solving_);
    solveAction_->setToolTip(solving_ ? tr("正在计算并保存位移…") : tr("求解并保存位移 · F5"));
    viewResultsAction_->setEnabled(hasModel && hasResultFile && !solving_);
    pickNodeAction_->setEnabled(hasModel);
    pickElementAction_->setEnabled(hasModel);
    viewResultsAction_->setChecked(workspace_ == Workspace::Postprocess && componentCombo_->currentIndex() >= 0 &&
                                   contourCheck_->isChecked());
}

void MainWindow::showResultOptions() {
    if (!switchWorkspace(Workspace::Postprocess)) {
        updateActionStates();
        return;
    }
    contourCheck_->setChecked(true);
    updateResultDisplay();
    resultDialog_->adjustSize();
    QPoint anchor = analysisPalette_->mapToGlobal(QPoint(analysisPalette_->width() + 14, 0));
    QScreen *screen = QGuiApplication::screenAt(anchor);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect bounds = screen->availableGeometry().adjusted(8, 8, -8, -8);
        anchor.setX(std::clamp(anchor.x(), bounds.left(), std::max(bounds.left(), bounds.right() - resultDialog_->width() + 1)));
        anchor.setY(std::clamp(anchor.y(), bounds.top(), std::max(bounds.top(), bounds.bottom() - resultDialog_->height() + 1)));
    }
    resultDialog_->move(anchor);
    resultDialog_->show();
    resultDialog_->raise();
    resultDialog_->activateWindow();
}

void MainWindow::updateResultDisplay() {
    const int component = componentCombo_->currentIndex();
    const bool hasComponent = component >= 0 && component <= 3;
    const bool showContour = workspace_ == Workspace::Postprocess && resultGrid_ && hasComponent && contourCheck_->isChecked();
    legendCheck_->setEnabled(hasComponent && contourCheck_->isChecked());
    viewResultsAction_->setChecked(showContour);
    if (!showContour) {
        viewport_->clearContour();
        return;
    }
    static const char *arrays[] = {"DisplacementMagnitude", "DisplacementX", "DisplacementY", "DisplacementZ"};
    static const char *titles[] = {"Total\nDisplacement", "U1\nDisplacement", "U2\nDisplacement", "U3\nDisplacement"};
    if (!viewport_->setContour(arrays[component], titles[component], legendCheck_->isChecked(),
                               analysisKeyword(currentModel_.analysisType))) {
        viewport_->clearContour();
        viewResultsAction_->setChecked(false);
        appendLog(tr("无法显示位移数组：%1").arg(QString::fromLatin1(arrays[component])));
    }
}

void MainWindow::onOpenFile() {
    if (solving_)
        return;

    while (true) {
        const QString selected = QFileDialog::getExistingDirectory(this, tr("选择包含唯一 INP 文件的工作目录"), workingDirectory_);
        if (selected.isEmpty())
            return;

        const QDir directory(selected);
        QFileInfoList inputFiles;
        const auto files = directory.entryInfoList(QDir::Files | QDir::Hidden);
        for (const QFileInfo &file : files) {
            if (file.suffix().compare(QStringLiteral("inp"), Qt::CaseInsensitive) == 0)
                inputFiles.append(file);
        }
        if (inputFiles.size() != 1) {
            QMessageBox::warning(this, tr("工作目录无效"),
                                 tr("工作目录中必须恰好包含一个 INP 文件。\n当前找到：%1 个。").arg(inputFiles.size()));
            continue;
        }

        const QFileInfo inputInfo = inputFiles.front();
        const QString inputPath = inputInfo.absoluteFilePath();
        QString error;
        const QByteArray inputHash = DisplacementResultIO::fingerprint(inputPath, error);
        if (inputHash.isEmpty()) {
            QMessageBox::warning(this, tr("读取失败"), error);
            continue;
        }

        AbaqusInpReader reader;
        auto readResult = reader.read(std::filesystem::path(inputPath.toStdWString()));
        for (const auto &warning : readResult.warnings)
            appendLog(QString::fromStdString(warning));
        if (!readResult.success()) {
            QMessageBox::warning(this, tr("模型读取失败"), QString::fromStdString(readResult.error));
            continue;
        }
        const QByteArray hashAfterRead = DisplacementResultIO::fingerprint(inputPath, error);
        if (hashAfterRead.isEmpty() || hashAfterRead != inputHash) {
            QMessageBox::warning(this, tr("模型读取失败"), tr("读取期间 INP 文件发生变化或无法访问，请重新选择。"));
            continue;
        }
        const auto built = vtkBuilder{}.build(readResult.model.mesh);
        if (!built.success()) {
            QMessageBox::warning(this, tr("网格构建失败"), built.error);
            continue;
        }

        // 验证成功后才替换当前模型；取消或失败不会丢失已打开的模型。
        workingDirectory_ = directory.absolutePath();
        inputFilePath_ = inputPath;
        inputFingerprint_ = inputHash;
        resultFilePath_ = directory.filePath(inputInfo.completeBaseName() + QStringLiteral(".displacement.csv"));
        currentModel_ = std::move(readResult.model);
        modelGrid_ = built.grid;
        resultGrid_ = nullptr;
        {
            const QSignalBlocker componentBlocker(componentCombo_);
            const QSignalBlocker contourBlocker(contourCheck_);
            const QSignalBlocker legendBlocker(legendCheck_);
            componentCombo_->setCurrentIndex(-1);
            contourCheck_->setChecked(true);
            legendCheck_->setChecked(true);
        }
        const bool hasResultFile = QFileInfo(resultFilePath_).isFile();
        switchWorkspace(Workspace::Preprocess);
        viewport_->setMesh(modelGrid_, true, true);
        setWindowTitle(tr("CAEStudio — %1 — %2").arg(QString::fromLatin1(analysisKeyword(currentModel_.analysisType)), workingDirectory_));
        appendLog(tr("工作目录：%1").arg(workingDirectory_));
        appendLog(tr("已加载模型：%1").arg(inputFilePath_));
        if (hasResultFile)
            appendLog(tr("发现结果文件：%1\n进入后处理时将检查并读取。").arg(resultFilePath_));
        return;
    }
}

bool MainWindow::loadSavedResults() {
    QString error;
    const QByteArray currentHash = DisplacementResultIO::fingerprint(inputFilePath_, error);
    if (currentHash.isEmpty()) {
        QMessageBox::warning(this, tr("结果读取失败"), error);
        return false;
    }
    if (currentHash != inputFingerprint_) {
        QMessageBox::warning(this, tr("模型已变化"), tr("INP 文件已被修改。\n请重新选择工作目录以加载新模型，然后重新求解。"));
        return false;
    }
    const auto loaded = DisplacementResultIO::load(resultFilePath_, modelGrid_, inputFingerprint_);
    if (!loaded.success()) {
        QMessageBox::warning(this, tr("结果读取失败"), loaded.error);
        return false;
    }
    resultGrid_ = loaded.grid;
    viewport_->setMesh(resultGrid_, false, false);
    appendLog(tr("已从磁盘读取位移结果：%1").arg(resultFilePath_));
    return true;
}

void MainWindow::onSolve() {
    if (solving_ || !modelGrid_ || resultFilePath_.isEmpty())
        return;
    if (currentModel_.analysisType == AnalysisType::ExplicitDynamics) {
        QMessageBox::information(this, tr("显式动力学"), tr("已识别显式动力学分析步。当前版本暂未接入显式求解器。"));
        return;
    }

    QDialog methodDialog(this);
    methodDialog.setWindowTitle(tr("线性静力求解"));
    methodDialog.setMinimumWidth(300);

    auto *methods = new QComboBox(&methodDialog);
    methods->addItem(tr("约化法"), static_cast<int>(LinearStaticConstraintMethod::Reduction));
    methods->addItem(tr("乘大数法"), static_cast<int>(LinearStaticConstraintMethod::Penalty));
    auto *form = new QFormLayout();
    form->addRow(tr("位移约束处理方法"), methods);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &methodDialog);
    connect(buttons, &QDialogButtonBox::accepted, &methodDialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &methodDialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(&methodDialog);
    layout->addLayout(form);
    layout->addWidget(buttons);
    methodDialog.adjustSize();
    if (methodDialog.exec() != QDialog::Accepted)
        return;
    const auto selectedMethod = methods->currentText();
    const auto constraintMethod = static_cast<LinearStaticConstraintMethod>(methods->currentData().toInt());

    QString error;
    const QByteArray currentHash = DisplacementResultIO::fingerprint(inputFilePath_, error);
    if (currentHash.isEmpty()) {
        QMessageBox::warning(this, tr("无法求解"), error);
        return;
    }
    if (currentHash != inputFingerprint_) {
        QMessageBox::warning(this, tr("无法求解"), tr("INP 文件已被修改，请重新选择工作目录加载模型。"));
        return;
    }

    solving_ = true;
    switchWorkspace(Workspace::Solve);
    updateActionStates();
    appendLog(tr("正在求解（%1）…").arg(selectedMethod));

    // 空字符串表示成功，后台临时结果网格不直接交给显示层。
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        const QString jobError = watcher->result();
        watcher->deleteLater();
        solving_ = false;
        if (!jobError.isEmpty()) {
            appendLog(jobError);
            updateActionStates();
            return;
        }
        updateActionStates();
        appendLog(tr("位移结果已保存：%1").arg(resultFilePath_));
        appendLog(tr("点击云图图标，读取结果并选择位移分量。"));
    });

    watcher->setFuture(
        QtConcurrent::run([model = currentModel_, outputPath = resultFilePath_, inputHash = inputFingerprint_, constraintMethod]() -> QString {
            try {
                const AnalysisProblemBuilder builder(model);
                const auto system = builder.buildLinearStaticSystem();
                LinearStaticSolver solver;
                const auto &solution = solver.solve(system, LinearStaticAlgorithm::Cholesky, constraintMethod);
                const auto built = vtkBuilder{}.buildResults(model.mesh, builder, solution);
                if (!built.success())
                    return built.error;
                QString saveError;
                if (!DisplacementResultIO::save(outputPath, built.grid, inputHash, saveError))
                    return saveError;
                return {};
            } catch (const std::exception &error) {
                return QString::fromUtf8(error.what());
            }
        }));
}
