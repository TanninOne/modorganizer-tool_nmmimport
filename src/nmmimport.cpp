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

#include "nmmimport.h"
#include "modselectiondialog.h"
#include "modedialog.h"
#include "nmmpathsdialog.h"
#include <versioninfo.h>
#include <utility.h>
#include <report.h>
#include <QInputDialog>
#include <QProgressDialog>
#include <QMessageBox>
#include <regex>


using namespace MOBase;


template <typename T> T resolveFunction(QLibrary &lib, const char *name)
{
  T temp = reinterpret_cast<T>(lib.resolve(name));
  if (temp == NULL) {
    throw std::runtime_error(QObject::tr("invalid archive.dll: %1").arg(lib.errorString()).toLatin1().constData());
  }
  return temp;
}


NMMImport::NMMImport()
{
}

bool NMMImport::init(IOrganizer *moInfo)
{
  m_MOInfo = moInfo;
  return true;
}

QString NMMImport::name() const
{
  return tr("NMM Import");
}

QString NMMImport::author() const
{
  return "Tannin";
}

QString NMMImport::description() const
{
  return tr("Imports Mods installed through Nexus Mod Manager into MO");
}

VersionInfo NMMImport::version() const
{
  return "0.0.1";
}

bool NMMImport::isActive() const
{
  return true;
}

QList<PluginSetting> NMMImport::settings() const
{
  return QList<PluginSetting>();
}

QString NMMImport::displayName() const
{
  return tr("NMM Import");
}

QString NMMImport::tooltip() const
{
  return tr("Imports Mods installed through Nexus Mod Manager");
}

QIcon NMMImport::icon() const
{
  return QIcon(":/nmmimport/icon_import");
}

typedef Archive* (*CreateArchiveType)();


void queryPassword(LPSTR)
{
}


void updateProgress(float)
{

}


void updateProgressFile(LPCWSTR)
{
}


void report7ZipError(LPCWSTR errorMessage)
{
  reportError(QObject::tr("extraction error: %1").arg(ToQString(errorMessage)));
}


void NMMImport::unpackMissingFiles(const QString &archiveFile, const std::set<QString> &extractFiles, Archive *archive, const QString &modFolder, IModInterface *mod) const
{
  if (!archive->open(ToWString(QDir::toNativeSeparators(modFolder + "/" + archiveFile)).c_str(),
                     new FunctionCallback<void, LPSTR>(&queryPassword))) {
    reportError(tr("failed top open archive \"%1\": %2").arg(archiveFile).arg(archive->getLastError()));
  }
  FileData* const *data;
  size_t size;
  archive->getFileList(data, size);
  for (size_t i = 0; i < size; ++i) {
    QString fileName = ToQString(data[i]->getFileName()).toLower();
    if (extractFiles.find(fileName) == extractFiles.end()) {
      data[i]->setSkip(true);
    } else {
      data[i]->setSkip(false);
      if (fileName.startsWith("Data/", Qt::CaseInsensitive)) {
        fileName.remove(0, 5);
      }
      data[i]->setOutputFileName(ToWString(fileName).c_str());
    }
  }
  if (!archive->extract(ToWString(QDir::toNativeSeparators(mod->absolutePath())).c_str(),
                        new FunctionCallback<void, float>(&updateProgress),
                        new FunctionCallback<void, LPCWSTR>(&updateProgressFile),
                        new FunctionCallback<void, LPCWSTR>(&report7ZipError))) {
    reportError(tr("failed to extract missing files from %1, mod is incomplete: %2").arg(archiveFile).arg(archive->getLastError()));
  }
  archive->close();
}

IModInterface *NMMImport::initMod(const QString &modName, const ModInfo &info) const
{
  static std::tr1::regex exp("([a-zA-Z0-9_\\- ]*?)([-_ ]V?[0-9_]+)?-([1-9][0-9]+).*");

  GuessedValue<QString> temp(modName);
  IModInterface *mod = m_MOInfo->createMod(temp);
  mod->setVersion(VersionInfo(info.version));

  std::tr1::match_results<std::string::const_iterator> result;
  if (std::tr1::regex_search(std::string(info.installFile.toUtf8().constData()), result, exp)) {
    std::string temp = result[3].str();
    mod->setNexusID(strtol(temp.c_str(), NULL, 10));
  } else {
    qWarning("no nexus id found in %s", qPrintable(info.installFile));
  }

  return mod;
}

bool NMMImport::installMod(const ModInfo &modInfo, ModeDialog::InstallMode mode, IModInterface *mod, Archive *archive, const QString &modFolder) const
{
  // set of files to extract from the archive because the installed file comes from
  // a different mod (relative to the game-directory, backslashes, lower-case)
  std::set<QString> extractFiles;

  QDir dataDir(m_MOInfo->gameInfo().path() + "/data");

  QStringList sourceFiles;
  QStringList destinationFiles;

  for (auto fileIter = modInfo.files.begin(); fileIter != modInfo.files.end(); ++fileIter) {
    QString fileName = QDir::fromNativeSeparators(fileIter->first);

    if (fileName.startsWith("Data/", Qt::CaseInsensitive)) {
      fileName.remove(0, 5);
    } else {
      qWarning("file doesn't start with Data/: %s", qPrintable(fileName));
      continue;
    }
    if (fileIter->second) {
      QString sourcePath = dataDir.absoluteFilePath(fileName);
      sourceFiles.append(sourcePath);
      destinationFiles.append(mod->absolutePath() + "/" + dataDir.relativeFilePath(sourcePath));
    } else {
      extractFiles.insert(fileIter->first.mid(0).toLower());
      qWarning("extract \"%s\" from \"%s\"",
               qPrintable(fileIter->first), qPrintable(modInfo.installFile));
    }
  }

  bool error = false;

  if (mode == ModeDialog::MODE_MOVE) {
    error = !shellMove(sourceFiles, destinationFiles, parentWidget());
  } else {
    error = !shellCopy(sourceFiles, destinationFiles, parentWidget());
  }

  if (error) {
    reportError(tr("Import failed at mod \"%1\": %2").arg(modInfo.name).arg(windowsErrorString(::GetLastError())));
  }

  if (extractFiles.size() != 0) {
    unpackMissingFiles(modInfo.installFile, extractFiles, archive, modFolder, mod);
  }

  if (!error && (mode == ModeDialog::MODE_COPYDELETE)) {
    // copy successful, iterate again over all files and remove them
    for (auto fileIter = modInfo.files.begin(); fileIter != modInfo.files.end() && !error; ++fileIter) {
      QString fileName = QDir::fromNativeSeparators(fileIter->first);
      QString sourcePath = m_MOInfo->gameInfo().path() + "/" + fileName;
      if (fileIter->second) {
        QFile(sourcePath).remove();
      }
    }
  }
  return !error;
}


void NMMImport::transferMods(const std::map<QString, ModInfo> &modsByKey, QDomDocument &document,
                             const QString &installLog, const QString &modFolder) const
{

  // query which mods to transfer
  ModSelectionDialog modsDialog(parentWidget());

  for (auto iter = modsByKey.begin(); iter != modsByKey.end(); ++iter) {
    if (iter->second.name != "ORIGINAL_VALUE") {
      modsDialog.addMod(iter->first, iter->second.name, iter->second.version, iter->second.files.size());
    }
  }
  if (modsDialog.exec() == QDialog::Rejected) {
    return;
  }

  // query the mode by which to transfer
  ModeDialog modeDialog(parentWidget());
  if (modeDialog.exec() == QDialog::Rejected) {
    return;
  }


  QLibrary archiveLib("dlls\\archive.dll");
  if (!archiveLib.load()) {
    reportError(tr("archive.dll not loaded: \"%1\"").arg(archiveLib.errorString()));
    return;
  }

  CreateArchiveType CreateArchiveFunc = resolveFunction<CreateArchiveType>(archiveLib, "CreateArchive");

  Archive *archive = CreateArchiveFunc();
  if (!archive->isValid()) {
    reportError(tr("incompatible archive.dll: %1").arg(archive->getLastError()));
    return;
  }

  // do it!
  std::set<QString> enabledMods = modsDialog.getEnabledMods();
  QProgressDialog progress(parentWidget());
  progress.setMaximum(enabledMods.size());
  progress.setValue(0);
  progress.setCancelButton(NULL);
  progress.show();

  bool error = false;
  for (auto iter = enabledMods.begin(); iter != enabledMods.end() && !error; ++iter) {
    auto modIter = modsByKey.find(*iter);
    if (modIter == modsByKey.end()) {
      reportError(tr("invalid mod key \"%1\". This is a bug. The mod will not be transfered").arg(*iter));
      continue;
    }

    QString modName = modIter->second.name;
    progress.setLabelText(modName);
    bool ok = true;
    while (m_MOInfo->getMod(modName) != NULL) {
      modName = QInputDialog::getText(parentWidget(), tr("Mod exists!"),
          tr("A mod with this name already exists, please enter a new name or press "
             "\"Cancel\" to skip import of this mod."), QLineEdit::Normal, modName, &ok);
      if (!ok || modName.isEmpty()) {
        ok = false;
        break;
      }
    }
    if (!ok) {
      continue;
    }

    // init new MO mod
    IModInterface *mod = initMod(modName, modIter->second);

    if (installMod(modIter->second, modeDialog.getMode(), mod, archive, modFolder)) {
      if (( modeDialog.getMode() == ModeDialog::MODE_COPYDELETE) || ( modeDialog.getMode() == ModeDialog::MODE_MOVE)) {
        removeModFromInstallLog(document, *iter);
      }
    } else {
      error = true;
    }

    progress.setValue(progress.value() + 1);
    m_MOInfo->modDataChanged(mod);
  }

  QFile::copy(installLog, installLog.mid(0).append(".backup"));

  QFile installFile(installLog);
  if (!installFile.open(QIODevice::WriteOnly)) {
    reportError(tr("failed to update NMMs \"InstallLog.xml\""));
  } else {
    QTextStream textStream(&installFile);
    document.save(textStream, 0);
  }
  installFile.close();
  progress.hide();
}


void NMMImport::display() const
{
  QString installLog;
  QString modFolder;
  if (!determineNMMFolders(installLog, modFolder)) {
    return;
  }

  installLog.append("/InstallLog.xml");
  std::map<QString, ModInfo> modsByKey;
  QDomDocument document("InstallLog");

  if (!parseInstallLog(document, installLog, modsByKey)) {
    return;
  }

  if (modsByKey.size() > 1) {
    transferMods(modsByKey, document, installLog, modFolder);
  } else {
    QMessageBox::information(parentWidget(), tr("Nothing to import"),
                             tr("There are no mods installed by NMM."), QMessageBox::Ok);
  }

  if (QMessageBox::question(parentWidget(), tr("Import Downloads?"),
          tr("Do you want to import the mod archives downloaded through NMM?"),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    QStringList sources;
    sources.append(QDir::fromNativeSeparators(modFolder)+ "/*.7z");
    sources.append(QDir::fromNativeSeparators(modFolder)+ "/*.zip");
    sources.append(QDir::fromNativeSeparators(modFolder)+ "/*.rar");
    sources.append(QDir::fromNativeSeparators(modFolder)+ "/*.fomod");

    if (!shellCopy(sources, QStringList(m_MOInfo->downloadsPath()), parentWidget())) {
      qWarning("failed to copy archives to MO download directory: %s", qPrintable(windowsErrorString(::GetLastError())));
    }
  }
}


QDomNode NMMImport::getNode(const QDomElement &parent, const QString &name)
{
  if (parent.isNull()) {
    throw MyException(tr("Element missing."));
  }
  QDomNodeList nodes = parent.elementsByTagName(name);
  if (nodes.count() == 0) {
    throw MyException(tr("Section \"%1\" missing.").arg(name));
  } else if (nodes.count() > 1) {
    throw MyException(tr("Multiple sections \"%1\", expected only one.").arg(name));
  }
  return nodes.at(0);
}


QString NMMImport::getTextNodeValue(const QDomElement &parent, const QString &tag)
{
  QDomNode textNode = getNode(parent, tag);
  if (textNode.isNull()) {
    throw MyException(tr("Expected tag \"%1\" below \"%2\"").arg(tag).arg(parent.tagName()));
  }
  QDomNodeList childNodes = textNode.childNodes();
  if (childNodes.count() != 1) {
    return QString();
  }

  QDomText textContent = childNodes.at(0).toText();
  if (textContent.isNull()) {
    throw MyException(tr("no text in \"%1\" found!").arg(tag));
  }
  return textContent.data();
}


void NMMImport::removeModFromInstallLog(QDomDocument &document, const QString &key) const
{
  QDomNode modList = getNode(document.documentElement(), "modList").toElement();
  for (QDomElement ele = modList.firstChildElement(); !ele.isNull();) {
    QDomElement next = ele.nextSiblingElement();
    if (key == ele.attribute("key")) {
      modList.removeChild(ele);
    }
    ele = next;
  }
  QDomNode dataFilesNode = getNode(document.documentElement(), "dataFiles");
  QDomNodeList files = dataFilesNode.childNodes();
  QList<QDomNode> deleteNodes;
  for (int i = 0; i < files.count(); ++i) {
    QDomElement fileEle = files.at(i).toElement();
    if (fileEle.isNull()) {
      throw MyException(tr("unrecognized file structure"));
    }
    QDomNode mods = getNode(fileEle, "installingMods");
    if (!mods.isNull()) {
      int sourcesLeft = 0;
      for (QDomElement sourceEle = mods.firstChildElement(); !sourceEle.isNull();) {
        QDomElement next = sourceEle.nextSiblingElement();
        if (key == sourceEle.attribute("key")) {
          mods.removeChild(sourceEle);
        } else {
          ++sourcesLeft;
        }
        sourceEle = next;
      }

      if (sourcesLeft == 0) {
        deleteNodes.push_back(fileEle);
      }
    } else {
      qCritical("installingMods not found in \"%s\"", qPrintable(fileEle.attribute("path")));
    }
  }
  foreach (QDomNode node, deleteNodes) {
    QDomNode result = dataFilesNode.removeChild(node);
    if (result.isNull()) {
      qCritical("failed to remove node");
    }
  }
}


bool NMMImport::readMods(const QDomDocument &document, std::map<QString, ModInfo> &modsByKey) const
{
  try {
    QDomNode modList = getNode(document.documentElement(), "modList");
    for (QDomElement ele = modList.firstChildElement(); !ele.isNull(); ele = ele.nextSiblingElement()) {
      QString key = ele.attribute("key");

      ModInfo info;
      info.name = getTextNodeValue(ele, "name");
      info.version = getTextNodeValue(ele, "version");
      info.installFile = ele.attribute("path");

      modsByKey[key] = info;
    }
    return true;
  } catch (const MyException &e) {
    reportError(tr("failed to parse \"modList\"-section of InstallLog.xml: %1").arg(e.what()));
    return false;
  }
}


bool NMMImport::readFiles(const QDomDocument &document, std::map<QString, ModInfo> &modsByKey) const
{
  try {
    QDomNodeList files = getNode(document.documentElement(), "dataFiles").childNodes();
    for (int i = 0; i < files.count(); ++i) {
      QDomElement fileEle = files.at(i).toElement();
      if (fileEle.isNull()) {
        throw MyException(tr("unrecognized file structure"));
      }
      QString path = fileEle.attribute("path");

      QDomNode mods = getNode(fileEle, "installingMods");
      for (QDomElement sourceEle = mods.firstChildElement(); !sourceEle.isNull(); sourceEle = sourceEle.nextSiblingElement()) {
        QString key = sourceEle.attribute("key");
        std::map<QString, ModInfo>::iterator iter = modsByKey.find(key);
        if (iter == modsByKey.end()) {
          qWarning("NMM Importer: data file \"%s\" references undeclared mod (key \"%s\")", qPrintable(path), qPrintable(key));
        }
        // ASSUMPTION: mod is the primary source if it's the last in the list
        iter->second.files.push_back(std::make_pair(path, sourceEle == mods.lastChildElement()));
      }
    }
    return true;
  } catch (const MyException &e) {
    reportError(tr("failed to parse \"dataFiles\"-section of InstallLog.xml: %1").arg(e.what()));
    return false;
  }
}

QString NMMImport::getLocalAppFolder()
{
  HKEY key;
  LONG errorcode = ::RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders",
                                  0, KEY_QUERY_VALUE, &key);

  if (errorcode != ERROR_SUCCESS) {
    return QString();
  }

  WCHAR temp[MAX_PATH];
  DWORD bufferSize = MAX_PATH;

  errorcode = ::RegQueryValueExW(key, L"Local AppData", NULL, NULL, (LPBYTE)temp, &bufferSize);
  if (errorcode != ERROR_SUCCESS) {
    return QString();
  }

  WCHAR expanded[MAX_PATH];

  DWORD res = ::ExpandEnvironmentStrings(temp, expanded, MAX_PATH);

  if ((res == 0) || (res >= MAX_PATH)) {
    qCritical("failed to expand appdata path: %d (%d)",
              ::GetLastError(), res);
    return QString();
  }

  return QDir::fromNativeSeparators(ToQString(expanded));
}


QString NMMImport::getGameModeName(IGameInfo::Type type)
{
  switch (type) {
    case IGameInfo::TYPE_OBLIVION: return "Oblivion";
    case IGameInfo::TYPE_FALLOUT3: return "Fallout3";
    case IGameInfo::TYPE_FALLOUTNV: return "FalloutNV";
    case IGameInfo::TYPE_SKYRIM: return "Skyrim";
    default: return QString();
  }
}


QString NMMImport::digForSetting(QDomElement element) const
{
  /* <setting name="InstallInfoFolder" serializeAs="Xml">
      <value>
          <PerGameModeSettingsOfString>
              <item modeId="Skyrim">
                  <string>c:\temp\Nexus Mod Manager\Skyrim\Install Info</string>
              </item>
          </PerGameModeSettingsOfString>
      </value>
  </setting> */

  QString gameModeName = getGameModeName(m_MOInfo->gameInfo().type());

  try {
    QDomNodeList items =
        getNode(
            getNode(element, "value").toElement(), "PerGameModeSettingsOfString").childNodes();
    for (int i = 0; i < items.count(); ++i) {
      QDomElement ele = items.at(i).toElement();
      if (ele.attribute("modeId") == gameModeName) {
        QDomText text = ele.elementsByTagName("string").at(0).firstChild().toText();
        if (!text.isNull()) {
          return text.data();
        }
      }
    }
  } catch (const std::exception &e) {
    qWarning("NMMImport: failed to find per-game settings: %s", e.what());
  }
  return QString();
}


bool NMMImport::testInstallLog(const QString &path, QString &problem)
{
  if (path.isEmpty()) {
    return false;
  } else {
    bool res = QFile::exists(path.mid(0).append("/InstallLog.xml"));
    if (!res) {
      problem = tr("The \"Installation Info\" directory is required to contain a file "
                   "called \"InstallLog.xml\"");
    }
    return res;
  }
}

bool NMMImport::testModFolder(const QString &path, QString &problem)
{
  if (path.isEmpty()) {
    return false;
  } else {
    bool res = QFile::exists(path);
    if (!res) {
      problem = tr("The Mod Folder doesn't exist");
    }
    return res;
  }
}


bool NMMImport::determineNMMFolders(QString &installLog, QString &modFolder) const
{
  modFolder.clear();
  installLog.clear();

  QString path(getLocalAppFolder());

  if (!path.isEmpty()) {
    QDirIterator dirIter(path.append("/Black_Tree_Gaming"),
                         QDir::Dirs | QDir::NoDotAndDotDot);
    if (dirIter.hasNext()) {
      dirIter.next();
      path = dirIter.fileInfo().absoluteFilePath();
    } else {
      qDebug("no folder below %s", qPrintable(path));
      path = "";
    }
  }

  // dig one level deeper. Pick the newest version here...
  if (!path.isEmpty()) {
    QDirIterator dirIter(path, QDir::Dirs | QDir::NoDotAndDotDot);
    QString newestVersionPath;
    VersionInfo newestVersion(0, 0, 0);
    while (dirIter.hasNext()) {
      dirIter.next();
      VersionInfo ver(dirIter.fileName());
      if (newestVersion < ver) {
        newestVersionPath = dirIter.fileInfo().absoluteFilePath();
        newestVersion = ver;
      }
    }
    path = newestVersionPath;
  }

  if (!path.isEmpty()) {
    QFile file(path.append("/user.config"));
    if (!file.open(QIODevice::ReadOnly)) {
      qWarning("failed to open NMMs \"user.config\"");
    }
    QDomDocument document("nmmconfig");
    bool res = document.setContent(&file);
    file.close();
    if (res) {
      QDomNode node = getNode(
                          getNode(document.documentElement(), "userSettings").toElement(),
                          "Nexus.Client.Properties.Settings");
      for (QDomElement iter = node.firstChildElement("setting");
           iter != node.lastChildElement("setting"); iter = iter.nextSiblingElement("setting")) {
        if (iter.attribute("name") == "ModFolder") {
          modFolder = digForSetting(iter);
        } else if (iter.attribute("name") == "InstallInfoFolder") {
          installLog = digForSetting(iter);
        }
      }
    }
  }

  // at this point we should usually have read our settings from the user.config
  // if not, ask the user
  QString problem;
  while (!testInstallLog(installLog, problem) || !testModFolder(modFolder, problem)) {
    NMMPathsDialog dialog(parentWidget());
    dialog.setInstallInfoPreset(installLog);
    dialog.setModFolderPreset(modFolder);
    dialog.setMessage(problem);
    if (dialog.exec() == QDialog::Accepted) {
      installLog = dialog.getInstallInfoFolder();
      modFolder = dialog.getModFolder();
    } else {
      return false;
    }
  }
  return true;
}


bool NMMImport::parseInstallLog(QDomDocument &document, const QString &installLog,
                                std::map<QString, ModInfo> &modsByKey) const
{
  QFile installFile(installLog);
  if (!installFile.open(QIODevice::ReadOnly)) {
    reportError(tr("\"%1\" not found").arg(installLog));
    return false;
  }

  {
    bool res = document.setContent(&installFile);
    installFile.close();
    if (!res) {
      reportError(tr("failed top open InstallLog.xml"));
      return false;
    }
  }

  if (!readMods(document, modsByKey)) {
    return false;
  }

  if (!readFiles(document, modsByKey)) {
    return false;
  }

  return true;
}

#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
Q_EXPORT_PLUGIN2(NMMImport, NMMImport)
#endif
