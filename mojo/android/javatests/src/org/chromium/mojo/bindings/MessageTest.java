// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.mojo.MojoTestCase;
import org.chromium.mojo.TestUtils;
import org.chromium.mojo.bindings.BindingsTestUtils.RecordingMessageReceiver;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.DataPipe;
import org.chromium.mojo.system.Handle;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.MojoResult;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.impl.CoreImpl;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Testing {@link Message}.
 */
public class MessageTest extends MojoTestCase {

    private static final int DATA_SIZE = 1024;

    private ByteBuffer mData;
    private Pair<MessagePipeHandle, MessagePipeHandle> mHandles;
    private List<Handle> mHandlesToSend = new ArrayList<Handle>();
    private List<Handle> mHandlesToClose = new ArrayList<Handle>();
    private RecordingMessageReceiver mMessageReceiver;

    /**
     * @see org.chromium.mojo.MojoTestCase#setUp()
     */
    @Override
    protected void setUp() throws Exception {
        super.setUp();
        Core core = CoreImpl.getInstance();
        mData = TestUtils.newRandomBuffer(DATA_SIZE);
        mMessageReceiver = new RecordingMessageReceiver();
        mHandles = core.createMessagePipe(new MessagePipeHandle.CreateOptions());
        Pair<DataPipe.ProducerHandle, DataPipe.ConsumerHandle> datapipe = core.createDataPipe(null);
        mHandlesToSend.addAll(Arrays.asList(datapipe.first, datapipe.second));
        mHandlesToClose.addAll(Arrays.asList(mHandles.first, mHandles.second));
        mHandlesToClose.addAll(mHandlesToSend);
    }

    /**
     * @see org.chromium.mojo.MojoTestCase#tearDown()
     */
    @Override
    protected void tearDown() throws Exception {
        for (Handle handle : mHandlesToClose) {
            handle.close();
        }
        super.tearDown();
    }

    /**
     * Testing {@link Message#readAndDispatchMessage(MessagePipeHandle, MessageReceiver)}
     */
    @SmallTest
    public void testReadAndDispatchMessage() {
        mHandles.first.writeMessage(mData, mHandlesToSend, MessagePipeHandle.WriteFlags.NONE);
        assertEquals(MojoResult.OK,
                Message.readAndDispatchMessage(mHandles.second, mMessageReceiver));
        assertEquals(1, mMessageReceiver.messages.size());
        Message message = mMessageReceiver.messages.get(0);
        mHandlesToClose.addAll(message.handles);
        assertEquals(mData, message.buffer);
        assertEquals(2, message.handles.size());
        for (Handle handle : message.handles) {
            assertTrue(handle.isValid());
        }
    }

    /**
     * Testing {@link Message#readAndDispatchMessage(MessagePipeHandle, MessageReceiver)} with no
     * message available.
     */
    @SmallTest
    public void testReadAndDispatchMessageOnEmptyHandle() {
        assertEquals(MojoResult.SHOULD_WAIT,
                Message.readAndDispatchMessage(mHandles.second, mMessageReceiver));
        assertEquals(0, mMessageReceiver.messages.size());
    }

    /**
     * Testing {@link Message#readAndDispatchMessage(MessagePipeHandle, MessageReceiver)} on closed
     * handle.
     */
    @SmallTest
    public void testReadAndDispatchMessageOnClosedHandle() {
        mHandles.first.close();
        try {
            Message.readAndDispatchMessage(mHandles.second, mMessageReceiver);
            fail("MojoException should have been thrown");
        } catch (MojoException expected) {
            assertEquals(MojoResult.FAILED_PRECONDITION, expected.getMojoResult());
        }
        assertEquals(0, mMessageReceiver.messages.size());
    }
}
