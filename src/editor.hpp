#pragma once

#include "chart.hpp"

#include <QMainWindow>
#include <QWidget>
#include <QMediaPlayer>

class QComboBox;
class PerspectivePreview;
class QLineEdit;
class QListWidget;
class QCheckBox;
class QLabel;
class QMouseEvent;
class QScrollArea;
class QSpinBox;
class QSlider;
class QTimer;
class QToolButton;

class JudgePlane : public QWidget {
    Q_OBJECT
public:
    explicit JudgePlane(QWidget *parent = nullptr);
    void setNotes(const QVector<Note> *notes, int selected);
    void setTool(const QString &tool, int edge);
    void setTimingGrid(int subdivision, int beatsPerMeasure);
    void setPlaybackTick(qreal tick, int durationTicks);
signals:
    void notePlaced(int tick, int lane);
    void noteSelected(int index);
    void noteMoved(int index, int tick, int lane, bool resizeEnd);
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
private:
    QRectF gridRect() const;
    double laneWidth() const;
    int tickAt(double y) const;
    int laneAt(double x) const;
    int nearestNote(const QPointF &point) const;
    int contentHeight() const;
    const QVector<Note> *m_notes = nullptr;
    int m_selected = -1;
    QString m_tool = QStringLiteral("select");
    int m_edge = 0;
    int m_dragIndex = -1;
    bool m_resizeEnd = false;
    int m_subdivision = 4;
    int m_beatsPerMeasure = 4;
    qreal m_playbackTick = -1.0;
    int m_durationTicks = 0;
};

class EditorWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit EditorWindow(QWidget *parent = nullptr);
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    bool confirmDiscard();
    void newBundle();
    void openChart();
    bool saveChartFile();
    void placeNote(int tick, int lane);
    void setTiming(int bpm, int subdivision, int beatsPerMeasure);
    void refresh();
    void refreshPreview();
    void updateStatus(const QString &message = {});
    void markDirty();
    void selectNote(int index);
    void applyProperties();
    void deleteSelected();
    void duplicateSelected();
    void placePreviewNote(int tick, qreal x, qreal y);
    void movePreviewNote(int index, qreal x, qreal y, bool resizeEnd);
    void updatePlaybackPosition(qint64 position);
    void updatePlaybackTick(qint64 position = -1);
    void setMusicSource();

    Chart m_chart;
    QString m_chartPath;
    QString m_loadedMusicSource;
    QString m_tool = QStringLiteral("select");
    int m_edge = 0;
    int m_selected = -1;
    bool m_dirty = false;
    JudgePlane *m_plane = nullptr;
    QLineEdit *m_title = nullptr;
    QComboBox *m_kind = nullptr;
    QComboBox *m_edgeBox = nullptr;
    QSpinBox *m_bpm = nullptr;
    QSpinBox *m_subdivision = nullptr;
    QSpinBox *m_beatsPerMeasure = nullptr;
    QLineEdit *m_tick = nullptr;
    QLineEdit *m_endTick = nullptr;
    QScrollArea *m_timelineScroll = nullptr;
    QCheckBox *m_fake = nullptr;
    QListWidget *m_noteList = nullptr;
    QLabel *m_status = nullptr;
    PerspectivePreview *m_coverPreview = nullptr;
    QLabel *m_playbackTime = nullptr;
    QMediaPlayer *m_mediaPlayer = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QSlider *m_playbackSlider = nullptr;
    QToolButton *m_playButton = nullptr;
    QTimer *m_playbackTimer = nullptr;
    int m_currentPlaybackTick = -1;
    qint64 m_currentPlaybackPosition = 0;
    bool m_seeking = false;
};
