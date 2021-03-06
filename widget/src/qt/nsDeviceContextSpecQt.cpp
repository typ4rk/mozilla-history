/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Ken Herron <kherron+mozilla@fmailbox.com>
 *   Julien Lafon <julien.lafon@gmail.com>
 *   Michael Ventnor <m.ventnor@gmail.com>
 *   Florian Hänel <heeen@gmx.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


#include <QTemporaryFile>
#include <QPrinterInfo>

#define SET_PRINTER_FEATURES_VIA_PREFS 1
#define PRINTERFEATURES_PREF "print.tmp.printerfeatures"

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG 1 /* Allow logging in the release build */
#endif /* MOZ_LOGGING */
#include "prlog.h"

#include "plstr.h"

#include "nsDeviceContextSpecQt.h"

#include "prenv.h" /* for PR_GetEnv */

#include "nsReadableUtils.h"
#include "nsStringEnumerator.h"
#include "nsIServiceManager.h"
#include "nsPrintSettingsQt.h"
#include "nsIFileStreams.h"
#include "nsILocalFile.h"
#include "nsTArray.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "gfxPDFSurface.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* DeviceContextSpecQtLM =
    PR_NewLogModule("DeviceContextSpecQt");
#endif /* PR_LOGGING */
/* Macro to make lines shorter */
#define DO_PR_DEBUG_LOG(x) PR_LOG(DeviceContextSpecQtLM, PR_LOG_DEBUG, x)

nsDeviceContextSpecQt::nsDeviceContextSpecQt()
{
    DO_PR_DEBUG_LOG(("nsDeviceContextSpecQt::nsDeviceContextSpecQt()\n"));
}

nsDeviceContextSpecQt::~nsDeviceContextSpecQt()
{
    DO_PR_DEBUG_LOG(("nsDeviceContextSpecQt::~nsDeviceContextSpecQt()\n"));
}

NS_IMPL_ISUPPORTS1(nsDeviceContextSpecQt,
        nsIDeviceContextSpec)

NS_IMETHODIMP nsDeviceContextSpecQt::GetSurfaceForPrinter(
        gfxASurface** aSurface)
{
    NS_ENSURE_ARG_POINTER(aSurface);
    *aSurface = nsnull;

    double width, height;
    mPrintSettings->GetEffectivePageSize(&width, &height);

    // If we're in landscape mode, we'll be rotating the output --
    // need to swap width & height.
    PRInt32 orientation;
    mPrintSettings->GetOrientation(&orientation);
    if (nsIPrintSettings::kLandscapeOrientation == orientation) {
        double tmp = width;
        width = height;
        height = tmp;
    }

    // convert twips to points
    width  /= TWIPS_PER_POINT_FLOAT;
    height /= TWIPS_PER_POINT_FLOAT;

    DO_PR_DEBUG_LOG(("\"%s\", %f, %f\n", mPath, width, height));

    QTemporaryFile file;
    if(!file.open()) {
        return NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE;
    }
    file.setAutoRemove(false);

    nsresult rv = NS_NewNativeLocalFile(
            nsDependentCString(file.fileName().toAscii().constData()),
            PR_FALSE,
            getter_AddRefs(mSpoolFile));
    if (NS_FAILED(rv)) {
        file.remove();
        return NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE;
    }

    mSpoolName = file.fileName().toUtf8().constData();

    mSpoolFile->SetPermissions(0600);

    nsCOMPtr<nsIFileOutputStream> stream =
        do_CreateInstance("@mozilla.org/network/file-output-stream;1");

    rv = stream->Init(mSpoolFile, -1, -1, 0);
    if (NS_FAILED(rv))
        return rv;

    PRInt16 format;
    mPrintSettings->GetOutputFormat(&format);

    nsRefPtr<gfxASurface> surface;
    gfxSize surfaceSize(width, height);

    if (format == nsIPrintSettings::kOutputFormatNative) {
        if (mIsPPreview) {
            // There is nothing to detect on Print Preview, use PS.
            // TODO: implement for Qt?
            //format = nsIPrintSettings::kOutputFormatPS;
            return NS_ERROR_NOT_IMPLEMENTED;
        }
        format = nsIPrintSettings::kOutputFormatPDF;
    }
    if (format == nsIPrintSettings::kOutputFormatPDF) {
        surface = new gfxPDFSurface(stream, surfaceSize);
    } else {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_ABORT_IF_FALSE(surface, "valid address expected");

    surface.swap(*aSurface);
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::Init(nsIWidget* aWidget,
        nsIPrintSettings* aPS,
        bool aIsPrintPreview)
{
    DO_PR_DEBUG_LOG(("nsDeviceContextSpecQt::Init(aPS=%p)\n", aPS));

    mPrintSettings = aPS;
    mIsPPreview = aIsPrintPreview;

    // This is only set by embedders
    bool toFile;
    aPS->GetPrintToFile(&toFile);

    mToPrinter = !toFile && !aIsPrintPreview;

    nsCOMPtr<nsPrintSettingsQt> printSettingsQt(do_QueryInterface(aPS));
    if (!printSettingsQt)
        return NS_ERROR_NO_INTERFACE;
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::GetPath(const char** aPath)
{
    *aPath = mPath;
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::BeginDocument(
        PRUnichar* aTitle,
        PRUnichar* aPrintToFileName,
        PRInt32 aStartPage,
        PRInt32 aEndPage)
{
    if (mToPrinter) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::EndDocument()
{
    if (mToPrinter) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }
    // Handle print-to-file ourselves for the benefit of embedders
    nsXPIDLString targetPath;
    nsCOMPtr<nsILocalFile> destFile;
    mPrintSettings->GetToFileName(getter_Copies(targetPath));

    nsresult rv = NS_NewNativeLocalFile(NS_ConvertUTF16toUTF8(targetPath),
            PR_FALSE, getter_AddRefs(destFile));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString destLeafName;
    rv = destFile->GetLeafName(destLeafName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> destDir;
    rv = destFile->GetParent(getter_AddRefs(destDir));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mSpoolFile->MoveTo(destDir, destLeafName);
    NS_ENSURE_SUCCESS(rv, rv);

    // This is the standard way to get the UNIX umask. Ugh.
    mode_t mask = umask(0);
    umask(mask);
    // If you're not familiar with umasks, they contain the bits of what NOT
    // to set in the permissions
    // (thats because files and directories have different numbers of bits
    // for their permissions)
    destFile->SetPermissions(0666 & ~(mask));

    return NS_OK;
}

//  Printer Enumerator
nsPrinterEnumeratorQt::nsPrinterEnumeratorQt()
{
}

NS_IMPL_ISUPPORTS1(nsPrinterEnumeratorQt, nsIPrinterEnumerator)

NS_IMETHODIMP nsPrinterEnumeratorQt::GetPrinterNameList(
        nsIStringEnumerator** aPrinterNameList)
{
    NS_ENSURE_ARG_POINTER(aPrinterNameList);
    *aPrinterNameList = nsnull;

    QList<QPrinterInfo> qprinters = QPrinterInfo::availablePrinters();
    if (qprinters.size() == 0)
        return NS_ERROR_NOT_AVAILABLE;

    nsTArray<nsString>* printers =
        new nsTArray<nsString>(qprinters.size()); 

    for (PRInt32 i = 0; i < qprinters.size(); ++i) {
        printers->AppendElement(
                nsDependentString(
                    (const PRUnichar*)qprinters[i].printerName().constData()));
    }

    return NS_NewAdoptingStringEnumerator(aPrinterNameList, printers);
}

NS_IMETHODIMP nsPrinterEnumeratorQt::GetDefaultPrinterName(
        PRUnichar** aDefaultPrinterName)
{
    DO_PR_DEBUG_LOG(("nsPrinterEnumeratorQt::GetDefaultPrinterName()\n"));
    NS_ENSURE_ARG_POINTER(aDefaultPrinterName);

    QString defprinter = QPrinterInfo::defaultPrinter().printerName();
    *aDefaultPrinterName = ToNewUnicode(nsDependentString(
        (const PRUnichar*)defprinter.constData()));

    DO_PR_DEBUG_LOG(("GetDefaultPrinterName(): default printer='%s'.\n",
        NS_ConvertUTF16toUTF8(*aDefaultPrinterName).get()));

    return NS_OK;
}

NS_IMETHODIMP nsPrinterEnumeratorQt::InitPrintSettingsFromPrinter(
        const PRUnichar* aPrinterName,
        nsIPrintSettings* aPrintSettings)
{
    DO_PR_DEBUG_LOG(("nsPrinterEnumeratorQt::InitPrintSettingsFromPrinter()"));
    // XXX Leave NS_OK for now
    // Probably should use NS_ERROR_NOT_IMPLEMENTED
    return NS_OK;
}

NS_IMETHODIMP nsPrinterEnumeratorQt::DisplayPropertiesDlg(
        const PRUnichar* aPrinter,
        nsIPrintSettings* aPrintSettings)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

