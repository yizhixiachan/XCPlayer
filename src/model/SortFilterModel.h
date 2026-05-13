#ifndef SORTFILTERMODEL_H
#define SORTFILTERMODEL_H

#include <QQmlEngine>
#include <QSortFilterProxyModel>
#include <QCollator>

class SortFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit SortFilterModel(QObject *parent = nullptr);

    Q_INVOKABLE int GetIDByIndex(int index) const;
    Q_INVOKABLE int GetIndexByID(int id) const;

    Q_INVOKABLE void SetFilterRoles(const QList<int>& roleList) {
        roles = roleList;
        invalidateFilter();
    }

    Q_INVOKABLE void Sort(int order) {
        sort(0, static_cast<Qt::SortOrder>(order));
    }
protected:
    // 更好的中文排序
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;
    // 支持多role过滤
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
private:
    QList<int> roles;
    QCollator chineseCollator;
};

#endif // SORTFILTERMODEL_H
