/*=========================================================================

  Library:   CTK

  Copyright (c) Kitware Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

=========================================================================*/

// Qt includes
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QVariant>
#include <QDate>
#include <QStringList>
#include <QSet>
#include <QFile>
#include <QDirIterator>
#include <QFileInfo>
#include <QDebug>
#include <QPixmap>
#include <QtConcurrentRun>

// ctkDICOM includes
#include "ctkLogger.h"
#include "ctkDICOMIndexer.h"
#include "ctkDICOMIndexer_p.h"
#include "ctkDICOMDatabase.h"

// DCMTK includes
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/ofstd/ofcond.h>
#include <dcmtk/ofstd/ofstring.h>
#include <dcmtk/ofstd/ofstd.h>        /* for class OFStandard */
#include <dcmtk/dcmdata/dcddirif.h>   /* for class DicomDirInterface */
#include <dcmtk/dcmimgle/dcmimage.h>  /* for class DicomImage */
#include <dcmtk/dcmimage/diregist.h>  /* include support for color images */

class AddFileFunctor
{
public:
     AddFileFunctor(ctkDICOMIndexer* indexer, ctkDICOMDatabase& database,
                    const QString& destinationDirectoryName = "")
       : Indexer(indexer), Database(database), DestinationDirectoryName(destinationDirectoryName) { }

     bool operator()(const QString &filePath)
     {
         Indexer->addFile(Database,filePath,DestinationDirectoryName);
         return false; // make sure it is removed;
     }

     ctkDICOMIndexer* Indexer;
     ctkDICOMDatabase& Database;
     QString DestinationDirectoryName;

 };


//------------------------------------------------------------------------------
static ctkLogger logger("org.commontk.dicom.DICOMIndexer" );
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// ctkDICOMIndexerPrivate methods

//------------------------------------------------------------------------------
ctkDICOMIndexerPrivate::ctkDICOMIndexerPrivate(ctkDICOMIndexer& o) : q_ptr(&o), Canceled(false), CurrentPercentageProgress(-1)
{
  Q_Q(ctkDICOMIndexer);
  connect(&DirectoryImportWatcher,SIGNAL(progressValueChanged(int)),this,SLOT(OnProgress(int)));
  connect(&DirectoryImportWatcher,SIGNAL(finished()),q,SIGNAL(indexingComplete()));
  connect(&DirectoryImportWatcher,SIGNAL(canceled()),q,SIGNAL(indexingComplete()));
}

//------------------------------------------------------------------------------
ctkDICOMIndexerPrivate::~ctkDICOMIndexerPrivate()
{
  DirectoryImportWatcher.cancel();
  DirectoryImportWatcher.waitForFinished();

}

void ctkDICOMIndexerPrivate::OnProgress(int progress)
{
  Q_Q(ctkDICOMIndexer);

  int newPercentageProgress = ( 100 * DirectoryImportFuture.progressValue() ) / DirectoryImportFuture.progressMaximum();
  if (newPercentageProgress != CurrentPercentageProgress)
    {
      CurrentPercentageProgress = newPercentageProgress;
      emit q->progress(newPercentageProgress);
    }

}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// ctkDICOMIndexer methods

//------------------------------------------------------------------------------
ctkDICOMIndexer::ctkDICOMIndexer(QObject *parent):d_ptr(new ctkDICOMIndexerPrivate(*this))
{
  Q_UNUSED(parent);
}

//------------------------------------------------------------------------------
ctkDICOMIndexer::~ctkDICOMIndexer()
{
}

//------------------------------------------------------------------------------
void ctkDICOMIndexer::addFile(ctkDICOMDatabase& database,
                                   const QString filePath,
                                   const QString& destinationDirectoryName)
{
  std::cout << filePath.toStdString();
  if (!destinationDirectoryName.isEmpty())
  {
    logger.warn("Ignoring destinationDirectoryName parameter, just taking it as indication we should copy!");
  }

  emit indexingFilePath(filePath);

  database.insert(filePath, !destinationDirectoryName.isEmpty(), true);
}

//------------------------------------------------------------------------------
void ctkDICOMIndexer::addDirectory(ctkDICOMDatabase& ctkDICOMDatabase, 
                                   const QString& directoryName,
                                   const QString& destinationDirectoryName)
{
  Q_D(ctkDICOMIndexer);

  // currently it is not supported to have multiple
  // parallel directory imports so the second call blocks
  //
  d->DirectoryImportWatcher.waitForFinished();

  const std::string src_directory(directoryName.toStdString());

  OFList<OFString> originalDcmtkFileNames;
  OFList<OFString> dcmtkFileNames;
  OFStandard::searchDirectoryRecursively( QDir::toNativeSeparators(src_directory.c_str()).toAscii().data(), originalDcmtkFileNames, "", "");

  int totalNumberOfFiles = originalDcmtkFileNames.size();

  // hack to reverse list of filenames (not neccessary when image loading works correctly)
  for ( OFListIterator(OFString) iter = originalDcmtkFileNames.begin(); iter != originalDcmtkFileNames.end(); ++iter )
  {
    dcmtkFileNames.push_front( *iter );
  }

  OFListIterator(OFString) iter = dcmtkFileNames.begin();

  OFListIterator(OFString) last = dcmtkFileNames.end();

  if(iter == last) return;

  emit foundFilesToIndex(totalNumberOfFiles);

  /* iterate over all input filenames */
  int fileNumber = 0;
  int currentProgress = -1;
  d->Canceled = false;
  while (iter != last)
  {
    if (d->Canceled)
      {
      break;
      }
    emit indexingFileNumber(++fileNumber);
    int newProgress = ( fileNumber * 100 ) / totalNumberOfFiles;
    if (newProgress != currentProgress)
    {
      currentProgress = newProgress;
      emit progress( currentProgress );
    }
    QString filePath((*iter).c_str());
    d->FilesToIndex << filePath;
    ++iter;
  }
  d->DirectoryImportFuture = QtConcurrent::filter(d->FilesToIndex,AddFileFunctor(this,ctkDICOMDatabase,destinationDirectoryName));
  d->DirectoryImportWatcher.setFuture(d->DirectoryImportFuture);
}

//------------------------------------------------------------------------------
void ctkDICOMIndexer::refreshDatabase(ctkDICOMDatabase& dicomDatabase, const QString& directoryName)
{
  Q_UNUSED(dicomDatabase);
  Q_UNUSED(directoryName);
  /*
   * Probably this should go to the database class as well
   * Or we have to extend the interface to make possible what we do here
   * without using SQL directly
   

  /// get all filenames from the database
  QSqlQuery allFilesQuery(dicomDatabase.database());
  QStringList databaseFileNames;
  QStringList filesToRemove;
  this->loggedExec(allFilesQuery, "SELECT Filename from Images;");

  while (allFilesQuery.next())
    {
    QString fileName = allFilesQuery.value(0).toString();
    databaseFileNames.append(fileName);
    if (! QFile::exists(fileName) )
      {
      filesToRemove.append(fileName);
      }
    }

  QSet<QString> filesytemFiles;
  QDirIterator dirIt(directoryName);
  while (dirIt.hasNext())
    {
    filesytemFiles.insert(dirIt.next());
    }
  
  // TODO: it looks like this function was never finished...
  // 
  // I guess the next step is to remove all filesToRemove from the database
  // and also to add filesystemFiles into the database tables
  */ 
  }

//----------------------------------------------------------------------------
void ctkDICOMIndexer::cancel()
{
  Q_D(ctkDICOMIndexer);
  d->DirectoryImportWatcher.cancel();
}
