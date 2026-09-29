#include "MainWindow.h"

/**
 * @brief MainWindow 构造函数
 * @param parent 父窗口指针
 */
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // 1. 初始化模型面板
    setupModelPanel();
}

/**
 * @brief 初始化模型面板
 */
void MainWindow::setupModelPanel() {
    // 创建模型树
    modelTree_ = new QTreeWidget(this);
    modelTree_->setColumnCount(2);
    QStringList headers = {"Type", "size"};
    modelTree_->setHeaderLabels(headers);
    modelTree_->setColumnWidth(0, 180);
    modelTree_->setColumnWidth(1, 20);

    // 根节点
    QTreeWidgetItem *root_item = new QTreeWidgetItem(modelTree_);
    root_item->setText(0, "Model-1");

    // 子节点
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
void setupCentral() {}