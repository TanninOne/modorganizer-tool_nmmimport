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
: m_Progress(NULL)
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
  return VersionInfo(0, 2, 0, VersionInfo::RELEASE_BETA);
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
  QCoreApplication::processEvents();
}


void updateProgressFile(LPCWSTR)
{
}

void report7ZipError(LPCWSTR errorMessage)
{
  reportError(QObject::tr("extraction error: %1").arg(ToQString(errorMessage)));
}


void NMMImport::unpackFiles(const QString &archiveFile, const QString &outputDirectory,
                            const std::set<QString> &extractFiles, Archive *archive) const
{
  if (!archive->open(ToWString(archiveFile).c_str(), new FunctionCallback<void, LPSTR>(&queryPassword))) {
    reportError(tr("failed to open archive \"%1\": %2").arg(archiveFile).arg(archive->getLastError()));
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
      if ((fileName.startsWith("Data/", Qt::CaseInsensitive)) ||
          (fileName.startsWith("Data\\", Qt::CaseInsensitive))) {
        fileName.remove(0, 5);
      }
      data[i]->setOutputFileName(ToWString(fileName).c_str());
    }
  }
  if (!archive->extract(ToWString(outputDirectory).c_str(),
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
  if (mod == NULL) {
    return NULL;
  }
  mod->setVersion(VersionInfo(info.version));

  std::tr1::match_results<std::string::const_iterator> result;
  std::string fileName = std::string(info.installFile.toUtf8().constData());
  if (std::tr1::regex_search(fileName, result, exp)) {
    std::string temp = result[3].str();
    mod->setNexusID(strtol(temp.c_str(), NULL, 10));
  } else {
    qWarning("no nexus id found in %s", qPrintable(info.installFile));
  }

  return mod;
}

NMMImport::EResult NMMImport::installMod(const ModInfo &modInfo, ModeDialog::InstallMode mode, IModInterface *mod,
                           const QString &modFolder) const
{
  bool incomplete = false;

  QDir dataDir(m_MOInfo->gameInfo().path() + "/data");
  QString virtualFolder = modFolder + "/VirtualModActivator";
  QStringList sourceFiles;
  QStringList destinationFiles;
  for (auto fileIter = modInfo.files.begin(); fileIter != modInfo.files.end(); ++fileIter) {
    if (fileIter->second) {
      QString sourcePath = QDir::fromNativeSeparators(fileIter->first);
      QString destinationPath;

      if (sourcePath.startsWith(virtualFolder, Qt::CaseInsensitive)) {
        int index = sourcePath.indexOf('/', virtualFolder.size() + 2);
        destinationPath = mod->absolutePath() + "/" + sourcePath.mid(index);
      } else {
        if (sourcePath.startsWith("Data/", Qt::CaseInsensitive)) {
          // path relative to skyrim base folder
          sourcePath.remove(0, 5);
        } else {
          qWarning("unrecognized file path: %s", qPrintable(sourcePath));
          incomplete = true;
          continue;
        }
        destinationPath = mod->absolutePath() + "/" + sourcePath;
        sourcePath = dataDir.absoluteFilePath(sourcePath);
      }

      sourceFiles.append(sourcePath);
      destinationFiles.append(destinationPath);
    } else {
      incomplete = true;
    }
  }

  bool error = false;

  if (mode == ModeDialog::MODE_MOVE) {
    error = !shellMove(sourceFiles, destinationFiles, parentWidget());
  } else {
    error = !shellCopy(sourceFiles, destinationFiles, parentWidget());
  }

  if (error) {
    reportError(tr("Problem importing \"%1\", please check if it imported correctly once this process completed: %2").arg(modInfo.name).arg(windowsErrorString(::GetLastError())));
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
  if (error) {
    return RES_FAILED;
  } else if (incomplete) {
    return RES_PARTIAL;
  } else {
    return RES_SUCCESS;
  }
}


void NMMImport::transferMods(const std::vector<std::pair<QString, ModInfo> > &modList, QDomDocument &document,
                             const QString &installLog, const QString &modFolder) const
{
  if (m_Progress == NULL) {
    throw MyException("nmm import plugin not correctly initialised.");
  }

  // query which mods to transfer
  ModSelectionDialog modsDialog(parentWidget());

  for (auto iter = modList.begin(); iter != modList.end(); ++iter) {
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
  std::vector<QString> enabledMods = modsDialog.getEnabledMods();
  m_Progress->setMaximum(enabledMods.size());
  m_Progress->setValue(0);
  m_Progress->setCancelButton(NULL);
  m_Progress->show();

  std::map<QString, ModInfo> modsByKey;
  for (auto iter = modList.begin(); iter != modList.end(); ++iter) {
    modsByKey[iter->first] = iter->second;
  }

  QStringList incompleteMods;

  bool error = false;
  for (auto iter = enabledMods.begin(); iter != enabledMods.end() && !error; ++iter) {
    auto modIter = modsByKey.find(*iter);
    if (modIter == modsByKey.end()) {
      reportError(tr("invalid mod key \"%1\". This is a bug. The mod will not be transfered").arg(*iter));
      continue;
    }

    QString modName = modIter->second.name;
    if (!fixDirectoryName(modName)) {
      modName.clear();
    }
    m_Progress->setLabelText(modName);
    bool ok = true;
    while (modName.isEmpty() || (m_MOInfo->getMod(modName) != NULL)) {
      modName = QInputDialog::getText(parentWidget(), tr("Mod exists!"),
          tr("A mod with this name already exists or the name is invalid, please enter a new name or press "
             "\"Cancel\" to skip import of this mod."), QLineEdit::Normal, modName, &ok);
      fixDirectoryName(modName);
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
    if (mod == NULL) {
      return;
    }

    std::set<QString> extractFiles;
    extractFiles.insert("data\\fomod\\info.xml");
    extractFiles.insert("fomod\\info.xml");
    unpackFiles(QDir::toNativeSeparators(modFolder + "/cache/" + modIter->second.installFile + ".zip"),
                QDir::toNativeSeparators(QDir::tempPath()),
                extractFiles, archive);

    QString xmlPath = QDir::tempPath() + "/fomod/info.xml";
    QFile infoXML(xmlPath);
    if (infoXML.open(QIODevice::ReadOnly)) {
      QDomDocument document("fomod");
      if (document.setContent(&infoXML)) {
        QDomElement tlEle = document.documentElement();

        QString nexusID = getTextNodeValue(tlEle, "Id");
        QString latestVer = getTextNodeValue(tlEle, "LastKnownVersion");
        QString endorsedString = getTextNodeValue(tlEle, "IsEndorsed");
        QString categoryId = getTextNodeValue(tlEle, "CategoryId", true);

        mod->setNexusID(nexusID.toInt());
        mod->setNewestVersion(latestVer);
        mod->setIsEndorsed(endorsedString.compare("true", Qt::CaseInsensitive));
        if (!categoryId.isEmpty()) {
          mod->addNexusCategory(categoryId.toInt());
        }
      } else {
        qDebug("failed to parse %s", qPrintable(xmlPath));
      }
      infoXML.remove();
    } else {
      qDebug("failed to open: %s", qPrintable(xmlPath));
    }
    // this may fail if the directory isn't empty otherwise. That's ok because it
    // means the directory wasn't empty before
    QDir().remove(QDir::tempPath() + "/fomod");

    EResult res = installMod(modIter->second, modeDialog.getMode(), mod, modFolder);
    if (res != RES_FAILED) {
      if ((modeDialog.getMode() == ModeDialog::MODE_COPYDELETE) || ( modeDialog.getMode() == ModeDialog::MODE_MOVE)) {
        removeModFromInstallLog(document, *iter);
      }
      if (res == RES_PARTIAL) {
        incompleteMods.append(modName);
      }
    } else {
      error = true;
    }

    QString readmeArchive = QDir::toNativeSeparators(modFolder + "/ReadMe/" + modIter->second.installFile);
    if (QFile::exists(readmeArchive)) {
      unpackFiles(readmeArchive,
                  QDir::toNativeSeparators(mod->absolutePath() + "/readmes"),
                  std::set<QString>(), archive);
    }

    m_Progress->setValue(m_Progress->value() + 1);
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
  m_Progress->hide();

  if (incompleteMods.size() > 0) {
    QMessageBox::information(parentWidget(), tr("Incomplete Import"),
      tr("Some mods were only imported partially because some of their files were overwritten during installation of other mods."
         "Everything should work fine as long as you don't deactivate the mod that did overwrite them."
         "It's suggested you reinstall these mods soon-ish:") + "<ul><li>" + incompleteMods.join("</li><li>") + "</li></ul>");
  }
}

void NMMImport::setParentWidget(QWidget *widget)
{
  IPluginTool::setParentWidget(widget);
  if (m_Progress != NULL) {
    m_Progress->deleteLater();
  }
  m_Progress = new QProgressDialog(widget);
}


void NMMImport::display() const
{
  QString installLog;
  QString modFolder;
  if (!determineNMMFolders(installLog, modFolder)) {
    return;
  }

  installLog.append("/InstallLog.xml");
  std::vector<std::pair<QString, ModInfo>> modList;
  QDomDocument document("InstallLog");

  if (!parseInstallLog(document, installLog, modList)) {
    return;
  }

  if (!QFile::exists(modFolder + "/VirtualModActivator/VirtualModConfig.xml")) {
    if (QMessageBox::warning(parentWidget(), tr("Pre-0.5 NMM"),
          tr("When importing from NMM versions before 0.5 MO can restore only the files installed on disc. This means "
             "files that exist in multiple mods will be imported into only one (the one installed last)."),
          QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel) {
      return;
    }
  } else {
    if (QMessageBox::warning(parentWidget(), tr("Post-0.5 NMM"),
          tr("At the time this plugin was written, NMM 0.5 was in alpha state and its very possible it's not compatible "
             "with the Beta/Final version."),
          QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel) {
      return;
    }
  }

  if (modList.size() > 1) {
    transferMods(modList, document, installLog, modFolder);
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


QDomNode NMMImport::getNode(const QDomElement &parent, const QString &name, bool mayBeEmpty)
{
  if (parent.isNull()) {
    throw MyException(tr("Element missing."));
  }
  QDomNodeList nodes = parent.elementsByTagName(name);
  if (nodes.count() == 0) {
    if (mayBeEmpty) {
      return QDomNode();
    } else {
      throw MyException(tr("Section \"%1\" missing.").arg(name));
    }
  } else if (nodes.count() > 1) {
    throw MyException(tr("Multiple sections \"%1\", expected only one.").arg(name));
  }
  return nodes.at(0);
}


QString NMMImport::getTextNodeValue(const QDomElement &parent, const QString &tag, bool mayBeEmpty)
{
  QDomNode textNode = getNode(parent, tag, mayBeEmpty);
  if (textNode.isNull()) {
    if (mayBeEmpty) {
      return QString();
    } else {
      throw MyException(tr("Expected tag \"%1\" below \"%2\"").arg(tag).arg(parent.tagName()));
    }
  }
  QDomNodeList childNodes = textNode.childNodes();
  if (childNodes.count() != 1) {
    return QString();
  }

  QDomText textContent = childNodes.at(0).toText();
  if (textContent.isNull()) {
    if (mayBeEmpty) {
      return QString();
    } else {
      throw MyException(tr("no text in \"%1\" found!").arg(tag));
    }
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


bool NMMImport::readMods(const QDomDocument &document, std::vector<std::pair<QString, ModInfo>> &modKeyList) const
{
  try {
    QDomNode modList = getNode(document.documentElement(), "modList");
    for (QDomElement ele = modList.firstChildElement(); !ele.isNull(); ele = ele.nextSiblingElement()) {
      QString key = ele.attribute("key");

      ModInfo info;
      info.name = getTextNodeValue(ele, "name");
      info.version = getTextNodeValue(ele, "version");
      info.installFile = ele.attribute("path");


      modKeyList.push_back(std::make_pair(key, info));
    }
    return true;
  } catch (const MyException &e) {
    reportError(tr("failed to parse \"modList\"-section of InstallLog.xml: %1").arg(e.what()));
    return false;
  }
}


bool NMMImport::readFiles(const QDomDocument &document, std::vector<std::pair<QString, ModInfo>> &modList) const
{
  try {
    // create lookup map
    std::map<QString, std::vector<std::pair<QString, ModInfo>>::iterator> modsByKey;
    for (auto iter = modList.begin(); iter != modList.end(); ++iter) {
      modsByKey[iter->first] = iter;
    }

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
        auto iter = modsByKey.find(key);
        if (iter == modsByKey.end()) {
          qWarning("NMM Importer: data file \"%s\" references undeclared mod (key \"%s\")", qPrintable(path), qPrintable(key));
        }
        // ASSUMPTION: mod is the primary source if it's the last in the list
        iter->second->second.files.push_back(std::make_pair(path, sourceEle == mods.lastChildElement()));
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
          modFolder = QDir::fromNativeSeparators(digForSetting(iter));
        } else if (iter.attribute("name") == "InstallInfoFolder") {
          installLog = QDir::fromNativeSeparators(digForSetting(iter));
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
                                std::vector<std::pair<QString, ModInfo>> &modList) const
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

  if (!readMods(document, modList)) {
    return false;
  }

  if (!readFiles(document, modList)) {
    return false;
  }

  return true;
}

#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
Q_EXPORT_PLUGIN2(NMMImport, NMMImport)
#endif
