#include "chart.hpp"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QFile>
#include <cassert>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    Chart chart;
    chart.title = QStringLiteral("测试曲包");
    chart.bpm = 174;
    chart.subdivision = 6;
    chart.beatsPerMeasure = 7;
    chart.musicPath = QStringLiteral("music.ogg");
    chart.jacketPath = QStringLiteral("jp.png");
    Note space;
    space.type = QStringLiteral("SpaceNote");
    space.x = 6.25;
    space.y = 4.5;
    space.tick = 48;
    space.kind = QStringLiteral("slider");
    space.endTick = 60;
    space.endX = 9.5;
    space.endY = 2.25;
    chart.notes.append(space);
    Note edge;
    edge.type = QStringLiteral("EdgeNote");
    edge.edge = 2;
    edge.pos = 3.5;
    edge.kind = QStringLiteral("link");
    edge.tick = 20;
    edge.endTick = 32;
    edge.endPos = 8.0;
    edge.isFake = true;
    chart.notes.append(edge);

    QString error;
    const auto parsed = Chart::fromJson(chart.toJsonBytes(), &error);
    assert(parsed && error.isEmpty());
    assert(parsed->title == chart.title && parsed->notes.size() == 2);
    assert(parsed->bpm == 174 && parsed->subdivision == 6 && parsed->beatsPerMeasure == 7);
    assert(parsed->musicPath == QStringLiteral("music.ogg"));
    assert(parsed->notes.at(0).coordinates() == QPointF(6.25, 4.5));
    assert(parsed->notes.at(0).endCoordinates() == QPointF(9.5, 2.25));
    assert(parsed->notes.at(0).endTick == 60);
    assert(parsed->notes.at(1).coordinates() == QPointF(3.5, 9.0));
    assert(parsed->notes.at(1).endCoordinates() == QPointF(8.0, 9.0));
    assert(parsed->notes.at(1).endTick == 32);

    const QByteArray legacy = R"({"format":"ParadigmOriginChart","version":1,"title":"旧谱","notes":[]})";
    const auto legacyChart = Chart::fromJson(legacy, &error);
    assert(legacyChart && legacyChart->bpm == 120 && legacyChart->subdivision == 4
           && legacyChart->beatsPerMeasure == 4);

    const QByteArray invalidTiming = R"({"format":"ParadigmOriginChart","version":1,"title":"bad","bpm":1001,"subdivision":4,"notes":[]})";
    assert(!Chart::fromJson(invalidTiming, &error));
    assert(!error.isEmpty());
    const QByteArray invalidMeter = R"({"format":"ParadigmOriginChart","version":1,"title":"bad","bpm":120,"subdivision":4,"beatsPerMeasure":33,"notes":[]})";
    assert(!Chart::fromJson(invalidMeter, &error));
    assert(!error.isEmpty());

    const QByteArray invalid = R"({"format":"ParadigmOriginChart","version":1,"title":"bad","notes":[{"type":"SpaceNote","kind":"tap","tick":0,"isFake":false,"x":13,"y":4}]})";
    assert(!Chart::fromJson(invalid, &error));
    assert(!error.isEmpty());
    const QByteArray negativeTick = R"({"format":"ParadigmOriginChart","version":1,"title":"bad","notes":[{"type":"SpaceNote","kind":"tap","tick":-1,"isFake":false,"x":2,"y":3}]})";
    assert(!Chart::fromJson(negativeTick, &error));
    assert(!error.isEmpty());
    const QByteArray invalidEndTick = R"({"format":"ParadigmOriginChart","version":1,"title":"bad","notes":[{"type":"SpaceNote","kind":"slider","tick":8,"endTick":7,"x":2,"y":3,"endX":4,"endY":5}]})";
    assert(!Chart::fromJson(invalidEndTick, &error));
    assert(!error.isEmpty());
    const QByteArray invalidEndPosition = R"({"format":"ParadigmOriginChart","version":1,"title":"bad","notes":[{"type":"EdgeNote","kind":"link","tick":1,"endTick":2,"edge":0,"pos":4,"endPos":10}]})";
    assert(!Chart::fromJson(invalidEndPosition, &error));
    assert(!error.isEmpty());
    const QByteArray legacyLong = R"({"format":"ParadigmOriginChart","version":1,"title":"legacy","notes":[{"type":"SpaceNote","kind":"slider","tick":8,"x":2,"y":3}]})";
    const auto parsedLegacyLong = Chart::fromJson(legacyLong, &error);
    assert(parsedLegacyLong);
    assert(parsedLegacyLong->notes.at(0).endTick == 8);
    assert(parsedLegacyLong->notes.at(0).endCoordinates() == QPointF(2.0, 3.0));

    QTemporaryDir temp;
    assert(temp.isValid());
    const QString path = temp.filePath(QStringLiteral("chart.json"));
    assert(saveChart(path, chart, &error));
    QFile saved(path);
    assert(saved.open(QIODevice::ReadOnly));
    assert(Chart::fromJson(saved.readAll(), &error));
    return 0;
}
