/*=========================================================================

  Library:   CTK

  Copyright (c) PerkLab 2013

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

#ifndef __ctkDICOMDisplayedFieldGeneratorDefaultRule_h
#define __ctkDICOMDisplayedFieldGeneratorDefaultRule_h

// Qt includes
#include <QStringList>

#include "ctkDICOMDisplayedFieldGeneratorAbstractRule.h"

/// \ingroup DICOM_Core
///
/// Default rule for generating displayed fields from DICOM fields
class CTK_DICOM_CORE_EXPORT ctkDICOMDisplayedFieldGeneratorDefaultRule : public ctkDICOMDisplayedFieldGeneratorAbstractRule
{
public: 
  virtual EvaluationResult evaluate(QStringList dicomFields, QMap<QString, QString> displayedFields, const ctkDICOMDatabase &db)
  {
    // TODO: fill default patient, study, series tables
    // Patient:PatientID, Patient:PatientName
    return RULE_APPLIED_CONTINUE_PROCESSING;
  }

  virtual QStringList getRequiredDICOMTags()
  {
    QStringList requiredTags;

    requiredTags << dicomTagToString(DCM_SOPInstanceUID);

    requiredTags << dicomTagToString(DCM_PatientID);
    requiredTags << dicomTagToString(DCM_PatientName);
    requiredTags << dicomTagToString(DCM_PatientBirthDate);
    requiredTags << dicomTagToString(DCM_PatientBirthTime);
    requiredTags << dicomTagToString(DCM_PatientSex);
    requiredTags << dicomTagToString(DCM_PatientAge);
    requiredTags << dicomTagToString(DCM_PatientComments);

    requiredTags << dicomTagToString(DCM_StudyInstanceUID);
    requiredTags << dicomTagToString(DCM_StudyID);
    requiredTags << dicomTagToString(DCM_StudyDate);
    requiredTags << dicomTagToString(DCM_StudyTime);
    requiredTags << dicomTagToString(DCM_AccessionNumber);
    requiredTags << dicomTagToString(DCM_ModalitiesInStudy);
    requiredTags << dicomTagToString(DCM_InstitutionName);
    requiredTags << dicomTagToString(DCM_PerformingPhysicianName);
    requiredTags << dicomTagToString(DCM_ReferringPhysicianName);
    requiredTags << dicomTagToString(DCM_StudyDescription);

    requiredTags << dicomTagToString(DCM_SeriesInstanceUID);
    requiredTags << dicomTagToString(DCM_SeriesDate);
    requiredTags << dicomTagToString(DCM_SeriesTime);
    requiredTags << dicomTagToString(DCM_SeriesDescription);
    requiredTags << dicomTagToString(DCM_Modality);
    requiredTags << dicomTagToString(DCM_BodyPartExamined);
    requiredTags << dicomTagToString(DCM_FrameOfReferenceUID);
    requiredTags << dicomTagToString(DCM_ContrastBolusAgent);
    requiredTags << dicomTagToString(DCM_ScanningSequence);
    requiredTags << dicomTagToString(DCM_SeriesNumber);
    requiredTags << dicomTagToString(DCM_AcquisitionNumber);
    requiredTags << dicomTagToString(DCM_EchoNumbers);
    requiredTags << dicomTagToString(DCM_TemporalPositionIdentifier);   

    return requiredTags;
  }

};

#endif
