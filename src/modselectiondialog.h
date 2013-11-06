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

#ifndef MODSELECTIONDIALOG_H
#define MODSELECTIONDIALOG_H

#include <QDialog>
#include <vector>
#include <set>

namespace Ui {
class ModSelectionDialog;
}

class ModSelectionDialog : public QDialog
{
  Q_OBJECT
  
public:
  explicit ModSelectionDialog(QWidget *parent = 0);
  ~ModSelectionDialog();

  /**
   * @brief add a mod to be displayed in the dialog
   * @param key key of the mod
   * @param name name of the mod (displayed)
   * @param version version of the mod
   * @param fileCount number of files belonging to the mod
   */
  void addMod(const QString &key, const QString &name, const QString &version,
              int fileCount);

  /**
   * @brief retrieve a set of enabled mods
   * @return the keys of mods that were checked by the user
   */
  std::vector<QString> getEnabledMods() const;

private slots:
  void on_cancelButton_clicked();

  void on_continueButton_clicked();

private:
  Ui::ModSelectionDialog *ui;
};

#endif // MODSELECTIONDIALOG_H
