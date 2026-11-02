<!-- omit from toc -->
# Qt_Notes
- [1. QPlainTextEdit](#1-qplaintextedit)
- [2. QSplitter](#2-qsplitter)
- [3. QTableWidgetItem](#3-qtablewidgetitem)
- [4. QTreeWidget](#4-qtreewidget)
- [5. QVTKOpenGLNativeWidget](#5-qvtkopenglnativewidget)

## 1. QPlainTextEdit

`QPlainTextEdit`用于显示和编辑多行纯文本的控件, 如日志窗口

### 1.1. 引入头文件

```cpp
#include <QPlainTextEdit>
```

### 1.2. 指针声明与对象创建

声明指针并将其初始化为`nullptr`, 不会创建控件:

```cpp
QPlainTextEdit* textEdit = nullptr;
```

使用关键字`new`和构造函数`QPlainTextEdit(QWidget* parent=nullptr)`创建, 指针才指向实际控件:

```cpp
///@param parent 父控件指针
QPlainTextEdit* textEdit = new QPlainTextEdit(parent);
```

### 1.3. 设置只读状态

使用`setReadOnly(bool)`设置是否允许用户编辑文本:

```cpp
///@param readOnly 是否只读标识
textEdit->setReadOnly(true);
```

### 1.4. 加入分割器

使用`QSplitter::addWidget(QWidget* widget)`将控件加入分割器:

```cpp
///@param widget 要加入分割器的控件
splitter->addWidget(textEdit);
```

控件排列方向由分割器决定:

- `QSplitter::Horizontal` 水平方向
- `QSplitter::Vertical` 垂直方向

控件按加入顺序排列, 加入后, 分割器会成为该控件的父控件, 并管理它的大小和位置

## 2. QSplitter

分割器控件, 用于将多个控件排列在一起

### 2.1. 引入头文件

```cpp
#include <QSplitter>
```

### 2.2. 创建分割器

创建时可以指定排列方向和父控件:

```cpp
///@param orientation 控件排列方向
///@param parent 父控件指针
QSplitter* splitter = new QSplitter(Qt::Horizontal, parent);
```

如果只传入父控件, 默认水平排列

### 2.3. 添加控件

使用`addWidget(QWidget* widget)`向分割器中添加控件:

```cpp
///@param widget 已经创建的控件
splitter->addWidget(widget);
```

加入后, 分割器会成为该控件的父控件, 管理其大小,位置和生命周期

传入的指针必须指向已经创建的控件, 不能是空指针

### 2.4. 设置各区域大小

### 2.5. 设置伸缩因子

### 2.6. 嵌套分割器

### 2.7. 作为主窗口中央控件

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