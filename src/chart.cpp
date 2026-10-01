#include "chart.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <cmath>
#include <limits>

namespace {
constexpr int GridWidth = 12;
constexpr int GridHeight = 9;

bool readInteger(const QJsonObject &object, const QString &key, int fallback, int minimum, int maximum, int *value)
{
    const QJsonValue jsonValue = object.value(key);
    if (jsonValue.isUndefined()) {
        *value = fallback;
        return true;
    }
    if (!jsonValue.isDouble())
        return false;
    const double number = jsonValue.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number || number < minimum || number > maximum)
        return false;
    *value = static_cast<int>(number);
    return true;
}

bool readNumber(const QJsonObject &object, const QString &key, double max, double *value)
{
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isDouble())
        return false;
    const double number = jsonValue.toDouble();
    if (!std::isfinite(number) || number < 0.0 || number > max)
        return false;
    *value = number;
    return true;
}
}

bool Note::isLong() const
{
    return kind == QStringLiteral("link") || kind == QStringLiteral("slider");
}

QJsonObject Note::toJson() const
{
    QJsonObject object{{QStringLiteral("type"), type}, {QStringLiteral("kind"), kind},
                       {QStringLiteral("tick"), tick}, {QStringLiteral("isFake"), isFake}};
    if (type == QStringLiteral("EdgeNote")) {
        object.insert(QStringLiteral("edge"), edge);
        object.insert(QStringLiteral("pos"), pos);
        if (isLong()) object.insert(QStringLiteral("endPos"), endPos);
    } else {
        object.insert(QStringLiteral("x"), x);
        object.insert(QStringLiteral("y"), y);
        if (isLong()) {
            object.insert(QStringLiteral("endX"), endX);
            object.insert(QStringLiteral("endY"), endY);
        }
    }
    if (isLong()) object.insert(QStringLiteral("endTick"), endTick);
    return object;
}

QPointF Note::coordinates() const
{
    if (type == QStringLiteral("EdgeNote")) {
        if (edge == 0) return {0.0, pos};
        if (edge == 1) return {GridWidth, pos};
        if (edge == 2) return {pos, GridHeight};
        return {pos, 0.0};
    }
    return {x, y};
}

QPointF Note::endCoordinates() const
{
    if (type == QStringLiteral("EdgeNote")) {
        if (edge == 0) return {0.0, endPos};
        if (edge == 1) return {GridWidth, endPos};
        if (edge == 2) return {endPos, GridHeight};
        return {endPos, 0.0};
    }
    return {endX, endY};
}

QJsonObject Chart::toJson() const
{
    QJsonArray noteArray;
    for (const Note &note : notes)
        noteArray.append(note.toJson());
    QJsonObject object{{QStringLiteral("format"), QStringLiteral("ParadigmOriginChart")},
                       {QStringLiteral("version"), 1}, {QStringLiteral("title"), title},
                       {QStringLiteral("bpm"), bpm}, {QStringLiteral("subdivision"), subdivision},
                       {QStringLiteral("beatsPerMeasure"), beatsPerMeasure}, {QStringLiteral("notes"), noteArray}};
    if (!musicPath.isEmpty() || !jacketPath.isEmpty()) {
        QJsonObject assets;
        if (!musicPath.isEmpty()) assets.insert(QStringLiteral("music"), musicPath);
        if (!jacketPath.isEmpty()) assets.insert(QStringLiteral("jacket"), jacketPath);
        object.insert(QStringLiteral("assets"), assets);
    }
    return object;
}

QByteArray Chart::toJsonBytes() const
{
    return QJsonDocument(toJson()).toJson(QJsonDocument::Indented);
}

std::optional<Chart> Chart::fromJson(const QByteArray &data, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = parseError.error == QJsonParseError::NoError
            ? QStringLiteral("谱面内容必须是 JSON 对象。")
            : QStringLiteral("JSON 格式错误：%1").arg(parseError.errorString());
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toString(QStringLiteral("ParadigmOriginChart")) != QStringLiteral("ParadigmOriginChart")
        || root.value(QStringLiteral("version")).toInt(1) != 1) {
        if (error) *error = QStringLiteral("不支持的谱面格式或版本。");
        return std::nullopt;
    }
    if ((!root.value(QStringLiteral("title")).isUndefined() && !root.value(QStringLiteral("title")).isString())
        || !root.value(QStringLiteral("notes")).isArray()) {
        if (error) *error = QStringLiteral("谱面必须包含字符串 title 和音符数组 notes。");
        return std::nullopt;
    }

    Chart chart;
    chart.title = root.value(QStringLiteral("title")).toString(QStringLiteral("未命名谱面"));
    if (!readInteger(root, QStringLiteral("bpm"), 120, 1, 1000, &chart.bpm)
        || !readInteger(root, QStringLiteral("subdivision"), 4, 1, 64, &chart.subdivision)
        || !readInteger(root, QStringLiteral("beatsPerMeasure"), 4, 1, 32, &chart.beatsPerMeasure)) {
        if (error) *error = QStringLiteral("BPM 必须是 1～1000 的整数，分音必须是 1～64 的整数，每小节拍数必须是 1～32 的整数。");
        return std::nullopt;
    }
    const QJsonObject assets = root.value(QStringLiteral("assets")).toObject();
    chart.musicPath = assets.value(QStringLiteral("music")).toString();
    chart.jacketPath = assets.value(QStringLiteral("jacket")).toString();
    const QJsonArray notes = root.value(QStringLiteral("notes")).toArray();
    for (qsizetype i = 0; i < notes.size(); ++i) {
        const QString prefix = QStringLiteral("第 %1 个音符").arg(i + 1);
        if (!notes.at(i).isObject()) {
            if (error) *error = prefix + QStringLiteral(" 必须是 JSON 对象。");
            return std::nullopt;
        }
        const QJsonObject item = notes.at(i).toObject();
        Note note;
        note.type = item.value(QStringLiteral("type")).toString();
        note.kind = item.value(QStringLiteral("kind")).toString(QStringLiteral("tap")).trimmed();
        const QJsonValue tick = item.value(QStringLiteral("tick"));
        const QJsonValue fake = item.value(QStringLiteral("isFake"));
        if ((note.type != QStringLiteral("EdgeNote") && note.type != QStringLiteral("SpaceNote"))
            || note.kind.isEmpty() || (!tick.isUndefined() && (!tick.isDouble() || std::floor(tick.toDouble()) != tick.toDouble()))
            || (!fake.isUndefined() && !fake.isBool())) {
            if (error) *error = prefix + QStringLiteral(" 的类型、kind、tick 或 isFake 无效。");
            return std::nullopt;
        }
        note.tick = tick.toInt();
        note.endTick = note.tick;
        note.isFake = fake.toBool();
        double coordinate = 0.0;
        if (note.type == QStringLiteral("EdgeNote")) {
            const QJsonValue edge = item.value(QStringLiteral("edge"));
            if (!edge.isDouble() || std::floor(edge.toDouble()) != edge.toDouble()
                || edge.toInt() < 0 || edge.toInt() > 3) {
                if (error) *error = prefix + QStringLiteral(" 的 edge 必须是 0～3。");
                return std::nullopt;
            }
            note.edge = edge.toInt();
            const double limit = note.edge < 2 ? GridHeight : GridWidth;
            if (!readNumber(item, QStringLiteral("pos"), limit, &coordinate)) {
                if (error) *error = prefix + QStringLiteral(" 的 pos 超出范围。");
                return std::nullopt;
            }
            note.pos = coordinate;
            note.endPos = note.pos;
            if (note.isLong() && !item.value(QStringLiteral("endPos")).isUndefined()
                && !readNumber(item, QStringLiteral("endPos"), limit, &note.endPos)) {
                if (error) *error = prefix + QStringLiteral(" 的 endPos 超出范围。");
                return std::nullopt;
            }
        } else if (!readNumber(item, QStringLiteral("x"), GridWidth, &note.x)
                   || !readNumber(item, QStringLiteral("y"), GridHeight, &note.y)) {
            if (error) *error = prefix + QStringLiteral(" 的坐标超出判面范围。");
            return std::nullopt;
        } else {
            note.endX = note.x;
            note.endY = note.y;
            if (note.isLong()
                && ((!item.value(QStringLiteral("endX")).isUndefined()
                     && !readNumber(item, QStringLiteral("endX"), GridWidth, &note.endX))
                    || (!item.value(QStringLiteral("endY")).isUndefined()
                        && !readNumber(item, QStringLiteral("endY"), GridHeight, &note.endY)))) {
                if (error) *error = prefix + QStringLiteral(" 的结束坐标超出判面范围。");
                return std::nullopt;
            }
        }
        if (note.isLong()) {
            const QJsonValue endTick = item.value(QStringLiteral("endTick"));
            if (!endTick.isUndefined()
                && (!endTick.isDouble() || std::floor(endTick.toDouble()) != endTick.toDouble()
                    || endTick.toDouble() < note.tick || endTick.toDouble() > std::numeric_limits<int>::max())) {
                if (error) *error = prefix + QStringLiteral(" 的 endTick 必须是不小于 tick 的整数。");
                return std::nullopt;
            }
            if (!endTick.isUndefined()) note.endTick = endTick.toInt();
        }
        chart.notes.append(note);
    }
    return chart;
}

bool saveChart(const QString &path, const Chart &chart, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(chart.toJsonBytes()) < 0 || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}
