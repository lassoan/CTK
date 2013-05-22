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

#ifndef __ctkDICOMDisplayedFieldGeneratorAbstractRule_h
#define __ctkDICOMDisplayedFieldGeneratorAbstractRule_h

// Qt includes
#include <QMap>
#include <QStringList>

// DCMTK includes
#include <dcmtk/dcmdata/dcdeftag.h>

class ctkDICOMDatabase;

/// \ingroup DICOM_Core
///
/// Abstract base class for generating displayed fields from DICOM fields
class CTK_DICOM_CORE_EXPORT ctkDICOMDisplayedFieldGeneratorAbstractRule
{
public:

  enum EvaluationResult
  {
    RULE_ERROR,
    RULE_NOT_APPLICABLE,
    RULE_APPLIED_CONTINUE_PROCESSING,
    RULE_APPLIED_STOP_PROCESSING
  };
  
  virtual EvaluationResult evaluate(QStringList dicomFields, QMap<QString, QString> displayedFields, const ctkDICOMDatabase &db)=0;
  virtual QStringList getRequiredDICOMTags()=0;

  static QString dicomTagToString(DcmTagKey& tag)
  {    
    return QString("%1,%2").arg(tag.getGroup(),4,16,QLatin1Char('0')).arg(tag.getElement(),4,16,QLatin1Char('0'));
  }  

};

#endif
