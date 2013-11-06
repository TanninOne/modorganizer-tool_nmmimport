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

#include "modselectiondialog.h"
#include "ui_modselectiondialog.h"

ModSelectionDialog::ModSelectionDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::ModSelectionDialog)
{
  ui->setupUi(this);
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
  ui->modsList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  ui->modsList->header()->setSectionResizeMode(1, QHeaderView::Fixed);
  ui->modsList->header()->setSectionResizeMode(2, QHeaderView::Fixed);
#else
  ui->modsList->header()->setResizeMode(0, QHeaderView::Stretch);
  ui->modsList->header()->setResizeMode(1, QHeaderView::Fixed);
  ui->modsList->header()->setResizeMode(2, QHeaderView::Fixed);
#endif
  ui->modsList->header()->resizeSection(1, 80);
  ui->modsList->header()->resizeSection(2, 80);
}

ModSelectionDialog::~ModSelectionDialog()
{
  delete ui;
}

void ModSelectionDialog::addMod(const QString &key,const QString &name,
                                const QString &version, int fileCount)
{
  QStringList data;
  data.append(name);
  data.append(version);
  data.append(QString("%1").arg(fileCount));

  QTreeWidgetItem *newItem = new QTreeWidgetItem(data);
  newItem->setFlags(newItem->flags() | Qt::ItemIsUserCheckable);
  newItem->setCheckState(0, Qt::Checked);
  newItem->setTextAlignment(1, Qt::AlignHCenter);
  newItem->setTextAlignment(2, Qt::AlignHCenter);
  newItem->setData(0, Qt::UserRole, key);

  ui->modsList->addTopLevelItem(newItem);
}

std::vector<QString> ModSelectionDialog::getEnabledMods() const
{
  std::vector<QString> result;
  for (int i = 0; i < ui->modsList->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = ui->modsList->topLevelItem(i);
    if (item->checkState(0) == Qt::Checked) {
      result.push_back(item->data(0, Qt::UserRole).toString());
    }
  }
  return result;
}


void ModSelectionDialog::on_continueButton_clicked()
{
  accept();
}


void ModSelectionDialog::on_cancelButton_clicked()
{
  reject();
}
