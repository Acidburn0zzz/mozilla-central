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
 * John C. Griggs <johng@corel.com>.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Zack Rusin <zack@kde.org>
 *   Lars Knoll <knoll@kde.org>
 *   John C. Griggs <johng@corel.com>
 *   Dan Rosen <dr@netscape.com>
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

#include "mozilla/ModuleUtils.h"

#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"
#include "nsAppShell.h"
#include "nsWindow.h"

#include "nsHTMLFormatConverter.h"
//#include "nsTransferable.h"
#include "nsLookAndFeel.h"
#include "nsAppShellSingleton.h"
#include "nsScreenManagerB2G.h"
//#include "nsFilePicker.h"
//#include "nsClipboard.h"
//#include "nsClipboardHelper.h"
//#include "nsIdleServiceB2G.h"
//#include "nsDragService.h"
//#include "nsSound.h"
//#include "nsBidiKeyboard.h"
//#include "nsNativeThemeB2G.h"
#include "nsFilePickerProxy.h"
#include "nsXULAppAPI.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
//NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerB2G)
//NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
//NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
//NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragService)
//NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiKeyboard)
//NS_GENERIC_FACTORY_CONSTRUCTOR(nsIdleServiceB2G)
//NS_GENERIC_FACTORY_CONSTRUCTOR(nsSound)

NS_DEFINE_NAMED_CID(NS_WINDOW_CID);
NS_DEFINE_NAMED_CID(NS_CHILD_CID);
NS_DEFINE_NAMED_CID(NS_APPSHELL_CID);
//NS_DEFINE_NAMED_CID(NS_FILEPICKER_CID);
//NS_DEFINE_NAMED_CID(NS_TRANSFERABLE_CID);
//NS_DEFINE_NAMED_CID(NS_CLIPBOARD_CID);
//NS_DEFINE_NAMED_CID(NS_CLIPBOARDHELPER_CID);
//NS_DEFINE_NAMED_CID(NS_DRAGSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_HTMLFORMATCONVERTER_CID);
//NS_DEFINE_NAMED_CID(NS_BIDIKEYBOARD_CID);
NS_DEFINE_NAMED_CID(NS_SCREENMANAGER_CID);
//NS_DEFINE_NAMED_CID(NS_THEMERENDERER_CID);
//NS_DEFINE_NAMED_CID(NS_IDLE_SERVICE_CID);

static const mozilla::Module::CIDEntry kWidgetCIDs[] = {
    { &kNS_WINDOW_CID, false, NULL, nsWindowConstructor },
    { &kNS_CHILD_CID, false, NULL, nsWindowConstructor },
    { &kNS_APPSHELL_CID, false, NULL, nsAppShellConstructor },
//    { &kNS_FILEPICKER_CID, false, NULL, nsFilePickerConstructor },
//    { &kNS_TRANSFERABLE_CID, false, NULL, nsTransferableConstructor },
//    { &kNS_CLIPBOARD_CID, false, NULL, nsClipboardConstructor },
//    { &kNS_CLIPBOARDHELPER_CID, false, NULL, nsClipboardHelperConstructor },
//    { &kNS_DRAGSERVICE_CID, false, NULL, nsDragServiceConstructor },
    { &kNS_HTMLFORMATCONVERTER_CID, false, NULL, nsHTMLFormatConverterConstructor },
//    { &kNS_BIDIKEYBOARD_CID, false, NULL, nsBidiKeyboardConstructor },
    { &kNS_SCREENMANAGER_CID, false, NULL, nsScreenManagerB2GConstructor },
//    { &kNS_IDLE_SERVICE_CID, false, NULL, nsIdleServiceB2GConstructor },
//    { &kNS_POPUP_CID, false, NULL, nsPopupWindowConstructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kWidgetContracts[] = {
    { "@mozilla.org/widget/window/b2g;1", &kNS_WINDOW_CID },
    { "@mozilla.org/widget/child_window/b2g;1", &kNS_CHILD_CID },
    { "@mozilla.org/widget/appshell/b2g;1", &kNS_APPSHELL_CID },
//    { "@mozilla.org/filepicker;1", &kNS_FILEPICKER_CID },
//    { "@mozilla.org/widget/transferable;1", &kNS_TRANSFERABLE_CID },
//    { "@mozilla.org/widget/clipboard;1", &kNS_CLIPBOARD_CID },
//    { "@mozilla.org/widget/clipboardhelper;1", &kNS_CLIPBOARDHELPER_CID },
//    { "@mozilla.org/widget/dragservice;1", &kNS_DRAGSERVICE_CID },
    { "@mozilla.org/widget/htmlformatconverter;1", &kNS_HTMLFORMATCONVERTER_CID },
//    { "@mozilla.org/widget/bidikeyboard;1", &kNS_BIDIKEYBOARD_CID },
    { "@mozilla.org/gfx/screenmanager;1", &kNS_SCREENMANAGER_CID },
//    { "@mozilla.org/chrome/chrome-native-theme;1", &kNS_THEMERENDERER_CID },
//    { "@mozilla.org/widget/idleservice;1", &kNS_IDLE_SERVICE_CID },
    { NULL }
};

static void
nsWidgetB2GModuleDtor()
{
    nsLookAndFeel::Shutdown();
 //   nsWindow::ReleaseGlobals();
    nsAppShellShutdown();
}

static const mozilla::Module kWidgetModule = {
    mozilla::Module::kVersion,
    kWidgetCIDs,
    kWidgetContracts,
    NULL,
    NULL,
    nsAppShellInit,
    nsWidgetB2GModuleDtor
};

NSMODULE_DEFN(nsWidgetB2GModule) = &kWidgetModule;
