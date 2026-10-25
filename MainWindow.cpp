#include "MainWindow.h"
#include "AbaqusInpReader.h"
#include "MeshViewport.h"

/**
 * @brief MainWindow 构造函数
 * @param parent 父窗口指针
 */
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

/**
 * @brief 初始化模型面板
 */
void MainWindow::setupModelPanel() {
    /// 创建树控件, this为父对象
    modelTree_ = new QTreeWidget(this);
    modelTree_->setColumnCount(2);          // 设置列数
    QStringList headers = {"Type", "size"}; // 表头
    modelTree_->setHeaderLabels(headers);   // 设置表头
    modelTree_->setColumnWidth(0, 180);     // 设置第一列宽度
    modelTree_->setColumnWidth(1, 20);      // 设置第二列宽度

    // 创建根节点
    QTreeWidgetItem *root_item = new QTreeWidgetItem(modelTree_);
    root_item->setText(0, "Model-1");

    // 创建子节点
    QTreeWidgetItem *node_item = new QTreeWidgetItem(root_item);
    node_item->setText(0, "Node");
    QTreeWidgetItem *elem_item = new QTreeWidgetItem(root_item);
    elem_item->setText(0, "Element");
    QTreeWidgetItem *nset_item = new QTreeWidgetItem(root_item);
    nset_item->setText(0, "Nset");
    QTreeWidgetItem *elemset_item = new QTreeWidgetItem(root_item);
    elemset_item->setText(0, "Eset");
    QTreeWidgetItem *mat_item = new QTreeWidgetItem(root_item);
    mat_item->setText(0, "Material");
    QTreeWidgetItem *bc_item = new QTreeWidgetItem(root_item);
    bc_item->setText(0, "Boundary");
    QTreeWidgetItem *job_item = new QTreeWidgetItem(root_item);
    job_item->setText(0, "Job");

    // 展开所有节点
    modelTree_->expandAll();
}

/**
 * @brief 初始化中心区域
 */
void MainWindow::setupCentral() {
    // 创建vtk渲染窗口
    viewport_ = new MeshViewport(this);

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

/**
 * @brief 初始化信息表格
 * @details 创建信息表格并加入布局
 */
void MainWindow::setupInfoTable() {
    const QStringList vertical = {"Element"};
    const QStringList horizontal = {"Item", "Count"};
    QTableWidgetItem *elem = new QTableWidgetItem(vertical[0]);
    infoTable_ = new QTableWidget(vertical.size(), horizontal.size(), this);
    infoTable_->setHorizontalHeaderLabels(horizontal);
    infoTable_->setItem(0, 0, elem);
}

/**
 * @brief 初始化日志窗口
 */
void MainWindow::setupLogWidget() {
    logWidget_ = new QPlainTextEdit(this);
    logWidget_->setReadOnly(true);
}

/**
 * @brief 初始化工具栏
 */
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
}

/**
 * @brief 打开文件
 */
void MainWindow::onOpenFile() {
    const QString filter = tr("Mesh files (*.msh *.vtu *.vtk *.inp)");
    const QString path = QFileDialog::getOpenFileName(this, tr("Open File"), QString(), filter);
    if (path.isEmpty())
        return;

    AbaqusInpReader reader;
    const auto result = reader.read(path);
    if (!result.success()) {
        logWidget_->appendPlainText(result.error);
        return;
    }
    viewport_->setMesh(result.grid);

    for (const auto &warning : result.warnings)
        logWidget_->appendPlainText(warning);
    logWidget_->appendPlainText(tr("Successfully loaded!"));
}
