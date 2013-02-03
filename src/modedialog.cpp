/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of NMM Import plugin for MO

NMM Import plugin is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

NMM Import plugin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with NMM Import plugin.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "modedialog.h"
#include "ui_modedialog.h"

ModeDialog::ModeDialog(QWidget *parent)
  : QDialog(parent), ui(new Ui::ModeDialog), m_Mode(MODE_COPYONLY)
{
  ui->setupUi(this);
}

ModeDialog::~ModeDialog()
{
  delete ui;
}

void ModeDialog::on_cancelButton_clicked()
{
  reject();
}

void ModeDialog::on_copyButton_clicked()
{
  m_Mode = MODE_COPYONLY;
  accept();
}

void ModeDialog::on_copyDeleteButton_clicked()
{
  m_Mode = MODE_COPYDELETE;
  accept();
}

void ModeDialog::on_moveButton_clicked()
{
  m_Mode = MODE_MOVE;
  accept();
}
