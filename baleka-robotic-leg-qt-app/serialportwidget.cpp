/*
 * Copyright (C) 2012 Jorge Aparicio <jorge.aparicio.r@gmail.com>
 *
 * This file is part of qSerialTerm.
 *
 * qSerialTerm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * qSerialTerm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with qSerialTerm.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "serialportwidget.h"
#include "ui_serialportwidget.h"

#include <QMessageBox>

#include <QTextStream>
#include <QString>
#include <QFile>

#include <iostream>
#include <fstream>
using std::ofstream;
using std::pow;

//Kinematics: SI Units and Degrees
float *ForwardKinematics(float phi1, float phi2);
float *InverseKinematics(float r, float theta);
#define PI 3.141592653f

#define PAYLOAD_RX 21
#define PAYLOAD_MISC 32

#define REFRESH_RATE_MS 5

Q_DECLARE_METATYPE(QSerialPort::DataBits)
Q_DECLARE_METATYPE(QSerialPort::StopBits)
Q_DECLARE_METATYPE(QSerialPort::Parity)
Q_DECLARE_METATYPE(QSerialPort::FlowControl)

SerialPortWidget::SerialPortWidget(QWidget *parent) :
  QWidget(parent),
  ui(new Ui::SerialPortWidget),
  serialPort(0),
  refreshRateTimer(0),
  isThereCommunication(false)
{
  ui->setupUi(this);

  ui->dataBitsComboBox->addItem(QLatin1String("5"),
                                QSerialPort::Data5);
  ui->dataBitsComboBox->addItem(QLatin1String("6"),
                                QSerialPort::Data6);
  ui->dataBitsComboBox->addItem(QLatin1String("7"),
                                QSerialPort::Data7);
  ui->dataBitsComboBox->addItem(QLatin1String("8"),
                                QSerialPort::Data8);
  ui->dataBitsComboBox->setCurrentIndex(-1);


  ui->stopBitsComboBox->addItem(QLatin1String("1"),
                                QSerialPort::OneStop);
  ui->stopBitsComboBox->addItem(QLatin1String("1.5"),
                                QSerialPort::OneAndHalfStop);
  ui->stopBitsComboBox->addItem(QLatin1String("2"),
                                QSerialPort::TwoStop);
  ui->stopBitsComboBox->setCurrentIndex(-1);


  ui->parityComboBox->addItem(QLatin1String("No"),
                              QSerialPort::NoParity);
  ui->parityComboBox->addItem(QLatin1String("Even"),
                              QSerialPort::EvenParity);
  ui->parityComboBox->addItem(QLatin1String("Odd"),
                              QSerialPort::OddParity);
  ui->parityComboBox->addItem(QLatin1String("Space"),
                              QSerialPort::SpaceParity);
  ui->parityComboBox->addItem(QLatin1String("Mark"),
                              QSerialPort::MarkParity);
  ui->parityComboBox->setCurrentIndex(-1);


  ui->flowControlComboBox->addItem(QLatin1String("No"),
                                   QSerialPort::NoFlowControl);
  ui->flowControlComboBox->addItem(QLatin1String("Hardware"),
                                   QSerialPort::HardwareControl);
  ui->flowControlComboBox->addItem(QLatin1String("Software"),
                                   QSerialPort::SoftwareControl);
  ui->flowControlComboBox->setCurrentIndex(-1);

  /////////////////////////////////////////////////////////////////
  //Baleka Code

  initCRC(0);

  /////////////////////////////////////////////////////////////////

}

SerialPortWidget::~SerialPortWidget()
{
  delete ui;
}

void SerialPortWidget::write(QByteArray data)
{
  if (refreshRateTimer) // There is ongoing communication
    serialPort->write(data);
}

void SerialPortWidget::on_getPortsPushButton_clicked()
{
  serialPortInfoList = QSerialPortInfo::availablePorts();

  ui->portComboBox->clear();

  foreach(QSerialPortInfo entry, serialPortInfoList)
    ui->portComboBox->addItem(entry.portName());
}

void SerialPortWidget::on_openPortPushButton_clicked()
{
  if (serialPort) {
    if (refreshRateTimer) // Communication ongoing
      on_startCommunicationPushButton_clicked();

    serialPort->close();

    delete serialPort;
    serialPort = 0;

    ui->portStatusLabel->setText(QLatin1String("<font color=red>"
                                               "Closed"
                                               "</font>"));
    ui->openPortPushButton->setText(QLatin1String("Open"));

    ui->portComboBox->setEnabled(true);
    ui->getPortsPushButton->setEnabled(true);

    disableCommunicationSettings();

    ui->baudRateComboBox->setCurrentIndex(-1);
    ui->dataBitsComboBox->setCurrentIndex(-1);
    ui->stopBitsComboBox->setCurrentIndex(-1);
    ui->parityComboBox->setCurrentIndex(-1);
    ui->flowControlComboBox->setCurrentIndex(-1);

  } else {
    serialPort = new QSerialPort(ui->portComboBox->currentText());

    if (serialPort->open(QIODevice::ReadWrite)) {
      enableCommunicationSettings();

      ui->portStatusLabel->setText(QLatin1String("<font color=green>"
                                                 "Open"
                                                 "</font>"));

      ui->openPortPushButton->setText(QLatin1String("Close"));

      ui->portComboBox->setDisabled(true);
      ui->getPortsPushButton->setDisabled(true);
    } else {
      QMessageBox::warning(this,
                           QLatin1String("Port error"),
                           QLatin1String("Couldn't open the requested port"));

      delete serialPort;
      serialPort = 0;
    }
  }
}

void SerialPortWidget::on_startCommunicationPushButton_clicked()
{
  if (refreshRateTimer) { // There is ongoing communication
    enableCommunicationSettings();
    ui->startCommunicationPushButton->setText(QLatin1String("Start"));
    ui->communicationStatusLabel->setText(QLatin1String("<font color=red>"
                                                        "No communication"
                                                        "</font>"));

    delete refreshRateTimer;
    refreshRateTimer = 0;

    emit communicationStart(false);
  } else {
    disableCommunicationSettings();

    ui->startCommunicationPushButton->setText(QLatin1String("Stop"));
    ui->communicationStatusLabel->setText(QLatin1String("<font color=green>"
                                                        "Ongoing"
                                                        "</font>"));

    refreshRateTimer = new QTimer(this);

    connect(refreshRateTimer, SIGNAL(timeout()),
            this,             SLOT(on_refreshRateTimer_timeout()));

    refreshRateTimer->start(REFRESH_RATE_MS);

    emit communicationStart(true);
  }
}

void SerialPortWidget::on_portComboBox_currentIndexChanged(int index)
{
  if (index == -1) {
    ui->openPortPushButton->setDisabled(true);

    ui->baudRateComboBox->clear();

    ui->pidLabel->setText(QLatin1String("-"));
    ui->vidLabel->setText(QLatin1String("-"));
    ui->portStatusLabel->setText(QLatin1String("<font color=red>"
                                               "No port selected"
                                               "</font>"));
  } else {
    ui->openPortPushButton->setEnabled(true);

    QList<qint32> baudRateList = serialPortInfoList[index].standardBaudRates();

    ui->baudRateComboBox->clear();

    foreach (qint32 baudRate, baudRateList)
      ui->baudRateComboBox->addItem(QString::number(baudRate), baudRate);

    ui->baudRateComboBox->setCurrentIndex(-1);

    QString vid = QString(serialPortInfoList[index].vendorIdentifier());

    if (vid.isEmpty())
      ui->vidLabel->setText(QLatin1String("-"));
    else
      ui->vidLabel->setText(QLatin1String("0x") + vid);

    QString pid = QString(serialPortInfoList[index].productIdentifier());

    if (pid.isEmpty())
      ui->pidLabel->setText(QLatin1String("-"));
    else
      ui->pidLabel->setText(QLatin1String("0x") + pid);

    ui->portStatusLabel->setText(QLatin1String("<font color=red>"
                                               "Closed"
                                               "</font>"));
  }
}

void SerialPortWidget::on_baudRateComboBox_currentIndexChanged(int index)
{
  if (index != -1 && serialPort != 0) {
    if(serialPort->setBaudRate(ui->baudRateComboBox->itemData(index).value<int>())) {
      validateCommunicationSettings();

      return;
    } else {
      QMessageBox::warning(this,
                           QLatin1String("Configuration error"),
                           QLatin1String("Couldn't change the baud rate."));
      ui->baudRateComboBox->setCurrentIndex(-1);
    }
  }

  ui->startCommunicationPushButton->setDisabled(true);
}

void SerialPortWidget::on_dataBitsComboBox_currentIndexChanged(int index)
{
  if (index != -1 && serialPort != 0) {
    if(serialPort->setDataBits(ui->dataBitsComboBox->itemData(index).value<QSerialPort::DataBits>())) {
      validateCommunicationSettings();

      return;
    } else {
      QMessageBox::warning(this,
                           QLatin1String("Configuration error"),
                           QLatin1String("Couldn't change the numbers of data bits."));
      ui->dataBitsComboBox->setCurrentIndex(-1);
    }
  }

  ui->startCommunicationPushButton->setDisabled(true);
}

void SerialPortWidget::on_stopBitsComboBox_currentIndexChanged(int index)
{
  if (index != -1 && serialPort != 0) {
    if(serialPort->setStopBits(ui->stopBitsComboBox->itemData(index).value<QSerialPort::StopBits>())) {
      validateCommunicationSettings();

      return;
    } else {
      QMessageBox::warning(this,
                           QLatin1String("Configuration error"),
                           QLatin1String("Couldn't change the numbers of stop bits."));
      ui->stopBitsComboBox->setCurrentIndex(-1);
    }
  }

  ui->startCommunicationPushButton->setDisabled(true);
}

void SerialPortWidget::on_parityComboBox_currentIndexChanged(int index)
{
  if (index != -1 && serialPort != 0) {
    if(serialPort->setParity(ui->parityComboBox->itemData(index).value<QSerialPort::Parity>())) {
      validateCommunicationSettings();

      return;
    } else {
      QMessageBox::warning(this,
                           QLatin1String("Configuration error"),
                           QLatin1String("Couldn't change the parity."));
      ui->parityComboBox->setCurrentIndex(-1);
    }
  }

  ui->startCommunicationPushButton->setDisabled(true);
}

void SerialPortWidget::on_flowControlComboBox_currentIndexChanged(int index)
{
  if (index != -1 && serialPort != 0) {
    if(serialPort->setFlowControl(ui->flowControlComboBox->itemData(index).value<QSerialPort::FlowControl>())) {
      validateCommunicationSettings();

      return;
    } else {
      QMessageBox::warning(this,
                           QLatin1String("Configuration error"),
                           QLatin1String("Couldn't change the flow control."));
      ui->flowControlComboBox->setCurrentIndex(-1);
    }
  }

  ui->startCommunicationPushButton->setDisabled(true);
}

/////////////////////////////////////////////////////////////////
//Baleka Code

struct __attribute__((__packed__)) TXPacketStruct {
                uint8_t START[2];

                uint8_t M1C[2];
                uint8_t M1P[4];
                uint8_t M1V[4];

                uint8_t M2C[2];
                uint8_t M2P[4];
                uint8_t M2V[4];

//                uint8_t ACCX[2];
//                uint8_t ACCY[2];
//                uint8_t ACCZ[2];
//                uint8_t GYRX[2];
//                uint8_t GYRY[2];
//                uint8_t GYRZ[2];
//                uint8_t TEMP;
                uint8_t MISC[PAYLOAD_MISC];

                uint8_t StatBIT_1 : 1;
                uint8_t StatBIT_2 : 1;
                uint8_t StatBIT_3 : 1;
                uint8_t StatBIT_4 : 1;
                uint8_t StatBIT_5 : 1;
                uint8_t StatBIT_6 : 1;
                uint8_t StatBIT_7 : 1;
                uint8_t StatBIT_8 : 1;

                uint8_t CRCCheck[2];

                uint8_t STOP[2];
        };

        struct TXPacketStruct PCPacket;
        //Transmit pointer PCPacketPTR with sizeof(PCPacket)
        uint8_t *PCPacketPTR = (uint8_t*)&PCPacket;

        union {
                uint32_t WORD;
                uint16_t HALFWORD;
                uint8_t BYTE[4];
        } WORDtoBYTE;

        union {
                float FLOAT;
                int32_t FLOAT32;
                int16_t FLOAT16;
                uint8_t BYTE[4];
        } FLOATtoBYTE;

QByteArray frameReadData;
QByteArray RXBuffer;
QByteArray PlotBuffer;
char *RXBufferArray;
uint8_t RXBufferSize;
int8_t INDEX;
uint32_t CALC_CRC_2;

uint8_t run = 0;
ofstream *CSVlogPTR;

QStringList CSVList;
QString CSVLog;

QString M1C;
QString M1P;
QString M1V;

QString M2C;
QString M2P;
QString M2V;

QString I_cmd_0;
QString I_cmd_1;
QString f_r;
QString f_s;
QString r_fbk;
QString s_fbk;
QString r_d_fbk;
QString s_d_fbk;

float *RET;
QString r;
QString theta;

uint32_t test = 100;


void SerialPortWidget::on_refreshRateTimer_timeout()
{
    PCPacket.START[0] = 0x7E;
    PCPacket.START[1] = 0x5B;

//    PCPacket.STOP[0] = 0x5D;
//    PCPacket.STOP[1] = 0x7E;

  RXBuffer = serialPort->readAll();

  RXBufferArray = RXBuffer.data();

  if(findBytes((uint8_t*)RXBufferArray, RXBuffer.size(), PCPacket.START, 2, 1)>=0){
      INDEX = findBytes((uint8_t*)RXBufferArray, RXBuffer.size(), PCPacket.START, 2, 1);

      memcpy(PCPacketPTR, &RXBufferArray[INDEX], sizeof(PCPacket));

      WORDtoBYTE.BYTE[1] = PCPacket.CRCCheck[0];
      WORDtoBYTE.BYTE[0] = PCPacket.CRCCheck[1];

      CALC_CRC_2 = crcCalc(PCPacketPTR, 2, PAYLOAD_RX + PAYLOAD_MISC, 0); //Check entire data CRC

//      if(WORDtoBYTE.HALFWORD==CALC_CRC_2) {
//        run = 1;
//      }
  }

//  count1 = 4*250*(phi1/180 - 1);
//  phi1 = (count1/(4*250) + 1)*180;

//  count2 = 4*250*(1 - phi2/180);
//  phi2 = (count2/(4*250) - 1)*(-180);

  //Temporary without CRC check
  if(PCPacket.STOP[0] == 0x5D){

      //Motor log data

      memcpy(FLOATtoBYTE.BYTE, PCPacket.M1C, 2);
      FLOATtoBYTE.FLOAT = FLOATtoBYTE.FLOAT16/(pow(2.0,13)/60.0);
      M1C = QString::number(FLOATtoBYTE.FLOAT);
      PlotBuffer.append((const char *)&FLOATtoBYTE.FLOAT, 4);

      memcpy(FLOATtoBYTE.BYTE, PCPacket.M1P, 4);
      FLOATtoBYTE.FLOAT = (FLOATtoBYTE.FLOAT32/(4*250.0) - 1)*(-180.0);
      M1P = QString::number(FLOATtoBYTE.FLOAT);
      PlotBuffer.append((const char *)&FLOATtoBYTE.FLOAT, 4);

      memcpy(FLOATtoBYTE.BYTE, PCPacket.M1V, 4);
      FLOATtoBYTE.FLOAT = (FLOATtoBYTE.FLOAT32/(pow(2.0,17)/20000.0))*(1/2000.0)*60.0;
      M1V = QString::number(FLOATtoBYTE.FLOAT);
      PlotBuffer.append((const char *)&FLOATtoBYTE.FLOAT, 4);

      memcpy(FLOATtoBYTE.BYTE, PCPacket.M2C, 2);
      FLOATtoBYTE.FLOAT = FLOATtoBYTE.FLOAT16/(pow(2.0,13)/60.0);
      M2C = QString::number(FLOATtoBYTE.FLOAT);
      PlotBuffer.append((const char *)&FLOATtoBYTE.FLOAT, 4);

      memcpy(FLOATtoBYTE.BYTE, PCPacket.M2P, 4);
      FLOATtoBYTE.FLOAT = (FLOATtoBYTE.FLOAT32/(4*250.0) + 1)*180.0;
      M2P = QString::number(FLOATtoBYTE.FLOAT);
      PlotBuffer.append((const char *)&FLOATtoBYTE.FLOAT, 4);

      memcpy(FLOATtoBYTE.BYTE, PCPacket.M2V, 4);
      FLOATtoBYTE.FLOAT = (FLOATtoBYTE.FLOAT32/(pow(2.0,17)/20000.0))*(1/2000.0)*60.0;
      M2V = QString::number(FLOATtoBYTE.FLOAT);
      PlotBuffer.append((const char *)&FLOATtoBYTE.FLOAT, 4);

      //Control log data
      memcpy(FLOATtoBYTE.BYTE, PCPacket.MISC, 4);
      //FLOATtoBYTE.FLOAT = FLOATtoBYTE.FLOAT32;
      I_cmd_0 = QString::number(FLOATtoBYTE.FLOAT);
      PlotBuffer.append((const char *)&FLOATtoBYTE.FLOAT, 4);

      memcpy(FLOATtoBYTE.BYTE, &PCPacket.MISC[4], 4);
      //FLOATtoBYTE.FLOAT = FLOATtoBYTE.FLOAT32;
      I_cmd_1 = QString::number(FLOATtoBYTE.FLOAT);
      PlotBuffer.append((const char *)&FLOATtoBYTE.FLOAT, 4);

      memcpy(FLOATtoBYTE.BYTE, &PCPacket.MISC[8], 4);
      //FLOATtoBYTE.FLOAT = FLOATtoBYTE.FLOAT32;
      f_r = QString::number(FLOATtoBYTE.FLOAT);
      PlotBuffer.append((const char *)&FLOATtoBYTE.FLOAT, 4);

      memcpy(FLOATtoBYTE.BYTE, &PCPacket.MISC[12], 4);
      //FLOATtoBYTE.FLOAT = FLOATtoBYTE.FLOAT32;
      f_s = QString::number(FLOATtoBYTE.FLOAT);
      PlotBuffer.append((const char *)&FLOATtoBYTE.FLOAT, 4);

      memcpy(FLOATtoBYTE.BYTE, &PCPacket.MISC[16], 4);
      //FLOATtoBYTE.FLOAT = FLOATtoBYTE.FLOAT32;
      r_fbk = QString::number(FLOATtoBYTE.FLOAT);
      PlotBuffer.append((const char *)&FLOATtoBYTE.FLOAT, 4);

      memcpy(FLOATtoBYTE.BYTE, &PCPacket.MISC[20], 4);
      //FLOATtoBYTE.FLOAT = FLOATtoBYTE.FLOAT32;
      s_fbk = QString::number(FLOATtoBYTE.FLOAT);
      PlotBuffer.append((const char *)&FLOATtoBYTE.FLOAT, 4);

      memcpy(FLOATtoBYTE.BYTE, &PCPacket.MISC[24], 4);
      //FLOATtoBYTE.FLOAT = FLOATtoBYTE.FLOAT32;
      r_d_fbk = QString::number(FLOATtoBYTE.FLOAT);
      PlotBuffer.append((const char *)&FLOATtoBYTE.FLOAT, 4);

      memcpy(FLOATtoBYTE.BYTE, &PCPacket.MISC[28], 4);
      //FLOATtoBYTE.FLOAT = FLOATtoBYTE.FLOAT32;
      s_d_fbk = QString::number(FLOATtoBYTE.FLOAT);
      PlotBuffer.append((const char *)&FLOATtoBYTE.FLOAT, 4);

//      RET = ForwardKinematics(M1P.toFloat(), M2P.toFloat());
//      r = QString::number(RET[0]);
//      PlotBuffer.append((const char *)&RET[0], 4);
//      theta = QString::number(RET[1]);
//      PlotBuffer.append((const char *)&RET[1], 4);

      CSVList.clear();

        if(run==0){
            CSVList << "M1C" << "M1P" << "M1V" << "M2C" << "M2P" << "M2V";
            run=1;
        }
        else{
            CSVList << M1C << M1P << M1V << M2C << M2P << M2V << I_cmd_0 << I_cmd_1 << f_r << f_s << r_fbk << s_fbk << r_d_fbk << s_d_fbk;
        }

      CSVLog = CSVList.join(',');
      CSVLog.append("\n");

      emit read(CSVLog.toStdString().c_str());

      emit readPlot(PlotBuffer);
      PlotBuffer.clear();
  }

}

float *ForwardKinematics(float phi1, float phi2){
  //function [r,theta] = fcn(phi1,phi2)

  uint8_t valid = 1;
  static float ret[2];

  phi1 = (phi1*2*PI)/360.0;
  phi2 = (phi2*2*PI)/360.0;

  static float l1 = 0.15; //length of upper linkage in m (measured from center of joint of 5 cm diameter)
  static float l2 = 0.3; //length of lower linkage in m (measured from center of joint of 5 cm diameter)

  ret[0] = fabs(-l1*cos((phi1 + phi2)/2.0) + sqrt(pow(l2,2) - pow(l1,2)*pow(sin((phi1 + phi2)/2.0),2))); //r
  ret[1] = (phi1 - phi2)/2; //theta

  ret[1] = (ret[1]*360)/(2.0*PI);

  if(valid){
    return ret;
  }
  else{
    return NULL;
  }
}

float *InverseKinematics(float r, float theta){
  //function [phi1,phi2] = fcn(r,theta)

  uint8_t valid = 1;
  static float ret[2];

  theta = (theta*2*PI)/360.0;

  static float l1 = 0.15; //length of upper linkage in m (measured from center of joint of 5 cm diameter)
  static float l2 = 0.3; //length of lower linkage in m (measured from center of joint of 5 cm diameter)

  if (r == 0){r = 0.000001;}

  //float complex cmp1;
  float cmp1 = (pow(r,2) + pow(l1,2) - pow(l2,2))/(2.0*r*l1);

  //float complex cmp2;
  float cmp2 = (pow(r,2) + pow(l1,2) - pow(l2,2))/(2.0*r*l1);

  ret[0] = fabs(PI - acos(cmp1) + theta); //phi1
  ret[1] = fabs(PI - acos(cmp2) - theta); //phi2

  ret[0] = (ret[0]*360)/(2.0*PI);
  ret[1] = (ret[1]*360)/(2.0*PI);

  if(valid){
    return ret;
  }
  else{
    return NULL;
  }
}

/////////////////////////////////////////////////////////////////

void SerialPortWidget::enableCommunicationSettings()
{
  ui->baudRateComboBox->setEnabled(true);
  ui->dataBitsComboBox->setEnabled(true);
  ui->stopBitsComboBox->setEnabled(true);
  ui->parityComboBox->setEnabled(true);
  ui->flowControlComboBox->setEnabled(true);
}

void SerialPortWidget::disableCommunicationSettings()
{
  ui->baudRateComboBox->setDisabled(true);
  ui->dataBitsComboBox->setDisabled(true);
  ui->stopBitsComboBox->setDisabled(true);
  ui->parityComboBox->setDisabled(true);
  ui->flowControlComboBox->setDisabled(true);
}

void SerialPortWidget::validateCommunicationSettings(void)
{
  if (serialPort->isOpen() &&
      (ui->baudRateComboBox->currentIndex() != -1) &&
      (ui->dataBitsComboBox->currentIndex() != -1) &&
      (ui->stopBitsComboBox->currentIndex() != -1) &&
      (ui->parityComboBox->currentIndex() != -1) &&
      (ui->flowControlComboBox->currentIndex() != -1)) {
    ui->startCommunicationPushButton->setEnabled(true);
  } else {
    ui->startCommunicationPushButton->setDisabled(true);
  }
}

void SerialPortWidget::on_pushButton_clicked()
{
    //Set defaults
    ui->baudRateComboBox->setCurrentIndex(19);
    ui->baudRateComboBox->update();
    ui->dataBitsComboBox->setCurrentIndex(3);
    ui->dataBitsComboBox->update();
    ui->stopBitsComboBox->setCurrentIndex(0);
    ui->stopBitsComboBox->update();
    ui->parityComboBox->setCurrentIndex(0);
    ui->parityComboBox->update();
    ui->flowControlComboBox->setCurrentIndex(0);
    ui->flowControlComboBox->update();
}
