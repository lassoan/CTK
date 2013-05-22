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

#ifndef __ctkDICOMDisplayedFieldGeneratorRadiotherapySeriesDescriptionRule_h
#define __ctkDICOMDisplayedFieldGeneratorRadiotherapySeriesDescriptionRule_h

// Qt includes
#include <QStringList>

#include "ctkDICOMDisplayedFieldGeneratorAbstractRule.h"

/// \ingroup DICOM_Core
///
/// Default rule for generating displayed fields from DICOM fields
class CTK_DICOM_CORE_EXPORT ctkDICOMDisplayedFieldGeneratorRadiotherapySeriesDescriptionRule : public ctkDICOMDisplayedFieldGeneratorAbstractRule
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

    requiredTags << dicomTagToString(DCM_Modality);
    
    requiredTags << dicomTagToString(DCM_RTPlanName);
    requiredTags << dicomTagToString(DCM_RTPlanLabel);    
    //requiredTags << dicomTagToString(DCM_RTPlanDate);
    //requiredTags << dicomTagToString(DCM_RTPlanTime);
    //requiredTags << dicomTagToString(DCM_RTPlanDescription);    
    
    requiredTags << dicomTagToString(DCM_StructureSetLabel);
    requiredTags << dicomTagToString(DCM_StructureSetName);
    //requiredTags << dicomTagToString(DCM_StructureSetDescription);
    //requiredTags << dicomTagToString(DCM_StructureSetDate);
    //requiredTags << dicomTagToString(DCM_StructureSetTime);
          
    requiredTags << dicomTagToString(DCM_RTImageLabel);
    requiredTags << dicomTagToString(DCM_RTImageName);
    requiredTags << dicomTagToString(DCM_RTImageDescription);
          
    return requiredTags;
  }

};

#endif
