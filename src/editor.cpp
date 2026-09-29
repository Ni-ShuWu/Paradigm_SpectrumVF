#include "editor.hpp"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDirIterator>

namespace {
constexpr double PlaneWidth = 12.0;
constexpr double PlaneHeight = 9.0;
const QColor Bg("#101715");
const QColor Panel("#18211d");
const QColor Raised("#202c26");
const QColor Grid("#34453b");
const QColor Text("#f1f0e6");
const QColor Muted("#91a497");
const QColor Mint("#83f0b2");
const QColor Cyan("#84cde0");
const QColor Amber("#eeae6a");

QPushButton *button(const QString &text, QWidget *parent, bool primary = false)
{
    auto *result = new QPushButton(text, parent);
    result->setCursor(Qt::PointingHandCursor);
    result->setStyleSheet(primary
        ? "QPushButton{background:#83f0b2;color:#122019;border:0;padding:9px 14px;font-weight:600} QPushButton:hover{background:#a5f8c8}"
        : "QPushButton{background:#202c26;color:#f1f0e6;border:0;padding:9px 14px} QPushButton:hover{background:#34473a}");
    return result;
}

QString noteDescription(const Note &note)
{
    const QString category = note.type == QStringLiteral("EdgeNote") ? QStringLiteral("边线") : QStringLiteral("判面");
    QString position;
    if (note.type == QStringLiteral("EdgeNote")) {
        static const QStringList edges{QStringLiteral("左"), QStringLiteral("右"), QStringLiteral("上"), QStringLiteral("下")};
        position = edges.value(note.edge) + QStringLiteral(" ") + QString::number(note.pos, 'g', 4);
    } else {
        position = QStringLiteral("(%1, %2)").arg(note.x, 0, 'g', 4).arg(note.y, 0, 'g', 4);
    }
    return QStringLiteral("%1%2 · %3 · tick %4 · %5")
        .arg(note.isFake ? QStringLiteral("◇ ") : QString(), category, note.kind)
        .arg(note.tick).arg(position);
}

bool isAllowedExtension(const QString &path, const QStringList &extensions)
{
    return extensions.contains(QFileInfo(path).suffix().toLower());
}
}

JudgePlane::JudgePlane(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(520, 400);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setAutoFillBackground(false);
}

void JudgePlane::setNotes(const QVector<Note> *notes, int selected)
{
    m_notes = notes;
    m_selected = selected;
    update();
}

void JudgePlane::setTool(const QString &tool, int edge)
{
    m_tool = tool;
    m_edge = edge;
    setCursor(tool == QStringLiteral("select") ? Qt::ArrowCursor : Qt::CrossCursor);
}

QRectF JudgePlane::gridRect() const
{
    const double unit = qMin((width() - 76.0) / PlaneWidth, (height() - 66.0) / PlaneHeight);
    const double w = unit * PlaneWidth;
    const double h = unit * PlaneHeight;
    return QRectF((width() - w) / 2.0, (height() - h) / 2.0, w, h);
}

QPointF JudgePlane::toGrid(const QPointF &point) const
{
    const QRectF rect = gridRect();
    return {(point.x() - rect.left()) / (rect.width() / PlaneWidth),
            PlaneHeight - (point.y() - rect.top()) / (rect.height() / PlaneHeight)};
}

int JudgePlane::nearestNote(const QPointF &point) const
{
    if (!m_notes) return -1;
    const QRectF rect = gridRect();
    const double unitX = rect.width() / PlaneWidth;
    const double unitY = rect.height() / PlaneHeight;
    const double radius = qMax(11.0, qMin(unitX, unitY) * 0.30);
    double best = radius;
    int index = -1;
    for (int i = 0; i < m_notes->size(); ++i) {
        const QPointF position = m_notes->at(i).coordinates();
        const QPointF pixel(rect.left() + position.x() * unitX,
                            rect.bottom() - position.y() * unitY);
        const double distance = QLineF(pixel, point).length();
        if (distance <= best) { best = distance; index = i; }
    }
    return index;
}

void JudgePlane::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Bg);
    const QRectF plane = gridRect();
    const double unitX = plane.width() / PlaneWidth;
    const double unitY = plane.height() / PlaneHeight;
    QFont utility(QStringLiteral("Consolas"), 9);
    painter.setFont(utility);
    for (int col = 0; col <= 12; ++col) {
        const bool major = col % 3 == 0;
        painter.setPen(QPen(major ? Grid : QColor("#24332a"), major ? 1.2 : 0.7));
        const double x = plane.left() + col * unitX;
        painter.drawLine(QPointF(x, plane.top()), QPointF(x, plane.bottom()));
        if (col < 12 && major) {
            painter.setPen(Muted);
            painter.drawText(QRectF(x, plane.bottom() + 7, unitX, 18), Qt::AlignHCenter, QString::number(col));
        }
    }
    for (int row = 0; row <= 9; ++row) {
        const bool major = row % 3 == 0;
        painter.setPen(QPen(major ? Grid : QColor("#24332a"), major ? 1.2 : 0.7));
        const double y = plane.top() + row * unitY;
        painter.drawLine(QPointF(plane.left(), y), QPointF(plane.right(), y));
        if (row < 9 && major) {
            painter.setPen(Muted);
            painter.drawText(QRectF(plane.left() - 28, y - 9, 20, 18), Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(9 - row));
        }
    }
    painter.setPen(QPen(QColor("#788f7c"), 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(plane);
    if (!m_notes) return;
    for (int i = 0; i < m_notes->size(); ++i) {
        const Note &note = m_notes->at(i);
        const QPointF position = note.coordinates();
        const QPointF center(plane.left() + position.x() * unitX, plane.bottom() - position.y() * unitY);
        const double radius = qBound(6.0, qMin(unitX, unitY) * 0.19, 14.0);
        const QColor color = note.type == QStringLiteral("EdgeNote") ? Mint : Cyan;
        painter.setPen(QPen(note.isFake ? Amber : (i == m_selected ? Text : color), i == m_selected ? 3 : 1,
                            note.isFake ? Qt::DashLine : Qt::SolidLine));
        painter.setBrush(note.isFake ? Qt::NoBrush : color);
        painter.drawEllipse(center, radius, radius);
        if (note.type == QStringLiteral("EdgeNote")) {
            QPolygonF diamond{QPointF(center.x(), center.y() - radius * .8),
                              QPointF(center.x() + radius * .8, center.y()),
                              QPointF(center.x(), center.y() + radius * .8),
                              QPointF(center.x() - radius * .8, center.y())};
            painter.setPen(QPen(note.isFake ? Amber : color, 2));
            painter.setBrush(note.isFake ? Qt::NoBrush : color);
            painter.drawPolygon(diamond);
        }
        if (i == m_selected) {
            painter.setPen(Text);
            painter.drawText(center + QPointF(radius + 5, -radius - 5), QStringLiteral("%1 · %2").arg(note.kind).arg(note.tick));
        }
    }
}

void JudgePlane::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    const QPointF point = event->position();
    if (m_tool == QStringLiteral("select")) {
        m_dragIndex = nearestNote(point);
        emit noteSelected(m_dragIndex);
    } else {
        const QRectF plane = gridRect();
        if (plane.contains(point)) {
            const QPointF grid = toGrid(point);
            emit notePlaced(qBound(0.0, grid.x(), PlaneWidth), qBound(0.0, grid.y(), PlaneHeight));
        }
    }
}

void JudgePlane::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragIndex < 0 || !(event->buttons() & Qt::LeftButton)) return;
    const QRectF plane = gridRect();
    if (!plane.adjusted(-20, -20, 20, 20).contains(event->position())) return;
    const QPointF grid = toGrid(event->position());
    emit noteMoved(m_dragIndex, qBound(0.0, grid.x(), PlaneWidth), qBound(0.0, grid.y(), PlaneHeight));
}

void JudgePlane::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) m_dragIndex = -1;
}

EditorWindow::EditorWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("范式：起源 · 制谱器"));
    resize(1180, 800);
    setMinimumSize(920, 650);
    setStyleSheet("QMainWindow{background:#101715;color:#f1f0e6} QWidget{color:#f1f0e6;font-family:'Microsoft YaHei UI'} QLineEdit,QComboBox,QListWidget{background:#202c26;border:1px solid #34453b;padding:7px;color:#f1f0e6} QComboBox QAbstractItemView{background:#202c26;selection-background-color:#2d4939} QCheckBox{spacing:8px} QCheckBox::indicator{width:15px;height:15px} QCheckBox::indicator:checked{background:#83f0b2;border:1px solid #83f0b2}");

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(20, 14, 20, 16);
    root->setSpacing(12);
    auto *header = new QHBoxLayout;
    auto *brand = new QLabel(QStringLiteral("范式：起源"), central);
    brand->setStyleSheet("font-size:21px;font-weight:700");
    header->addWidget(brand);
    auto *tag = new QLabel(QStringLiteral("/  SCORE EDITOR"), central);
    tag->setStyleSheet("color:#83f0b2;font-family:Consolas;font-weight:700");
    header->addWidget(tag);
    header->addStretch();
    m_status = new QLabel(central);
    m_status->setStyleSheet("color:#91a497;font-family:Consolas");
    header->addWidget(m_status);
    root->addLayout(header);

    auto *toolbar = new QHBoxLayout;
    auto *newButton = button(QStringLiteral("新建曲包"), central);
    auto *openButton = button(QStringLiteral("打开 JSON"), central);
    auto *saveButton = button(QStringLiteral("保存"), central, true);
    toolbar->addWidget(newButton); toolbar->addWidget(openButton); toolbar->addWidget(saveButton);
    toolbar->addSpacing(10);
    toolbar->addWidget(new QLabel(QStringLiteral("谱面名称"), central));
    m_title = new QLineEdit(m_chart.title, central);
    m_title->setMaximumWidth(260);
    toolbar->addWidget(m_title); toolbar->addStretch();
    root->addLayout(toolbar);

    auto *body = new QHBoxLayout;
    body->setSpacing(14);
    auto *work = new QWidget(central);
    work->setStyleSheet("background:#18211d");
    auto *workLayout = new QVBoxLayout(work);
    workLayout->setContentsMargins(14, 12, 14, 12);
    auto *caption = new QHBoxLayout;
    auto *cap = new QLabel(QStringLiteral("JUDGE PLANE"), work);
    cap->setStyleSheet("color:#91a497;font-family:Consolas;font-weight:700");
    caption->addWidget(cap); caption->addStretch();
    auto *measure = new QLabel(QStringLiteral("12 × 9  /  半开矩形判定"), work);
    measure->setStyleSheet("color:#91a497");
    caption->addWidget(measure); workLayout->addLayout(caption);
    m_plane = new JudgePlane(work);
    workLayout->addWidget(m_plane, 1);
    auto *legend = new QLabel(QStringLiteral("● 判面音符　　◆ 边线音符　　◇ 假音符　·　点击放置 / 拖动移动"), work);
    legend->setStyleSheet("color:#91a497;padding:4px");
    workLayout->addWidget(legend);
    body->addWidget(work, 1);

    auto *side = new QWidget(central);
    side->setFixedWidth(292);
    side->setStyleSheet("background:#18211d");
    auto *sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(14, 14, 14, 14);
    auto *toolsLabel = new QLabel(QStringLiteral("TOOLS  /  工具"), side);
    toolsLabel->setStyleSheet("color:#91a497;font-family:Consolas;font-weight:700");
    sideLayout->addWidget(toolsLabel);
    auto *selectButton = button(QStringLiteral("↖ 选取 / 移动"), side);
    auto *spaceButton = button(QStringLiteral("● 判面音符"), side);
    auto *edgeButton = button(QStringLiteral("◆ 边线音符"), side);
    sideLayout->addWidget(selectButton); sideLayout->addWidget(spaceButton); sideLayout->addWidget(edgeButton);
    sideLayout->addSpacing(8);
    auto *propsLabel = new QLabel(QStringLiteral("NOTE  /  音符属性"), side);
    propsLabel->setStyleSheet("color:#91a497;font-family:Consolas;font-weight:700");
    sideLayout->addWidget(propsLabel);
    sideLayout->addWidget(new QLabel(QStringLiteral("音符种类"), side));
    m_kind = new QComboBox(side);
    m_kind->addItems({QStringLiteral("tap"), QStringLiteral("link"), QStringLiteral("slider")});
    sideLayout->addWidget(m_kind);
    sideLayout->addWidget(new QLabel(QStringLiteral("边线位置"), side));
    m_edgeBox = new QComboBox(side);
    m_edgeBox->addItems({QStringLiteral("左边线"), QStringLiteral("右边线"), QStringLiteral("上边线"), QStringLiteral("下边线")});
    sideLayout->addWidget(m_edgeBox);
    sideLayout->addWidget(new QLabel(QStringLiteral("tick / 谱面原始刻度"), side));
    m_tick = new QLineEdit(QStringLiteral("0"), side);
    sideLayout->addWidget(m_tick);
    m_fake = new QCheckBox(QStringLiteral("假音符（不参与判定）"), side);
    sideLayout->addWidget(m_fake);
    auto *apply = button(QStringLiteral("应用属性到选中音符"), side, true);
    sideLayout->addWidget(apply);
    auto *listHeader = new QHBoxLayout;
    auto *listLabel = new QLabel(QStringLiteral("NOTES  /  音符列表"), side);
    listLabel->setStyleSheet("color:#91a497;font-family:Consolas;font-weight:700");
    listHeader->addWidget(listLabel); listHeader->addStretch();
    m_noteList = new QListWidget(side);
    m_noteList->setSelectionMode(QAbstractItemView::SingleSelection);
    sideLayout->addLayout(listHeader);
    sideLayout->addWidget(m_noteList, 1);
    auto *actions = new QHBoxLayout;
    auto *duplicate = button(QStringLiteral("复制"), side);
    auto *remove = button(QStringLiteral("删除"), side);
    actions->addWidget(duplicate); actions->addWidget(remove);
    sideLayout->addLayout(actions);
    body->addWidget(side);
    root->addLayout(body, 1);
    setCentralWidget(central);

    connect(newButton, &QPushButton::clicked, this, &EditorWindow::newBundle);
    connect(openButton, &QPushButton::clicked, this, &EditorWindow::openChart);
    connect(saveButton, &QPushButton::clicked, this, [this] { saveChartFile(); });
    connect(m_title, &QLineEdit::textEdited, this, [this] { m_chart.title = m_title->text(); markDirty(); });
    connect(selectButton, &QPushButton::clicked, this, [this] { m_tool = QStringLiteral("select"); m_plane->setTool(m_tool, m_edge); });
    connect(spaceButton, &QPushButton::clicked, this, [this] { m_tool = QStringLiteral("space"); m_plane->setTool(m_tool, m_edge); });
    connect(edgeButton, &QPushButton::clicked, this, [this] { m_tool = QStringLiteral("edge"); m_plane->setTool(m_tool, m_edge); });
    connect(m_edgeBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) { m_edge = index; m_plane->setTool(m_tool, m_edge); });
    connect(m_plane, &JudgePlane::notePlaced, this, &EditorWindow::placeNote);
    connect(m_plane, &JudgePlane::noteSelected, this, &EditorWindow::selectNote);
    connect(m_plane, &JudgePlane::noteMoved, this, [this](int index, double x, double y) {
        if (index < 0 || index >= m_chart.notes.size()) return;
        Note &note = m_chart.notes[index];
        if (note.type == QStringLiteral("EdgeNote"))
            note.pos = note.edge < 2 ? y : x;
        else { note.x = x; note.y = y; }
        markDirty(); refresh();
    });
    connect(m_noteList, &QListWidget::currentRowChanged, this, [this](int row) { if (row >= 0) selectNote(row); });
    connect(apply, &QPushButton::clicked, this, &EditorWindow::applyProperties);
    connect(remove, &QPushButton::clicked, this, &EditorWindow::deleteSelected);
    connect(duplicate, &QPushButton::clicked, this, &EditorWindow::duplicateSelected);
    new QShortcut(QKeySequence::Save, this, [this] { saveChartFile(); });
    new QShortcut(QKeySequence::Open, this, [this] { openChart(); });
    new QShortcut(QKeySequence(Qt::Key_Delete), this, [this] { deleteSelected(); });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+D")), this, [this] { duplicateSelected(); });
    refresh();
    updateStatus();
}

void EditorWindow::updateStatus(const QString &message)
{
    m_status->setText(message.isEmpty()
        ? QStringLiteral("%1　·　12 × 9 判面　·　%2 个音符%3").arg(m_dirty ? QStringLiteral("未保存") : QStringLiteral("就绪"))
              .arg(m_chart.notes.size()).arg(m_chartPath.isEmpty() ? QString() : QStringLiteral("　·　") + QFileInfo(m_chartPath).fileName())
        : message);
}

void EditorWindow::markDirty()
{
    m_dirty = true;
    updateStatus();
}

void EditorWindow::refresh()
{
    m_plane->setNotes(&m_chart.notes, m_selected);
    m_noteList->blockSignals(true);
    m_noteList->clear();
    for (const Note &note : m_chart.notes) m_noteList->addItem(noteDescription(note));
    if (m_selected >= 0 && m_selected < m_noteList->count()) m_noteList->setCurrentRow(m_selected);
    m_noteList->blockSignals(false);
}

bool EditorWindow::confirmDiscard()
{
    if (!m_dirty) return true;
    const auto answer = QMessageBox::question(this, QStringLiteral("谱面尚未保存"), QStringLiteral("是否先保存当前谱面？"),
                                               QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                               QMessageBox::Save);
    if (answer == QMessageBox::Cancel) return false;
    return answer == QMessageBox::Discard || saveChartFile();
}

void EditorWindow::newBundle()
{
    if (!confirmDiscard()) return;
    bool accepted = false;
    const QString title = QInputDialog::getText(this, QStringLiteral("新建曲包"), QStringLiteral("曲包 / 谱面名称："),
                                                 QLineEdit::Normal, QString(), &accepted).trimmed();
    if (!accepted) return;
    if (title.isEmpty() || title == QStringLiteral(".") || title == QStringLiteral("..")
        || title.contains(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*]")))) {
        QMessageBox::warning(this, QStringLiteral("名称无效"), QStringLiteral("请输入不含 \\/:*?\"<>| 的名称。"));
        return;
    }
    const QString music = QFileDialog::getOpenFileName(this, QStringLiteral("选择音乐 (MP3 / OGG)"), {},
                                                        QStringLiteral("音乐文件 (*.mp3 *.ogg)"));
    if (music.isEmpty()) return;
    if (!isAllowedExtension(music, {QStringLiteral("mp3"), QStringLiteral("ogg")})) {
        QMessageBox::warning(this, QStringLiteral("格式不支持"), QStringLiteral("音乐仅支持 MP3 或 OGG。"));
        return;
    }
    const QString jacket = QFileDialog::getOpenFileName(this, QStringLiteral("选择曲绘 (JPG / PNG)"), {},
                                                         QStringLiteral("曲绘 (*.jpg *.jpeg *.png)"));
    if (jacket.isEmpty()) return;
    if (!isAllowedExtension(jacket, {QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png")})) {
        QMessageBox::warning(this, QStringLiteral("格式不支持"), QStringLiteral("曲绘仅支持 JPG 或 PNG。"));
        return;
    }
    const QString parent = QFileDialog::getExistingDirectory(this, QStringLiteral("选择曲包保存位置"),
                                                               QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    if (parent.isEmpty()) return;
    const QString destination = QDir(parent).filePath(title);
    if (QFileInfo::exists(destination)) {
        QMessageBox::warning(this, QStringLiteral("曲包已存在"), QStringLiteral("目标目录已存在，未修改任何文件：\n%1").arg(destination));
        return;
    }

    QTemporaryDir staging(QDir(parent).filePath(QStringLiteral(".chart-staging-XXXXXX")));
    if (!staging.isValid()) {
        QMessageBox::critical(this, QStringLiteral("创建失败"), QStringLiteral("无法在目标位置创建临时曲包目录。"));
        return;
    }
    const QString musicName = QStringLiteral("music.") + QFileInfo(music).suffix().toLower();
    const QString jacketName = QStringLiteral("jp.") + (QFileInfo(jacket).suffix().toLower() == QStringLiteral("jpeg")
                                                           ? QStringLiteral("jpg") : QFileInfo(jacket).suffix().toLower());
    if (!QFile::copy(music, QDir(staging.path()).filePath(musicName))
        || !QFile::copy(jacket, QDir(staging.path()).filePath(jacketName))) {
        QMessageBox::critical(this, QStringLiteral("导入失败"), QStringLiteral("复制音乐或曲绘失败，原有文件未改动。"));
        return;
    }
    Chart chart;
    chart.title = title;
    chart.musicPath = musicName;
    chart.jacketPath = jacketName;
    const QString chartFile = QDir(staging.path()).filePath(title + QStringLiteral(".json"));
    QString error;
    if (!saveChart(chartFile, chart, &error)) {
        QMessageBox::critical(this, QStringLiteral("创建失败"), error);
        return;
    }
    const QString stagingName = QFileInfo(staging.path()).fileName();
    if (!QDir(parent).rename(stagingName, title)) {
        QMessageBox::critical(this, QStringLiteral("创建失败"), QStringLiteral("无法将曲包移至目标目录。"));
        return;
    }
    staging.setAutoRemove(false);
    m_chart = chart;
    m_chartPath = QDir(destination).filePath(title + QStringLiteral(".json"));
    m_selected = -1;
    m_title->setText(title);
    m_dirty = false;
    refresh();
    updateStatus(QStringLiteral("曲包已创建　·　%1").arg(destination));
}

void EditorWindow::openChart()
{
    if (!confirmDiscard()) return;
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开谱面 JSON"), {}, QStringLiteral("JSON 谱面 (*.json);;所有文件 (*.*)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, QStringLiteral("打开失败"), file.errorString());
        return;
    }
    QString error;
    const auto parsed = Chart::fromJson(file.readAll(), &error);
    if (!parsed) {
        QMessageBox::critical(this, QStringLiteral("打开失败"), error);
        return;
    }
    m_chart = *parsed;
    m_chartPath = path;
    m_selected = -1;
    m_title->setText(m_chart.title);
    m_dirty = false;
    refresh();
    updateStatus();
}

bool EditorWindow::saveChartFile()
{
    m_chart.title = m_title->text().trimmed().isEmpty() ? QStringLiteral("未命名谱面") : m_title->text().trimmed();
    if (m_chartPath.isEmpty()) {
        m_chartPath = QFileDialog::getSaveFileName(this, QStringLiteral("保存谱面 JSON"), m_chart.title + QStringLiteral(".json"),
                                                   QStringLiteral("JSON 谱面 (*.json)"));
        if (m_chartPath.isEmpty()) return false;
    }
    QString error;
    if (!saveChart(m_chartPath, m_chart, &error)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), error);
        return false;
    }
    m_dirty = false;
    updateStatus();
    return true;
}

void EditorWindow::placeNote(double x, double y)
{
    bool ok = false;
    const int tick = m_tick->text().toInt(&ok);
    if (!ok) { QMessageBox::warning(this, QStringLiteral("无法放置音符"), QStringLiteral("tick 必须为整数。")); return; }
    Note note;
    note.kind = m_kind->currentText(); note.tick = tick; note.isFake = m_fake->isChecked();
    if (m_tool == QStringLiteral("edge")) {
        note.type = QStringLiteral("EdgeNote"); note.edge = m_edge;
        note.pos = m_edge < 2 ? y : x;
    } else {
        note.type = QStringLiteral("SpaceNote"); note.x = x; note.y = y;
    }
    m_chart.notes.append(note);
    m_selected = m_chart.notes.size() - 1;
    markDirty(); refresh(); selectNote(m_selected);
}

void EditorWindow::selectNote(int index)
{
    m_selected = index;
    if (index >= 0 && index < m_chart.notes.size()) {
        const Note &note = m_chart.notes.at(index);
        const int kindIndex = m_kind->findText(note.kind);
        if (kindIndex >= 0) m_kind->setCurrentIndex(kindIndex);
        m_tick->setText(QString::number(note.tick));
        m_fake->setChecked(note.isFake);
        if (note.type == QStringLiteral("EdgeNote")) m_edgeBox->setCurrentIndex(note.edge);
    }
    refresh();
}

void EditorWindow::applyProperties()
{
    if (m_selected < 0 || m_selected >= m_chart.notes.size()) { updateStatus(QStringLiteral("请先选择一个音符")); return; }
    bool ok = false;
    const int tick = m_tick->text().toInt(&ok);
    if (!ok) { QMessageBox::warning(this, QStringLiteral("属性无效"), QStringLiteral("tick 必须为整数。")); return; }
    Note &note = m_chart.notes[m_selected];
    note.kind = m_kind->currentText(); note.tick = tick; note.isFake = m_fake->isChecked();
    if (note.type == QStringLiteral("EdgeNote")) note.edge = m_edgeBox->currentIndex();
    markDirty(); refresh();
}

void EditorWindow::deleteSelected()
{
    if (m_selected < 0 || m_selected >= m_chart.notes.size()) return;
    m_chart.notes.removeAt(m_selected); m_selected = -1; markDirty(); refresh();
}

void EditorWindow::duplicateSelected()
{
    if (m_selected < 0 || m_selected >= m_chart.notes.size()) return;
    Note note = m_chart.notes.at(m_selected);
    if (note.type == QStringLiteral("EdgeNote")) note.pos = qMin(note.pos + .5, note.edge < 2 ? PlaneHeight : PlaneWidth);
    else { note.x = qMin(note.x + .5, PlaneWidth); note.y = qMin(note.y + .5, PlaneHeight); }
    m_chart.notes.append(note); m_selected = m_chart.notes.size() - 1; markDirty(); refresh();
}

void EditorWindow::closeEvent(QCloseEvent *event)
{
    if (confirmDiscard()) event->accept(); else event->ignore();
}
