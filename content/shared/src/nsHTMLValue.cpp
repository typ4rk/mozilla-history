/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"
#include "nsHTMLValue.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsMemory.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsUnitConversion.h"
#include "prprf.h"

nsHTMLValue::nsHTMLValue(nsHTMLUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION(GetUnitClass() == HTMLUNIT_NOSTORE, "not a valueless unit");
  if (GetUnitClass() != HTMLUNIT_NOSTORE) {
    mUnit = eHTMLUnit_Null;
  }
  mValue.mString = nsnull;
}

nsHTMLValue::nsHTMLValue(PRInt32 aValue, nsHTMLUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION(GetUnitClass() == HTMLUNIT_INTEGER ||
               GetUnitClass() == HTMLUNIT_PIXEL, "unit not an integer unit");
  if (GetUnitClass() == HTMLUNIT_INTEGER ||
      GetUnitClass() == HTMLUNIT_PIXEL) {
    mValue.mInt = aValue;
  } else {
    mUnit = eHTMLUnit_Null;
    mValue.mString = nsnull;
  }
}

nsHTMLValue::nsHTMLValue(float aValue)
  : mUnit(eHTMLUnit_Percent)
{
  mValue.mFloat = aValue;
}

nsHTMLValue::nsHTMLValue(const nsAString& aValue, nsHTMLUnit aUnit)
  : mUnit(aUnit)
{
  SetStringValueInternal(aValue, aUnit);
}

nsHTMLValue::nsHTMLValue(nsISupports* aValue)
  : mUnit(eHTMLUnit_ISupports)
{
  mValue.mISupports = aValue;
  NS_IF_ADDREF(mValue.mISupports);
}

nsHTMLValue::nsHTMLValue(nscolor aValue)
  : mUnit(eHTMLUnit_Color)
{
  mValue.mColor = aValue;
}

nsHTMLValue::nsHTMLValue(nsCOMArray<nsIAtom>* aArray)
  : mUnit(eHTMLUnit_AtomArray)
{
  mValue.mAtomArray = aArray;
}

nsHTMLValue::nsHTMLValue(const nsHTMLValue& aCopy)
{
  InitializeFrom(aCopy);
}

nsHTMLValue::~nsHTMLValue(void)
{
  Reset();
}

nsHTMLValue& nsHTMLValue::operator=(const nsHTMLValue& aCopy)
{
  Reset();
  InitializeFrom(aCopy);
  return *this;
}

PRBool nsHTMLValue::operator==(const nsHTMLValue& aOther) const
{
  if (mUnit != aOther.mUnit) {
    return PR_FALSE;
  }
  // Call GetUnitClass() so that we turn StringWithLength into String
  PRUint32 unitClass = GetUnitClass();
  switch (unitClass) {
    case HTMLUNIT_NOSTORE:
      return PR_TRUE;

    case HTMLUNIT_STRING:
      if (mValue.mString && aOther.mValue.mString) {
        return GetDependentString().Equals(aOther.GetDependentString(),
                                           nsCaseInsensitiveStringComparator());
      }
      // One of them is null.  An == check will see if they are both null.
      return mValue.mString == aOther.mValue.mString;

    case HTMLUNIT_INTEGER:
    case HTMLUNIT_PIXEL:
      return mValue.mInt == aOther.mValue.mInt;

    case HTMLUNIT_COLOR:
      return mValue.mColor == aOther.mValue.mColor;

    case HTMLUNIT_ISUPPORTS:
      return mValue.mISupports == aOther.mValue.mISupports;

    case HTMLUNIT_PERCENT:
      return mValue.mFloat == aOther.mValue.mFloat;

    case HTMLUNIT_ATOMARRAY:
    {
      // Currently this isn't called (since atomarrays are never the value of
      // a mapped attribute) so it doesn't matter that it's slow.
      // It would be a lot simpler/faster if it was case sensitive though

      PRInt32 count = mValue.mAtomArray->Count();
      if (count != aOther.mValue.mAtomArray->Count()) {
        return PR_FALSE;
      }

      PRInt32 i;
      for (i = 0; i < count; ++i) {
        const char *class1;
        mValue.mAtomArray->ObjectAt(i)->GetUTF8String(&class1);
        PRInt32 j;
        for (j = 0; j < count; ++j) {
          const char* class2;
          aOther.mValue.mAtomArray->ObjectAt(j)->GetUTF8String(&class2);
          if (nsCRT::strcasecmp(class1, class2) == 0) {
            break;
          }
        }
        if (j == count) {
          return PR_FALSE;
        }
      }
      return PR_TRUE;
    }

    default:
      NS_WARNING("Unknown unit");
      return PR_TRUE;
  }
}

PRUint32 nsHTMLValue::HashValue(void) const
{
  PRUint32 retval;
  if (GetUnitClass() == HTMLUNIT_STRING) {
    retval = mValue.mString ? nsCheapStringBufferUtils::HashCode(mValue.mString)
                            : 0;
  } else if (mUnit == eHTMLUnit_AtomArray) {
    retval = 0;
    PRInt32 i, count = mValue.mAtomArray->Count();
    for (i = 0; i < count; ++i) {
      retval ^= NS_PTR_TO_INT32(mValue.mAtomArray->ObjectAt(i));
    }
  } else {
    retval = mValue.mInt;
  }
  return retval ^ PRUint32(mUnit);
}


void nsHTMLValue::Reset(void)
{
  if (GetUnitClass() == HTMLUNIT_STRING) {
    if (mValue.mString) {
      nsCheapStringBufferUtils::Free(mValue.mString);
    }
  }
  else if (mUnit == eHTMLUnit_ISupports) {
    NS_IF_RELEASE(mValue.mISupports);
  }
  else if (mUnit == eHTMLUnit_AtomArray) {
    delete mValue.mAtomArray;
  }
  mUnit = eHTMLUnit_Null;
  mValue.mString = nsnull;
}


void nsHTMLValue::SetIntValue(PRInt32 aValue, nsHTMLUnit aUnit)
{
  Reset();
  mUnit = aUnit;
  NS_ASSERTION(GetUnitClass() == HTMLUNIT_INTEGER, "not an int value");
  if (GetUnitClass() == HTMLUNIT_INTEGER) {
    mValue.mInt = aValue;
  } else {
    mUnit = eHTMLUnit_Null;
  }
}

void nsHTMLValue::SetPixelValue(PRInt32 aValue)
{
  Reset();
  mUnit = eHTMLUnit_Pixel;
  mValue.mInt = aValue;
}

void nsHTMLValue::SetPercentValue(float aValue)
{
  Reset();
  mUnit = eHTMLUnit_Percent;
  mValue.mFloat = aValue;
}

void nsHTMLValue::SetStringValueInternal(const nsAString& aValue,
                                         nsHTMLUnit aUnit)
{
  NS_ASSERTION(GetUnitClass() == HTMLUNIT_STRING, "unit not a string unit!");
  if (GetUnitClass() == HTMLUNIT_STRING) {
    // Remember the length of the string if necessary
    if (aValue.IsEmpty()) {
      mValue.mString = nsnull;
    } else {
      nsCheapStringBufferUtils::CopyToBuffer(mValue.mString, aValue);
    }
  } else {
    mUnit = eHTMLUnit_Null;
    mValue.mString = nsnull;
  }
}

void nsHTMLValue::SetStringValue(const nsAString& aValue,
                                 nsHTMLUnit aUnit)
{
  Reset();
  mUnit = aUnit;
  SetStringValueInternal(aValue, aUnit);
}

void nsHTMLValue::SetISupportsValue(nsISupports* aValue)
{
  Reset();
  mUnit = eHTMLUnit_ISupports;
  mValue.mISupports = aValue;
  NS_IF_ADDREF(mValue.mISupports);
}

void nsHTMLValue::SetColorValue(nscolor aValue)
{
  Reset();
  mUnit = eHTMLUnit_Color;
  mValue.mColor = aValue;
}

void nsHTMLValue::SetEmptyValue(void)
{
  Reset();
  mUnit = eHTMLUnit_Empty;
}

void
nsHTMLValue::InitializeFrom(const nsHTMLValue& aCopy)
{
  mUnit = aCopy.mUnit;
  switch (GetUnitClass()) {
    case HTMLUNIT_NOSTORE:
      mValue.mString = nsnull;
      break;

    case HTMLUNIT_STRING:
      if (aCopy.mValue.mString) {
        nsCheapStringBufferUtils::Clone(mValue.mString, aCopy.mValue.mString);
      } else {
        mValue.mString = nsnull;
      }
      break;

    case HTMLUNIT_INTEGER:
    case HTMLUNIT_PIXEL:
      mValue.mInt = aCopy.mValue.mInt;
      break;

    case HTMLUNIT_COLOR:
      mValue.mColor = aCopy.mValue.mColor;
      break;

    case HTMLUNIT_ISUPPORTS:
      mValue.mISupports = aCopy.mValue.mISupports;
      NS_IF_ADDREF(mValue.mISupports);
      break;

    case HTMLUNIT_PERCENT:
      mValue.mFloat = aCopy.mValue.mFloat;
      break;

    case HTMLUNIT_ATOMARRAY:
      mValue.mAtomArray = new nsCOMArray<nsIAtom>(*aCopy.mValue.mAtomArray);
      if (!mValue.mAtomArray) {
        mUnit = eHTMLUnit_Null;
      }
      break;

    default:
      NS_ERROR("Unknown HTMLValue type!");
  }
}


//
// Parsing methods
//

PRBool
nsHTMLValue::ParseEnumValue(const nsAString& aValue,
                            const EnumTable* aTable,
                            PRBool aCaseSensitive)
{
  nsAutoString val(aValue);
  while (aTable->tag) {
    if (aCaseSensitive ? val.EqualsWithConversion(aTable->tag) :
                         val.EqualsIgnoreCase(aTable->tag)) {
      SetIntValue(aTable->value, eHTMLUnit_Enumerated);
      return PR_TRUE;
    }
    aTable++;
  }
  return PR_FALSE;
}

PRBool
nsHTMLValue::EnumValueToString(const EnumTable* aTable,
                               nsAString& aResult) const
{
  if (GetUnit() == eHTMLUnit_Enumerated) {
    PRInt32 v = GetIntValue();
    while (aTable->tag) {
      if (aTable->value == v) {
        CopyASCIItoUCS2(nsDependentCString(aTable->tag), aResult);

        return PR_TRUE;
      }
      aTable++;
    }
  }
  aResult.Truncate();
  return PR_FALSE;
}

/* used to parse attribute values that could be either:
 *   integer  (n),
 *   percent  (n%),
 *   or proportional (n*)
 */
PRBool
nsHTMLValue::ParseSpecialIntValue(const nsAString& aString,
                                  nsHTMLUnit aDefaultUnit,
                                  PRBool aCanBePercent,
                                  PRBool aCanBeProportional)
{
  nsAutoString tmp(aString);
  PRInt32 ec;
  PRInt32 val = tmp.ToInteger(&ec);

  if (NS_SUCCEEDED(ec)) {
    if (val < 0) {
      val = 0;
    }
    // % (percent) (XXX RFindChar means that 5%x will be parsed!)
    if (aCanBePercent && tmp.RFindChar('%') >= 0) {
      if (val > 100) {
        val = 100;
      }
      SetPercentValue(float(val)/100.0f);
      return PR_TRUE;
    }
      
    // * (proportional) (XXX RFindChar means that 5*x will be parsed!)
    if (aCanBeProportional && tmp.RFindChar('*') >= 0) {
      SetIntValue(val, eHTMLUnit_Proportional);
      return PR_TRUE;
    }

    // Straight number is interpreted with the default unit
    if (aDefaultUnit == eHTMLUnit_Pixel) {
      SetPixelValue(val);
    } else {
      SetIntValue(val, aDefaultUnit);
    }
    return PR_TRUE;
  }

  // Even if the integer could not be parsed, it might just be "*"
  tmp.CompressWhitespace(PR_TRUE, PR_TRUE);
  if (tmp.Length() == 1 && tmp.Last() == '*') {
    // special case: HTML spec says a value '*' == '1*'
    // see http://www.w3.org/TR/html4/types.html#type-multi-length
    // b=29061
    SetIntValue(1, eHTMLUnit_Proportional);
    return PR_TRUE;
  }
  
  return PR_FALSE;
}

PRBool
nsHTMLValue::ToString(nsAString& aResult) const
{
  nsAutoString intStr;
  aResult.Truncate();

  switch (GetUnit()) {
    case eHTMLUnit_Integer:
    case eHTMLUnit_Proportional:
      intStr.AppendInt(GetIntValue());
      aResult.Append(intStr);
      if (GetUnit() == eHTMLUnit_Proportional) {
        aResult.Append(PRUnichar('*'));
      }
      return PR_TRUE;

    case eHTMLUnit_Pixel:
      intStr.AppendInt(GetPixelValue());
      aResult.Append(intStr);
      return PR_TRUE;

    case eHTMLUnit_Percent:
    {
      float percentVal = GetPercentValue() * 100.0f;
      intStr.AppendInt(NSToCoordRoundExclusive(percentVal));
      aResult.Append(intStr);
      aResult.Append(PRUnichar('%'));
      return PR_TRUE;
    }
    case eHTMLUnit_Color:
    {
      nscolor v = GetColorValue();
      char buf[10];
      PR_snprintf(buf, sizeof(buf), "#%02x%02x%02x",
                  NS_GET_R(v), NS_GET_G(v), NS_GET_B(v));
      AppendASCIItoUTF16(buf, aResult);
      return PR_TRUE;
    }
    case eHTMLUnit_ColorName:
    case eHTMLUnit_String:
      GetStringValue(aResult);
      return PR_TRUE;
    case eHTMLUnit_AtomArray:
    {
      PRInt32 count = mValue.mAtomArray->Count();
      if (count) {
        mValue.mAtomArray->ObjectAt(0)->ToString(aResult);
        nsAutoString tmp;
        PRInt32 i;
        for (i = 1; i < count; ++i) {
          mValue.mAtomArray->ObjectAt(i)->ToString(tmp);
          aResult.Append(NS_LITERAL_STRING(" ") + tmp);
        }
      }
      return PR_TRUE;
    }
    default:
      return PR_FALSE;
  }
}

PRBool
nsHTMLValue::ParseIntWithBounds(const nsAString& aString,
                                nsHTMLUnit aDefaultUnit,
                                PRInt32 aMin, PRInt32 aMax)
{
  nsAutoString str(aString);
  PRInt32 ec;
  PRInt32 val = str.ToInteger(&ec);
  if (NS_SUCCEEDED(ec)) {
    val = PR_MAX(val, aMin);
    val = PR_MIN(val, aMax);
    if (aDefaultUnit == eHTMLUnit_Pixel) {
      SetPixelValue(val);
    } else {
      SetIntValue(val, aDefaultUnit);
    }
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
nsHTMLValue::ParseColor(const nsAString& aString, nsIDocument* aDocument)
{
  if (aString.IsEmpty()) {
    return PR_FALSE;
  }

  // Previously we did a complicated algorithm to strip leading and trailing
  // whitespace; now we just use CompressWhitespace like everyone else.
  // Since all color values are just one word, this is ok.

  nsAutoString colorStr(aString);
  colorStr.CompressWhitespace(PR_TRUE, PR_TRUE);
  if (colorStr.IsEmpty()) {
    return PR_FALSE;
  }

  nscolor color;

  // No color names begin with a '#', but numerical colors do so
  // it is a very common first char
  if ((colorStr.CharAt(0) != '#') && NS_ColorNameToRGB(colorStr, &color)) {
    SetStringValue(colorStr, eHTMLUnit_ColorName);
    return PR_TRUE;
  }

  // Check if we are in compatibility mode
  PRBool inNavQuirksMode;
  {
    nsCOMPtr<nsIHTMLDocument> doc(do_QueryInterface(aDocument));
    if (doc) {
      nsCompatibility mode;
      doc->GetCompatibilityMode(mode);
      inNavQuirksMode = (mode == eCompatibility_NavQuirks);
    } else {
      inNavQuirksMode = PR_FALSE;
    }
  }
  if (!inNavQuirksMode) {
    if (colorStr.CharAt(0) == '#') {
      colorStr.Cut(0, 1);
      if (NS_HexToRGB(colorStr, &color)) {
        SetColorValue(color);
        return PR_TRUE;
      }
    }
  }
  else {
    if (NS_LooseHexToRGB(colorStr, &color)) { 
      SetColorValue(color);
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}
