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
#include <QMenu>
#include <QMenuBar>
#include <QSplitter>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QSizePolicy>
#include <QShortcut>
#include <QVBoxLayout>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QFrame>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QStatusBar>
#include <QStandardPaths>
#include <QDirIterator>

namespace {
constexpr int LaneCount = 5;
constexpr int LaneWidth = 92;
constexpr int TickHeight = 24;
constexpr int HeaderHeight = 32;
constexpr int MinimumTicks = 64;
const QColor Bg("#191a1c");
const QColor Panel("#202123");
const QColor Raised("#2b2c2f");
const QColor Grid("#393a3d");
const QColor Text("#e7e7e8");
const QColor Muted("#898b90");
const QColor Mint("#48a9ff");
const QColor Cyan("#55d7ee");
const QColor Amber("#ffb85c");

QPushButton *button(const QString &text, QWidget *parent, bool primary = false)
{
    auto *result = new QPushButton(text, parent);
    result->setCursor(Qt::PointingHandCursor);
    result->setStyleSheet(primary
        ? "QPushButton{background:#48a9ff;color:#101820;border:0;padding:9px 14px;font-weight:600} QPushButton:hover{background:#79bdff}"
        : "QPushButton{background:#2b2c2f;color:#e7e7e8;border:0;padding:9px 14px} QPushButton:hover{background:#393a3d}");
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

class CircularPreview : public QLabel {
public:
    explicit CircularPreview(QWidget *parent = nullptr) : QLabel(parent)
    {
        setAlignment(Qt::AlignCenter);
        setMinimumSize(240, 240);
        setStyleSheet("background:#202123;color:#898b90");
    }

    void setCover(const QPixmap &cover)
    {
        m_cover = cover;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QLabel::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const int side = qMin(width(), height()) - 8;
        const QRectF circle((width() - side) / 2.0, (height() - side) / 2.0, side, side);
        if (m_cover.isNull()) {
            painter.setPen(QColor("#898b90"));
            painter.drawText(circle, Qt::AlignCenter, QStringLiteral("曲绘将在此预览"));
        } else {
            const QPixmap scaled = m_cover.scaled(side, side, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            QPainterPath clip;
            clip.addEllipse(circle);
            painter.setClipPath(clip);
            painter.drawPixmap(QRect((width() - side) / 2, (height() - side) / 2, side, side), scaled,
                               QRect((scaled.width() - side) / 2, (scaled.height() - side) / 2, side, side));
            painter.setClipping(false);
            painter.setPen(QPen(QColor("#e7e7e8"), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(circle);
        }
    }

private:
    QPixmap m_cover;
};

JudgePlane::JudgePlane(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(LaneCount * LaneWidth, HeaderHeight + MinimumTicks * TickHeight);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setAutoFillBackground(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    resize(LaneCount * LaneWidth, contentHeight());
}

void JudgePlane::setNotes(const QVector<Note> *notes, int selected)
{
    m_notes = notes;
    m_selected = selected;
    resize(width(), contentHeight());
    update();
}

void JudgePlane::setTool(const QString &tool, int edge)
{
    m_tool = tool;
    m_edge = edge;
    setCursor(tool == QStringLiteral("select") ? Qt::ArrowCursor : Qt::CrossCursor);
}

void JudgePlane::setTimingGrid(int subdivision, int beatsPerMeasure)
{
    m_subdivision = subdivision;
    m_beatsPerMeasure = beatsPerMeasure;
    update();
}

QRectF JudgePlane::gridRect() const
{
    return QRectF(0.0, HeaderHeight, width(), height() - HeaderHeight);
}

double JudgePlane::laneWidth() const
{
    return width() / static_cast<double>(LaneCount);
}

int JudgePlane::tickAt(double y) const
{
    return qMax(0, qFloor((y - HeaderHeight) / TickHeight));
}

int JudgePlane::laneAt(double x) const
{
    return qBound(0, qFloor(x / laneWidth()), LaneCount - 1);
}

int JudgePlane::contentHeight() const
{
    int lastTick = MinimumTicks - 1;
    if (m_notes) {
        for (const Note &note : *m_notes)
            lastTick = qMax(lastTick, note.tick);
    }
    return HeaderHeight + (lastTick + 2) * TickHeight;
}

int JudgePlane::nearestNote(const QPointF &point) const
{
    if (!m_notes || !gridRect().contains(point)) return -1;
    const int targetTick = tickAt(point.y());
    const int targetLane = laneAt(point.x());
    for (int i = m_notes->size() - 1; i >= 0; --i) {
        const Note &note = m_notes->at(i);
        const int lane = note.type == QStringLiteral("EdgeNote") ? note.edge : 4;
        if (note.tick == targetTick && lane == targetLane) return i;
    }
    return -1;
}

void JudgePlane::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Bg);
    const QRectF plane = gridRect();
    QFont utility(QStringLiteral("Consolas"), 9);
    painter.setFont(utility);
    const QStringList lanes{QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2"),
                            QStringLiteral("3"), QStringLiteral("判定区")};
    const double laneSize = laneWidth();
    painter.fillRect(QRectF(0, 0, width(), HeaderHeight), Panel);
    for (int lane = 0; lane < LaneCount; ++lane) {
        const double x = lane * laneSize;
        painter.setPen(QPen(Grid, 1));
        painter.drawLine(QPointF(x, 0), QPointF(x, height()));
        painter.setPen(lane == 4 ? Cyan : Mint);
        painter.drawText(QRectF(x, 0, laneSize, HeaderHeight), Qt::AlignCenter, lanes.at(lane));
    }
    painter.setPen(QPen(Grid, 1));
    painter.drawLine(QPointF(width() - 1, 0), QPointF(width() - 1, height()));
    const int lastTick = tickAt(height());
    for (int tick = 0; tick <= lastTick; ++tick) {
        const double y = HeaderHeight + tick * TickHeight;
        const bool beat = tick % m_subdivision == 0;
        const bool measure = tick % (m_subdivision * m_beatsPerMeasure) == 0;
        painter.setPen(QPen(measure ? QColor("#788f7c") : (beat ? Grid : QColor("#24332a")), measure ? 1.4 : (beat ? 1.0 : 0.6)));
        painter.drawLine(QPointF(0, y), QPointF(width(), y));
        if (tick % m_subdivision == 0) {
            painter.setPen(Muted);
            painter.drawText(QRectF(4, y + 2, laneSize - 8, 16), Qt::AlignLeft, QStringLiteral("%1 拍").arg(tick / m_subdivision + 1));
        }
    }
    painter.setPen(QPen(QColor("#788f7c"), 1.5));
    painter.drawRect(plane);
    if (!m_notes) return;
    for (int i = 0; i < m_notes->size(); ++i) {
        const Note &note = m_notes->at(i);
        if (note.tick < 0) continue;
        const int lane = note.type == QStringLiteral("EdgeNote") ? note.edge : 4;
        const QRectF cell(lane * laneSize + 4, HeaderHeight + note.tick * TickHeight + 3, laneSize - 8, TickHeight - 6);
        const QColor color = lane == 4 ? Cyan : Mint;
        painter.setPen(QPen(note.isFake ? Amber : (i == m_selected ? Text : color), i == m_selected ? 2 : 1,
                            note.isFake ? Qt::DashLine : Qt::SolidLine));
        painter.setBrush(note.isFake ? Qt::NoBrush : QColor(color.red(), color.green(), color.blue(), 130));
        painter.drawRoundedRect(cell, 5, 5);
        painter.setPen(note.isFake ? Amber : Text);
        painter.drawText(cell, Qt::AlignCenter, QStringLiteral("%1 · %2").arg(note.kind).arg(note.tick));
    }
}

void JudgePlane::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !gridRect().contains(event->position())) return;
    const QPointF point = event->position();
    if (m_tool == QStringLiteral("select")) {
        m_dragIndex = nearestNote(point);
        emit noteSelected(m_dragIndex);
    } else {
        emit notePlaced(tickAt(point.y()), laneAt(point.x()));
    }
}

void JudgePlane::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragIndex < 0 || !(event->buttons() & Qt::LeftButton)) return;
    const QRectF plane = gridRect();
    if (event->position().y() < plane.top() || event->position().y() >= plane.bottom()) return;
    emit noteMoved(m_dragIndex, tickAt(event->position().y()), laneAt(event->position().x()));
}

void JudgePlane::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) m_dragIndex = -1;
}

EditorWindow::EditorWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("范式：起源 · 制谱器"));
    resize(1500, 900);
    setMinimumSize(1080, 680);
    setStyleSheet("QMainWindow{background:#191a1c;color:#e7e7e8} QWidget{color:#e7e7e8;font-family:'Microsoft YaHei UI'} QLineEdit,QComboBox,QListWidget,QSpinBox{background:#252629;border:1px solid #414246;padding:6px;color:#e7e7e8} QComboBox QAbstractItemView{background:#252629;selection-background-color:#315274} QCheckBox{spacing:8px} QCheckBox::indicator{width:15px;height:15px} QCheckBox::indicator:checked{background:#48a9ff;border:1px solid #48a9ff} QMenuBar{background:#292a2d;color:#e7e7e8;padding:4px} QMenuBar::item:selected,QMenu::item:selected{background:#393a3d} QMenu{background:#292a2d;border:1px solid #414246} QStatusBar{background:#292a2d;color:#a7a8ab}");

    auto *fileMenu = menuBar()->addMenu(QStringLiteral("文件(F)"));
    auto *editMenu = menuBar()->addMenu(QStringLiteral("编辑(E)"));
    auto *optionsMenu = menuBar()->addMenu(QStringLiteral("选项(O)"));
    auto *newAction = fileMenu->addAction(QStringLiteral("新建曲包"));
    auto *openAction = fileMenu->addAction(QStringLiteral("打开谱面…"));
    fileMenu->addSeparator();
    auto *saveAction = fileMenu->addAction(QStringLiteral("保存"));
    saveAction->setShortcut(QKeySequence::Save);
    auto *copyAction = editMenu->addAction(QStringLiteral("复制音符"));
    copyAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    auto *deleteAction = editMenu->addAction(QStringLiteral("删除音符"));
    deleteAction->setShortcut(QKeySequence(Qt::Key_Delete));
    auto *selectAction = optionsMenu->addAction(QStringLiteral("选取 / 移动工具"));
    auto *placeAction = optionsMenu->addAction(QStringLiteral("放置音符工具"));

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto *header = new QHBoxLayout;
    header->setContentsMargins(16, 8, 16, 8);
    auto *brand = new QLabel(QStringLiteral("范式：起源"), central);
    brand->setStyleSheet("font-size:16px;font-weight:700");
    header->addWidget(brand);
    header->addStretch();
    m_status = new QLabel(central);
    m_status->setStyleSheet("color:#48a9ff;font-family:Consolas");
    header->addWidget(m_status);
    root->addLayout(header);

    auto *columns = new QSplitter(Qt::Horizontal, central);
    columns->setChildrenCollapsible(false);
    auto *work = new QWidget(columns);
    work->setStyleSheet("background:#202123");
    auto *workLayout = new QVBoxLayout(work);
    workLayout->setContentsMargins(10, 8, 10, 8);
    auto *timelineCaption = new QLabel(QStringLiteral("五轨时间轴"), work);
    timelineCaption->setStyleSheet("color:#898b90;font-size:12px");
    workLayout->addWidget(timelineCaption);
    m_plane = new JudgePlane(work);
    m_timelineScroll = new QScrollArea(work);
    m_timelineScroll->setWidgetResizable(true);
    m_timelineScroll->setFrameShape(QFrame::NoFrame);
    m_timelineScroll->setWidget(m_plane);
    workLayout->addWidget(m_timelineScroll, 1);
    auto *legend = new QLabel(QStringLiteral("0–3：边线　·　判定区：区内音符　·　每格 = 1 tick"), work);
    legend->setStyleSheet("color:#898b90;padding:5px 0");
    workLayout->addWidget(legend);
    columns->addWidget(work);

    auto *centerScroll = new QScrollArea(columns);
    centerScroll->setWidgetResizable(true);
    centerScroll->setFrameShape(QFrame::NoFrame);
    auto *side = new QWidget(centerScroll);
    side->setMinimumWidth(330);
    side->setStyleSheet("background:#191a1c");
    auto *sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(12, 0, 12, 10);
    sideLayout->setSpacing(8);
    auto section = [side](const QString &title) {
        auto *label = new QLabel(title, side);
        label->setStyleSheet("background:#252629;color:#d7d8da;padding:8px 9px;font-size:13px");
        return label;
    };
    sideLayout->addWidget(section(QStringLiteral("编辑器设置")));
    auto *timing = new QFormLayout;
    timing->setContentsMargins(4, 4, 4, 8);
    m_bpm = new QSpinBox(side);
    m_bpm->setRange(1, 1000);
    m_bpm->setValue(m_chart.bpm);
    m_subdivision = new QSpinBox(side);
    m_subdivision->setRange(1, 64);
    m_subdivision->setValue(m_chart.subdivision);
    m_beatsPerMeasure = new QSpinBox(side);
    m_beatsPerMeasure->setRange(1, 32);
    m_beatsPerMeasure->setValue(m_chart.beatsPerMeasure);
    timing->addRow(QStringLiteral("BPM"), m_bpm);
    timing->addRow(QStringLiteral("每拍分音"), m_subdivision);
    timing->addRow(QStringLiteral("每小节拍数"), m_beatsPerMeasure);
    sideLayout->addLayout(timing);
    auto *toolsHeader = section(QStringLiteral("音符编辑"));
    sideLayout->addWidget(toolsHeader);
    auto *toolRow = new QHBoxLayout;
    auto *selectButton = button(QStringLiteral("选取 / 移动"), side);
    auto *placeButton = button(QStringLiteral("＋ 放置音符"), side, true);
    toolRow->addWidget(selectButton);
    toolRow->addWidget(placeButton);
    sideLayout->addLayout(toolRow);
    auto *props = new QFormLayout;
    m_kind = new QComboBox(side);
    m_kind->addItems({QStringLiteral("tap"), QStringLiteral("link"), QStringLiteral("slider")});
    m_edgeBox = new QComboBox(side);
    m_edgeBox->addItems({QStringLiteral("0 · 左边线"), QStringLiteral("1 · 右边线"), QStringLiteral("2 · 上边线"), QStringLiteral("3 · 下边线")});
    m_tick = new QLineEdit(QStringLiteral("0"), side);
    m_fake = new QCheckBox(QStringLiteral("假音符"), side);
    props->addRow(QStringLiteral("音符种类"), m_kind);
    props->addRow(QStringLiteral("边线轨道"), m_edgeBox);
    props->addRow(QStringLiteral("Tick"), m_tick);
    props->addRow(QString(), m_fake);
    sideLayout->addLayout(props);
    auto *apply = button(QStringLiteral("应用到选中音符"), side, true);
    sideLayout->addWidget(apply);
    sideLayout->addWidget(section(QStringLiteral("音符列表")));
    m_noteList = new QListWidget(side);
    m_noteList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_noteList->setMinimumHeight(130);
    sideLayout->addWidget(m_noteList, 1);
    auto *actions = new QHBoxLayout;
    auto *duplicate = button(QStringLiteral("复制"), side);
    auto *remove = button(QStringLiteral("删除"), side);
    actions->addWidget(duplicate);
    actions->addWidget(remove);
    sideLayout->addLayout(actions);
    centerScroll->setWidget(side);
    columns->addWidget(centerScroll);

    auto *preview = new QWidget(columns);
    preview->setStyleSheet("background:#191a1c");
    auto *previewLayout = new QVBoxLayout(preview);
    previewLayout->setContentsMargins(14, 10, 14, 10);
    previewLayout->addWidget(new QLabel(QStringLiteral("判定区 / 曲绘预览"), preview));
    m_coverPreview = new CircularPreview(preview);
    previewLayout->addWidget(m_coverPreview, 1, Qt::AlignCenter);
    previewLayout->addWidget(section(QStringLiteral("谱面信息")));
    auto *metadata = new QFormLayout;
    m_title = new QLineEdit(m_chart.title, preview);
    metadata->addRow(QStringLiteral("曲名"), m_title);
    auto *musicLabel = new QLabel(QStringLiteral("未导入音乐"), preview);
    musicLabel->setObjectName(QStringLiteral("musicPathLabel"));
    metadata->addRow(QStringLiteral("音乐"), musicLabel);
    auto *coverLabel = new QLabel(QStringLiteral("未设置曲绘"), preview);
    coverLabel->setObjectName(QStringLiteral("coverPathLabel"));
    metadata->addRow(QStringLiteral("曲绘"), coverLabel);
    previewLayout->addLayout(metadata);
    columns->addWidget(preview);
    columns->setStretchFactor(0, 4);
    columns->setStretchFactor(1, 5);
    columns->setStretchFactor(2, 5);
    columns->setSizes({360, 500, 500});
    root->addWidget(columns, 1);
    statusBar()->showMessage(QStringLiteral("就绪"));
    setCentralWidget(central);

    connect(newAction, &QAction::triggered, this, &EditorWindow::newBundle);
    connect(openAction, &QAction::triggered, this, &EditorWindow::openChart);
    connect(saveAction, &QAction::triggered, this, [this] { saveChartFile(); });
    connect(copyAction, &QAction::triggered, this, &EditorWindow::duplicateSelected);
    connect(deleteAction, &QAction::triggered, this, &EditorWindow::deleteSelected);
    connect(selectAction, &QAction::triggered, this, [this] { m_tool = QStringLiteral("select"); m_plane->setTool(m_tool, m_edge); });
    connect(placeAction, &QAction::triggered, this, [this] { m_tool = QStringLiteral("place"); m_plane->setTool(m_tool, m_edge); });
    connect(m_title, &QLineEdit::textEdited, this, [this] { m_chart.title = m_title->text(); markDirty(); });
    connect(m_bpm, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { setTiming(m_bpm->value(), m_subdivision->value(), m_beatsPerMeasure->value()); });
    connect(m_subdivision, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { setTiming(m_bpm->value(), m_subdivision->value(), m_beatsPerMeasure->value()); });
    connect(m_beatsPerMeasure, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { setTiming(m_bpm->value(), m_subdivision->value(), m_beatsPerMeasure->value()); });
    connect(selectButton, &QPushButton::clicked, this, [this] { m_tool = QStringLiteral("select"); m_plane->setTool(m_tool, m_edge); });
    connect(placeButton, &QPushButton::clicked, this, [this] { m_tool = QStringLiteral("place"); m_plane->setTool(m_tool, m_edge); });
    connect(m_edgeBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) { m_edge = index; m_plane->setTool(m_tool, m_edge); });
    connect(m_plane, &JudgePlane::notePlaced, this, &EditorWindow::placeNote);
    connect(m_plane, &JudgePlane::noteSelected, this, &EditorWindow::selectNote);
    connect(m_plane, &JudgePlane::noteMoved, this, [this](int index, int tick, int lane) {
        if (index < 0 || index >= m_chart.notes.size()) return;
        Note &note = m_chart.notes[index];
        note.tick = tick;
        if (lane < 4) {
            note.type = QStringLiteral("EdgeNote");
            note.edge = lane;
            note.pos = lane < 2 ? 4.5 : 6.0;
        } else {
            note.type = QStringLiteral("SpaceNote");
            note.x = 6.0;
            note.y = 4.5;
        }
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
        ? QStringLiteral("%1　·　BPM %2　·　每拍 %3 分音　·　每小节 %4 拍　·　%5 个音符%6")
              .arg(m_dirty ? QStringLiteral("未保存") : QStringLiteral("就绪"))
              .arg(m_chart.bpm).arg(m_chart.subdivision).arg(m_chart.beatsPerMeasure).arg(m_chart.notes.size())
              .arg(m_chartPath.isEmpty() ? QString() : QStringLiteral("　·　") + QFileInfo(m_chartPath).fileName())
              .arg(m_chartPath.isEmpty() ? QString() : QStringLiteral("　·　") + QFileInfo(m_chartPath).fileName())
        : message);
    statusBar()->showMessage(message.isEmpty()
        ? QStringLiteral("BPM %1　|　每拍 %2 分音　|　每小节 %3 拍　|　%4 音符")
              .arg(m_chart.bpm).arg(m_chart.subdivision).arg(m_chart.beatsPerMeasure).arg(m_chart.notes.size())
        : message);
}

void EditorWindow::markDirty()
{
    m_dirty = true;
    updateStatus();
}

void EditorWindow::refresh()
{
    m_plane->setTimingGrid(m_chart.subdivision, m_chart.beatsPerMeasure);
    m_plane->setNotes(&m_chart.notes, m_selected);
    m_noteList->blockSignals(true);
    m_noteList->clear();
    for (const Note &note : m_chart.notes) m_noteList->addItem(noteDescription(note));
    if (m_selected >= 0 && m_selected < m_noteList->count()) m_noteList->setCurrentRow(m_selected);
    m_noteList->blockSignals(false);
    refreshPreview();
}

void EditorWindow::refreshPreview()
{
    const QString coverPath = m_chartPath.isEmpty() || m_chart.jacketPath.isEmpty()
        ? QString() : QFileInfo(m_chartPath).dir().filePath(m_chart.jacketPath);
    QPixmap cover;
    if (!coverPath.isEmpty()) cover.load(coverPath);
    m_coverPreview->setCover(cover);
    const auto labels = findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (label->objectName() == QStringLiteral("musicPathLabel"))
            label->setText(m_chart.musicPath.isEmpty() ? QStringLiteral("未导入音乐") : m_chart.musicPath);
        else if (label->objectName() == QStringLiteral("coverPathLabel"))
            label->setText(m_chart.jacketPath.isEmpty() ? QStringLiteral("未设置曲绘") : m_chart.jacketPath);
    }
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
    m_bpm->setValue(m_chart.bpm);
    m_subdivision->setValue(m_chart.subdivision);
    m_beatsPerMeasure->setValue(m_chart.beatsPerMeasure);
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
    m_bpm->setValue(m_chart.bpm);
    m_subdivision->setValue(m_chart.subdivision);
    m_beatsPerMeasure->setValue(m_chart.beatsPerMeasure);
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

void EditorWindow::placeNote(int tick, int lane)
{
    Note note;
    note.kind = m_kind->currentText();
    note.tick = tick;
    note.isFake = m_fake->isChecked();
    if (lane < 4) {
        note.type = QStringLiteral("EdgeNote");
        note.edge = lane;
        note.pos = lane < 2 ? 4.5 : 6.0;
    } else {
        note.type = QStringLiteral("SpaceNote");
        note.x = 6.0;
        note.y = 4.5;
    }
    m_chart.notes.append(note);
    m_selected = m_chart.notes.size() - 1;
    m_tick->setText(QString::number(tick));
    markDirty();
    refresh();
    selectNote(m_selected);
}

void EditorWindow::setTiming(int bpm, int subdivision, int beatsPerMeasure)
{
    if (m_chart.bpm == bpm && m_chart.subdivision == subdivision && m_chart.beatsPerMeasure == beatsPerMeasure) return;
    m_chart.bpm = bpm;
    m_chart.subdivision = subdivision;
    m_chart.beatsPerMeasure = beatsPerMeasure;
    m_plane->setTimingGrid(subdivision, beatsPerMeasure);
    markDirty();
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
    if (note.type == QStringLiteral("EdgeNote")) {
        note.edge = m_edgeBox->currentIndex();
        note.pos = note.edge < 2 ? 4.5 : 6.0;
    }
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
    ++note.tick;
    m_chart.notes.append(note); m_selected = m_chart.notes.size() - 1; markDirty(); refresh();
}

void EditorWindow::closeEvent(QCloseEvent *event)
{
    if (confirmDiscard()) event->accept(); else event->ignore();
}
