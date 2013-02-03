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

#include "nmmpathsdialog.h"
#include "ui_nmmpathsdialog.h"
#include <QFileDialog>

NMMPathsDialog::NMMPathsDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::NMMPathsDialog)
{
  ui->setupUi(this);
}

NMMPathsDialog::~NMMPathsDialog()
{
  delete ui;
}

void NMMPathsDialog::setInstallInfoPreset(const QString &path)
{
  ui->installInfoEdit->setText(path);
}

void NMMPathsDialog::setModFolderPreset(const QString &path)
{
  ui->modFolderEdit->setText(path);
}

void NMMPathsDialog::setMessage(const QString &message)
{
  ui->messageLabel->setText(message);
}

QString NMMPathsDialog::getInstallInfoFolder() const
{
  return ui->installInfoEdit->text();
}

QString NMMPathsDialog::getModFolder() const
{
  return ui->modFolderEdit->text();
}

void NMMPathsDialog::on_browseInfoBtn_clicked()
{
  ui->installInfoEdit->setText(
        QFileDialog::getExistingDirectory(this, tr("Select directory"),
                                          ui->installInfoEdit->text()));
}

void NMMPathsDialog::on_browseModBtn_clicked()
{
  ui->modFolderEdit->setText(
        QFileDialog::getExistingDirectory(this, tr("Select directory"),
                                          ui->modFolderEdit->text()));
}
