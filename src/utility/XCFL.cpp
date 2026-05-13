#include "XCFL.h"
#include <regex>
#include <QPixmap>
#include <filesystem>
#include <fstream>

#include <QRegularExpression>

QMap<double, QPair<QString, QString>> XCFL::FormatLyrics(const QString &lyrics)
{
    QMap<double, QPair<QString, QString>> lyricsMap;
    // [mm:ss:xx*\xxx*]
    static QRegularExpression timeRegex(R"(\[(\d{2}):(\d{2})\.(\d{2,3})[^\]]*\])");

    // 优先级1：〖〗、【】
    auto getP1Open = [](QChar close) {
        if (close == QChar(0x3017)) return QChar(0x3016); // 〗→〖
        if (close == QChar(0x3011)) return QChar(0x3010); // 】→【
        return QChar(0);
    };
    auto isP1Close = [](QChar c) {
        return c == QChar(0x3017) || c == QChar(0x3011);
    };

    // 优先级3：「」、『』
    auto getP3Open = [](QChar close) {
        if (close == QChar(0x300D)) return QChar(0x300C); // 」→「
        if (close == QChar(0x300F)) return QChar(0x300E); // 』→『
        return QChar(0);
    };
    auto isP3Close = [](QChar c) {
        return c == QChar(0x300D) || c == QChar(0x300F);
    };

    // 删除最外层括号
    auto removeOuterBrackets = [&](QString s) {
        s = s.trimmed();
        if(s.length() < 2) return s;

        QChar first = s.front();
        QChar last = s.back();

        if(isP1Close(last)) {
            if(first == getP1Open(last)) return s.mid(1, s.length() - 2).trimmed();
        }
        if(isP3Close(last)) {
            if (first == getP3Open(last)) return s.mid(1, s.length() - 2).trimmed();
        }
        return s;
    };

    QStringList lines = lyrics.split('\n', Qt::SkipEmptyParts);

    for(const QString &line : lines){
        QString text = line.trimmed();
        // 跳过元数据行和空时间标签行
        if(text.startsWith('[') && text.endsWith(']')) continue;

        QRegularExpressionMatch match = timeRegex.match(text);
        if(!match.hasMatch()) continue;
        double totalSeconds = match.captured(1).toInt() * 60.0 +
                              match.captured(2).toInt() +
                              ((match.captured(3).length() == 3)
                                   ? match.captured(3).toInt() / 1000.0
                                   : match.captured(3).toInt() / 100.0);

        text.remove(timeRegex);
        text = text.trimmed();
        if(text.isEmpty()) continue;

        QString originalText = text;
        QString translationText = "";
        QChar lastChar = text.back();

        // 优先级 1: 〖〗、【】
        if(isP1Close(lastChar)) {
            QChar openChar = getP1Open(lastChar);
            int openIdx = text.lastIndexOf(openChar);
            if(openIdx > 0) {
                originalText = text.left(openIdx).trimmed();
                translationText = text.mid(openIdx + 1, text.length() - openIdx - 2).trimmed();
            }
        }

        // 优先级 2: 窄空格
        if(translationText.isEmpty()) {
            int sepIndex = text.indexOf(QChar(0x2009));
            if(sepIndex > 0) {
                originalText = text.left(sepIndex).trimmed();
                translationText = text.mid(sepIndex + 1).trimmed();
            }
        }

        // 优先级 3: 「」、『』
        if (translationText.isEmpty()) {
            if(isP3Close(lastChar)) {
                QChar openChar = getP3Open(lastChar);
                int openIdx = text.lastIndexOf(openChar);
                if(openIdx > 0) {
                    originalText = text.left(openIdx).trimmed();
                    translationText = text.mid(openIdx + 1, text.length() - openIdx - 2).trimmed();
                }
            }
        }

        // 删除译文最外层括号
        translationText = removeOuterBrackets(translationText);

        lyricsMap.insert(totalSeconds, {originalText, translationText});
    }

    return lyricsMap;
}

QList<XC::ExternalSubtitle> XCFL::ParseSubtitle(const QString &localPath)
{
    QList<XC::ExternalSubtitle> subs;

    std::filesystem::path p;
#ifdef Q_OS_WIN
    p = localPath.toStdWString();
#else
    p = localPath.toStdString();
#endif

    std::ifstream file(p, std::ios::binary);
    if (!file.is_open()) return subs;

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    QString content = DecodeText(buffer.str());

    // 统一换行符
    content.replace("\r\n", "\n");

    // 文件内容包含了 ASS 特有的头信息，则走 ASS 解析
    if(content.contains("[Script Info]") || content.contains("[Events]")) {
        subs = ParseASS(content);
    } else {
        subs = ParseSRTorVTT(content);
    }

    // 进行稳定排序
    std::stable_sort(subs.begin(), subs.end(),[](const XC::ExternalSubtitle& a, const XC::ExternalSubtitle& b) {
        return a.start < b.start;
    });

    // 合并时间轴完全相同的双语字幕
    QList<XC::ExternalSubtitle> mergedSubs;
    for (const auto& sub : subs) {
        if (!mergedSubs.isEmpty()) {
            auto& lastSub = mergedSubs.last();
            if (std::abs(lastSub.start - sub.start) < 0.001 &&
                std::abs(lastSub.end - sub.end) < 0.001) {
                lastSub.text += "\n" + sub.text;
                continue;
            }
        }
        mergedSubs.append(sub);
    }

    return mergedSubs;
}

QString XCFL::DecodeText(const std::string &str)
{
    if(str.empty()) return QString();

    QByteArrayView view(str.data(), static_cast<qsizetype>(str.size()));

    // 探测 BOM (Byte Order Mark)
    auto encodingOpt = QStringConverter::encodingForData(view);
    if(encodingOpt.has_value()) {
        QStringDecoder decoder(encodingOpt.value());
        return decoder(view);
    }

    // 如果没有 BOM，默认先尝试按照纯 UTF-8 解码
    QStringDecoder utf8Decoder(QStringConverter::Utf8);
    QString text = utf8Decoder(view);

    if(utf8Decoder.hasError()) {
        // 回退到操作系统本地编码
        QStringDecoder localDecoder(QStringConverter::System);
        return localDecoder(view);
    }

    return text;
}

QList<QColor> XCFL::ExtractDominantColors(const QPixmap &src, int k, int maxIterations)
{
    if (src.isNull() || k <= 0) return {};

    QImage image = src.toImage().convertToFormat(QImage::Format_ARGB32);
    if (image.isNull()) return {};

    // 降采样
    const int targetSize = 64;
    if (image.width() > targetSize || image.height() > targetSize) {
        image = image.scaled(targetSize, targetSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }


    std::vector<FastColor> points;
    points.reserve(image.width() * image.height());

    const QRgb* data = reinterpret_cast<const QRgb*>(image.constBits());
    int pixelCount = image.width() * image.height();
    for (int i = 0; i < pixelCount; ++i) {
        QRgb p = data[i];
        if (qAlpha(p) > 125) { // 忽略透明或半透明像素
            points.push_back({qRed(p), qGreen(p), qBlue(p)});
        }
    }

    if (points.empty()) return {};
    if (points.size() <= static_cast<size_t>(k)) {
        QList<QColor> res;
        for (const auto& p : points) res.append(QColor(p.r, p.g, p.b));
        return res;
    }

    // K-Means++ 初始化中心点
    std::vector<FastColor> centers;
    centers.reserve(k);
    centers.push_back(points[points.size() / 2]);

    // 距离缓存
    std::vector<int> minDists(points.size(), std::numeric_limits<int>::max());

    for(int i = 1; i < k; ++i) {
        int bestIdx = 0;
        int maxDist = -1;
        const FastColor& lastCenter = centers.back();

        for(size_t j = 0; j < points.size(); ++j) {
            int d = ColorDistance(points[j], lastCenter);
            if(d < minDists[j]) minDists[j] = d; // 更新每个点到最近中心的距离

            if(minDists[j] > maxDist) {
                maxDist = minDists[j];
                bestIdx = j;
            }
        }
        centers.push_back(points[bestIdx]);
    }

    // K-Means 迭代
    std::vector<int> clusterSizes(k, 0);
    std::vector<long long> sumR(k, 0), sumG(k, 0), sumB(k, 0);

    for(int iter = 0; iter < maxIterations; ++iter) {
        std::fill(clusterSizes.begin(), clusterSizes.end(), 0);
        std::fill(sumR.begin(), sumR.end(), 0);
        std::fill(sumG.begin(), sumG.end(), 0);
        std::fill(sumB.begin(), sumB.end(), 0);

        // 分配簇
        for(const auto& p : points) {
            int bestC = 0;
            int minD = std::numeric_limits<int>::max();
            for (int c = 0; c < k; ++c) {
                int d = ColorDistance(p, centers[c]);
                if (d < minD) {
                    minD = d;
                    bestC = c;
                }
            }
            clusterSizes[bestC]++;
            sumR[bestC] += p.r;
            sumG[bestC] += p.g;
            sumB[bestC] += p.b;
        }

        // 重新计算中心点
        bool changed = false;
        for(int c = 0; c < k; ++c) {
            if (clusterSizes[c] == 0) continue;

            FastColor newCenter = {
                static_cast<int>(sumR[c] / clusterSizes[c]),
                static_cast<int>(sumG[c] / clusterSizes[c]),
                static_cast<int>(sumB[c] / clusterSizes[c])
            };

            if(!(newCenter == centers[c])) {
                centers[c] = newCenter;
                changed = true;
            }
        }
        if(!changed) break; // 中心点不再移动，提前结束
    }

    // 排序并组装结果
    struct ClusterResult { int size; FastColor color; };
    std::vector<ClusterResult> results;
    for(int c = 0; c < k; ++c) {
        if(clusterSizes[c] > 0) {
            results.push_back({clusterSizes[c], centers[c]});
        }
    }

    // 按照该颜色占比大小降序排序
    std::sort(results.begin(), results.end(), [](const ClusterResult& a, const ClusterResult& b) {
        return a.size > b.size;
    });

    QList<QColor> finalColors;
    for(const auto& res : results) {
        finalColors.append(QColor(res.color.r, res.color.g, res.color.b));
    }

    return finalColors;
}

double XCFL::ParseTime(const QString &timeStr)
{
    std::regex re(R"((?:(\d+):)?(\d{1,2}):(\d{2})[,.](\d+))");
    std::string timeStrStd = timeStr.toStdString();
    std::smatch match;

    if (std::regex_search(timeStrStd, match, re)) {
        double h = match[1].str().empty() ? 0.0 : std::stod(match[1].str());
        double m = std::stod(match[2].str());
        double s = std::stod(match[3].str());

        QString fractionalStr = "0." + QString::fromStdString(match[4].str());
        double fractionalSec = fractionalStr.toDouble();

        return h * 3600.0 + m * 60.0 + s + fractionalSec;
    }
    return 0.0;
}

QList<XC::ExternalSubtitle> XCFL::ParseASS(const QString &content)
{
    QList<XC::ExternalSubtitle> subs;
    QStringList lines = content.split("\n", Qt::SkipEmptyParts);
    bool inEvents = false;

    // 清理 ASS 样式
    static std::regex tagRegex("\\{[^}]*\\}");

    for (const QString& line : lines) {
        if (line.trimmed() == "[Events]") {
            inEvents = true;
            continue;
        }
        if (inEvents && line.startsWith("Dialogue:")) {
            // Dialogue: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
            QString data = line.mid(9).trimmed();
            QStringList parts = data.split(",", Qt::KeepEmptyParts);

            if (parts.size() >= 10) {
                XC::ExternalSubtitle sub;
                sub.start = ParseTime(parts[1].trimmed());
                sub.end = ParseTime(parts[2].trimmed());

                // 合并第 9 个逗号后的所有文本
                QString text;
                for(int i = 9; i < parts.size(); ++i) {
                    text += parts[i] + (i == parts.size() - 1 ? "" : ",");
                }

                // 清理 ASS 标签并将 \N 替换为标准换行
                std::string textStr = text.toStdString();
                textStr = std::regex_replace(textStr, tagRegex, "");
                QString cleanText = QString::fromStdString(textStr);
                cleanText.replace("\\N", "\n");
                cleanText.replace("\\n", "\n");
                sub.text = cleanText.trimmed();

                subs.append(sub);
            }
        }
    }
    return subs;
}

QList<XC::ExternalSubtitle> XCFL::ParseSRTorVTT(const QString &content)
{
    QList<XC::ExternalSubtitle> subs;
    QStringList lines = content.split("\n");

    static std::regex timeRegex(R"(((?:\d+:)?\d{1,2}:\d{2}[,\.]\d+)\s*-->\s*((?:\d+:)?\d{1,2}:\d{2}[,\.]\d+))");
    static std::regex vttTagRegex("<[^>]*>");

    XC::ExternalSubtitle currentSub;
    QString currentText;
    bool parsingText = false;

    for(int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();

        if (line == "WEBVTT" || line.startsWith("NOTE")) continue;

        std::string lineStr = line.toStdString();
        std::smatch match;
        if(std::regex_search(lineStr, match, timeRegex)) {
            // 结算并保存上一条正在解析的字幕
            if(parsingText && !currentText.isEmpty()) {
                currentSub.text = currentText.trimmed();
                subs.append(currentSub);
                currentText.clear();
            }

            currentSub.start = ParseTime(QString::fromStdString(match[1].str()));
            currentSub.end = ParseTime(QString::fromStdString(match[2].str()));
            parsingText = true;
        }
        else if(parsingText) {
            if(line.isEmpty()) {
                // 空行代表当前字幕块结束
                currentSub.text = currentText.trimmed();
                subs.append(currentSub);
                currentText.clear();
                parsingText = false;
            } else {
                // 累加文本，并过滤掉 VTT 自带的行内 HTML 标签
                std::string cleanLineStr = std::regex_replace(lineStr, vttTagRegex, "");
                QString cleanLine = QString::fromStdString(cleanLineStr);
                currentText += cleanLine + "\n";
            }
        }
    }

    if(parsingText && !currentText.isEmpty()) {
        currentSub.text = currentText.trimmed();
        subs.append(currentSub);
    }

    return subs;
}

int XCFL::ColorDistance(const FastColor& c1, const FastColor& c2) {
    int dr = c1.r - c2.r;
    int dg = c1.g - c2.g;
    int db = c1.b - c2.b;
    return 2 * dr * dr + 4 * dg * dg + 3 * db * db;
}
