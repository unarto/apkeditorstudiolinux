#ifndef ICONITEMSMODEL_H
#define ICONITEMSMODEL_H

#include <QAbstractProxyModel>
#include "apk/resourceitemsmodel.h"
#include "base/treenode.h"

class ManifestScope;

class IconItemsModel : public QAbstractProxyModel, public IResourceItemsModel
{
    Q_OBJECT
    Q_INTERFACES(IResourceItemsModel)

public:
    enum Row {
        ApplicationRow,
        ActivitiesRow,
        RowCount
    };

    enum Column {
        CaptionColumn,
        PathColumn,
        TypeColumn,
        ColumnCount
    };

    enum IconType {
        TypeIcon,
        TypeRoundIcon,
        TypeBanner
    };

    explicit IconItemsModel(QObject *parent = nullptr);
    ~IconItemsModel() override;

    void setSourceModel(QAbstractItemModel *sourceModel) override;
    void setManifestScopes(const QList<ManifestScope *> &scopes);
    ResourceItemsModel *sourceModel() const;

    QIcon getIcon() const;
    QIcon getIcon(const QModelIndex &index) const;
    QString getIconPath(const QModelIndex &index) const;
    QString getIconCaption(const QModelIndex &index) const;
    IconType getIconType(const QModelIndex &index) const;

    bool replaceApplicationIcons(const QString &path, QWidget *parent = nullptr);
    bool replaceResource(const QModelIndex &index, const QString &path = QString(), QWidget *parent = nullptr) override;
    bool removeResource(const QModelIndex &index) override;
    QString getResourcePath(const QModelIndex &index) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;
    void sort(int column = 0, Qt::SortOrder order = Qt::AscendingOrder) override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    bool removeRows(int row, int count, const QModelIndex &parent) override;

private:
    struct IconNode : public TreeNode
    {
        IconNode(IconType iconType) : iconType(iconType) {}
        void addChild(TreeNode *node) = delete;
        const IconType iconType;
    };

    struct ActivityNode : public TreeNode
    {
        explicit ActivityNode(ManifestScope *scope) : scope(scope) {}
        void addChild(IconNode *node);
        const ManifestScope *scope;
    };

    bool appendIcon(const QPersistentModelIndex &index, ManifestScope *scope, IconType type = TypeIcon);
    void populateFromSource(const QModelIndex &parent = {});
    void sourceRowsInserted(const QModelIndex &parent, int first, int last);
    void sourceRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
    void sourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void sourceModelReset();

    QList<ManifestScope *> scopes;
    QHash<QPersistentModelIndex, IconNode *> sourceToProxyMap;
    QHash<IconNode *, QPersistentModelIndex> proxyToSourceMap;
    TreeNode *root;
    TreeNode *applicationNode;
    TreeNode *activitiesNode;
};

#endif // ICONITEMSMODEL_H
