#include "MainWindow.h"
#include "AbaqusInpReader.h"
#include "LoadData.h"
#include "MeshData.h"
#include "MeshViewport.h"
#include "vtkBuilder.h"

#include <utility>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // 初始化模型面板
    setupModelPanel();
    // 初始化信息表格
    setupInfoTable();
    // 初始化日志窗口
    setupLogWidget();
    // 初始化中心区域
    setupCentral();
    // 初始化工具栏
    setupToolBar();
    // 初始窗口尺寸
    resize(1000, 700);
    // setMinimumSize(600, 400);
}

void MainWindow::setupModelPanel() {
    /// 创建树控件, this为父对象
    modelTree_ = new QTreeWidget(this);
    modelTree_->setColumnCount(2);                       // 设置列数
    QStringList headers = {tr("项目"), tr("数量 / 值")}; // 表头
    modelTree_->setHeaderLabels(headers);                // 设置表头
    modelTree_->setColumnWidth(0, 180);                  // 设置第一列宽度
    modelTree_->setColumnWidth(1, 20);                   // 设置第二列宽度
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
    QTreeWidgetItem *elemset_item = new QTreeWidgetItem(root_item);
    elemset_item->setText(0, "Eset");
    QTreeWidgetItem *mat_item = new QTreeWidgetItem(root_item);
    mat_item->setText(0, "Material");
    boundaryItem_ = new QTreeWidgetItem(root_item);
    boundaryItem_->setText(0, tr("Boundary"));
    loadItem_ = new QTreeWidgetItem(root_item);
    loadItem_->setText(0, tr("Load"));

    modelTree_->expandAll();
    updateModelTree();
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
    left->addWidget(modelTree_);
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
    nodeItem_->setText(1, QString::number(static_cast<qulonglong>(currentMesh_.nodes.size())));
    elemItem_->setText(1, QString::number(static_cast<qulonglong>(currentMesh_.elements.size())));
    while (nsetItem_->childCount() > 0)
        delete nsetItem_->takeChild(0); // 删除旧模型节点集条目
    while (loadItem_->childCount() > 0)
        delete loadItem_->takeChild(0); // 删除旧模型荷载条目
    while (boundaryItem_->childCount() > 0)
        delete boundaryItem_->takeChild(0); // 删除旧模型边界条件条目
    nsetItem_->setText(1, QString::number(static_cast<qulonglong>(currentMesh_.nodeSets.size())));
    loadItem_->setText(1, QString::number(static_cast<qlonglong>(currentLoads_.size())));
    boundaryItem_->setText(1, QString::number(static_cast<qulonglong>(currentBoundaries_.size())));
    for (const auto &[name, nodeIds] : currentMesh_.nodeSets) {
        auto *setItem = new QTreeWidgetItem(nsetItem_);
        setItem->setText(0, QString::fromStdString(name));
        setItem->setText(1, QString::number(static_cast<qulonglong>(nodeIds.size())));
    }
    for (int i = 0; i < currentLoads_.size(); ++i) {
        auto &load = currentLoads_[i];
        auto *item = new QTreeWidgetItem(loadItem_);
        item->setText(0, tr(" %1, in %2")
                             .arg(QString::fromStdString(load.nodeSetName))
                             .arg(load.degreeOfFreedom));
        item->setText(1, QString::number(load.magnitude, 'g', 12));
        item->setToolTip(1, tr("节点集中每个节点的荷载值"));
    }
    for (const auto &boundary : currentBoundaries_) {
        auto *item = new QTreeWidgetItem(boundaryItem_);
        item->setText(0, tr("%1, in %2")
                             .arg(QString::fromStdString(boundary.nodeSetName))
                             .arg(boundary.degreeOfFreedom));
        item->setText(1, QString::number(boundary.displacement, 'g', 12));
        item->setToolTip(1, tr("节点集中每个节点的指定位移值"));
    }
    nsetItem_->setExpanded(true);
    loadItem_->setExpanded(true);
    boundaryItem_->setExpanded(true);
    modelTree_->resizeColumnToContents(1);
}

void MainWindow::setupInfoTable() {
    infoTable_ = new QTableWidget(0, 2, this);
    infoTable_->setHorizontalHeaderLabels({tr("属性"), tr("值")});
    infoTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void MainWindow::showElementInfo(MeshId elementId) {
    const auto &elements = currentMesh_.elements;
    const auto it =
        std::find_if(elements.begin(), elements.end(),
                     [elementId](const MeshElement &element) { return element.id == elementId; });
    if (it == elements.end()) {
        infoTable_->setRowCount(0);
        return;
    }
    const MeshElement &element = *it;
    QStringList nodeIds;
    for (MeshId nodeId : element.nodeIds)
        nodeIds.append(QString::number(static_cast<qlonglong>(nodeId)));
    QString typeName = tr("未知");
    switch (element.type) {
    case ElementType::Hex8:
        typeName = QStringLiteral("Hex8");
        break;
    }
    infoTable_->setRowCount(3);
    infoTable_->setItem(0, 0, new QTableWidgetItem(tr("单元编号")));
    infoTable_->setItem(0, 1,
                        new QTableWidgetItem(QString::number(static_cast<qlonglong>(element.id))));
    infoTable_->setItem(1, 0, new QTableWidgetItem(tr("单元类型")));
    infoTable_->setItem(1, 1, new QTableWidgetItem(typeName));
    infoTable_->setItem(2, 0, new QTableWidgetItem(tr("拓扑节点")));
    infoTable_->setItem(2, 1, new QTableWidgetItem(nodeIds.join(QStringLiteral(", "))));
    infoTable_->resizeColumnToContents(0);
    infoTable_->resizeColumnToContents(1);
}

void MainWindow::showNodeInfo(MeshId nodeId) {
    const auto &nodes = currentMesh_.nodes;
    const auto it = std::find_if(nodes.begin(), nodes.end(),
                                 [nodeId](const MeshNode &node) { return node.id == nodeId; });
    if (it == nodes.end()) {
        infoTable_->setRowCount(0);
        return;
    }
    const QStringList labels = {tr("节点编号"), QStringLiteral("X"), QStringLiteral("Y"),
                                QStringLiteral("Z")};
    const auto &xyz = it->coordinates;
    const QStringList values = {QString::number(static_cast<qlonglong>(it->id)),
                                QString::number(xyz[0], 'g', 12), QString::number(xyz[1], 'g', 12),
                                QString::number(xyz[2], 'g', 12)};
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
    toolBar_ = addToolBar(tr("Main")); // 添加工具栏
    toolBar_->setMovable(false);       // 禁用用户拖动
    toolBar_->setFloatable(false);     // 禁用用户浮动
    // 创建打开文件action
    QAction *actOpen_ = new QAction(tr("Open File"), this);
    actOpen_->setShortcut(QKeySequence::Open);
    actOpen_->setStatusTip(tr("Open a mesh file"));
    connect(actOpen_, &QAction::triggered, this, &MainWindow::onOpenFile);
    // 添加打开文件action到工具栏
    toolBar_->addAction(actOpen_);

    auto *pickNodeAction = toolBar_->addAction(tr("选节点"));
    pickNodeAction->setCheckable(true);
    connect(pickNodeAction, &QAction::toggled, viewport_, &MeshViewport::setNodePickingEnabled);
}

void MainWindow::onOpenFile() {
    const QString filter = tr("Mesh files (*.msh *.vtu *.vtk *.inp)");
    const QString path = QFileDialog::getOpenFileName(this, tr("Open File"), QString(), filter);
    if (path.isEmpty())
        return;
    AbaqusInpReader reader;
    auto meshdata = reader.read(path);
    for (const auto &warning : meshdata.warnings)
        logWidget_->appendPlainText(warning);
    if (!meshdata.success()) {
        logWidget_->appendPlainText(meshdata.error);
        return;
    }
    vtkBuilder builder;
    const auto result = builder.build(meshdata.mesh);
    if (!result.success()) {
        logWidget_->appendPlainText(result.error);
        return;
    }
    currentMesh_ = std::move(meshdata.mesh);
    currentLoads_ = std::move(meshdata.loads);
    currentBoundaries_ = std::move(meshdata.boundaries);
    viewport_->setMesh(result.grid);
    updateModelTree();
    infoTable_->setRowCount(0);
    // 加载成功打印日志
    logWidget_->appendPlainText(tr("模型加载成功: %1 个节点, %2 个单元.")
                                    .arg(static_cast<qulonglong>(currentMesh_.nodes.size()))
                                    .arg(static_cast<qulonglong>(currentMesh_.elements.size())));
    // for (const auto &[name, nodeIds] : currentMesh_.nodeSets) {
    //     logWidget_->appendPlainText(tr("节点集 %1: %2 个节点")
    //                                     .arg(QString::fromStdString(name))
    //                                     .arg(static_cast<qulonglong>(nodeIds.size())));
    // }
    // for (const auto &load : currentLoads_) {
    //     logWidget_->appendPlainText(tr("荷载: 节点集 %1, 方向 %2, 每节点力值 %3")
    //                                     .arg(QString::fromStdString(load.nodeSetName))
    //                                     .arg(load.degreeOfFreedom)
    //                                     .arg(load.magnitude, 0, 'g', 12));
    // }
    // for (const auto &boundary : currentBoundaries_) {
    //     logWidget_->appendPlainText(tr("边界条件: 节点集 %1, 自由度 %2, 指定位移 %3")
    //                                     .arg(QString::fromStdString(boundary.nodeSetName))
    //                                     .arg(boundary.degreeOfFreedom)
    //                                     .arg(boundary.displacement, 0, 'g', 12));
    // }
}
