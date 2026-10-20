#include "MainWindow.h"

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
    vtkWidget_ = new QVTKOpenGLNativeWidget(this);

    // 创建右侧区域: vtk渲染窗口 + 日志窗口
    QSplitter *right = new QSplitter(Qt::Vertical, this);
    right->addWidget(vtkWidget_);
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

    auto renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    vtkWidget_->setRenderWindow(renderWindow);
    renderer_ = vtkSmartPointer<vtkRenderer>::New();
    renderWindow->AddRenderer(renderer_);
    vtkWidget_->renderWindow()->Render();
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

    if (loadAbaqusInp(path))
        logWidget_->appendPlainText(tr("Successfully loaded!"));
    else
        logWidget_->appendPlainText(tr("Failed to load file!"));
}

/**
 * @brief 加载 ABAQUS inp 文件
 * @param path 文件路径
 * @return true 表示加载成功,false 表示加载失败
 */
bool MainWindow::loadAbaqusInp(const QString &path) {
    renderer_->RemoveAllViewProps();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logWidget_->appendPlainText(tr("Failed to open: %1").arg(path));
        return false;
    }

    QTextStream in(&f);
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto ugrid = vtkSmartPointer<vtkUnstructuredGrid>::New();

    QHash<int, vtkIdType> id2pid;
    id2pid.reserve(1000000);

    enum Section { None, Node, ElementC3D8 };
    Section sec = None;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith("**"))
            continue;
        if (line.startsWith("*")) {
            const QString upper = line.toUpper();
            if (upper.startsWith("*NODE")) {
                sec = Node;
            } else if (upper.startsWith("*ELEMENT")) {
                if (upper.contains("TYPE=C3D8"))
                    sec = ElementC3D8;
                else
                    sec = None;
            } else {
                sec = None;
            }
            continue;
        }

        const auto parts = line.split(',', Qt::SkipEmptyParts);

        if (sec == Node) {
            if (parts.size() < 4)
                continue;
            bool okId = false, okx = false, oky = false, okz = false;
            int nid = parts[0].trimmed().toInt(&okId);
            double x = parts[1].trimmed().toDouble(&okx);
            double y = parts[2].trimmed().toDouble(&oky);
            double z = parts[3].trimmed().toDouble(&okz);
            if (!okId || !okx || !oky || !okz)
                continue;

            vtkIdType pid = points->InsertNextPoint(x, y, z);
            id2pid.insert(nid, pid);
        } else if (sec == ElementC3D8) {
            if (parts.size() < 9)
                continue;
            bool ok = false;
            int eid = parts[0].trimmed().toInt(&ok);
            Q_UNUSED(eid);
            if (!ok)
                continue;

            vtkIdType conn[8];
            bool allok = true;
            for (int i = 0; i < 8; i++) {
                bool okn = false;
                int nid = parts[i + 1].trimmed().toInt(&okn);
                if (!okn || !id2pid.contains(nid)) {
                    allok = false;
                    break;
                }
                conn[i] = id2pid[nid];
            }
            if (!allok)
                continue;
            ugrid->InsertNextCell(VTK_HEXAHEDRON, 8, conn);
        }
    }

    f.close();
    ugrid->SetPoints(points);
    grid_ = ugrid;
    showGrid();
    return true;
}

/**
 * @brief 显示网格
 */
void MainWindow::showGrid() {
    if (!renderer_)
        return;

    auto mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    mapper->SetInputData(grid_);
    auto meshActor = vtkSmartPointer<vtkActor>::New();
    meshActor->SetMapper(mapper);
    // set mesh property here.
    meshActor->GetProperty()->SetColor(0.8, 0.8, 0.9);
    meshActor->GetProperty()->EdgeVisibilityOn();
    meshActor->GetProperty()->SetEdgeColor(0.2, 0.2, 0.3);
    meshActor->GetProperty()->SetLineWidth(1.5);
    meshActor_ = meshActor;

    renderer_->AddActor(meshActor_);
    renderer_->ResetCamera();
    vtkWidget_->renderWindow()->Render();
}