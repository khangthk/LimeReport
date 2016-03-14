/***************************************************************************
 *   This file is part of the Lime Report project                          *
 *   Copyright (C) 2015 by Alexander Arin                                  *
 *   arin_a@bk.ru                                                          *
 *                                                                         *
 **                   GNU General Public License Usage                    **
 *                                                                         *
 *   This library is free software: you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 **                  GNU Lesser General Public License                    **
 *                                                                         *
 *   This library is free software: you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License as        *
 *   published by the Free Software Foundation, either version 3 of the    *
 *   License, or (at your option) any later version.                       *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library.                                      *
 *   If not, see <http://www.gnu.org/licenses/>.                           *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 ****************************************************************************/
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>
#include "lrreportengine.h"
#include "lrcallbackdatasourceintf.h"
#include <QDebug>
#include <QStringListModel>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow), m_progressDialog(0), m_customers(0), m_orders(0)
{
    ui->setupUi(this);
    report = new LimeReport::ReportEngine(this);

    connect(report, SIGNAL(renderStarted()), this, SLOT(renderStarted()));
    connect(report, SIGNAL(renderPageFinished(int)),
            this, SLOT(renderPageFinished(int)));
    connect(report, SIGNAL(renderFinished()), this, SLOT(renderFinished()));

    QFile dbFile(QApplication::applicationDirPath()+"/demo_reports/northwind.db");
    if (dbFile.exists()){
        m_db = QSqlDatabase::addDatabase("QSQLITE");
        m_db.setDatabaseName(dbFile.fileName());
        if (m_db.open()){
            QSqlQueryModel* customersModel = new QSqlQueryModel();
            customersModel->setQuery("select * from customers", m_db);
            report->dataManager()->addModel("external_customers_data",customersModel,true);
            QSqlQueryModel* ordersModel = new QSqlQueryModel();
            ordersModel->setQuery("Select * from orders",m_db);
            report->dataManager()->addModel("external_orders_data",ordersModel,true);
            m_customers = new QSqlQuery("Select * from customers limit 10",m_db);
            m_customers->first();
            m_orders = new QSqlQuery(m_db);
            m_orders->prepare("Select * from orders where CustomerID = :id");
            int index = m_customers->record().indexOf("CustomerID");
            m_orders->bindValue(":id",m_customers->value(index));
            m_orders->exec();
        };
    }

    LimeReport::ICallbackDatasource * callbackDatasource = report->dataManager()->createCallbackDatasouce("master");
    connect(callbackDatasource, SIGNAL(getCallbackData(LimeReport::CallbackInfo,QVariant&)),
            this, SLOT(slotGetCallbackData(LimeReport::CallbackInfo,QVariant&)));
    connect(callbackDatasource, SIGNAL(changePos(const LimeReport::CallbackInfo::ChangePosType&,bool&)),
            this, SLOT(slotChangePos(const LimeReport::CallbackInfo::ChangePosType&,bool&)));
    //report->dataManager()->addCallbackDatasource(callbackDatasource,"master");

    callbackDatasource = report->dataManager()->createCallbackDatasouce("detail");
    connect(callbackDatasource, SIGNAL(getCallbackData(LimeReport::CallbackInfo,QVariant&)),
            this, SLOT(slotGetCallbackChildData(LimeReport::CallbackInfo,QVariant&)));
    connect(callbackDatasource, SIGNAL(changePos(const LimeReport::CallbackInfo::ChangePosType&,bool&)),
            this, SLOT(slotChangeChildPos(const LimeReport::CallbackInfo::ChangePosType&,bool&)));
    //report->dataManager()->addCallbackDatasource(callbackDatasource,"detail");

    QStringList simpleData;
    simpleData << "value1" << "value2" << "value3";
    QStringListModel* stringListModel = new QStringListModel();
    stringListModel->setStringList(simpleData);

    report->dataManager()->addModel("string_list",stringListModel,true);


}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_clicked()
{
    report->dataManager()->clearUserVariables();
    if (!ui->leVariableName->text().isEmpty() && !ui->leVariableValue->text().isEmpty()){
        report->dataManager()->setReportVariable(ui->leVariableName->text(), ui->leVariableValue->text());
    }
    report->setShowProgressDialog(false);
    report->designReport();
}

void MainWindow::on_pushButton_2_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,"Select report file",QApplication::applicationDirPath()+"/demo_reports/","*.lrxml");
    if (!fileName.isEmpty()) {
        report->loadFromFile(fileName);
        if (!ui->leVariableName->text().isEmpty() && !ui->leVariableValue->text().isEmpty()){
            report->dataManager()->setReportVariable(ui->leVariableName->text(), ui->leVariableValue->text());
        }
        report->previewReport();
    }
}

void MainWindow::renderStarted()
{
    m_currentPage = 0;
    m_progressDialog = new QProgressDialog(tr("Start render"),tr("Cancel"),0,0,this);
    m_progressDialog->setWindowModality(Qt::WindowModal);
    connect(m_progressDialog, SIGNAL(canceled()), report, SLOT(cancelRender()));
    m_progressDialog->show();
    QApplication::processEvents();
}

void MainWindow::renderPageFinished(int renderedPageCount)
{
    if (m_progressDialog){
        m_progressDialog->setLabelText(QString::number(renderedPageCount)+tr(" page rendered"));
        m_progressDialog->setValue(renderedPageCount);
    }
}

void MainWindow::renderFinished()
{
    if (m_progressDialog){
        m_progressDialog->close();
        delete m_progressDialog;
    }
    m_progressDialog = 0;
}

void MainWindow::prepareData(QSqlQuery* ds, LimeReport::CallbackInfo info, QVariant &data)
{
    switch (info.dataType) {
    case LimeReport::CallbackInfo::IsEmpty:
        data = !ds->first();
        break;
    case LimeReport::CallbackInfo::HasNext:
        data = ds->next();
        ds->previous();
        break;
    case LimeReport::CallbackInfo::ColumnHeaderData:
        if (info.index < ds->record().count())
            data = ds->record().fieldName(info.index);
        break;
    case LimeReport::CallbackInfo::ColumnData:
        data = ds->value(ds->record().indexOf(info.columnName));
        break;
    }
}

void MainWindow::slotGetCallbackData(LimeReport::CallbackInfo info, QVariant &data)
{
    if (!m_customers) return;
    prepareData(m_customers, info, data);
}

void MainWindow::slotChangePos(const LimeReport::CallbackInfo::ChangePosType &type, bool &result)
{
    QSqlQuery* ds = m_customers;
    if (!ds) return;
    if (type == LimeReport::CallbackInfo::First) {result = ds->first(); m_nextCount=0;}
    else {result = ds->next(); m_nextCount++;}
    if (result){
        m_orders->bindValue(":id",m_customers->value(m_customers->record().indexOf("CustomerID")));
        m_orders->exec();
    }
}

void MainWindow::slotGetCallbackChildData(LimeReport::CallbackInfo info, QVariant &data)
{
    if (!m_orders) return ;
    prepareData(m_orders, info, data);
}

void MainWindow::slotChangeChildPos(const LimeReport::CallbackInfo::ChangePosType &type, bool &result)
{
    QSqlQuery* ds = m_orders;
    if (!ds) return;
    if (type == LimeReport::CallbackInfo::First) result = ds->first();
    else result = ds->next();
}
