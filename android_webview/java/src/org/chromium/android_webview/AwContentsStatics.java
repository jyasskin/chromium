// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;

import java.lang.Runnable;

/**
 * Implementations of various static methods, and also a home for static
 * data structures that are meant to be shared between all webviews.
 */
@JNINamespace("android_webview")
public class AwContentsStatics {

    private static ClientCertLookupTable sClientCertLookupTable;

    /**
     * Return the client certificate lookup table.
     */
    public static ClientCertLookupTable getClientCertLookupTable() {
        ThreadUtils.assertOnUiThread();
        if (sClientCertLookupTable == null) {
            sClientCertLookupTable = new ClientCertLookupTable();
        }
        return sClientCertLookupTable;
    }

    /**
     * Clear client cert lookup table. Should only be called from UI thread.
     */
    public static void clearClientCertPreferences(Runnable callback) {
        ThreadUtils.assertOnUiThread();
        getClientCertLookupTable().clear();
        nativeClearClientCertPreferences(callback);
    }

    @CalledByNative
    private static void clientCertificatesCleared(Runnable callback) {
        if (callback == null) return;
        callback.run();
    }

    /**
     * Set Data Reduction Proxy key for authentication.
     */
    public static void setDataReductionProxyKey(String key) {
        nativeSetDataReductionProxyKey(key);
    }

    //--------------------------------------------------------------------------------------------
    //  Native methods
    //--------------------------------------------------------------------------------------------
    private static native void nativeClearClientCertPreferences(Runnable callback);
    private static native void nativeSetDataReductionProxyKey(String key);
}
