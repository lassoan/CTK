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


//------------------------------------------------------------------------------
static ctkLogger logger("org.commontk.dicom.DICOMIndexer" );
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// ctkDICOMIndexerPrivate methods


//------------------------------------------------------------------------------
ctkDICOMIndexerPrivateWorker::ctkDICOMIndexerPrivateWorker(DICOMIndexingQueue* queue)
: RequestQueue(queue)
, RemainingRequestCount(0)
, CompletedRequestCount(0)
{
}

//------------------------------------------------------------------------------
ctkDICOMIndexerPrivateWorker::~ctkDICOMIndexerPrivateWorker()
{
  this->RequestQueue->setStopRequested(true);
}

//------------------------------------------------------------------------------
void ctkDICOMIndexerPrivateWorker::start()
{
  if (this->RequestQueue->setIndexing(true))
  {
    // it was already indexing, nothing to do
    return;
  }
  // Make a local copy to avoid the need of frequent locking
  this->RequestQueue->modifiedTimeForFilepath(this->ModifiedTimeForFilepath);
  this->CompletedRequestCount = 0;
  do 
  {
    DICOMIndexingQueue::IndexingRequest indexingRequest;
    this->RemainingRequestCount = this->RequestQueue->popIndexingRequest(indexingRequest);
    if (this->RemainingRequestCount == -1 || this->RequestQueue->isStopRequested())
    {
      // finished
      this->RequestQueue->setStopRequested(false);
      this->RequestQueue->setIndexing(false);
      emit indexingComplete();
      return;
    }
    this->processIndexingRequest(indexingRequest);
    this->CompletedRequestCount++;
  } while (true);
}

//------------------------------------------------------------------------------
void ctkDICOMIndexerPrivateWorker::processIndexingRequest(DICOMIndexingQueue::IndexingRequest& indexingRequest)
{
  QDir::Filters filters = QDir::Files;
  if (indexingRequest.includeHiddenFolders)
  {
    filters |= QDir::Hidden;
  }
  if (!indexingRequest.inputFolderPath.isEmpty())
  {
    QDirIterator it(indexingRequest.inputFolderPath, filters, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
      indexingRequest.inputFilesPath << it.next();
    }
  }

  QTime timeProbe;
  timeProbe.start();

  int currentFileIndex = 0;
  int lastReportedPercent = 0;
  foreach(const QString& filePath, indexingRequest.inputFilesPath)
  {
    int percent = int(100.0 * (this->CompletedRequestCount + double(currentFileIndex) / double(indexingRequest.inputFilesPath.size()))
      / double(this->CompletedRequestCount + this->RemainingRequestCount + 1));
    emit this->progress(percent);
    emit indexingFilePath(filePath);

    QDateTime fileModifiedTime = QFileInfo(filePath).lastModified();
    bool datasetAlreadyInDatabase = this->ModifiedTimeForFilepath.contains(filePath);
    if (datasetAlreadyInDatabase && this->ModifiedTimeForFilepath[filePath] >= fileModifiedTime)
    {
      logger.debug("File " + filePath + " already added.");
      continue;
    }
    this->ModifiedTimeForFilepath[filePath] = fileModifiedTime;

    ctkDICOMDatabase::IndexingResult indexingResult;
    indexingResult.dataset = QSharedPointer<ctkDICOMItem>(new ctkDICOMItem);
    indexingResult.dataset->InitializeFromFile(filePath);
    if (indexingResult.dataset->IsInitialized())
    {
      indexingResult.filePath = filePath;
      indexingResult.storeFile = indexingRequest.storeFile;
      indexingResult.overwriteExistingDataset = datasetAlreadyInDatabase;
      this->RequestQueue->pushIndexingResult(indexingResult);
    }
    else
    {
      logger.warn(QString("Could not read DICOM file:") + filePath);
    }

    currentFileIndex++;

    if (this->RequestQueue->isStopRequested())
    {
      break;
    }
  }

  float elapsedTimeInSeconds = timeProbe.elapsed() / 1000.0;
  qDebug() << QString("DICOM indexer has successfully processed %1 files [%2s]")
    .arg(currentFileIndex).arg(QString::number(elapsedTimeInSeconds, 'f', 2));
}


//------------------------------------------------------------------------------
// ctkDICOMIndexerPrivate methods

//------------------------------------------------------------------------------
ctkDICOMIndexerPrivate::ctkDICOMIndexerPrivate(ctkDICOMIndexer& o)
  : q_ptr(&o)
  , BackgroundIndexingDatabase(nullptr)
{
  ctkDICOMIndexerPrivateWorker* worker = new ctkDICOMIndexerPrivateWorker(&this->RequestQueue);
  worker->moveToThread(&this->WorkerThread);
  
  connect(&this->WorkerThread, &QThread::finished, worker, &QObject::deleteLater);
  connect(this, &ctkDICOMIndexerPrivate::startWorker, worker, &ctkDICOMIndexerPrivateWorker::start);

  // Progress report
  connect(worker, &ctkDICOMIndexerPrivateWorker::indexingFilePath, q_ptr, &ctkDICOMIndexer::progressDetail);
  connect(worker, &ctkDICOMIndexerPrivateWorker::progress, q_ptr, &ctkDICOMIndexer::progress);
  connect(worker, &ctkDICOMIndexerPrivateWorker::indexingComplete, this, &ctkDICOMIndexerPrivate::backgroundIndexingComplete);

  this->WorkerThread.start();
}

//------------------------------------------------------------------------------
ctkDICOMIndexerPrivate::~ctkDICOMIndexerPrivate()
{
  this->RequestQueue.setStopRequested(true);
  this->WorkerThread.quit();
  this->WorkerThread.wait();
}

//------------------------------------------------------------------------------
void ctkDICOMIndexerPrivate::pushIndexingRequest(ctkDICOMDatabase& database, const DICOMIndexingQueue::IndexingRequest& request)
{
  Q_Q(ctkDICOMIndexer);
  emit q->progressStep("Parsing DICOM files");
  // TODO: handle database switch
  this->BackgroundIndexingDatabase = &database;
  this->RequestQueue.pushIndexingRequest(request);
  if (!this->RequestQueue.isIndexing())
  {
    // Start background indexing
    QMap<QString, QDateTime> modifiedTimeForFilepath;
    database.allFilesModifiedTimes(modifiedTimeForFilepath);
    this->RequestQueue.setModifiedTimeForFilepath(modifiedTimeForFilepath);
    emit startWorker();
  }
}

//------------------------------------------------------------------------------
void ctkDICOMIndexerPrivate::backgroundIndexingComplete()
{
  Q_Q(ctkDICOMIndexer);
  QTime timeProbe;
  timeProbe.start();
  emit q->progressDetail("");
  emit q->progressStep("Updating database");
  QList<ctkDICOMDatabase::IndexingResult> indexingResults;
  this->RequestQueue.popAllIndexingResults(indexingResults);
  int patientsCount = this->BackgroundIndexingDatabase->patientsCount();
  int studiesCount = this->BackgroundIndexingDatabase->studiesCount();
  int seriesCount = this->BackgroundIndexingDatabase->seriesCount();
  int imagesCount = this->BackgroundIndexingDatabase->imagesCount();

  // Activate batch update
  emit q->updatingDatabase(true);

  this->BackgroundIndexingDatabase->insert(indexingResults);
/*  
  this->BackgroundIndexingDatabase->prepareInsert();
  foreach(const ctkDICOMDatabase::IndexingResult& indexingResult, indexingResults)
  {
    this->BackgroundIndexingDatabase->insert(indexingResult.filePath, *indexingResult.dataset.data(), indexingResult.storeFile, false);
  }
  */

  int patientsAdded = this->BackgroundIndexingDatabase->patientsCount() - patientsCount;
  int studiesAdded = this->BackgroundIndexingDatabase->studiesCount() - studiesCount;
  int seriesAdded = this->BackgroundIndexingDatabase->seriesCount() - seriesCount;
  int imagesAdded = this->BackgroundIndexingDatabase->imagesCount() - imagesCount;

  // Update displayed fields according to inserted DICOM datasets
  emit q->progressDetail("");
  emit q->progressStep("Updating database displayed fields");
  this->BackgroundIndexingDatabase->updateDisplayedFields();

  float elapsedTimeInSeconds = timeProbe.elapsed() / 1000.0;
  qDebug() << QString("DICOM indexer has successfully inserted %1 files [%2s]")
    .arg(indexingResults.count()).arg(QString::number(elapsedTimeInSeconds, 'f', 2));

  emit q->updatingDatabase(false);

  emit q->indexingComplete(patientsAdded, studiesAdded, seriesAdded, imagesAdded);
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// ctkDICOMIndexer methods

//------------------------------------------------------------------------------
ctkDICOMIndexer::ctkDICOMIndexer(QObject *parent)
  : d_ptr(new ctkDICOMIndexerPrivate(*this))
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
  Q_D(ctkDICOMIndexer);
  DICOMIndexingQueue::IndexingRequest request;
  request.inputFilesPath << filePath;
  request.includeHiddenFolders = false;
  request.storeFile = !destinationDirectoryName.isEmpty();
  d->pushIndexingRequest(database, request);
}

//------------------------------------------------------------------------------
void ctkDICOMIndexer::addDirectory(ctkDICOMDatabase& database,
                                   const QString& directoryName,
                                   const QString& destinationDirectoryName,
                                   bool includeHidden/*=true*/)
{
  Q_D(ctkDICOMIndexer);

  QStringList listOfFiles;
  QDir directory(directoryName);
  if (directory.exists("DICOMDIR"))
  {
    addDicomdir(database, directoryName, destinationDirectoryName);
  }
  else
  {
    DICOMIndexingQueue::IndexingRequest request;
    request.inputFolderPath = directoryName;
    request.includeHiddenFolders = includeHidden;
    request.storeFile = !destinationDirectoryName.isEmpty();
    d->pushIndexingRequest(database, request);
  }
}

//------------------------------------------------------------------------------
void ctkDICOMIndexer::addListOfFiles(ctkDICOMDatabase& database,
                                     const QStringList& listOfFiles,
                                     const QString& destinationDirectoryName)
{
  Q_D(ctkDICOMIndexer);
  DICOMIndexingQueue::IndexingRequest request;
  request.inputFilesPath = listOfFiles;
  request.includeHiddenFolders = false;
  request.storeFile = !destinationDirectoryName.isEmpty();
  d->pushIndexingRequest(database, request);
}

//------------------------------------------------------------------------------
bool ctkDICOMIndexer::addDicomdir(ctkDICOMDatabase& database,
                 const QString& directoryName,
                 const QString& destinationDirectoryName
                 )
{
  //Initialize dicomdir with directory path
  QString dcmFilePath = directoryName;
  dcmFilePath.append("/DICOMDIR");
  DcmDicomDir* dicomDir = new DcmDicomDir(dcmFilePath.toStdString().c_str());

  //Values to store records data at the moment only uid needed
  OFString patientsName, studyInstanceUID, seriesInstanceUID, sopInstanceUID, referencedFileName ;

  //Variables for progress operations
  QString instanceFilePath;
  QStringList listOfInstances;

  DcmDirectoryRecord* rootRecord = &(dicomDir->getRootRecord());
  DcmDirectoryRecord* patientRecord = NULL;
  DcmDirectoryRecord* studyRecord = NULL;
  DcmDirectoryRecord* seriesRecord = NULL;
  DcmDirectoryRecord* fileRecord = NULL;

  QTime timeProbe;
  timeProbe.start();

  /*Iterate over all records in dicomdir and setup path to the dataset of the filerecord
  then insert. the filerecord into the database.
  If any UID is missing the record and all of it's subelements won't be added to the database*/
  bool success = true;
  if(rootRecord != NULL)
  {
    while ((patientRecord = rootRecord->nextSub(patientRecord)) != NULL)
    {
      logger.debug( "Reading new Patient:" );
      if (patientRecord->findAndGetOFString(DCM_PatientName, patientsName).bad())
      {
        logger.warn( "DICOMDIR file at "+directoryName+" is invalid: patient name not found. All records belonging to this patient will be ignored.");
        success = false;
        continue;
      }
      logger.debug( "Patient's Name: " + QString(patientsName.c_str()) );
      while ((studyRecord = patientRecord->nextSub(studyRecord)) != NULL)
      {
        logger.debug( "Reading new Study:" );
        if (studyRecord->findAndGetOFString(DCM_StudyInstanceUID, studyInstanceUID).bad())
        {
          logger.warn( "DICOMDIR file at "+directoryName+" is invalid: study instance UID not found for patient "+ QString(patientsName.c_str())+". All records belonging to this study will be ignored.");
          success = false;
          continue;
        }
        logger.debug( "Study instance UID: " + QString(studyInstanceUID.c_str()) );

        while ((seriesRecord = studyRecord->nextSub(seriesRecord)) != NULL)
        {
          logger.debug( "Reading new Series:" );
          if (seriesRecord->findAndGetOFString(DCM_SeriesInstanceUID, seriesInstanceUID).bad())
          {
            logger.warn( "DICOMDIR file at "+directoryName+" is invalid: series instance UID not found for patient "+ QString(patientsName.c_str())+", study "+ QString(studyInstanceUID.c_str())+". All records belonging to this series will be ignored.");
            success = false;
            continue;
          }
          logger.debug( "Series instance UID: " + QString(seriesInstanceUID.c_str()) );

          while ((fileRecord = seriesRecord->nextSub(fileRecord)) != NULL)
          {
            if (fileRecord->findAndGetOFStringArray(DCM_ReferencedSOPInstanceUIDInFile, sopInstanceUID).bad()
              || fileRecord->findAndGetOFStringArray(DCM_ReferencedFileID,referencedFileName).bad())
            {
              logger.warn( "DICOMDIR file at "+directoryName+" is invalid: referenced SOP instance UID or file name is invalid for patient "
                + QString(patientsName.c_str())+", study "+ QString(studyInstanceUID.c_str())+", series "+ QString(seriesInstanceUID.c_str())+
                ". This file will be ignored.");
              success = false;
              continue;
            }

            //Get the filepath of the instance and insert it into a list
            instanceFilePath = directoryName;
            instanceFilePath.append("/");
            instanceFilePath.append(QString( referencedFileName.c_str() ));
            instanceFilePath.replace("\\","/");
            listOfInstances << instanceFilePath;
          }
        }
      }
    }
    float elapsedTimeInSeconds = timeProbe.elapsed() / 1000.0;
    qDebug()
        << QString("DICOM indexer has successfully processed DICOMDIR in %1 [%2s]")
           .arg(directoryName)
           .arg(QString::number(elapsedTimeInSeconds,'f', 2));
    this->addListOfFiles(database,listOfInstances,destinationDirectoryName);
  }
  return success;
}


//------------------------------------------------------------------------------
void ctkDICOMIndexer::waitForImportFinished(int msecTimeout /*=-1*/)
{
  QTimer timer;
  timer.setSingleShot(true);
  QEventLoop loop;
  connect(this, &ctkDICOMIndexer::indexingComplete, &loop, &QEventLoop::quit);
  if (msecTimeout >= 0)
  {
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(msecTimeout);
  }  
  loop.exec();
}

//----------------------------------------------------------------------------
void ctkDICOMIndexer::cancel()
{
  Q_D(ctkDICOMIndexer);
  d->RequestQueue.setStopRequested(true);
}
