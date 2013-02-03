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


#ifndef MODEDIALOG_H
#define MODEDIALOG_H

#include <QDialog>

namespace Ui {
class ModeDialog;
}

class ModeDialog : public QDialog
{
  Q_OBJECT
public:
  enum InstallMode {
    MODE_COPYONLY,
    MODE_COPYDELETE,
    MODE_MOVE
  };
public:
  explicit ModeDialog(QWidget *parent = 0);
  ~ModeDialog();

  InstallMode getMode() const { return m_Mode; }

private slots:
  void on_cancelButton_clicked();

  void on_copyButton_clicked();

  void on_copyDeleteButton_clicked();

  void on_moveButton_clicked();

private:
  Ui::ModeDialog *ui;
  InstallMode m_Mode;
};

#endif // MODEDIALOG_H
