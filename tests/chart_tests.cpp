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
    chart.notes.append(space);
    Note edge;
    edge.type = QStringLiteral("EdgeNote");
    edge.edge = 2;
    edge.pos = 3.5;
    edge.isFake = true;
    chart.notes.append(edge);

    QString error;
    const auto parsed = Chart::fromJson(chart.toJsonBytes(), &error);
    assert(parsed && error.isEmpty());
    assert(parsed->title == chart.title && parsed->notes.size() == 2);
    assert(parsed->bpm == 174 && parsed->subdivision == 6 && parsed->beatsPerMeasure == 7);
    assert(parsed->musicPath == QStringLiteral("music.ogg"));
    assert(parsed->notes.at(0).coordinates() == QPointF(6.25, 4.5));
    assert(parsed->notes.at(1).coordinates() == QPointF(3.5, 9.0));

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

    QTemporaryDir temp;
    assert(temp.isValid());
    const QString path = temp.filePath(QStringLiteral("chart.json"));
    assert(saveChart(path, chart, &error));
    QFile saved(path);
    assert(saved.open(QIODevice::ReadOnly));
    assert(Chart::fromJson(saved.readAll(), &error));
    return 0;
}
