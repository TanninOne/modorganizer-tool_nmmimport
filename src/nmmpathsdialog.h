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

#ifndef NMMPATHSDIALOG_H
#define NMMPATHSDIALOG_H

#include <QDialog>

namespace Ui {
class NMMPathsDialog;
}

class NMMPathsDialog : public QDialog
{
  Q_OBJECT
  
public:
  explicit NMMPathsDialog(QWidget *parent = 0);
  ~NMMPathsDialog();

  void setInstallInfoPreset(const QString &path);
  void setModFolderPreset(const QString &path);
  void setMessage(const QString &message);

  QString getInstallInfoFolder() const;
  QString getModFolder() const;

private slots:
  void on_browseInfoBtn_clicked();

  void on_browseModBtn_clicked();

private:
  Ui::NMMPathsDialog *ui;
};

#endif // NMMPATHSDIALOG_H
