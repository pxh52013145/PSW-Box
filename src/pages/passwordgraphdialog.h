#pragma once

#include <QDialog>

class PasswordRepository;
class PasswordVault;

class QGraphicsScene;
class QGraphicsView;
class QPushButton;

class PasswordGraphDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordGraphDialog(PasswordRepository *repo, PasswordVault *vault, QWidget *parent = nullptr);
    ~PasswordGraphDialog() override;

signals:
    void filterRequested(int entryType, const QString &searchText);

private:
    void setupUi();
    void rebuildGraph();
    void fitViewToScene();

    PasswordRepository *repo_ = nullptr;
    PasswordVault *vault_ = nullptr;

    QGraphicsView *view_ = nullptr;
    QGraphicsScene *scene_ = nullptr;
    QPushButton *refreshBtn_ = nullptr;
    QPushButton *fitBtn_ = nullptr;
};

