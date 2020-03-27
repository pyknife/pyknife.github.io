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

#include "framewidget.h"
#include "ui_framewidget.h"

#include "hexstring.h"
#include <queue>
using std::queue;
using std::pow;

queue<int> txQueue; /* Declare a queue */

#define PAYLOAD_TX 63

#define KILL 0
#define WRITE 1
#define BRIDGE 2
#define CURRENT_SET 20
#define POSITION_SET 22
#define ZERO 8
#define GAIN_SET 9
#define GAIN_CHANGE_M1 10
#define GAIN_CHANGE_M2 13
#define CONFIG_SET 16

#define CONTROL_CURRENT_M1 40
#define CONTROL_CURRENT_M2 41

#define START_CONTROL 30
#define TRIGGER_ONESHOT 31

FrameWidget::FrameWidget(QWidget *parent) :
  QWidget(parent),
  ui(new Ui::FrameWidget),
  refreshRateTimer(0)
{
  ui->setupUi(this);

  ui->appendLineEdit->setValidator(new QIntValidator(0, 255));
  ui->prependLineEdit->setValidator(new QIntValidator(0, 255));

  ui->dataTypeComboBox->addItem(QLatin1String("uint8"), UINT8);
  ui->dataTypeComboBox->addItem(QLatin1String("uint16"), UINT16);
  ui->dataTypeComboBox->addItem(QLatin1String("uint32"), UINT32);
  ui->dataTypeComboBox->addItem(QLatin1String("int8"), INT8);
  ui->dataTypeComboBox->addItem(QLatin1String("int16"), INT16);
  ui->dataTypeComboBox->addItem(QLatin1String("int32"), INT32);

  ui->endiannessComboBox->addItem(QLatin1String("Little"), QDataStream::LittleEndian);
  ui->endiannessComboBox->addItem(QLatin1String("Big"), QDataStream::BigEndian);

  ui->formatComboBox->addItem(QLatin1String("Raw binary"), Binary);
  ui->formatComboBox->addItem(QLatin1String("Hex string"), Hexadecimal);

  ui->payloadSpinBox->setValue(4);

  /////////////////////////////////////////////////////////////////

  ui->GainSetcomboBox->addItem(QLatin1String("Gain Set 0 (No Leg)"), 0);
  ui->GainSetcomboBox->addItem(QLatin1String("Gain Set 1 (Leg)"), 1);

  ui->ConfigcomboBox->addItem(QLatin1String("Config Set 0 (Position)"), 0);
  ui->ConfigcomboBox->addItem(QLatin1String("Config Set 1 (Current)"), 1);

  ui->LegRadius->setValidator(new QIntValidator(10, 20));
  ui->LegAngle->setValidator(new QIntValidator(-1, 1));
  ui->M1Angle->setValidator(new QIntValidator(90, 162));
  ui->M2Angle->setValidator(new QIntValidator(90, 162));

  ui->PGainM1->setValidator(new QDoubleValidator(0.0, 0.5, 20, this));
  ui->IGainM1->setValidator(new QDoubleValidator(0.0, 9.766, 20, this));
  ui->DGainM1->setValidator(new QDoubleValidator(0.0, 0.008, 20, this));

  ui->PGainM2->setValidator(new QDoubleValidator(0.0, 0.5, 20, this));
  ui->IGainM2->setValidator(new QDoubleValidator(0.0, 9.766, 20, this));
  ui->DGainM2->setValidator(new QDoubleValidator(0.0, 0.008, 20, this));

  ui->Kr->setValidator(new QDoubleValidator(0.0, 1.0, 20, this));
  ui->Ktheta->setValidator(new QDoubleValidator(0.0, 1.0, 20, this));
  ui->Ki->setValidator(new QDoubleValidator(0.0, 2.0, 20, this));

  //Conservative
//  ui->PGainM1->setValidator(new QDoubleValidator(0.0, 0.001, 20, this));
//  ui->IGainM1->setValidator(new QDoubleValidator(0.0, 0.001, 20, this));
//  ui->DGainM1->setValidator(new QDoubleValidator(0.0, 0.001, 20, this));

//  ui->PGainM2->setValidator(new QDoubleValidator(0.0, 0.001, 20, this));
//  ui->IGainM2->setValidator(new QDoubleValidator(0.0, 0.001, 20, this));
//  ui->DGainM2->setValidator(new QDoubleValidator(0.0, 0.001, 20, this));

  initCRC(0);

  /////////////////////////////////////////////////////////////////
}

FrameWidget::~FrameWidget()
{
  delete ui;
}

void FrameWidget::on_dataTypeComboBox_currentIndexChanged(int index)
{
  int min = 0, max = 0;

  switch(ui->dataTypeComboBox->itemData(index).value<int>()) {
    case UINT8:
      max = 255;
      break;

    case UINT16:
      max = 65535;
      break;

    case UINT32:
      max = 2147483647;
      break;

    case INT8:
      min = -128;
      max = 127;
      break;

    case INT16:
      min = -32768;
      max = 32767;
      break;

    case INT32:
      min = -2147483648;
      max = 2147483647;
      break;
  }

  ui->minimumValueSpinBox->setMinimum(min);
  ui->maximumValueSpinBox->setMaximum(max);

  ui->minimumValueSpinBox->setValue(min);
  ui->maximumValueSpinBox->setValue(max);
}

void FrameWidget::on_payloadSpinBox_valueChanged(int value)
{
  for (int i = 0; i < sliderVector.size(); i++) {
    delete sliderVector[i];
    sliderVector[i] = 0;
  }

  sliderVector.clear();

  for (int i = 0; i < value; i++) {
    sliderVector.push_back(new Slider);

    sliderVector[i]->setMinimumValue(ui->minimumValueSpinBox->value());
    sliderVector[i]->setMaximumValue(ui->maximumValueSpinBox->value());
    sliderVector[i]->setId(QLatin1String("<b>") +
                           QString::number(i + 1) +
                           QLatin1String("</b>"));

    ui->gridLayout->addWidget(sliderVector[i]);
  }
}

void FrameWidget::on_minimumValueSpinBox_valueChanged(int min)
{
  ui->maximumValueSpinBox->setMinimum(min);

  for (int i = 0; i < sliderVector.size(); i++)
    sliderVector[i]->setMinimumValue(min);
}

void FrameWidget::on_maximumValueSpinBox_valueChanged(int max)
{
  ui->minimumValueSpinBox->setMaximum(max);

  for (int i = 0; i < sliderVector.size(); i++)
    sliderVector[i]->setMaximumValue(max);
}

void FrameWidget::on_continuousCheckBox_toggled(bool checked)
{
  if (checked)
    ui->sendPushButton->setText(QLatin1String("Start"));
  else
    ui->sendPushButton->setText(QLatin1String("Send"));
}

void FrameWidget::on_sendPushButton_clicked()
{
  if (ui->continuousCheckBox->isChecked()) {
    if (refreshRateTimer) { // Communication ongoing
      delete refreshRateTimer;
      refreshRateTimer = 0;

      ui->sendPushButton->setText(QLatin1String("Start"));

      enableSettings();
    } else {
      refreshRateTimer = new QTimer(this);
        refreshRateTimer->setTimerType(Qt::PreciseTimer);
      connect(refreshRateTimer, SIGNAL(timeout()),
              this,             SLOT(on_refreshRateTimer_timeout()));

      disableSettings();

      ui->sendPushButton->setText(QLatin1String("Stop"));

      refreshRateTimer->start(ui->refreshRateValueSpinBox->text().toInt());
    }
  } else {
    emit send(encode());
  }
}

void FrameWidget::on_refreshRateTimer_timeout()
{

    if(ui->ControlPosition->isChecked()){

        if(ui->M1Angle->text().toFloat() == 90)
                ui->M1Angle->setText(QString::number(162));
        else
            ui->M1Angle->setText(QString::number(90));

        if(ui->M2Angle->text().toFloat() == 90)
                ui->M2Angle->setText(QString::number(162));
        else
            ui->M2Angle->setText(QString::number(90));

        txQueue.push(POSITION_SET); //TODO make new opcode
}
    else if(ui->ControlCurrent->isChecked()){
        txQueue.push(CURRENT_SET); //TODO make new opcode
    }
    emit send(encode());
}

struct __attribute__((__packed__)) TXPacketStruct {
        uint8_t START[2];

        uint8_t OPCODE;

        uint8_t M1C[4];
        uint8_t M2C[4];

        uint8_t M1P[4];
        uint8_t M2P[4];

        float r_cmd;
        float s_cmd;

        float k_r;
        float k_s;
        float ki_r;
        float ki_s;

        float kr_s;
        float kr_d;
        float ks_s;
        float ks_d;

        float k_i;

        uint8_t StatBIT_1 : 1;
        uint8_t StatBIT_2 : 1;
        uint8_t StatBIT_3 : 1;
        uint8_t StatBIT_4 : 1;
        uint8_t StatBIT_5 : 1;
        uint8_t StatBIT_6 : 1;
        uint8_t StatBIT_7 : 1;
        uint8_t StatBIT_8 : 1;

        uint8_t TRIGGER;

        uint8_t CRCCheck[2];

        uint8_t STOP[2];
};

struct TXPacketStruct TXPacket;
uint8_t *TXPacketPTR = (uint8_t*)&TXPacket;

union {
        int32_t WORD;
        uint16_t HALFWORD;
        uint8_t BYTE[4];
} WORDtoBYTE;

int32_t TempDATA;

uint32_t CALC_CRC;

float tempFloat = 0;

QByteArray frameWriteData;

QByteArray FrameWidget::encode()
{
    TXPacket.START[0] = 0x7E;
    TXPacket.START[1] = 0x5B;

    TXPacket.STOP[0] = 0x5D;
    TXPacket.STOP[1] = 0x7E;

    uint8_t *SendPacket;

    QByteArray frame;

    QDataStream encoder(&frame, QIODevice::ReadWrite);

    encoder.setByteOrder(QDataStream::ByteOrder(ui->endiannessComboBox->itemData(ui->endiannessComboBox->currentIndex()).value<int>()));

    if(txQueue.size()>0){
        switch(txQueue.front()){
        case KILL:
            TXPacket.OPCODE = txQueue.front();
            SendPacket = TXPacketPTR;
            break;
        case WRITE:
            TXPacket.OPCODE = txQueue.front();
            SendPacket = TXPacketPTR;
            break;
        case BRIDGE:
            TXPacket.OPCODE = txQueue.front();
            SendPacket = TXPacketPTR;
            break;
        case CURRENT_SET:
            if(ui->ManualCurrentcheckBox->isChecked()){
                WORDtoBYTE.WORD = (ui->M1Current->text().toFloat())*(pow(2,15)/60);

                TXPacket.M1C[0] = WORDtoBYTE.BYTE[0];
                TXPacket.M1C[1] = WORDtoBYTE.BYTE[1];
                TXPacket.M1C[2] = WORDtoBYTE.BYTE[2];
                TXPacket.M1C[3] = WORDtoBYTE.BYTE[3];

                WORDtoBYTE.WORD = (ui->M2Current->text().toFloat())*(pow(2,15)/60);

                TXPacket.M2C[0] = WORDtoBYTE.BYTE[0];
                TXPacket.M2C[1] = WORDtoBYTE.BYTE[1];
                TXPacket.M2C[2] = WORDtoBYTE.BYTE[2];
                TXPacket.M2C[3] = WORDtoBYTE.BYTE[3];
            }
            TXPacket.OPCODE = txQueue.front();
            SendPacket = TXPacketPTR;
            break;
        case POSITION_SET:
            if(ui->ManualPositioncheckBox->isChecked()){

                //TXPacket.M1P = ;
                //TXPacket.M2P = ;
            }

            if(ui->ManualAnglecheckBox->isChecked()){
                if(ui->M1Angle->hasAcceptableInput()){
                    WORDtoBYTE.WORD = 4*250*(1 - ui->M2Angle->text().toFloat()/180);

                    TXPacket.M1P[0] = WORDtoBYTE.BYTE[0];
                    TXPacket.M1P[1] = WORDtoBYTE.BYTE[1];
                    TXPacket.M1P[2] = WORDtoBYTE.BYTE[2];
                    TXPacket.M1P[3] = WORDtoBYTE.BYTE[3];
                }

                if(ui->M2Angle->hasAcceptableInput()){
                    WORDtoBYTE.WORD = 4*250*(ui->M1Angle->text().toFloat()/180 - 1);

                    TXPacket.M2P[0] = WORDtoBYTE.BYTE[0];
                    TXPacket.M2P[1] = WORDtoBYTE.BYTE[1];
                    TXPacket.M2P[2] = WORDtoBYTE.BYTE[2];
                    TXPacket.M2P[3] = WORDtoBYTE.BYTE[3];
                }
            }

            else if(ui->ManualCountcheckBox->isChecked()){
                WORDtoBYTE.WORD = ui->M1Count->text().toFloat();

                TXPacket.M1P[0] = WORDtoBYTE.BYTE[0];
                TXPacket.M1P[1] = WORDtoBYTE.BYTE[1];
                TXPacket.M1P[2] = WORDtoBYTE.BYTE[2];
                TXPacket.M1P[3] = WORDtoBYTE.BYTE[3];

                WORDtoBYTE.WORD = ui->M2Count->text().toFloat();

                TXPacket.M2P[0] = WORDtoBYTE.BYTE[0];
                TXPacket.M2P[1] = WORDtoBYTE.BYTE[1];
                TXPacket.M2P[2] = WORDtoBYTE.BYTE[2];
                TXPacket.M2P[3] = WORDtoBYTE.BYTE[3];
            }

            TXPacket.OPCODE = txQueue.front();
            SendPacket = TXPacketPTR;
            break;
        case ZERO:
            TXPacket.OPCODE = txQueue.front();
            SendPacket = TXPacketPTR;
            break;
        case GAIN_SET:
            TXPacket.OPCODE = txQueue.front();
            SendPacket = TXPacketPTR;
            break;
        case GAIN_CHANGE_M1:
            //Using current/position data slot for gains
            WORDtoBYTE.WORD = ui->PGainM1->text().toFloat()*pow(2.0,32.0);
            TXPacket.M1C[0] = WORDtoBYTE.BYTE[0];
            TXPacket.M1C[1] = WORDtoBYTE.BYTE[1];
            TXPacket.M1C[2] = WORDtoBYTE.BYTE[2];
            TXPacket.M1C[3] = WORDtoBYTE.BYTE[3];

            WORDtoBYTE.WORD = ui->IGainM1->text().toFloat()*(pow(2.0,41.0)/10000.0);
            TXPacket.M1P[0] = WORDtoBYTE.BYTE[0];
            TXPacket.M1P[1] = WORDtoBYTE.BYTE[1];
            TXPacket.M1P[2] = WORDtoBYTE.BYTE[2];
            TXPacket.M1P[3] = WORDtoBYTE.BYTE[3];

            WORDtoBYTE.WORD = ui->DGainM1->text().toFloat()*pow(2.0,28.0)*10000;
            TXPacket.M2C[0] = WORDtoBYTE.BYTE[0];
            TXPacket.M2C[1] = WORDtoBYTE.BYTE[1];
            TXPacket.M2C[2] = WORDtoBYTE.BYTE[2];
            TXPacket.M2C[3] = WORDtoBYTE.BYTE[3];


            TXPacket.OPCODE = txQueue.front();
            SendPacket = TXPacketPTR;
            break;
        case GAIN_CHANGE_M2:
            //Using current/position data slot for gains
            WORDtoBYTE.WORD = ui->PGainM2->text().toFloat()*pow(2.0,32.0);
            TXPacket.M1C[0] = WORDtoBYTE.BYTE[0];
            TXPacket.M1C[1] = WORDtoBYTE.BYTE[1];
            TXPacket.M1C[2] = WORDtoBYTE.BYTE[2];
            TXPacket.M1C[3] = WORDtoBYTE.BYTE[3];

            WORDtoBYTE.WORD = ui->IGainM2->text().toFloat()*(pow(2.0,41.0)/10000.0);
            TXPacket.M1P[0] = WORDtoBYTE.BYTE[0];
            TXPacket.M1P[1] = WORDtoBYTE.BYTE[1];
            TXPacket.M1P[2] = WORDtoBYTE.BYTE[2];
            TXPacket.M1P[3] = WORDtoBYTE.BYTE[3];

            WORDtoBYTE.WORD = ui->DGainM2->text().toFloat()*pow(2.0,28.0)*10000;
            TXPacket.M2C[0] = WORDtoBYTE.BYTE[0];
            TXPacket.M2C[1] = WORDtoBYTE.BYTE[1];
            TXPacket.M2C[2] = WORDtoBYTE.BYTE[2];
            TXPacket.M2C[3] = WORDtoBYTE.BYTE[3];

            TXPacket.OPCODE = txQueue.front();
            SendPacket = TXPacketPTR;
            break;
        case CONFIG_SET:
            TXPacket.OPCODE = txQueue.front();
            SendPacket = TXPacketPTR;
            break;
        case START_CONTROL:
            TXPacket.OPCODE = txQueue.front();
            if(ui->Kr->hasAcceptableInput() && ui->Ktheta->hasAcceptableInput()){
                TXPacket.k_r = ui->Kr->text().toFloat();
                TXPacket.k_s = ui->Ktheta->text().toFloat();
            }
            else{
                TXPacket.k_r = 0;
                TXPacket.k_s = 0;
            }
            TXPacket.r_cmd = ui->R->text().toFloat();
            TXPacket.s_cmd = ui->Theta->text().toFloat();
            TXPacket.kr_s = ui->Kr_s->text().toFloat();
            TXPacket.kr_d = ui->Kr_d->text().toFloat();
            TXPacket.ki_r = ui->Ki_r->text().toFloat();
            TXPacket.ki_s = ui->Ki_theta->text().toFloat();
            TXPacket.ks_s = ui->Ktheta_s->text().toFloat();
            TXPacket.ks_d = ui->Ktheta_d->text().toFloat();
            TXPacket.k_i = ui->Ki->text().toFloat();

            SendPacket = TXPacketPTR;
            break;
        case TRIGGER_ONESHOT:
            TXPacket.OPCODE = txQueue.front();

            SendPacket = TXPacketPTR;
            break;
        default:
            SendPacket = TXPacketPTR;
        }

        txQueue.pop();
        if(txQueue.size()>10)
            while(!txQueue.empty())
                txQueue.pop();

        CALC_CRC = crcCalc(&TXPacket.OPCODE, 0, PAYLOAD_TX, 0); //Check entire data CRC
        WORDtoBYTE.HALFWORD = CALC_CRC;

        TXPacket.CRCCheck[0] = WORDtoBYTE.BYTE[1];
        TXPacket.CRCCheck[1] = WORDtoBYTE.BYTE[0];

        for (int i = 0; i < sizeof(TXPacket); i++){
            encoder << quint8(SendPacket[i]);
        }
    }

    //frame.insert(3, 0x22);
    //frame.insert(4, 0x22);

    if (ui->formatComboBox->itemData(ui->formatComboBox->currentIndex()).value<int>() == Hexadecimal)
        frame = HexString::fromRawBinary(frame);

    return frame;
}

//QByteArray FrameWidget::encode()
//{

//  QByteArray frame;

//  QDataStream encoder(&frame, QIODevice::ReadWrite);

//  encoder.setByteOrder(QDataStream::ByteOrder(ui->endiannessComboBox->itemData(ui->endiannessComboBox->currentIndex()).value<int>()));

//  switch(ui->dataTypeComboBox->itemData(ui->dataTypeComboBox->currentIndex()).value<int>()) {
//    case UINT8:
//      for (int i = 0; i < sliderVector.size(); i++)
//        encoder << quint8(sliderVector[i]->value());
//
//      break;

//    case UINT16:
//      for (int i = 0; i < sliderVector.size(); i++)
//        encoder << quint16(sliderVector[i]->value());
//      break;

//    case UINT32:
//      for (int i = 0; i < sliderVector.size(); i++)
//        encoder << quint32(sliderVector[i]->value());
//      break;

//    case INT8:
//      for (int i = 0; i < sliderVector.size(); i++)
//        encoder << qint8(sliderVector[i]->value());
//      break;

//    case INT16:
//      for (int i = 0; i < sliderVector.size(); i++)
//        encoder << qint16(sliderVector[i]->value());
//      break;

//    case INT32:
//      for (int i = 0; i < sliderVector.size(); i++)
//        encoder << qint32(sliderVector[i]->value());
//      break;
//  }

//  if (ui->formatComboBox->itemData(ui->formatComboBox->currentIndex()).value<int>() == Hexadecimal)
//    frame = HexString::fromRawBinary(frame);

//  if (ui->prependLineEdit->hasAcceptableInput())
//    //frame.insert(0, char(ui->prependLineEdit->text().toInt()));
//      frame.insert(0, 0xA5);

//  if(ui->appendLineEdit->hasAcceptableInput())
//    //frame.append(char(ui->appendLineEdit->text().toInt()));
//      frame.insert(1, 0xFF);

//  return frame;
//}

void FrameWidget::enableSettings()
{
  ui->payloadSpinBox->setEnabled(true);
  ui->dataTypeComboBox->setEnabled(true);
  ui->endiannessComboBox->setEnabled(true);
  ui->formatComboBox->setEnabled(true);
  ui->appendLineEdit->setEnabled(true);
  ui->prependLineEdit->setEnabled(true);
  ui->payloadSpinBox->setEnabled(true);
  ui->refreshRateValueSpinBox->setEnabled(true);
  ui->continuousCheckBox->setEnabled(true);
}


void FrameWidget::disableSettings()
{
  ui->payloadSpinBox->setDisabled(true);
  ui->dataTypeComboBox->setDisabled(true);
  ui->endiannessComboBox->setDisabled(true);
  ui->formatComboBox->setDisabled(true);
  ui->appendLineEdit->setDisabled(true);
  ui->prependLineEdit->setDisabled(true);
  ui->payloadSpinBox->setDisabled(true);
  ui->refreshRateValueSpinBox->setDisabled(true);
  ui->continuousCheckBox->setDisabled(true);
}

void FrameWidget::on_WriteEnable_clicked()
{
    txQueue.push(WRITE);
}

void FrameWidget::on_ZeroPosition_clicked()
{
    txQueue.push(ZERO);
}

void FrameWidget::on_LegRadius_textChanged(const QString &arg1)
{
    if(ui->ManualPositioncheckBox->isChecked())
        if(ui->LegRadius->hasAcceptableInput())
            txQueue.push(POSITION_SET);

}

void FrameWidget::on_LegAngle_textChanged(const QString &arg1)
{
    if(ui->ManualPositioncheckBox->isChecked())
        if(ui->LegAngle->hasAcceptableInput())
            txQueue.push(POSITION_SET);
}

void FrameWidget::on_BridgeEnable_clicked()
{
    txQueue.push(BRIDGE);
}

void FrameWidget::on_KillBridge_clicked()
{
    txQueue.push(KILL);
}

void FrameWidget::on_M1Angle_textChanged(const QString &arg1)
{
    if(ui->ManualAnglecheckBox->isChecked())
        if(ui->M1Angle->hasAcceptableInput())
            txQueue.push(POSITION_SET);
}

void FrameWidget::on_M2Angle_textChanged(const QString &arg1)
{
    if(ui->ManualAnglecheckBox->isChecked())
        if(ui->M2Angle->hasAcceptableInput())
            txQueue.push(POSITION_SET);
}

void FrameWidget::on_GainSetcomboBox_currentIndexChanged(int index)
{
    TXPacket.StatBIT_1 = ui->GainSetcomboBox->itemData(index).value<int>();
    txQueue.push(GAIN_SET);
}

void FrameWidget::on_ManualPositioncheckBox_stateChanged(int arg1)
{
    if(ui->ManualPositioncheckBox->isChecked())
        ui->ManualAnglecheckBox->setDisabled(true);
    else
        ui->ManualAnglecheckBox->setEnabled(true);
}

void FrameWidget::on_ManualAnglecheckBox_stateChanged(int arg1)
{
    if(ui->ManualAnglecheckBox->isChecked())
        ui->ManualPositioncheckBox->setDisabled(true);
    else
        ui->ManualPositioncheckBox->setEnabled(true);
}

void FrameWidget::on_ControlCurrent_toggled(bool checked)
{
    if(checked)
        ui->ControlPosition->setDisabled(true);
    else
        ui->ControlPosition->setEnabled(true);
}

void FrameWidget::on_ControlPosition_toggled(bool checked)
{
    if(checked)
        ui->ControlCurrent->setDisabled(true);
    else
        ui->ControlCurrent->setEnabled(true);
}

void FrameWidget::on_SetGainsM1_clicked()
{
    if(ui->PGainM1->hasAcceptableInput() && ui->IGainM1->hasAcceptableInput() && ui->DGainM1->hasAcceptableInput())
        txQueue.push(GAIN_CHANGE_M1);
}

void FrameWidget::on_SetGainsM2_clicked()
{
    if(ui->PGainM2->hasAcceptableInput() && ui->IGainM2->hasAcceptableInput() && ui->DGainM2->hasAcceptableInput())
        txQueue.push(GAIN_CHANGE_M2);
}

void FrameWidget::on_ConfigcomboBox_currentIndexChanged(int index)
{
    TXPacket.StatBIT_2 = ui->ConfigcomboBox->itemData(index).value<int>();
    txQueue.push(CONFIG_SET);
}

void FrameWidget::on_StartControlcheckBox_clicked(bool checked)
{
    txQueue.push(START_CONTROL);
    if(checked)
        TXPacket.StatBIT_3 = 1;
    else
        TXPacket.StatBIT_3 = 0;
}

void FrameWidget::on_Trigger1pushButton_toggled(bool checked)
{
    txQueue.push(TRIGGER_ONESHOT);
    if(checked)
        TXPacket.TRIGGER = 1;
    else
        TXPacket.TRIGGER = 0;
}

void FrameWidget::on_SetuppushButton_clicked()
{
    txQueue.push(WRITE);
    TXPacket.StatBIT_2 = 1;
    txQueue.push(CONFIG_SET);
    txQueue.push(CURRENT_SET);
    txQueue.push(BRIDGE);
}
