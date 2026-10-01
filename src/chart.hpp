#pragma once

#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QVector>
#include <optional>

inline constexpr int ChartGridWidth = 12;
inline constexpr int ChartGridHeight = 9;

struct Note {
    QString type;
    QString kind = QStringLiteral("tap");
    int tick = 0;
    int endTick = 0;
    bool isFake = false;
    int edge = 0;
    double pos = 0.0;
    double endPos = 0.0;
    double x = 0.0;
    double y = 0.0;
    double endX = 0.0;
    double endY = 0.0;

    bool isLong() const;
    QJsonObject toJson() const;
    QPointF coordinates() const;
    QPointF endCoordinates() const;
};

struct Chart {
    QString title = QStringLiteral("未命名谱面");
    int bpm = 120;
    int subdivision = 4;
    int beatsPerMeasure = 4;
    QVector<Note> notes;
    QString musicPath;
    QString jacketPath;

    QJsonObject toJson() const;
    QByteArray toJsonBytes() const;
    static std::optional<Chart> fromJson(const QByteArray &data, QString *error = nullptr);
};

bool saveChart(const QString &path, const Chart &chart, QString *error = nullptr);
