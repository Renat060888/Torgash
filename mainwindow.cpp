
// std
#include <iostream>
#include <cassert>
#include <mutex>
// qt
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QListWidgetItem>
#include <QComboBox>
#include "ui_mainwindow.h"
// project
#include "mainwindow.h"

using namespace std;


// --------------------------------------
// GUI LOGGER
// --------------------------------------
QPlainTextEdit * g_log;
mutex g_loggerMutex;
void g_logger( const string _msg ){

    lock_guard<mutex> lock( g_loggerMutex );

    if( ! g_log ){
        assert( 1 == 0 );
    }

    g_log->appendPlainText( QString::fromStdString(_msg) );
    g_log->viewport()->update();
}


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),    
    ui(new Ui::MainWindow),
    m_torgash(nullptr){

    ui->setupUi(this);

}

MainWindow::~MainWindow(){

    delete m_torgash;
    m_torgash = nullptr;

    delete ui;
}

bool MainWindow::init(){

    // 1
    initGUI();

    // 2
    m_torgash = new Torgash();
    if( ! m_torgash->init() ){
        return false;
    }

    // 3
    updateInterface();

    return true;
}

void MainWindow::initGUI(){

    // logger
    g_log = ui->plainTextLog;

    // items table
    ui->tableWidget->setColumnCount( 9 );
    ui->tableWidget->setColumnWidth( 0, 50 );
    ui->tableWidget->setColumnWidth( 1, 50 );
    ui->tableWidget->setHorizontalHeaderLabels( QStringList() << "active" << "pkey" << "object name" << "price_for_buy" << "buys_number" << "price_stop" << "class id" << "instance id" << "mode" );
}

void MainWindow::updateInterface(){

    vector< Profile_t * > profiles = m_torgash->getProfiles();

    // TODO после удаления профиля его имя остается в интерфейсе

    // current profile exists in GUI - program in process - fill interface by this profile
    QListWidgetItem * currentListItem = ui->listProfiles->currentItem();
    if( currentListItem ){

        const string currentProfileName = currentListItem->text().toStdString();
        ui->listProfiles->clear();

        for( Profile_t * profile : profiles ){

            // TODO memory leak
            QListWidgetItem * listItem = new QListWidgetItem( QString::fromStdString(profile->profileName) );
            ui->listProfiles->addItem( listItem );

            if( profile->profileName == currentProfileName ){

                listItem->setSelected( true );

                ui->lineHttps->setText( profile->httpsAddress.c_str() );
                ui->lineWebsocket->setText( profile->websocketAddress.c_str() );
                ui->lineSecretKey->setText( profile->secretKey.c_str() );
                ui->lineSteamKey->setText( profile->steamKey.c_str() );

                int profilesCount = ui->listProfiles->count();
                QListWidgetItem * item = ui->listProfiles->item( profilesCount - 1 );
                ui->listProfiles->setCurrentItem( item );

                updateInterfaceItems( profile );
            }
        }
    }
    // no profiles in GUI - program started - fill interface by last profile
    else{
        if( profiles.empty() ){
            g_logger( "no profiles found, you must create first one" );
            return;
        }

        const uint32_t lastProfileIndex = profiles.size() - 1;
        for( uint32_t i = 0; i < profiles.size(); i++ ){

            Profile_t * profile = profiles[ i ];

            ui->listProfiles->addItem( QString::fromStdString(profile->profileName) );

            if( lastProfileIndex == i ){

                ui->lineHttps->setText( profile->httpsAddress.c_str() );
                ui->lineWebsocket->setText( profile->websocketAddress.c_str() );
                ui->lineSecretKey->setText( profile->secretKey.c_str() );
                ui->lineSteamKey->setText( profile->steamKey.c_str() );

                int profilesCount = ui->listProfiles->count();
                QListWidgetItem * item = ui->listProfiles->item( profilesCount - 1 );
                ui->listProfiles->setCurrentItem( item );

                updateInterfaceItems( profile );
            }
        }
    }
}

void MainWindow::updateInterfaceItems( Profile_t * _profile ){

    // TODO memory leak
    if( ui->tableWidget->rowCount() != 0 ){
        ui->tableWidget->clearContents();
    }

    vector<Item_t *> currentProfileItems = _profile->items;
    ui->tableWidget->setRowCount( currentProfileItems.size() );

    for( uint32_t itemIndex = 0; itemIndex < currentProfileItems.size(); itemIndex++ ){

        Item_t * item = currentProfileItems[ itemIndex ];

        // check
        QTableWidgetItem * twi = new QTableWidgetItem();
        if( item->active ){
            twi->setCheckState( Qt::Checked );
        }
        else{
            twi->setCheckState( Qt::Unchecked );
        }
        ui->tableWidget->setItem( itemIndex, 0, twi );

        // pkey
        QTableWidgetItem * twi1 = new QTableWidgetItem( to_string( item->pkey ).c_str() );
        ui->tableWidget->setItem( itemIndex, 1, twi1 );
        twi1->setFlags( Qt::ItemIsEnabled );
        twi1->setBackgroundColor( Qt::lightGray );

        // name
        QTableWidgetItem * twi2 = new QTableWidgetItem( item->url.c_str() );
        ui->tableWidget->setItem( itemIndex, 2, twi2 );

        // price_for_buy
        QTableWidgetItem * twi3 = new QTableWidgetItem( to_string( item->maxPriceForBuy ).c_str() );
        ui->tableWidget->setItem( itemIndex, 3, twi3 );

        // buys_number
        QTableWidgetItem * twi4 = new QTableWidgetItem( to_string( item->purchasesNumber ).c_str() );
        ui->tableWidget->setItem( itemIndex, 4, twi4 );

        // price_stop
        QTableWidgetItem * twi5 = new QTableWidgetItem( to_string( item->stopPrice ).c_str() );
        ui->tableWidget->setItem( itemIndex, 5, twi5 );

        // class id
        QTableWidgetItem * twi6 = new QTableWidgetItem( to_string( item->classId ).c_str() );
        ui->tableWidget->setItem( itemIndex, 6, twi6 );

        // instance id
        QTableWidgetItem * twi7 = new QTableWidgetItem( to_string( item->instanceId ).c_str() );
        ui->tableWidget->setItem( itemIndex, 7, twi7 );

        // combo
        QComboBox * cb = new QComboBox();
        cb->addItem("buy");
        cb->addItem("buy_websocket");
        cb->addItem("order");
        if( ItemProcessType_en::BUY_HTTPS == item->processType ){
            cb->setCurrentIndex( 0 );
        }
        else if( ItemProcessType_en::BUY_WEBSOCKET == item->processType ){
            cb->setCurrentIndex( 1 );
        }
        else if( ItemProcessType_en::RACE == item->processType ){
            cb->setCurrentIndex( 2 );
        }
        else{
            g_logger( "ERROR: update interface - unknown item process type: " + to_string( (int)item->processType ) );
        }
        ui->tableWidget->setCellWidget( itemIndex, 8, cb );
    }
}

void MainWindow::snapshotCurrentProfile(){

    // current profile
    QListWidgetItem * currentListItem = ui->listProfiles->currentItem();
    if( ! currentListItem ){
        g_logger( "ERROR: no profiles, value save fail" );
        return;
    }
    const string currentProfileName = currentListItem->text().toStdString();

    const uint32_t profileID = m_torgash->getProfileIDByName( currentProfileName );

    // get GUI values
    for( Profile_t * profile : m_torgash->getProfiles() ){

        if( profile->profileID == profileID ){

            // profile options
            profile->httpsAddress = ui->lineHttps->text().toStdString();
            profile->websocketAddress = ui->lineWebsocket->text().toStdString();
            profile->secretKey = ui->lineSecretKey->text().toStdString();
            profile->steamKey = ui->lineSteamKey->text().toStdString();

            // items
            snapshotCurrentProfileItems( profile );
        }
    }

    // commit
    m_torgash->commitProfile( profileID );
}

void MainWindow::snapshotCurrentProfileItems( Profile_t * _profile ){

    const int rowCount = ui->tableWidget->rowCount();
    const int colCount = ui->tableWidget->columnCount();

    vector<Item_t *> currentProfileItems = _profile->items;

    for( int row = 0; row < rowCount; row++ ){

        QTableWidgetItem * widgetItemObjectPkey = ui->tableWidget->item( row, 1 );
        string ObjectPkey = widgetItemObjectPkey->text().toStdString();
        int pkey = 0;
        if( ! ObjectPkey.empty() ){
            pkey = stoi( ObjectPkey );
        }

        Item_t * matchedItem = nullptr;
        for( Item_t * item : currentProfileItems ){
            if( item->pkey == pkey ){

                matchedItem = item;
            }
        }

        // строка не совпала ни с одинм объектом
        if( ! matchedItem ){
            continue;
        }

        // active
        QTableWidgetItem * widgetItemActive = ui->tableWidget->item( row, 0 );
        if( Qt::CheckState::Checked == widgetItemActive->checkState() ){
            matchedItem->active = true;
        }
        else{
            matchedItem->active = false;
        }

        // object name
        QTableWidgetItem * widgetItemObjectName = ui->tableWidget->item( row, 2 );
        string itemURL = widgetItemObjectName->text().toStdString();
        matchedItem->url = itemURL;

        // price_for_buy
        QTableWidgetItem * widgetItemPrice = ui->tableWidget->item( row, 3 );
        string price = widgetItemPrice->text().toStdString();
        if( ! price.empty()){
            matchedItem->maxPriceForBuy = stoi( price );
        }
        else{
            matchedItem->maxPriceForBuy = 0;
        }

        // buys_number
        QTableWidgetItem * widgetItemBuysNumber = ui->tableWidget->item( row, 4 );
        string buysNumber = widgetItemBuysNumber->text().toStdString();
        if( ! buysNumber.empty()){
            matchedItem->purchasesNumber = stoi( buysNumber );
        }
        else{
            matchedItem->purchasesNumber = 0;
        }

        // price_stop
        QTableWidgetItem * widgetItemPriceStop = ui->tableWidget->item( row, 5 );
        string priceStop = widgetItemPriceStop->text().toStdString();
        if( ! priceStop.empty()){
            matchedItem->stopPrice = stoi( priceStop );
        }
        else{
            matchedItem->stopPrice = 0;
        }

        // class id
        QTableWidgetItem * widgetItemClassID = ui->tableWidget->item( row, 6 );
        string class_id = widgetItemClassID->text().toStdString();
        if( ! class_id.empty()){
            matchedItem->classId = stoi( class_id );
        }

        // instance id
        QTableWidgetItem * widgetItemInstanceID = ui->tableWidget->item( row, 7 );
        string instance_id = widgetItemInstanceID->text().toStdString();
        if( ! instance_id.empty()){
            matchedItem->instanceId = stoi( instance_id );
        }

        // trade type
        QComboBox * tradeType = (QComboBox *)ui->tableWidget->cellWidget( row, 8 );
        if( "buy" == tradeType->currentText() ){
            matchedItem->processType = ItemProcessType_en::BUY_HTTPS;
        }
        else if( "buy_websocket" == tradeType->currentText() ){
            matchedItem->processType = ItemProcessType_en::BUY_WEBSOCKET;
        }
        else if( "order" == tradeType->currentText() ){
            matchedItem->processType = ItemProcessType_en::RACE;
        }
        else{
            g_logger( "ERROR: unknown item trade type in cell widget: " + tradeType->currentText().toStdString() );
        }
    }
}


// ------------------------------------------------------------------------------------
// KEY EVENTS
// ------------------------------------------------------------------------------------

void MainWindow::on_buttonCreateProfile_clicked(){

    m_torgash->createProfile();

    updateInterface();
}

void MainWindow::on_buttonRemoveProfile_clicked(){

    // get current profile ID
    QListWidgetItem * currentListItem = ui->listProfiles->currentItem();
    const string currentProfileName = currentListItem->text().toStdString();

    const uint32_t profileID = m_torgash->getProfileIDByName( currentProfileName );

    // remove
    m_torgash->removeProfile( profileID );

    // GUI
    updateInterface();
}

void MainWindow::on_buttonAddObject_clicked(){

    //
    QListWidgetItem * currentListItem = ui->listProfiles->currentItem();
    const string currentProfileName = currentListItem->text().toStdString();

    const uint32_t profileID = m_torgash->getProfileIDByName( currentProfileName );

    //
    m_torgash->createItem( profileID );

    //
    updateInterface();
}

void MainWindow::on_buttonRemoveObject_clicked(){

    // profile ID
    QListWidgetItem * currentListItem = ui->listProfiles->currentItem();
    const string currentProfileName = currentListItem->text().toStdString();

    const uint32_t profileID = m_torgash->getProfileIDByName( currentProfileName );

    // pkey of current item
    int currentItemPkey = 0;
    QList< QTableWidgetItem * > selectedItems = ui->tableWidget->selectedItems();
    for( QTableWidgetItem * twi : selectedItems ){

        int itemAtRow = ui->tableWidget->row( twi );

        QTableWidgetItem * pkeyTableWidgetItem = ui->tableWidget->item( itemAtRow, 1 );
        const string itemPkeyString = pkeyTableWidgetItem->text().toStdString();
        if( ! itemPkeyString.empty() ){
            currentItemPkey = stoi( itemPkeyString );
        }
    }

    // remove
    m_torgash->removeItem( profileID, currentItemPkey );

    // update GUI
    updateInterface();
}

void MainWindow::on_buttonRunTrades_clicked(){

    snapshotCurrentProfile();

    updateInterface();

    // run current profile
    QListWidgetItem * currentListItem = ui->listProfiles->currentItem();

    if( currentListItem ){

        const string currentProfileName = currentListItem->text().toStdString();

        const uint32_t profileID = m_torgash->getProfileIDByName( currentProfileName );

        m_torgash->runTrades( profileID );
    }
    else{
        g_logger( "ERROR: run trades fail - no one profiles" );
        return;
    }

    ui->listProfiles->setEnabled( false );
    ui->buttonCreateProfile->setEnabled( false );
    ui->buttonRemoveProfile->setEnabled( false );
    ui->buttonAddObject->setEnabled( false );
    ui->buttonRemoveObject->setEnabled( false );
    ui->buttonSelectAll->setEnabled( false );
    ui->buttonDeselectAll->setEnabled( false );
    ui->lineHttps->setEnabled( false );
    ui->lineWebsocket->setEnabled( false );
    ui->lineSecretKey->setEnabled( false );
    ui->lineSteamKey->setEnabled( false );
    ui->tableWidget->setEnabled( false );
    ui->pushButton->setEnabled( false );
    ui->buttonRunTrades->setEnabled( false );
    ui->buttonStopTrades->setEnabled( true );
}

void MainWindow::on_buttonStopTrades_clicked(){

    m_torgash->stopTrades();

    ui->listProfiles->setEnabled( true );
    ui->buttonCreateProfile->setEnabled( true );
    ui->buttonRemoveProfile->setEnabled( true );
    ui->buttonAddObject->setEnabled( true );
    ui->buttonRemoveObject->setEnabled( true );
    ui->buttonSelectAll->setEnabled( true );
    ui->buttonDeselectAll->setEnabled( true );
    ui->lineHttps->setEnabled( true );
    ui->lineWebsocket->setEnabled( true );
    ui->lineSecretKey->setEnabled( true );
    ui->lineSteamKey->setEnabled( true );
    ui->tableWidget->setEnabled( true );
    ui->pushButton->setEnabled( true );
    ui->buttonRunTrades->setEnabled( true );
    ui->buttonStopTrades->setEnabled( false );
}

void MainWindow::on_tableWidget_clicked( const QModelIndex & index ){
    // dummy
}

void MainWindow::on_buttonDeselectAll_clicked(){

    const int rowCount = ui->tableWidget->rowCount();
    for( int row = 0; row < rowCount; row++ ){

        QTableWidgetItem * widgetItemActive = ui->tableWidget->item( row, 0 );
        widgetItemActive->setCheckState( Qt::Unchecked );
    }
}

void MainWindow::on_buttonSelectAll_clicked(){

    const int rowCount = ui->tableWidget->rowCount();
    for( int row = 0; row < rowCount; row++ ){

        QTableWidgetItem * widgetItemActive = ui->tableWidget->item( row, 0 );
        widgetItemActive->setCheckState( Qt::Checked );
    }
}

void MainWindow::on_lineHttps_returnPressed(){


}

void MainWindow::on_lineWebsocket_returnPressed(){


}

void MainWindow::on_lineSecretKey_returnPressed(){


}

void MainWindow::on_lineSteamKey_returnPressed(){


}

void MainWindow::on_listProfiles_itemSelectionChanged(){
    // dummy
}

void MainWindow::on_listProfiles_clicked( const QModelIndex & index ){

    updateInterface();
}

void MainWindow::on_tableWidget_entered(const QModelIndex &index){
    // dummy
}


void MainWindow::on_tableWidget_cellEntered(int row, int column){
    // dummy
}

void MainWindow::on_tableWidget_cellChanged(int row, int column){
    // dummy recursion !
}

void MainWindow::on_tableWidget_cellPressed(int row, int column){

}

void MainWindow::on_tableWidget_cellClicked(int row, int column){
    // dummy
}

void MainWindow::on_pushButton_clicked(){

    snapshotCurrentProfile();

    updateInterface();

    g_logger( "profile settings saved OK" );
}

void MainWindow::on_buttonClearLog_clicked(){

    ui->plainTextLog->clear();
}
