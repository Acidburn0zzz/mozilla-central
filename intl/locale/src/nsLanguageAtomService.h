/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsICharsetConverterManager.h"
#include "nsILanguageAtomService.h"
#include "nsIStringBundle.h"
#include "nsCRT.h"
#include "nsInterfaceHashtable.h"
#include "nsIAtom.h"

#define NS_LANGUAGEATOMSERVICE_CID \
  {0xB7C65853, 0x2996, 0x435E, {0x96, 0x54, 0xDC, 0xC1, 0x78, 0xAA, 0xB4, 0x8C}}

class nsLanguageAtomService : public nsILanguageAtomService
{
public:
  NS_DECL_ISUPPORTS

  // nsILanguageAtomService
  virtual NS_HIDDEN_(nsIAtom*)
    LookupLanguage(const nsACString &aLanguage, nsresult *aError);

  virtual NS_HIDDEN_(already_AddRefed<nsIAtom>)
    LookupCharSet(const char *aCharSet, nsresult *aError);

  virtual NS_HIDDEN_(nsIAtom*) GetLocaleLanguage(nsresult *aError);

  virtual NS_HIDDEN_(nsIAtom*) GetLanguageGroup(nsIAtom *aLanguage,
                                                nsresult *aError);

  nsLanguageAtomService() NS_HIDDEN;

private:
  NS_HIDDEN ~nsLanguageAtomService() { }

protected:
  NS_HIDDEN_(nsresult) InitLangGroupTable();

  nsCOMPtr<nsICharsetConverterManager> mCharSets;
  nsInterfaceHashtable<nsISupportsHashKey, nsIAtom> mLangToGroup;
  nsCOMPtr<nsIStringBundle> mLangGroups;
  nsCOMPtr<nsIAtom> mLocaleLanguage;
};
