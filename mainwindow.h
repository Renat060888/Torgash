#pragma once

// qt
#include <QMainWindow>
// project
#include "torgash.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow{
    Q_OBJECT
public:

    explicit MainWindow( QWidget * parent = 0 );
    ~MainWindow();

    bool init();

private slots:

    // events profile
    void on_buttonCreateProfile_clicked();
    void on_buttonRemoveProfile_clicked();
    void on_listProfiles_clicked(const QModelIndex &index);

    void on_lineHttps_returnPressed();
    void on_lineWebsocket_returnPressed();
    void on_lineSecretKey_returnPressed();
    void on_lineSteamKey_returnPressed();

    // events items
    void on_buttonAddObject_clicked();
    void on_buttonRemoveObject_clicked();
    void on_buttonSelectAll_clicked();
    void on_buttonDeselectAll_clicked();
    void on_tableWidget_cellChanged(int row, int column);

    // events trades
    void on_buttonRunTrades_clicked();
    void on_buttonStopTrades_clicked();

    // [dummy]
    void on_listProfiles_itemSelectionChanged();
    void on_tableWidget_clicked( const QModelIndex & index );
    void on_tableWidget_entered(const QModelIndex &index);
    void on_tableWidget_cellEntered(int row, int column);

    void on_tableWidget_cellPressed(int row, int column);
    void on_tableWidget_cellClicked(int row, int column);
    void on_pushButton_clicked();
    void on_buttonClearLog_clicked();

private:

    // init
    void initGUI();

    // current profile
    void updateInterface();
    void updateInterfaceItems( Profile_t * _profile );

    void snapshotCurrentProfile();
    void snapshotCurrentProfileItems( Profile_t * _profile );

    Ui::MainWindow *    ui;

    Torgash *           m_torgash;

};


























