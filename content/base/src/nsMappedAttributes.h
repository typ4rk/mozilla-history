/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corporation
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMappedAttributes_h___
#define nsMappedAttributes_h___

#include "nsAttrAndChildArray.h"
#include "nsIHTMLContent.h"
#include "nsIStyleRule.h"

class nsIAtom;
class nsIHTMLStyleSheet;
class nsRuleWalker;

class nsMappedAttributes : public nsIStyleRule
{
public:
  nsMappedAttributes(nsIHTMLStyleSheet* aSheet,
                     nsMapRuleToAttributesFunc aMapRuleFunc);
  nsMappedAttributes(const nsMappedAttributes& aCopy);

  NS_DECL_ISUPPORTS

  nsresult SetAndTakeAttr(nsIAtom* aAttrName, nsAttrValue& aValue);
  nsresult GetAttribute(nsIAtom* aAttrName, nsHTMLValue& aValue) const;
  const nsAttrValue* GetAttr(nsIAtom* aAttrName) const;

  PRUint32 Count() const
  {
    return mAttrCount;
  }

  PRBool Equals(const nsMappedAttributes* aAttributes) const;
  PRUint32 HashValue() const;

  void AddUse();
  void ReleaseUse();

  void DropStyleSheetReference()
  {
    mSheet = nsnull;
  }
  void SetStyleSheet(nsIHTMLStyleSheet* aSheet);
  nsIHTMLStyleSheet* GetStyleSheet()
  {
    return mSheet;
  }

  const nsAttrName* NameAt(PRUint32 aPos) const
  {
    NS_ASSERTION(aPos < mAttrCount, "out-of-bounds");
    return &mAttrs[aPos].mName;
  }
  const nsAttrValue* AttrAt(PRUint32 aPos) const
  {
    NS_ASSERTION(aPos < mAttrCount, "out-of-bounds");
    return &mAttrs[aPos].mValue;
  }
  void RemoveAttrAt(PRUint32 aPos);
  const nsAttrName* GetExistingAttrNameFromQName(const nsACString& aName) const;
  PRInt32 IndexOfAttr(nsIAtom* aLocalName, PRInt32 aNamespaceID) const;
  

  // nsIStyleRule 
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);
#ifdef DEBUG
  NS_METHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

private:
  ~nsMappedAttributes();

  PRBool EnsureBufferSize(PRUint32 aSize);

  struct InternalAttr
  {
    nsAttrName mName;
    nsAttrValue mValue;
  };

  PRUint16 mAttrCount;
  PRUint16 mBufferSize;
  PRUint32 mUseCount;
  nsIHTMLStyleSheet* mSheet; //weak
  nsMapRuleToAttributesFunc mRuleMapper;
  InternalAttr* mAttrs;
};

#endif /* nsMappedAttributes_h___ */
