#include "editor.hpp"

#include <QCheckBox>
#include <QCloseEvent>
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QTime>
#include <QComboBox>
#include <QUrl>
#include <functional>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLineF>
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
#include <limits>

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

QString formatPlaybackTime(qint64 milliseconds)
{
    const qint64 seconds = qMax<qint64>(0, milliseconds) / 1000;
    return QStringLiteral("%1:%2").arg(seconds / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

qreal pointToSegmentDistance(const QPointF &point, const QPointF &start, const QPointF &end)
{
    const QPointF segment = end - start;
    const qreal lengthSquared = segment.x() * segment.x() + segment.y() * segment.y();
    if (lengthSquared <= 0.0001) return QLineF(point, start).length();
    const QPointF offset = point - start;
    const qreal projection = qBound<qreal>(0.0,
        (offset.x() * segment.x() + offset.y() * segment.y()) / lengthSquared, 1.0);
    return QLineF(point, start + segment * projection).length();
}

int edgeAtChartPosition(qreal x, qreal y)
{
    constexpr qreal EdgeSnap = 0.45;
    if (x > EdgeSnap && x < 12.0 - EdgeSnap && y > EdgeSnap && y < 9.0 - EdgeSnap)
        return -1;
    const qreal nearestVertical = qMin(x, 12.0 - x);
    const qreal nearestHorizontal = qMin(y, 9.0 - y);
    if (nearestVertical <= nearestHorizontal) return x < 6.0 ? 0 : 1;
    return y > 4.5 ? 2 : 3;
}

QString noteDescription(const Note &note)
{
    const QString category = note.type == QStringLiteral("EdgeNote") ? QStringLiteral("边线") : QStringLiteral("判面");
    QString position;
    if (note.type == QStringLiteral("EdgeNote")) {
        static const QStringList edges{QStringLiteral("左"), QStringLiteral("右"), QStringLiteral("上"), QStringLiteral("下")};
        position = edges.value(note.edge) + QStringLiteral(" ") + QString::number(note.pos, 'g', 4);
        if (note.isLong()) position += QStringLiteral("→%1").arg(note.endPos, 0, 'g', 4);
    } else {
        position = QStringLiteral("(%1, %2)").arg(note.x, 0, 'g', 4).arg(note.y, 0, 'g', 4);
        if (note.isLong()) position += QStringLiteral("→(%1, %2)").arg(note.endX, 0, 'g', 4).arg(note.endY, 0, 'g', 4);
    }
    const QString timing = note.isLong()
        ? QStringLiteral("tick %1→%2").arg(note.tick).arg(note.endTick)
        : QStringLiteral("tick %1").arg(note.tick);
    return QStringLiteral("%1%2 · %3 · %4 · %5")
        .arg(note.isFake ? QStringLiteral("◇ ") : QString(), category, note.kind, timing, position);
}

bool isAllowedExtension(const QString &path, const QStringList &extensions)
{
    return extensions.contains(QFileInfo(path).suffix().toLower());
}
}

class PerspectivePreview : public QWidget {
public:
    explicit PerspectivePreview(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(480, 270);
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
    }

    void setChart(const QPixmap &cover, const QVector<Note> *notes, int selected,
                  int bpm, int subdivision, int beatsPerMeasure, qreal playbackTick,
                  bool playing, bool placementMode, const QString &chartTitle)
    {
        m_cover = cover;
        m_notes = notes;
        m_selected = selected;
        m_bpm = bpm;
        m_subdivision = subdivision;
        m_beatsPerMeasure = beatsPerMeasure;
        m_playbackTick = playbackTick;
        m_chartTitle = chartTitle;
        m_playing = playing;
        setPlacementMode(placementMode);
        update();
    }

    void setChartTitle(const QString &title)
    {
        if (m_chartTitle == title) return;
        m_chartTitle = title;
        update();
    }

    void setPlacementMode(bool enabled)
    {
        m_placementMode = enabled;
        setCursor(m_playing ? Qt::ArrowCursor : enabled ? Qt::CrossCursor : Qt::OpenHandCursor);
        update();
    }

    void setInteractionHandlers(std::function<void(qreal, qreal)> placeHandler,
                                std::function<void(int)> selectHandler,
                                std::function<void(int, qreal, qreal, bool)> moveHandler)
    {
        m_notePlacementHandler = std::move(placeHandler);
        m_noteSelectionHandler = std::move(selectHandler);
        m_noteMoveHandler = std::move(moveHandler);
    }

    void setPlaybackPosition(qreal tick, bool playing)
    {
        if (qAbs(m_playbackTick - tick) < 0.01 && m_playing == playing) return;
        m_playbackTick = tick;
        m_playing = playing;
        if (playing) {
            m_dragIndex = -1;
            setCursor(Qt::ArrowCursor);
        }
        else setCursor(m_placementMode ? Qt::CrossCursor : Qt::OpenHandCursor);
        update();
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton || m_playing || !judgePath().contains(event->position())) {
            QWidget::mousePressEvent(event);
            return;
        }
        if (m_placementMode) {
            if (m_notePlacementHandler) {
                const QPointF position = chartPosition(event->position());
                m_notePlacementHandler(position.x(), position.y());
            }
            event->accept();
            return;
        }

        const int hit = nearestNote(event->position());
        if (hit >= 0) {
            m_selected = hit;
            if (m_noteSelectionHandler) m_noteSelectionHandler(hit);
            const Note &note = m_notes->at(hit);
            m_resizeEnd = note.isLong()
                && QLineF(event->position(), planePoint(note.endCoordinates().x(), note.endCoordinates().y())).length() <= 22.0;
            m_dragIndex = hit;
            setCursor(Qt::ClosedHandCursor);
        }
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_dragIndex >= 0 && (event->buttons() & Qt::LeftButton) && !m_playing) {
            const QPointF position = chartPosition(event->position());
            if (m_noteMoveHandler) m_noteMoveHandler(m_dragIndex, position.x(), position.y(), m_resizeEnd);
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_dragIndex >= 0) {
            m_dragIndex = -1;
            m_resizeEnd = false;
            setCursor(m_placementMode ? Qt::CrossCursor : Qt::OpenHandCursor);
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const qreal w = width();
        const qreal h = height();
        const QRectF screen(0, 0, w, h);

        QLinearGradient atmosphere(0, 0, 0, h);
        atmosphere.setColorAt(0.0, QColor("#10161a"));
        atmosphere.setColorAt(0.28, QColor("#05080b"));
        atmosphere.setColorAt(1.0, QColor("#000204"));
        painter.fillRect(screen, atmosphere);
        if (!m_cover.isNull()) {
            const QPixmap scaled = m_cover.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            const QRect source((scaled.width() - width()) / 2, (scaled.height() - height()) / 2, width(), height());
            painter.setOpacity(0.08);
            painter.drawPixmap(rect(), scaled, source);
            painter.setOpacity(1.0);
            painter.fillRect(screen, QColor(0, 3, 7, 205));
        }

        const QPointF horizon(w * 0.5, h * 0.34);
        painter.setPen(QPen(QColor(230, 244, 247, 42), 1));
        for (int scan = 0; scan < 15; ++scan) {
            const qreal y = h * (0.12 + scan * 0.052);
            painter.drawLine(QPointF(w * 0.055, y), QPointF(w * 0.945, y));
        }
        for (int ray = 0; ray <= 8; ++ray) {
            const qreal x = w * (0.06 + ray * 0.11);
            painter.drawLine(horizon, QPointF(x, h * 0.88));
        }

        QPainterPath shell;
        shell.moveTo(w * 0.10, h * 0.055);
        shell.lineTo(w * 0.90, h * 0.055);
        shell.lineTo(w * 0.965, h * 0.13);
        shell.lineTo(w * 0.965, h * 0.86);
        shell.lineTo(w * 0.90, h * 0.945);
        shell.lineTo(w * 0.10, h * 0.945);
        shell.lineTo(w * 0.035, h * 0.86);
        shell.lineTo(w * 0.035, h * 0.13);
        shell.closeSubpath();
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(242, 250, 252, 220), qMax<qreal>(2.0, w * 0.006)));
        painter.drawPath(shell);
        painter.setPen(QPen(QColor(255, 255, 255, 70), 1));
        painter.drawLine(QPointF(w * 0.115, h * 0.075), QPointF(w * 0.885, h * 0.075));
        painter.drawLine(QPointF(w * 0.115, h * 0.925), QPointF(w * 0.885, h * 0.925));

        const QPolygonF field = judgeQuad();
        QPainterPath fieldPath;
        fieldPath.addPolygon(field);
        QLinearGradient track(0, h * 0.24, 0, h * 0.9);
        track.setColorAt(0.0, QColor("#171c20"));
        track.setColorAt(0.48, QColor("#090d11"));
        track.setColorAt(1.0, QColor("#13191e"));
        painter.fillPath(fieldPath, track);

        painter.save();
        painter.setClipPath(fieldPath);
        painter.setPen(QPen(QColor(206, 222, 227, 24), 1));
        for (int line = 1; line < 10; ++line) {
            const qreal depth = line / 10.0;
            const qreal y = horizon.y() + (h * 0.87 - horizon.y()) * depth * depth;
            const qreal halfWidth = w * (0.08 + 0.35 * depth);
            painter.drawLine(QPointF(w * 0.5 - halfWidth, y), QPointF(w * 0.5 + halfWidth, y));
        }
        for (int rail = -4; rail <= 4; ++rail) {
            const qreal bottomX = w * (0.5 + rail * 0.095);
            painter.setPen(QPen(rail == 0 ? QColor(89, 204, 230, 52) : QColor(160, 187, 198, 34),
                                rail == 0 ? 1.3 : 0.8));
            painter.drawLine(horizon, QPointF(bottomX, h * 0.88));
        }
        for (int panel = 0; panel < 3; ++panel) {
            const qreal depth = 0.28 + panel * 0.19;
            const qreal y = horizon.y() + (h * 0.86 - horizon.y()) * depth * depth;
            const qreal halfWidth = w * (0.06 + 0.36 * depth);
            painter.setPen(QPen(QColor(155, 175, 183, 38), 1));
            painter.setBrush(QColor(164, 180, 185, 10));
            painter.drawRect(QRectF(w * 0.5 - halfWidth, y - h * 0.012,
                                   halfWidth * 2, h * 0.024));
        }
        painter.restore();

        auto drawRail = [&painter](const QPointF &from, const QPointF &to, qreal width) {
            painter.setPen(QPen(QColor(48, 222, 255, 38), width * 3.2, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(from, to);
            painter.setPen(QPen(QColor(63, 220, 247, 205), width, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(from, to);
            painter.setPen(QPen(QColor(226, 252, 255, 210), qMax<qreal>(1.0, width * 0.22),
                                Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(from, to);
        };
        drawRail(QPointF(w * 0.35, h * 0.34), QPointF(w * 0.13, h * 0.89), qMax<qreal>(3.0, w * 0.006));
        drawRail(QPointF(w * 0.65, h * 0.34), QPointF(w * 0.87, h * 0.89), qMax<qreal>(3.0, w * 0.006));
        painter.setPen(QPen(QColor(217, 248, 252, 185), 1.2));
        painter.drawLine(QPointF(w * 0.13, h * 0.89), QPointF(w * 0.87, h * 0.89));
        painter.setPen(QPen(QColor(53, 218, 249, 95), 1));
        painter.drawLine(QPointF(w * 0.15, h * 0.91), QPointF(w * 0.85, h * 0.91));

        painter.setPen(QPen(QColor(55, 218, 246, 145), 1));
        painter.setBrush(QColor(13, 92, 119, 145));
        painter.drawRoundedRect(QRectF(w * 0.055, h * 0.27, w * 0.015, h * 0.31), w * 0.004, w * 0.004);
        painter.drawRoundedRect(QRectF(w * 0.93, h * 0.27, w * 0.015, h * 0.31), w * 0.004, w * 0.004);

        if (m_notes) {
            for (int index = 0; index < m_notes->size(); ++index) {
                const Note &note = m_notes->at(index);
                const qreal ticksToHit = note.tick - m_playbackTick;
                const qreal ticksToEnd = (note.isLong() ? note.endTick : note.tick) - m_playbackTick;
                const qreal approachTicks = qMax(1, m_subdivision) * 4.0;
                if (m_playbackTick >= 0.0 && (ticksToHit > approachTicks || ticksToEnd < -0.65)) continue;
                const QPointF hitPosition = planePoint(note.coordinates().x(), note.coordinates().y());
                const qreal progress = m_playbackTick < 0.0 ? 0.0
                    : qBound(0.0, ticksToHit / approachTicks, 1.0);
                const QPointF position = hitPosition * (1.0 - progress) + horizon * progress;
                const QPointF endHitPosition = planePoint(note.endCoordinates().x(), note.endCoordinates().y());
                const qreal endProgress = m_playbackTick < 0.0 ? 0.0
                    : qBound(0.0, (note.endTick - m_playbackTick) / approachTicks, 1.0);
                const QPointF endPosition = endHitPosition * (1.0 - endProgress) + horizon * endProgress;
                const qreal depth = qBound<qreal>(0.0, (position.y() - horizon.y()) / (h * 0.64), 1.0);
                const qreal size = qMax<qreal>(4.0, w * (0.008 + depth * 0.025));
                const QColor color = note.isFake ? QColor("#f2b65c")
                    : note.type == QStringLiteral("EdgeNote") ? QColor("#49b8ff") : QColor("#5be2f1");
                const bool approachingHit = m_playbackTick >= 0.0 && ticksToHit <= 0.0;
                if (note.isLong()) {
                    painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), index == m_selected ? 220 : 130),
                                        qMax<qreal>(3.0, size * 0.48), Qt::SolidLine, Qt::RoundCap));
                    painter.drawLine(position, endPosition);
                    painter.setPen(QPen(index == m_selected ? QColor("#ffffff") : color,
                                        qMax<qreal>(1.3, size * 0.12)));
                    painter.setBrush(QColor(color.red(), color.green(), color.blue(), 210));
                    painter.drawEllipse(endPosition, size * 0.45, size * 0.45);
                }
                painter.setPen(QPen(approachingHit || index == m_selected ? QColor("#ffffff") : color,
                                    qMax<qreal>(1.3, size * 0.13), note.isFake ? Qt::DashLine : Qt::SolidLine));
                painter.setBrush(QColor(color.red(), color.green(), color.blue(),
                                        m_playbackTick < 0.0 && index != m_selected ? 95 : 210));
                const QRectF noteRect(position.x() - size, position.y() - size * 0.55,
                                      size * 2, size * 1.1);
                painter.drawRoundedRect(noteRect, size * 0.18, size * 0.18);
                painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), 95), qMax<qreal>(1.0, size * 0.10)));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(position, size * 1.65, size * 0.9);
            }
        }

        painter.setPen(QPen(QColor(79, 219, 245, 90), 1));
        painter.setBrush(QColor(2, 9, 14, 190));
        painter.drawRoundedRect(QRectF(w * 0.15, h * 0.075, w * 0.70, h * 0.075), h * 0.012, h * 0.012);
        painter.setPen(QColor(226, 244, 247, 225));
        const QFont titleFont(QStringLiteral("Microsoft YaHei UI"), qMax(9, qRound(h * 0.028)), QFont::DemiBold);
        painter.setFont(titleFont);
        const QFontMetrics titleMetrics(titleFont);
        const QString displayTitle = titleMetrics.elidedText(m_chartTitle, Qt::ElideRight,
                                                               qRound(w * 0.34));
        painter.drawText(QRectF(w * 0.17, h * 0.085, w * 0.34, h * 0.05),
                         Qt::AlignLeft | Qt::AlignVCenter, displayTitle);
        const QFont timingFont(QStringLiteral("Consolas"), qMax(8, qRound(h * 0.021)), QFont::DemiBold);
        painter.setFont(timingFont);
        painter.setPen(QColor(92, 220, 244, 220));
        const QString timing = QStringLiteral("BPM %1  ·  %2 DIV  ·  %3 BEATS")
                                   .arg(m_bpm).arg(m_subdivision).arg(m_beatsPerMeasure);
        painter.drawText(QRectF(w * 0.51, h * 0.085, w * 0.32, h * 0.05),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QFontMetrics(timingFont).elidedText(timing, Qt::ElideLeft, qRound(w * 0.32)));

        painter.setPen(QColor(238, 246, 247, 225));
        painter.setFont(QFont(QStringLiteral("Consolas"), qMax(8, qRound(h * 0.025)), QFont::Bold));
        painter.drawText(QRectF(w * 0.04, h * 0.925, w * 0.92, h * 0.04),
                         Qt::AlignCenter,
                         QStringLiteral("%1   /   TICK %2   /   %3 NOTES   /   %4 BEATS PER BAR")
                             .arg(m_playing ? QStringLiteral("PLAYING") : QStringLiteral("PREVIEW"))
                             .arg(m_playbackTick < 0.0 ? 0 : static_cast<int>(m_playbackTick))
                             .arg(m_notes ? m_notes->size() : 0)
                             .arg(m_beatsPerMeasure));
        painter.setPen(QPen(QColor(255, 255, 255, 125), 1));
        painter.drawLine(QPointF(w * 0.04, h * 0.975), QPointF(w * 0.96, h * 0.975));
        painter.setPen(QColor(90, 212, 239, 195));
        painter.drawLine(QPointF(w * 0.05, h * 0.975), QPointF(w * 0.18, h * 0.975));
        painter.drawLine(QPointF(w * 0.82, h * 0.975), QPointF(w * 0.95, h * 0.975));
    }

private:
    QPolygonF judgeQuad() const
    {
        return QPolygonF{QPointF(width() * 0.35, height() * 0.34),
                         QPointF(width() * 0.65, height() * 0.34),
                         QPointF(width() * 0.87, height() * 0.89),
                         QPointF(width() * 0.13, height() * 0.89)};
    }

    QPainterPath judgePath() const
    {
        QPainterPath path;
        path.addPolygon(judgeQuad());
        return path;
    }

    QPointF planePoint(qreal x, qreal y) const
    {
        const QPolygonF plane = judgeQuad();
        const qreal vertical = 1.0 - qBound(0.0, y / 9.0, 1.0);
        const QPointF left = plane.at(0) * (1.0 - vertical) + plane.at(3) * vertical;
        const QPointF right = plane.at(1) * (1.0 - vertical) + plane.at(2) * vertical;
        return left * (1.0 - qBound(0.0, x / 12.0, 1.0))
            + right * qBound(0.0, x / 12.0, 1.0);
    }

    QPointF chartPosition(const QPointF &point) const
    {
        const QPolygonF plane = judgeQuad();
        const qreal vertical = qBound(0.0, (point.y() - plane.at(0).y())
                                               / (plane.at(3).y() - plane.at(0).y()), 1.0);
        const qreal left = plane.at(0).x() * (1.0 - vertical) + plane.at(3).x() * vertical;
        const qreal right = plane.at(1).x() * (1.0 - vertical) + plane.at(2).x() * vertical;
        const qreal x = qBound(0.0, (point.x() - left) / (right - left), 1.0) * 12.0;
        const qreal y = (1.0 - vertical) * 9.0;
        return QPointF(x, y);
    }

    int nearestNote(const QPointF &position) const
    {
        if (!m_notes) return -1;
        int closest = -1;
        qreal closestDistance = 18.0;
        for (int index = 0; index < m_notes->size(); ++index) {
            const Note &note = m_notes->at(index);
            const QPointF notePosition = planePoint(note.coordinates().x(), note.coordinates().y());
            qreal distance = QLineF(position, notePosition).length();
            if (note.isLong()) {
                const QPointF endPosition = planePoint(note.endCoordinates().x(), note.endCoordinates().y());
                distance = pointToSegmentDistance(position, notePosition, endPosition);
            }
            if (distance < closestDistance) {
                closestDistance = distance;
                closest = index;
            }
        }
        return closest;
    }

    QPixmap m_cover;
    const QVector<Note> *m_notes = nullptr;
    int m_selected = -1;
    int m_dragIndex = -1;
    bool m_resizeEnd = false;
    int m_bpm = 120;
    int m_subdivision = 4;
    int m_beatsPerMeasure = 4;
    QString m_chartTitle = QStringLiteral("范式：起源");
    qreal m_playbackTick = -1.0;
    bool m_playing = false;
    bool m_placementMode = false;
    std::function<void(qreal, qreal)> m_notePlacementHandler;
    std::function<void(int)> m_noteSelectionHandler;
    std::function<void(int, qreal, qreal, bool)> m_noteMoveHandler;
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
            lastTick = qMax(lastTick, note.isLong() ? note.endTick : note.tick);
    }
    return HeaderHeight + (lastTick + 2) * TickHeight;
}

int JudgePlane::nearestNote(const QPointF &point) const
{
    if (!m_notes || !gridRect().contains(point)) return -1;
    const int targetLane = laneAt(point.x());
    int closest = -1;
    qreal closestDistance = 14.0;
    for (int i = m_notes->size() - 1; i >= 0; --i) {
        const Note &note = m_notes->at(i);
        const int lane = note.type == QStringLiteral("EdgeNote") ? note.edge : 4;
        if (lane != targetLane) continue;
        const qreal startY = HeaderHeight + (note.tick + 0.5) * TickHeight;
        const qreal endY = HeaderHeight + ((note.isLong() ? note.endTick : note.tick) + 0.5) * TickHeight;
        const qreal distance = point.y() < startY ? startY - point.y()
            : (point.y() > endY ? point.y() - endY : 0.0);
        if (distance < closestDistance) {
            closestDistance = distance;
            closest = i;
        }
    }
    return closest;
}

void JudgePlane::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Bg);
    const QRectF plane = gridRect();
    QFont utility(QStringLiteral("Consolas"), 9);
    painter.setFont(utility);
    const QStringList lanes{QStringLiteral("左边线"), QStringLiteral("右边线"), QStringLiteral("上边线"),
                            QStringLiteral("下边线"), QStringLiteral("判面 12×9")};
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
        const qreal startY = HeaderHeight + note.tick * TickHeight + 3;
        const qreal endY = HeaderHeight + (note.isLong() ? note.endTick : note.tick) * TickHeight + TickHeight - 3;
        const QRectF cell(lane * laneSize + 5, startY, laneSize - 10, qMax<qreal>(TickHeight - 6, endY - startY));
        const QColor color = lane == 4 ? Cyan : Mint;
        painter.setPen(QPen(note.isFake ? Amber : (i == m_selected ? Text : color), i == m_selected ? 2 : 1,
                            note.isFake ? Qt::DashLine : Qt::SolidLine));
        painter.setBrush(note.isFake ? Qt::NoBrush : QColor(color.red(), color.green(), color.blue(), 130));
        painter.drawRoundedRect(cell, 5, 5);
        if (note.isLong()) {
            painter.setBrush(i == m_selected ? Text : color);
            painter.drawRoundedRect(QRectF(cell.left() + 4, cell.bottom() - 5, cell.width() - 8, 5), 2, 2);
        }
        painter.setPen(note.isFake ? Amber : Text);
        painter.drawText(cell.adjusted(2, 2, -2, -2), Qt::AlignTop | Qt::AlignHCenter,
                         note.isLong() ? QStringLiteral("%1 · %2→%3").arg(note.kind).arg(note.tick).arg(note.endTick)
                                       : QStringLiteral("%1 · %2").arg(note.kind).arg(note.tick));
    }
}

void JudgePlane::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !gridRect().contains(event->position())) return;
    const QPointF point = event->position();
    if (m_tool == QStringLiteral("select")) {
        m_dragIndex = nearestNote(point);
        m_resizeEnd = false;
        if (m_dragIndex >= 0 && m_notes) {
            const Note &note = m_notes->at(m_dragIndex);
            const qreal endY = HeaderHeight + ((note.isLong() ? note.endTick : note.tick) + 0.5) * TickHeight;
            m_resizeEnd = note.isLong() && qAbs(point.y() - endY) <= TickHeight * 0.65;
        }
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
    emit noteMoved(m_dragIndex, tickAt(event->position().y()), laneAt(event->position().x()), m_resizeEnd);
}

void JudgePlane::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragIndex = -1;
        m_resizeEnd = false;
    }
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
    m_endTick = new QLineEdit(QStringLiteral("0"), side);
    m_fake = new QCheckBox(QStringLiteral("假音符"), side);
    props->addRow(QStringLiteral("音符种类"), m_kind);
    props->addRow(QStringLiteral("边线区域"), m_edgeBox);
    props->addRow(QStringLiteral("起始 Tick"), m_tick);
    props->addRow(QStringLiteral("结束 Tick"), m_endTick);
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
    previewLayout->addWidget(new QLabel(QStringLiteral("3D 风格谱面预览 · 判定场 12×9"), preview));
    m_coverPreview = new PerspectivePreview(preview);
    previewLayout->addWidget(m_coverPreview, 1);
    auto *playback = new QHBoxLayout;
    m_playButton = new QToolButton(preview);
    m_playButton->setText(QStringLiteral("▶"));
    m_playButton->setToolTip(QStringLiteral("播放 / 暂停"));
    m_playButton->setEnabled(false);
    m_playbackSlider = new QSlider(Qt::Horizontal, preview);
    m_playbackSlider->setRange(0, 0);
    m_playbackSlider->setEnabled(false);
    m_playbackTime = new QLabel(QStringLiteral("00:00 / 00:00"), preview);
    m_playbackTime->setMinimumWidth(104);
    m_playbackTime->setStyleSheet("font-family:Consolas;color:#aeb4bb");
    playback->addWidget(m_playButton);
    playback->addWidget(m_playbackSlider, 1);
    playback->addWidget(m_playbackTime);
    previewLayout->addLayout(playback);
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
    connect(selectAction, &QAction::triggered, this, [this] { m_tool = QStringLiteral("select"); m_plane->setTool(m_tool, m_edge); refreshPreview(); });
    connect(placeAction, &QAction::triggered, this, [this] { m_tool = QStringLiteral("place"); m_plane->setTool(m_tool, m_edge); refreshPreview(); });
    connect(m_title, &QLineEdit::textEdited, this, [this] {
        m_chart.title = m_title->text();
        m_coverPreview->setChartTitle(m_chart.title);
        markDirty();
    });
    connect(m_bpm, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { setTiming(m_bpm->value(), m_subdivision->value(), m_beatsPerMeasure->value()); });
    connect(m_subdivision, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { setTiming(m_bpm->value(), m_subdivision->value(), m_beatsPerMeasure->value()); });
    connect(m_beatsPerMeasure, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { setTiming(m_bpm->value(), m_subdivision->value(), m_beatsPerMeasure->value()); });
    connect(selectButton, &QPushButton::clicked, this, [this] { m_tool = QStringLiteral("select"); m_plane->setTool(m_tool, m_edge); refreshPreview(); });
    connect(placeButton, &QPushButton::clicked, this, [this] { m_tool = QStringLiteral("place"); m_plane->setTool(m_tool, m_edge); refreshPreview(); });
    connect(m_edgeBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) { m_edge = index; m_plane->setTool(m_tool, m_edge); });
    m_coverPreview->setInteractionHandlers(
        [this](qreal x, qreal y) {
            bool ok = false;
            const int tick = m_tick->text().toInt(&ok);
            if (!ok || tick < 0) {
                QMessageBox::warning(this, QStringLiteral("Tick 无效"), QStringLiteral("请先输入非负整数 Tick。"));
                return;
            }
            placePreviewNote(tick, x, y);
        },
        [this](int index) { selectNote(index); },
        [this](int index, qreal x, qreal y, bool resizeEnd) { movePreviewNote(index, x, y, resizeEnd); });
    m_coverPreview->setPlacementMode(false);
    m_mediaPlayer = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_mediaPlayer->setAudioOutput(m_audioOutput);
    m_playbackTimer = new QTimer(this);
    m_playbackTimer->setInterval(50);
    connect(m_playButton, &QToolButton::clicked, this, [this] {
        if (m_mediaPlayer->source().isEmpty()) {
            updateStatus(QStringLiteral("请先打开包含音乐的曲包谱面"));
            return;
        }
        if (m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState)
            m_mediaPlayer->pause();
        else
            m_mediaPlayer->play();
    });
    connect(m_playbackSlider, &QSlider::sliderPressed, this, [this] { m_seeking = true; });
    connect(m_playbackSlider, &QSlider::sliderReleased, this, [this] {
        m_seeking = false;
        m_mediaPlayer->setPosition(m_playbackSlider->value());
        updatePlaybackPosition(m_mediaPlayer->position());
    });
    connect(m_playbackSlider, &QSlider::sliderMoved, this, [this](int position) {
        if (m_seeking) updatePlaybackPosition(position);
    });
    connect(m_mediaPlayer, &QMediaPlayer::durationChanged, this, [this](qint64 duration) {
        m_playbackSlider->setRange(0, static_cast<int>(qMin<qint64>(duration, std::numeric_limits<int>::max())));
        updatePlaybackPosition(m_mediaPlayer->position());
    });
    connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
        if (!m_seeking) updatePlaybackPosition(position);
    });
    connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        m_playButton->setText(state == QMediaPlayer::PlayingState ? QStringLiteral("Ⅱ") : QStringLiteral("▶"));
        if (state == QMediaPlayer::PlayingState) m_playbackTimer->start(); else m_playbackTimer->stop();
        updatePlaybackTick();
    });
    connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::InvalidMedia)
            updateStatus(QStringLiteral("音乐无法播放，请检查音频格式或 Qt Multimedia 后端"));
    });
    connect(m_mediaPlayer, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &error) {
        updateStatus(QStringLiteral("音频错误：%1").arg(error));
    });
    connect(m_playbackTimer, &QTimer::timeout, this, [this] {
        if (!m_seeking) updatePlaybackTick();
    });
    connect(m_plane, &JudgePlane::notePlaced, this, &EditorWindow::placeNote);
    connect(m_plane, &JudgePlane::noteSelected, this, &EditorWindow::selectNote);
    connect(m_plane, &JudgePlane::noteMoved, this, [this](int index, int tick, int lane, bool resizeEnd) {
        if (index < 0 || index >= m_chart.notes.size()) return;
        Note &note = m_chart.notes[index];
        if (resizeEnd && note.isLong()) {
            note.endTick = qMax(note.tick, tick);
        } else {
            const int duration = note.isLong() ? note.endTick - note.tick : 0;
            note.tick = tick;
            note.endTick = tick + duration;
            if (lane < 4 && note.type == QStringLiteral("SpaceNote")) {
                note.type = QStringLiteral("EdgeNote");
                note.edge = lane;
                note.pos = lane < 2 ? qBound(0.0, note.y, 9.0) : qBound(0.0, note.x, 12.0);
                note.endPos = lane < 2 ? qBound(0.0, note.endY, 9.0) : qBound(0.0, note.endX, 12.0);
            } else if (lane == 4 && note.type == QStringLiteral("EdgeNote")) {
                const QPointF start = note.coordinates();
                const QPointF end = note.endCoordinates();
                note.type = QStringLiteral("SpaceNote");
                note.x = start.x(); note.y = start.y();
                note.endX = end.x(); note.endY = end.y();
            } else if (lane < 4) {
                note.edge = lane;
                const qreal limit = lane < 2 ? 9.0 : 12.0;
                note.pos = qBound(0.0, note.pos, limit);
                note.endPos = qBound(0.0, note.endPos, limit);
            }
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
    const bool playing = m_mediaPlayer && m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState;
    const bool placementMode = m_tool == QStringLiteral("place");
    const qreal playbackTick = m_currentPlaybackPosition * static_cast<qreal>(m_chart.bpm)
        * m_chart.subdivision / 60000.0;
    m_coverPreview->setChart(cover, &m_chart.notes, m_selected, m_chart.bpm,
                             m_chart.subdivision, m_chart.beatsPerMeasure,
                             m_mediaPlayer && m_mediaPlayer->source().isEmpty() ? -1.0 : playbackTick,
                             playing, placementMode, m_chart.title);
    setMusicSource();
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

void EditorWindow::placePreviewNote(int tick, qreal x, qreal y)
{
    Note note;
    note.kind = m_kind->currentText();
    note.tick = tick;
    note.endTick = note.isLong() ? tick + qMax(1, m_chart.subdivision) : tick;
    note.isFake = m_fake->isChecked();
    const int edge = edgeAtChartPosition(x, y);
    if (edge >= 0) {
        note.type = QStringLiteral("EdgeNote");
        note.edge = edge;
        note.pos = edge < 2 ? qBound(0.0, y, 9.0) : qBound(0.0, x, 12.0);
        note.endPos = note.pos;
    } else {
        note.type = QStringLiteral("SpaceNote");
        note.x = qBound(0.0, x, 12.0);
        note.y = qBound(0.0, y, 9.0);
        note.endX = note.x;
        note.endY = note.y;
    }
    m_chart.notes.append(note);
    m_selected = m_chart.notes.size() - 1;
    m_tick->setText(QString::number(tick));
    m_endTick->setText(QString::number(note.endTick));
    m_tool = QStringLiteral("select");
    m_plane->setTool(m_tool, m_edge);
    markDirty();
    refresh();
    selectNote(m_selected);
}

void EditorWindow::movePreviewNote(int index, qreal x, qreal y, bool resizeEnd)
{
    if (index < 0 || index >= m_chart.notes.size()) return;
    Note &note = m_chart.notes[index];
    const qreal nextX = qBound(0.0, x, 12.0);
    const qreal nextY = qBound(0.0, y, 9.0);
    const int targetEdge = edgeAtChartPosition(nextX, nextY);
    if (!resizeEnd && targetEdge >= 0) {
        const QPointF oldStart = note.coordinates();
        const QPointF oldEnd = note.endCoordinates();
        const QPointF delta(nextX - oldStart.x(), nextY - oldStart.y());
        note.type = QStringLiteral("EdgeNote");
        note.edge = targetEdge;
        note.pos = targetEdge < 2 ? nextY : nextX;
        const QPointF movedEnd = oldEnd + delta;
        note.endPos = targetEdge < 2 ? qBound(0.0, movedEnd.y(), 9.0)
                                     : qBound(0.0, movedEnd.x(), 12.0);
    } else if (!resizeEnd && targetEdge < 0) {
        const QPointF oldStart = note.coordinates();
        const QPointF oldEnd = note.endCoordinates();
        const QPointF delta(nextX - oldStart.x(), nextY - oldStart.y());
        note.type = QStringLiteral("SpaceNote");
        note.x = nextX;
        note.y = nextY;
        note.endX = qBound(0.0, oldEnd.x() + delta.x(), 12.0);
        note.endY = qBound(0.0, oldEnd.y() + delta.y(), 9.0);
    } else if (note.type == QStringLiteral("EdgeNote")) {
        note.endPos = note.edge < 2 ? nextY : nextX;
    } else {
        note.endX = nextX;
        note.endY = nextY;
    }
    m_selected = index;
    m_plane->setNotes(&m_chart.notes, m_selected);
    m_noteList->item(index)->setText(noteDescription(note));
    markDirty();
    refreshPreview();
}

void EditorWindow::placeNote(int tick, int lane)
{
    Note note;
    note.kind = m_kind->currentText();
    note.tick = tick;
    note.endTick = note.isLong() ? tick + qMax(1, m_chart.subdivision) : tick;
    note.isFake = m_fake->isChecked();
    if (lane < 4) {
        note.type = QStringLiteral("EdgeNote");
        note.edge = lane;
        note.pos = lane < 2 ? 4.5 : 6.0;
        note.endPos = note.pos;
    } else {
        note.type = QStringLiteral("SpaceNote");
        note.x = 6.0;
        note.y = 4.5;
        note.endX = note.x;
        note.endY = note.y;
    }
    m_chart.notes.append(note);
    m_selected = m_chart.notes.size() - 1;
    m_tick->setText(QString::number(tick));
    m_endTick->setText(QString::number(note.endTick));
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
    updatePlaybackTick();
    markDirty();
    refreshPreview();
}

void EditorWindow::selectNote(int index)
{
    m_selected = index;
    if (index >= 0 && index < m_chart.notes.size()) {
        const Note &note = m_chart.notes.at(index);
        const int kindIndex = m_kind->findText(note.kind);
        if (kindIndex >= 0) m_kind->setCurrentIndex(kindIndex);
        m_tick->setText(QString::number(note.tick));
        m_endTick->setText(QString::number(note.isLong() ? note.endTick : note.tick));
        m_fake->setChecked(note.isFake);
        if (note.type == QStringLiteral("EdgeNote")) m_edgeBox->setCurrentIndex(note.edge);
    }
    refresh();
}

void EditorWindow::applyProperties()
{
    if (m_selected < 0 || m_selected >= m_chart.notes.size()) { updateStatus(QStringLiteral("请先选择一个音符")); return; }
    bool tickOk = false;
    bool endTickOk = false;
    const int tick = m_tick->text().toInt(&tickOk);
    const int endTick = m_endTick->text().toInt(&endTickOk);
    const QString kind = m_kind->currentText();
    const bool isLong = kind == QStringLiteral("link") || kind == QStringLiteral("slider");
    if (!tickOk || tick < 0 || (isLong && (!endTickOk || endTick < tick))) {
        QMessageBox::warning(this, QStringLiteral("属性无效"),
                             QStringLiteral("Tick 必须为非负整数，且长条结束 Tick 不得早于起始 Tick。"));
        return;
    }
    Note &note = m_chart.notes[m_selected];
    const bool wasLong = note.isLong();
    note.kind = kind;
    note.tick = tick;
    note.endTick = isLong ? (wasLong ? endTick : qMax(endTick, tick + qMax(1, m_chart.subdivision))) : tick;
    note.isFake = m_fake->isChecked();
    if (note.type == QStringLiteral("EdgeNote")) {
        const QPointF oldStart = note.coordinates();
        const QPointF oldEnd = note.endCoordinates();
        note.edge = m_edgeBox->currentIndex();
        note.pos = note.edge < 2 ? oldStart.y() : oldStart.x();
        note.endPos = note.edge < 2 ? oldEnd.y() : oldEnd.x();
        const qreal limit = note.edge < 2 ? 9.0 : 12.0;
        note.pos = qBound(0.0, note.pos, limit);
        note.endPos = qBound(0.0, note.endPos, limit);
    }
    if (isLong && note.endCoordinates().isNull() && !note.coordinates().isNull()) {
        if (note.type == QStringLiteral("EdgeNote")) note.endPos = note.pos;
        else { note.endX = note.x; note.endY = note.y; }
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
    if (note.isLong()) ++note.endTick;
    m_chart.notes.append(note); m_selected = m_chart.notes.size() - 1; markDirty(); refresh();
}

void EditorWindow::updatePlaybackPosition(qint64 position)
{
    if (!m_seeking) m_playbackSlider->setValue(static_cast<int>(position));
    m_playbackTime->setText(QStringLiteral("%1 / %2")
        .arg(formatPlaybackTime(position), formatPlaybackTime(m_mediaPlayer->duration())));
    updatePlaybackTick(position);
}

void EditorWindow::updatePlaybackTick(qint64 position)
{
    if (position < 0) position = m_mediaPlayer ? m_mediaPlayer->position() : 0;
    m_currentPlaybackPosition = position;
    const bool hasMusic = m_mediaPlayer && !m_mediaPlayer->source().isEmpty();
    qreal preciseTick = -1.0;
    if (hasMusic && m_chart.bpm > 0 && m_chart.subdivision > 0) {
        preciseTick = position * static_cast<qreal>(m_chart.bpm)
            * m_chart.subdivision / 60000.0;
        m_currentPlaybackTick = static_cast<int>(qMin<qreal>(preciseTick,
            std::numeric_limits<int>::max()));
    } else {
        m_currentPlaybackTick = -1;
    }
    m_coverPreview->setPlaybackPosition(preciseTick,
        hasMusic && m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState);
}

void EditorWindow::setMusicSource()
{
    const QString source = m_chartPath.isEmpty() || m_chart.musicPath.isEmpty()
        ? QString() : QFileInfo(m_chartPath).dir().absoluteFilePath(m_chart.musicPath);
    if (source == m_loadedMusicSource) return;
    m_loadedMusicSource = source;
    m_mediaPlayer->stop();
    m_mediaPlayer->setSource(source.isEmpty() ? QUrl() : QUrl::fromLocalFile(source));
    m_currentPlaybackTick = -1;
    m_currentPlaybackPosition = 0;
    m_coverPreview->setPlaybackPosition(-1.0, false);
    m_playbackSlider->setEnabled(!source.isEmpty());
    m_playbackSlider->setRange(0, 0);
    m_playbackSlider->setValue(0);
    m_playbackTime->setText(QStringLiteral("00:00 / 00:00"));
    m_playButton->setEnabled(!source.isEmpty());
    refreshPreview();
}

void EditorWindow::closeEvent(QCloseEvent *event)
{
    if (confirmDiscard()) event->accept(); else event->ignore();
}
