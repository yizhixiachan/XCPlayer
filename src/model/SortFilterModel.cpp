#include "SortFilterModel.h"
#include "MediaModel.h"

SortFilterModel::SortFilterModel(QObject *parent)
    : QSortFilterProxyModel{parent}
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    chineseCollator.setLocale(QLocale::Chinese);
    chineseCollator.setCaseSensitivity(Qt::CaseInsensitive);
    chineseCollator.setNumericMode(true);
}

int SortFilterModel::GetIDByIndex(int index) const
{
    if(index < 0 || index >= rowCount()) return -1;
    QModelIndex srcIdx = mapToSource(this->index(index, 0));
    return sourceModel()->data(srcIdx, MediaModel::IDRole).toInt();
}

int SortFilterModel::GetIndexByID(int id) const
{
    for(int i = 0; i < rowCount(); ++i) {
        if(GetIDByIndex(i) == id) return i;
    }
    return -1;
}

bool SortFilterModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    int role = sortRole();
    QVariant leftData = sourceModel()->data(source_left, role);
    QVariant rightData = sourceModel()->data(source_right, role);

    if(leftData.typeId() == QMetaType::QString) {
        return chineseCollator(leftData.toString(), rightData.toString());
    }
    return QSortFilterProxyModel::lessThan(source_left, source_right);
}

bool SortFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    QRegularExpression regExp = filterRegularExpression();
    if(regExp.pattern().isEmpty()) return true;

    QAbstractItemModel* model = sourceModel();
    int column = filterKeyColumn() == -1 ? 0 : filterKeyColumn();
    QModelIndex index = model->index(source_row, column, source_parent);

    for(int role : roles) {
        QString dataStr = model->data(index, role).toString();
        if (regExp.match(dataStr).hasMatch()) return true;
    }
    return false;
}
