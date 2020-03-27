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

#ifndef FRAMEWIDGET_H
#define FRAMEWIDGET_H

#include <QWidget>
#include <QTimer>

#include "slider.h"

#include <cmath>
#include <CRC.h>
#include <arrayFunctions.h>
#include <QDoubleValidator>

namespace Ui {
  class FrameWidget;
}

class FrameWidget : public QWidget
{
    Q_OBJECT

  public:
    enum DataType {
      UINT8 = 0,
      UINT16 = 1,
      UINT32 = 2,
      INT8 = 3,
      INT16 = 4,
      INT32 = 5
    };

    enum Format {
      Binary = 0,
      Hexadecimal = 1
    };

    explicit FrameWidget(QWidget *parent = 0);
    ~FrameWidget();
    
  signals:
    void send(QByteArray const&);

  private slots:
    void on_dataTypeComboBox_currentIndexChanged(int);

    void on_payloadSpinBox_valueChanged(int);
    void on_minimumValueSpinBox_valueChanged(int);
    void on_maximumValueSpinBox_valueChanged(int);

    void on_continuousCheckBox_toggled(bool checked);

    void on_sendPushButton_clicked();

    void on_refreshRateTimer_timeout();

    void on_WriteEnable_clicked();

    void on_BridgeEnable_clicked();

    void on_ZeroPosition_clicked();

    void on_LegRadius_textChanged(const QString &arg1);

    void on_LegAngle_textChanged(const QString &arg1);

    void on_KillBridge_clicked();

    void on_M1Angle_textChanged(const QString &arg1);

    void on_M2Angle_textChanged(const QString &arg1);

    void on_GainSetcomboBox_currentIndexChanged(int index);

    void on_ManualPositioncheckBox_stateChanged(int arg1);

    void on_ManualAnglecheckBox_stateChanged(int arg1);

    void on_ControlCurrent_toggled(bool checked);

    void on_ControlPosition_toggled(bool checked);

    void on_SetGainsM1_clicked();

    void on_SetGainsM2_clicked();

    void on_ConfigcomboBox_currentIndexChanged(int index);

    void on_StartControlcheckBox_clicked(bool checked);

    void on_Trigger1pushButton_toggled(bool checked);

    void on_SetuppushButton_clicked();

private:
    Ui::FrameWidget *ui;
    QTimer *refreshRateTimer;
    QVector<Slider*> sliderVector;

    QByteArray encode();
    void enableSettings();
    void disableSettings();
};

#endif // FRAMEWIDGET_H
