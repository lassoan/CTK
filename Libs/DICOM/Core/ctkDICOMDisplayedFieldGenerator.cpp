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

// Qt includes
#include <QStringList>

// ctkDICOM includes
#include "ctkLogger.h"
#include "ctkDICOMDisplayedFieldGenerator.h"
#include "ctkDICOMDisplayedFieldGenerator_p.h"

#include "ctkDICOMDisplayedFieldGeneratorDefaultRule.h"
#include "ctkDICOMDisplayedFieldGeneratorRadiotherapySeriesDescriptionRule.h"

//------------------------------------------------------------------------------
static ctkLogger logger("org.commontk.dicom.DICOMDisplayedFieldGenerator" );
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// ctkDICOMDisplayedFieldGeneratorPrivate methods

//------------------------------------------------------------------------------
ctkDICOMDisplayedFieldGeneratorPrivate::ctkDICOMDisplayedFieldGeneratorPrivate(ctkDICOMDisplayedFieldGenerator& o) : q_ptr(&o)
{
  // register commonly used rules
  AllRules.prepend(new ctkDICOMDisplayedFieldGeneratorDefaultRule);
  AllRules.prepend(new ctkDICOMDisplayedFieldGeneratorRadiotherapySeriesDescriptionRule);
}

//------------------------------------------------------------------------------
ctkDICOMDisplayedFieldGeneratorPrivate::~ctkDICOMDisplayedFieldGeneratorPrivate()
{
  foreach(ctkDICOMDisplayedFieldGeneratorAbstractRule* rule, AllRules)
  {
    delete rule;
  }
  AllRules.clear();
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// ctkDICOMDisplayedFieldGenerator methods

//------------------------------------------------------------------------------
ctkDICOMDisplayedFieldGenerator::ctkDICOMDisplayedFieldGenerator(QObject *parent):d_ptr(new ctkDICOMDisplayedFieldGeneratorPrivate(*this))
{
  Q_UNUSED(parent);
}

//------------------------------------------------------------------------------
ctkDICOMDisplayedFieldGenerator::~ctkDICOMDisplayedFieldGenerator()
{
}

//------------------------------------------------------------------------------
QStringList ctkDICOMDisplayedFieldGenerator::getRequiredTags()
{
  Q_D(ctkDICOMDisplayedFieldGenerator);

  QStringList requiredTags;
  foreach(ctkDICOMDisplayedFieldGeneratorAbstractRule* rule, d->AllRules)
  {
    requiredTags << rule->getRequiredDICOMTags();
  }

  // TODO: remove duplicates from requiredTags (maybe also sort)
  return requiredTags;
}
