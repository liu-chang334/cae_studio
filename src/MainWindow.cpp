#include "MainWindow.h"
#include "AbaqusInpReader.h"
#include "LoadData.h"
#include "MeshData.h"
#include "MeshViewport.h"
#include "vtkBuilder.h"

#include <QActionGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <utility>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupModelPanel();
    setupInfoTable();
    setupLogWidget();
    setupWorkspacePanels();
    setupCentral();
    setupToolBar();

    switchWorkspace(Workspace::Preprocess);
    resize(1100, 760);
}

void MainWindow::setupModelPanel() {
    /// 创建树控件, this为父对象
    modelTree_ = new QTreeWidget(this);
    modelTree_->setColumnCount(2);  // 设置列数
    QStringList headers = {tr("项目"), tr("数量 / 值")};  // 表头
    modelTree_->setHeaderLabels(headers);  // 设置表头
    modelTree_->setColumnWidth(0, 180);  // 设置第一列宽度
    modelTree_->setColumnWidth(1, 20);  // 设置第二列宽度
    // 创建根节点
    QTreeWidgetItem *root_item = new QTreeWidgetItem(modelTree_);
    root_item->setText(0, "Model-1");
    // 创建子节点
    nodeItem_ = new QTreeWidgetItem(root_item);
    nodeItem_->setText(0, "Node");
    elemItem_ = new QTreeWidgetItem(root_item);
    elemItem_->setText(0, "Element");
    nsetItem_ = new QTreeWidgetItem(root_item);
    nsetItem_->setText(0, "Nset");
    esetItem_ = new QTreeWidgetItem(root_item);
    esetItem_->setText(0, "Eset");
    materialItem_ = new QTreeWidgetItem(root_item);
    materialItem_->setText(0, tr("Material"));
    boundaryItem_ = new QTreeWidgetItem(root_item);
    boundaryItem_->setText(0, tr("Boundary"));
    loadItem_ = new QTreeWidgetItem(root_item);
    loadItem_->setText(0, tr("Load"));

    modelTree_->expandAll();
    updateModelTree();
}

void MainWindow::setupWorkspacePanels() {
    // 求解工作区
    solvePanel_ = new QWidget(this);
    auto *solveLayout = new QVBoxLayout(solvePanel_);
    auto *solveTitle = new QLabel(tr("线性静力分析"), solvePanel_);
    solveStatusLabel_ = new QLabel(tr("尚未求解"), solvePanel_);
    solveStatusLabel_->setWordWrap(true);
    solveButton_ = new QPushButton(tr("开始求解"), solvePanel_);
    viewResultsButton_ = new QPushButton(tr("查看结果"), solvePanel_);
    solveLayout->addWidget(solveTitle);
    solveLayout->addWidget(solveStatusLabel_);
    solveLayout->addWidget(solveButton_);
    solveLayout->addWidget(viewResultsButton_);
    solveLayout->addStretch();
    connect(solveButton_, &QPushButton::clicked, this, &MainWindow::onSolve);
    connect(viewResultsButton_, &QPushButton::clicked, this, [this]() { switchWorkspace(Workspace::Postprocess); });

    // 后处理工作区
    resultPanel_ = new QWidget(this);
    auto *resultLayout = new QVBoxLayout(resultPanel_);
    auto *title = new QLabel(tr("场结果"), resultPanel_);
    resultLayout->addWidget(title);
    fieldCombo_ = new QComboBox(resultPanel_);
    fieldCombo_->addItem(tr("请选择物理量"));
    fieldCombo_->addItem(tr("位移 U"));
    fieldCombo_->addItem(tr("约束反力 RF"));
    componentCombo_ = new QComboBox(resultPanel_);
    componentCombo_->addItem(tr("总量 / 模长"));
    componentCombo_->addItem(tr("X 分量"));
    componentCombo_->addItem(tr("Y 分量"));
    componentCombo_->addItem(tr("Z 分量"));
    auto *form = new QFormLayout();
    form->addRow(tr("物理量"), fieldCombo_);
    form->addRow(tr("分量"), componentCombo_);
    resultLayout->addLayout(form);
    contourCheck_ = new QCheckBox(tr("显式云图"), resultPanel_);
    contourCheck_->setChecked(true);
    legendCheck_ = new QCheckBox(tr("显示色标"), resultPanel_);
    legendCheck_->setChecked(true);
    resultLayout->addWidget(contourCheck_);
    resultLayout->addWidget(legendCheck_);
    auto *description = new QLabel(tr("数据位置: 节点\n"
                                      "坐标系: 全局坐标系\n"
                                      "色标范围: 自动\n"
                                      "单位: 与输入模型单位制一致"),
                                   resultPanel_);
    description->setWordWrap(true);
    resultLayout->addWidget(description);
    resultLayout->addStretch();
    connect(fieldCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::updateResultDisplay);
    connect(componentCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::updateResultDisplay);
    connect(contourCheck_, &QCheckBox::toggled, this, &MainWindow::updateResultDisplay);
    connect(legendCheck_, &QCheckBox::toggled, this, &MainWindow::updateResultDisplay);
}

void MainWindow::setupCentral() {
    // 创建vtk渲染窗口
    viewport_ = new MeshViewport(this);
    connect(viewport_, &MeshViewport::elementPicked, this, [this](MeshId elementId) {
        showElementInfo(elementId);
        logWidget_->appendPlainText(tr("选中单元：%1").arg(static_cast<qlonglong>(elementId)));
    });
    connect(viewport_, &MeshViewport::nodePicked, this, [this](MeshId nodeId) {
        showNodeInfo(nodeId);
        logWidget_->appendPlainText(tr("选中节点：%1").arg(static_cast<qlonglong>(nodeId)));
    });
    connect(viewport_, &MeshViewport::selectionCleared, this, [this]() {
        infoTable_->setRowCount(0);
        logWidget_->appendPlainText(tr("已取消选择"));
    });
    // 创建右侧区域: vtk渲染窗口 + 日志窗口
    QSplitter *right = new QSplitter(Qt::Vertical, this);
    right->addWidget(viewport_);
    right->addWidget(logWidget_);
    right->setStretchFactor(0, 5);
    right->setStretchFactor(1, 2);
    right->setSizes({760, 190});
    // 创建左侧区域: 模型树 + 信息表格
    QSplitter *left = new QSplitter(Qt::Vertical, this);
    navigationStack_ = new QStackedWidget(this);
    navigationStack_->addWidget(modelTree_);
    navigationStack_->addWidget(solvePanel_);
    navigationStack_->addWidget(resultPanel_);
    left->addWidget(navigationStack_);
    left->addWidget(infoTable_);
    left->setStretchFactor(0, 5);
    left->setStretchFactor(1, 2);
    left->setSizes({760, 190});
    // 整体布局
    QSplitter *splitter = new QSplitter(this);
    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({250, 950});
    setCentralWidget(splitter);
}

void MainWindow::updateModelTree() {
    nodeItem_->setText(1, QString::number(static_cast<qulonglong>(currentModel_.mesh.nodes.size())));
    elemItem_->setText(1, QString::number(static_cast<qulonglong>(currentModel_.mesh.elements.size())));
    while (nsetItem_->childCount() > 0)
        delete nsetItem_->takeChild(0);  // 删除旧模型节点集条目
    while (esetItem_->childCount() > 0)
        delete esetItem_->takeChild(0);  // 删除旧模型单元集条目
    while (loadItem_->childCount() > 0)
        delete loadItem_->takeChild(0);  // 删除旧模型荷载条目
    while (boundaryItem_->childCount() > 0)
        delete boundaryItem_->takeChild(0);  // 删除旧模型边界条件条目
    while (materialItem_->childCount() > 0)
        delete materialItem_->takeChild(0);  // 删除旧模型材料条目
    nsetItem_->setText(1, QString::number(static_cast<qulonglong>(currentModel_.nodeSets.size())));
    esetItem_->setText(1, QString::number(static_cast<qulonglong>(currentModel_.elemSets.size())));
    loadItem_->setText(1, QString::number(static_cast<qlonglong>(currentModel_.loads.size())));
    boundaryItem_->setText(1, QString::number(static_cast<qulonglong>(currentModel_.boundaries.size())));
    materialItem_->setText(1, QString::number(static_cast<qulonglong>(currentModel_.materials.size())));
    for (const auto &[name, nodeIds] : currentModel_.nodeSets) {
        auto *setItem = new QTreeWidgetItem(nsetItem_);
        setItem->setText(0, QString::fromStdString(name));
        setItem->setText(1, QString::number(static_cast<qulonglong>(nodeIds.size())));
    }
    for (const auto &[name, elemIds] : currentModel_.elemSets) {
        auto *setItem = new QTreeWidgetItem(esetItem_);
        setItem->setText(0, QString::fromStdString(name));
        setItem->setText(1, QString::number(static_cast<qulonglong>(elemIds.size())));
    }
    for (int i = 0; i < currentModel_.loads.size(); ++i) {
        auto &load = currentModel_.loads[i];
        auto *item = new QTreeWidgetItem(loadItem_);
        item->setText(0, tr(" %1, in %2").arg(QString::fromStdString(load.nodeSetName)).arg(load.degreeOfFreedom));
        item->setText(1, QString::number(load.magnitude, 'g', 12));
        item->setToolTip(1, tr("节点集中每个节点的荷载值"));
    }
    for (const auto &boundary : currentModel_.boundaries) {
        auto *item = new QTreeWidgetItem(boundaryItem_);
        item->setText(0, tr("%1, in %2").arg(QString::fromStdString(boundary.nodeSetName)).arg(boundary.degreeOfFreedom));
        item->setText(1, QString::number(boundary.displacement, 'g', 12));
        item->setToolTip(1, tr("节点集中每个节点的指定位移值"));
    }
    for (const auto &material : currentModel_.materials) {
        auto *item = new QTreeWidgetItem(materialItem_);
        item->setText(0, QString::fromStdString(material.name));
        if (material.elastic) {
            const auto &elastic = *material.elastic;
            auto *elasticItem = new QTreeWidgetItem(item);
            elasticItem->setText(0, tr("Elastic"));
            auto *youngItem = new QTreeWidgetItem(elasticItem);
            youngItem->setText(0, tr("E"));
            youngItem->setText(1, QString::number(elastic.youngModulus, 'g', 12));
            auto *poissonItem = new QTreeWidgetItem(elasticItem);
            poissonItem->setText(0, tr("nu"));
            poissonItem->setText(1, QString::number(elastic.poissonRatio, 'g', 12));
            elasticItem->setExpanded(false);
        }
        item->setExpanded(true);
    }
    nsetItem_->setExpanded(true);
    loadItem_->setExpanded(true);
    boundaryItem_->setExpanded(true);
    materialItem_->setExpanded(true);
    modelTree_->resizeColumnToContents(1);
}

void MainWindow::setupInfoTable() {
    infoTable_ = new QTableWidget(0, 2, this);
    infoTable_->setHorizontalHeaderLabels({tr("属性"), tr("值")});
    infoTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void MainWindow::showElementInfo(MeshId elementId) {
    const auto &elements = currentModel_.mesh.elements;
    const auto it =
        std::find_if(elements.begin(), elements.end(), [elementId](const MeshElement &element) { return element.id == elementId; });
    if (it == elements.end()) {
        infoTable_->setRowCount(0);
        return;
    }
    const MeshElement &element = *it;
    QStringList nodeIds;
    for (MeshId nodeId : element.nodeIds)
        nodeIds.append(QString::number(static_cast<qlonglong>(nodeId)));
    QString typeName = tr("未知");
    switch (element.topology) {
    case CellTopology::HEXAHEDRON:
        typeName = QStringLiteral("Hexahedral");
        break;
    }
    infoTable_->setRowCount(3);
    infoTable_->setItem(0, 0, new QTableWidgetItem(tr("单元编号")));
    infoTable_->setItem(0, 1, new QTableWidgetItem(QString::number(static_cast<qlonglong>(element.id))));
    infoTable_->setItem(1, 0, new QTableWidgetItem(tr("单元类型")));
    infoTable_->setItem(1, 1, new QTableWidgetItem(typeName));
    infoTable_->setItem(2, 0, new QTableWidgetItem(tr("拓扑节点")));
    infoTable_->setItem(2, 1, new QTableWidgetItem(nodeIds.join(QStringLiteral(", "))));
    infoTable_->resizeColumnToContents(0);
    infoTable_->resizeColumnToContents(1);
}

void MainWindow::showNodeInfo(MeshId nodeId) {
    const auto &nodes = currentModel_.mesh.nodes;
    const auto it = std::find_if(nodes.begin(), nodes.end(), [nodeId](const MeshNode &node) { return node.id == nodeId; });
    if (it == nodes.end()) {
        infoTable_->setRowCount(0);
        return;
    }
    const QStringList labels = {tr("节点编号"), QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")};
    const auto &xyz = it->coordinates;
    const QStringList values = {QString::number(static_cast<qlonglong>(it->id)), QString::number(xyz[0], 'g', 12),
                                QString::number(xyz[1], 'g', 12), QString::number(xyz[2], 'g', 12)};
    infoTable_->setRowCount(4);
    for (int row = 0; row < 4; ++row) {
        infoTable_->setItem(row, 0, new QTableWidgetItem(labels[row]));
        infoTable_->setItem(row, 1, new QTableWidgetItem(values[row]));
    }
    infoTable_->resizeColumnsToContents();
}

void MainWindow::setupLogWidget() {
    logWidget_ = new QPlainTextEdit(this);
    logWidget_->setReadOnly(true);
}

void MainWindow::setupToolBar() {
    // 创建操作
    openAction_ = new QAction(tr("打开模型"), this);
    openAction_->setShortcut(QKeySequence::Open);
    connect(openAction_, &QAction::triggered, this, &MainWindow::onOpenFile);
    solveAction_ = new QAction(tr("开始求解"), this);
    connect(solveAction_, &QAction::triggered, this, &MainWindow::onSolve);
    viewResultsAction_ = new QAction(tr("查看结果"), this);
    connect(viewResultsAction_, &QAction::triggered, this, [this]() { switchWorkspace(Workspace::Postprocess); });
    auto *fitAction = new QAction(tr("适应窗口"), this);
    connect(fitAction, &QAction::triggered, viewport_, &MeshViewport::resetCamera);
    pickNodeAction_ = new QAction(tr("选择节点"), this);
    pickElementAction_ = new QAction(tr("选择单元"), this);
    pickNodeAction_->setCheckable(true);
    pickElementAction_->setCheckable(true);
    auto *pickGroup = new QActionGroup(this);
    pickGroup->setExclusive(true);
    pickGroup->addAction(pickNodeAction_);
    pickGroup->addAction(pickElementAction_);
    pickElementAction_->setChecked(true);
    connect(pickNodeAction_, &QAction::toggled, viewport_, &MeshViewport::setNodePickingEnabled);
    // 菜单
    auto *fileMenu = menuBar()->addMenu(tr("文件"));
    fileMenu->addAction(openAction_);
    auto *viewMenu = menuBar()->addMenu(tr("视图"));
    viewMenu->addAction(fitAction);
    auto *modelMenu = menuBar()->addMenu(tr("模型"));
    modelMenu->addAction(pickNodeAction_);
    modelMenu->addAction(pickElementAction_);
    auto *analysisMenu = menuBar()->addMenu(tr("分析"));
    analysisMenu->addAction(solveAction_);
    auto *resultMenu = menuBar()->addMenu(tr("结果"));
    resultMenu->addAction(viewResultsAction_);
    // 公共工具栏
    toolBar_ = addToolBar(tr("公共"));
    toolBar_->setMovable(true);
    toolBar_->setFloatable(true);
    toolBar_->addAction(openAction_);
    toolBar_->addAction(fitAction);
    toolBar_->addSeparator();
    workspaceTabs_ = new QTabBar(toolBar_);
    workspaceTabs_->setExpanding(false);
    workspaceTabs_->addTab(tr("前处理"));
    workspaceTabs_->addTab(tr("求解"));
    workspaceTabs_->addTab(tr("后处理"));
    toolBar_->addWidget(workspaceTabs_);
    connect(workspaceTabs_, &QTabBar::currentChanged, this, [this](int index) {
        if (index >= 0 && index <= 2)
            switchWorkspace(static_cast<Workspace>(index));
    });
    // 当前工作区工具栏
    addToolBarBreak();
    workspaceToolBar_ = addToolBar(tr("工作区"));
    workspaceToolBar_->setMovable(false);
    workspaceToolBar_->setFloatable(false);
    workspaceToolBar_->addAction(pickNodeAction_);
    workspaceToolBar_->addAction(pickElementAction_);
    workspaceToolBar_->addAction(solveAction_);
    workspaceToolBar_->addAction(viewResultsAction_);
}

void MainWindow::switchWorkspace(Workspace workspace) {
    if (workspace == Workspace::Postprocess && !resultGrid_)
        return;
    workspace_ = workspace;
    const int index = static_cast<int>(workspace);
    // 防止setCurrentIndex再次触发工作区切换
    const QSignalBlocker blocker(workspaceTabs_);
    workspaceTabs_->setCurrentIndex(index);
    navigationStack_->setCurrentIndex(index);
    infoTable_->setRowCount(0);
    updateActionStates();
    updateResultDisplay();
    switch (workspace_) {
    case Workspace::Preprocess:
        statusBar()->showMessage(tr("前处理: 查看模型"));
        break;
    case Workspace::Solve:
        statusBar()->showMessage(tr("求解: 运行分析"));
        break;
    case Workspace::Postprocess:
        statusBar()->showMessage(tr("后处理: 查看结果"));
        break;
    }
}

void MainWindow::updateActionStates() {
    const bool hasModel = modelGrid_.GetPointer() != nullptr;
    const bool hasResults = resultGrid_.GetPointer() != nullptr;
    const bool canSolve = hasModel && !solving_;
    const bool showPicking = workspace_ != Workspace::Solve;

    openAction_->setEnabled(!solving_);
    solveAction_->setEnabled(canSolve);
    solveButton_->setEnabled(canSolve);

    viewResultsAction_->setEnabled(hasResults);
    viewResultsButton_->setEnabled(hasResults);
    workspaceTabs_->setTabEnabled(2, hasResults);

    pickNodeAction_->setEnabled(hasModel);
    pickElementAction_->setEnabled(hasModel);

    pickNodeAction_->setVisible(showPicking);
    pickElementAction_->setVisible(showPicking);

    solveAction_->setVisible(workspace_ == Workspace::Solve);
    viewResultsAction_->setVisible(workspace_ == Workspace::Solve);

    resultPanel_->setEnabled(hasResults);
}

void MainWindow::updateResultDisplay() {
    const bool hasField = fieldCombo_->currentIndex() > 0;
    const bool showContour =
        workspace_ == Workspace::Postprocess && resultGrid_.GetPointer() != nullptr && hasField && contourCheck_->isChecked();

    componentCombo_->setEnabled(hasField);
    legendCheck_->setEnabled(hasField && contourCheck_->isChecked());

    if (!showContour) {
        viewport_->clearContour();
        return;
    }

    const bool displacement = fieldCombo_->currentIndex() == 1;
    const QString prefix = displacement ? QStringLiteral("Displacement") : QStringLiteral("Reaction");

    const QString symbol = displacement ? QStringLiteral("U") : QStringLiteral("RF");

    static const char *suffixes[] = {"Magnitude", "X", "Y", "Z"};

    const int component = componentCombo_->currentIndex();
    const QString arrayName = prefix + QString::fromLatin1(suffixes[component]);

    const QString title =
        component == 0 ? symbol + QStringLiteral(" Magnitude") : symbol + QString::number(component) + QStringLiteral(" (global)");

    const QByteArray arrayBytes = arrayName.toUtf8();
    const QByteArray titleBytes = title.toUtf8();

    if (!viewport_->setContour(arrayBytes.constData(), titleBytes.constData(), legendCheck_->isChecked())) {
        viewport_->clearContour();
        logWidget_->appendPlainText(tr("无法显示结果数组：%1").arg(arrayName));
    }
}

void MainWindow::onOpenFile() {
    if (solving_)
        return;
    const QString filter = tr("Mesh files (*.msh *.vtu *.vtk *.inp)");
    const QString path = QFileDialog::getOpenFileName(this, tr("Open File"), QString(), filter);
    if (path.isEmpty())
        return;
    AbaqusInpReader reader;
    auto readresult = reader.read(std::filesystem::path(path.toStdWString()));
    for (const std::string &warning : readresult.warnings)
        logWidget_->appendPlainText(QString::fromStdString(warning));
    if (!readresult.success()) {
        logWidget_->appendPlainText(QString::fromStdString(readresult.error));
        return;
    }
    vtkBuilder builder;
    const auto vtkdata = builder.build(readresult.model.mesh);
    if (!vtkdata.success()) {
        logWidget_->appendPlainText(vtkdata.error);
        return;
    }
    currentModel_ = std::move(readresult.model);
    modelGrid_ = vtkdata.grid;
    resultGrid_ = nullptr;

    {
        const QSignalBlocker fieldBlocker(fieldCombo_);
        const QSignalBlocker componentBlocker(componentCombo_);
        fieldCombo_->setCurrentIndex(0);
        componentCombo_->setCurrentIndex(0);
    }

    solveStatusLabel_->setText(tr("尚未求解"));
    updateModelTree();
    infoTable_->setRowCount(0);

    // 先清理工作区状态，再启动新模型的导入动画
    switchWorkspace(Workspace::Preprocess);
    viewport_->setMesh(modelGrid_, true, true);
    // 加载成功打印日志
    logWidget_->appendPlainText(tr("模型加载成功: %1 个节点, %2 个单元, %3 种材料.")
                                    .arg(static_cast<qulonglong>(currentModel_.mesh.nodes.size()))
                                    .arg(static_cast<qulonglong>(currentModel_.mesh.elements.size()))
                                    .arg(static_cast<qulonglong>(currentModel_.materials.size())));
}

void MainWindow::onSolve() {
    if (solving_ || !modelGrid_)
        return;

    solving_ = true;
    solveStatusLabel_->setText(tr("正在求解…"));
    switchWorkspace(Workspace::Solve);
    logWidget_->appendPlainText(tr("正在求解…"));

    auto *watcher = new QFutureWatcher<vtkBuildResult>(this);

    connect(watcher, &QFutureWatcher<vtkBuildResult>::finished, this, [this, watcher]() {
        const auto result = watcher->result();
        watcher->deleteLater();
        solving_ = false;

        if (!result.success()) {
            solveStatusLabel_->setText(resultGrid_ ? tr("本次求解失败，保留上次成功结果") : tr("求解失败"));
            logWidget_->appendPlainText(result.error);
            updateActionStates();
            return;
        }

        resultGrid_ = result.grid;

        // 同一模型只补充结果数据，保留当前相机。
        // 此处不启用云图。
        viewport_->setMesh(resultGrid_, false, false);

        infoTable_->setRowCount(0);
        solveStatusLabel_->setText(tr("求解完成"));
        updateActionStates();

        // 若用户在求解过程中切换了工作区，
        // 按当前工作区及原有选择恢复显示。
        updateResultDisplay();

        logWidget_->appendPlainText(tr("求解完成。点击“查看结果”后选择物理量和分量。"));
    });

    watcher->setFuture(QtConcurrent::run([model = currentModel_]() -> vtkBuildResult {
        try {
            const AnalysisModel analysis(model);
            const auto stiffness = analysis.assembleElasticStiffness();
            const auto solution = analysis.solveLinearStatic(stiffness);

            return vtkBuilder{}.buildResults(model.mesh, analysis, solution);
        } catch (const std::exception &error) {
            return {nullptr, QString::fromUtf8(error.what())};
        }
    }));
}