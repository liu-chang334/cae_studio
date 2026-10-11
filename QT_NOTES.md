<!-- omit from toc -->
# Qt_Notes
- [1. QPlainTextEdit](#1-qplaintextedit)
- [2. QSplitter](#2-qsplitter)
- [3. QTableWidgetItem](#3-qtablewidgetitem)
- [4. QTreeWidget](#4-qtreewidget)
- [5. QVTKOpenGLNativeWidget](#5-qvtkopenglnativewidget)

## 1. QPlainTextEdit

## 2. QSplitter

## 3. QTableWidgetItem

## 4. QTreeWidget
### 4.1. 创建 

`QTreeWidget` 用于显示具有父子层级关系的数据

```cpp
modelTree_ = new QTreeWidget(this);
```

`this`是树控件的父对象, 父对象被销毁时, Qt自动销毁该树控件

### 4.2. 设置列数

使用`setColumnCount(int)`设置树控件的列数:

```cpp
modelTree_->setColumnCount(2);
```

索引从0开始

### 4.3. 设置表头

使用`setHeaderLabels(QStringList)`设置单列表头:

```cpp
modelTree_->setHeaderLabels({"Type", "size"});
```

### 4.4. 设置列宽

使用`setColumnWidth(int column, int width)`设置指定列的宽度:

```cpp
///@param column 列索引
///@param width 列宽度
modelTree_->setColumnWidth(0, 180);
modelTree_->setColumnWidth(1,60);
```

### 4.5. 创建根节点

以`QTreeWidget`为父对象, 创建根节点:

```cpp
auto* root_item = new QTreeWidgetItem(modelTree_);
root_item->setText(0, "Model-1");
```

`setText(int column, QString text)`设置指定列的文本:

```cpp
///@param column 列索引
///@param text 列文本
root_item->setText(0, "Model-1");
root_item->setText(1, "1");
```

| Type    | Size |
|---------|------|
| Model-1 |   1  |

### 4.6. 创建子节点

将另一个`QTreeWidgetItem`作为父对象:

```cpp
auto* node_item =  new QTreeWidgetItem(root_item);
node_item->setText(0, "Node");  
node_item->setText(1, "100");
```

| Type | Size |
|------|------|
| Node | 100  |

### 4.7. 展开节点

展开树中的全部节点：

```cpp
modelTree_->expandAll();
```

只展开指定节点：

```cpp
rootItem->setExpanded(true);
```

折叠指定节点：

```cpp
rootItem->setExpanded(false);
```

### 4.8. 对象所有权

Qt 使用父子对象机制管理内存:

```cpp
modelTree_ = new QTreeWidget(this);
auto* rootItem = new QTreeWidgetItem(modelTree_);
auto* childItem = new QTreeWidgetItem(rootItem);
```

对象之间的关系为：

```text
MainWindow
└── modelTree_
    └── rootItem
        └── childItem
```

父对象销毁时，其子对象也会被自动销毁

## 5. QVTKOpenGLNativeWidget