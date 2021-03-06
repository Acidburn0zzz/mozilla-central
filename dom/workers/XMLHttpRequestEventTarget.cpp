/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XMLHttpRequestEventTarget.h"

USING_WORKERS_NAMESPACE

void
XMLHttpRequestEventTarget::_Trace(JSTracer* aTrc)
{
  EventTarget::_Trace(aTrc);
}

void
XMLHttpRequestEventTarget::_Finalize(JSFreeOp* aFop)
{
  EventTarget::_Finalize(aFop);
}
