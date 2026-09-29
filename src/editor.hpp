#pragma once

#include "chart.hpp"

#include <QMainWindow>
#include <QWidget>

class QComboBox;
class QLineEdit;
class QListWidget;
class QCheckBox;
class QLabel;
class QMouseEvent;

class JudgePlane : public QWidget {
    Q_OBJECT
public:
    explicit JudgePlane(QWidget *parent = nullptr);
    void setNotes(const QVector<Note> *notes, int selected);
    void setTool(const QString &tool, int edge);
signals:
    void notePlaced(double x, double y);
    void noteSelected(int index);
    void noteMoved(int index, double x, double y);
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
private:
    QRectF gridRect() const;
    QPointF toGrid(const QPointF &point) const;
    int nearestNote(const QPointF &point) const;
    const QVector<Note> *m_notes = nullptr;
    int m_selected = -1;
    QString m_tool = QStringLiteral("select");
    int m_edge = 0;
    int m_dragIndex = -1;
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
    void placeNote(double x, double y);
    void refresh();
    void updateStatus(const QString &message = {});
    void markDirty();
    void selectNote(int index);
    void applyProperties();
    void deleteSelected();
    void duplicateSelected();

    Chart m_chart;
    QString m_chartPath;
    QString m_tool = QStringLiteral("select");
    int m_edge = 0;
    int m_selected = -1;
    bool m_dirty = false;
    JudgePlane *m_plane = nullptr;
    QLineEdit *m_title = nullptr;
    QComboBox *m_kind = nullptr;
    QComboBox *m_edgeBox = nullptr;
    QLineEdit *m_tick = nullptr;
    QCheckBox *m_fake = nullptr;
    QListWidget *m_noteList = nullptr;
    QLabel *m_status = nullptr;
};
