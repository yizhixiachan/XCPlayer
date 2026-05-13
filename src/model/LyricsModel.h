#ifndef LYRICSMODEL_H
#define LYRICSMODEL_H

#include <QAbstractListModel>
#include <QQmlEngine>

class LyricsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    // 暴露给桌面歌词使用
    Q_PROPERTY(int currIndex READ CurrentIndex NOTIFY currentLineChanged)
    Q_PROPERTY(QString currOriginal READ CurrentOriginal NOTIFY currentLineChanged)
    Q_PROPERTY(QString currTranslation READ CurrentTranslation NOTIFY currentLineChanged)
public:
    explicit LyricsModel(QObject *parent = nullptr);

    enum Roles {
        OriginalRole = Qt::UserRole + 1,
        TranslationRole,
        IsCurrentRole
    };

    int CurrentIndex() const { return currIndex; }
    QString CurrentOriginal() const {
        if(currIndex >= 0 && currIndex < items.size()) {
            return items[currIndex].original;
        }
        return "";
    }
    QString CurrentTranslation() const {
        if(currIndex >= 0 && currIndex < items.size()) {
            return items[currIndex].translation;
        }
        return "";
    }

    Q_INVOKABLE void LoadLyrics(const QString& lyrics);
    Q_INVOKABLE bool UpdateCurrentIndex(double sec);
    Q_INVOKABLE double GetTimeAt(int index) const;
protected:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return items.size(); }
    QHash<int, QByteArray> roleNames() const override {
        QHash<int, QByteArray> roles;
        roles[OriginalRole] = "original";
        roles[TranslationRole] = "translation";
        roles[IsCurrentRole] = "isCurrent";
        return roles;
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    struct LyricLine {
        QString original;
        QString translation;
        double time;
    };
    QList<LyricLine> items;

    int currIndex = -1;

signals:
    void currentLineChanged();
};

#endif // LYRICSMODEL_H
