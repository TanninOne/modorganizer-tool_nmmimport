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

#ifndef NMMIMPORT_H
#define NMMIMPORT_H

#include <iplugintool.h>
#include <imoinfo.h>
#include <archive.h>
#include <igameinfo.h>
#include <QtXml>
#include <vector>
#include <set>
#include <QProgressDialog>
#include "modedialog.h"


class NMMImport : public MOBase::IPluginTool
{

  Q_OBJECT
  Q_INTERFACES(MOBase::IPlugin MOBase::IPluginTool)
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
  Q_PLUGIN_METADATA(IID "org.tannin.NMMImport" FILE "nmmimport.json")
#endif

public:
  NMMImport();
  ~NMMImport();

  virtual bool init(MOBase::IOrganizer *moInfo);
  virtual QString name() const;
  virtual QString author() const;
  virtual QString description() const;
  virtual MOBase::VersionInfo version() const;
  virtual bool isActive() const;
  virtual QList<MOBase::PluginSetting> settings() const;

  virtual QString displayName() const;
  virtual QString tooltip() const;
  virtual QIcon icon() const;

public slots:
  virtual void display() const;

private:

  struct ModInfo {
    QString name;
    QString version;
    QString installFile;
    int nexusID;

    std::vector<std::pair<QString, bool> > files;
  };

  enum EResult {
    RES_FAILED,
    RES_PARTIAL,
    RES_SUCCESS
  };

private:

  static QDomNode getNode(const QDomElement &parent, const QString &displayName, bool mayBeEmpty = false);
  static QString getTextNodeValue(const QDomElement &parent, const QString &tag, bool mayBeEmpty = false);
  static QString getLocalAppFolder();
  static QString getGameModeName(MOBase::IGameInfo::Type type);
  static bool testInstallLog(const QString &path, QString &problem);
  static bool testModFolder(const QString &path, QString &problem);

  QString digForSetting(QDomElement element) const;
  bool determineNMMFolders(QString &installLog, QString &modFolder) const;

  void unpackFiles(const QString &archiveFile, const QString &outputDirectory, const std::set<QString> &extractFiles) const;
  MOBase::IModInterface *initMod(const QString &modName, const ModInfo &info) const;
  EResult installMod(const ModInfo &modInfo, ModeDialog::InstallMode mode, MOBase::IModInterface *mod, const QString &modFolder) const;

  bool readMods(const QDomDocument &document, std::vector<std::pair<QString, ModInfo>> &modList) const;
  bool readFiles(const QDomDocument &document, std::vector<std::pair<QString, ModInfo>> &modList) const;
  bool parseInstallLog(QDomDocument &document, const QString &installLog, std::vector<std::pair<QString, ModInfo> > &modList) const;
  void removeModFromInstallLog(QDomDocument &document, const QString &key) const;

  void transferMods(const std::vector<std::pair<QString, ModInfo>> &modList, QDomDocument &document, const QString &installLog, const QString &modFolder) const;

  virtual void setParentWidget(QWidget *widget);

private:

  MOBase::IOrganizer *m_MOInfo { nullptr };

  Archive *m_ArchiveHandler;

};

#endif // NMMIMPORT_H
