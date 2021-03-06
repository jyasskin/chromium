// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.system.AsyncWaiter;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.MojoResult;

/**
 * A {@link Connector} owns a {@link MessagePipeHandle} and will send any received messages to the
 * registered {@link MessageReceiver}. It also acts as a {@link MessageReceiver} and will send any
 * message through the handle.
 * <p>
 * The method |start| must be called before the {@link Connector} will start listening to incoming
 * messages.
 */
public class Connector implements MessageReceiver, HandleOwner<MessagePipeHandle> {

    /**
     * An {@link ErrorHandler} is notified of error happening while using the message pipe.
     */
    interface ErrorHandler {
        public void onError(MojoException e);
    }

    /**
     * The callback that is notified when the state of the owned handle changes.
     */
    private final AsyncWaiterCallback mAsyncWaiterCallback = new AsyncWaiterCallback();

    /**
     * The owned message pipe.
     */
    private final MessagePipeHandle mMessagePipeHandle;

    /**
     * A waiter which is notified when a new message is available on the owned message pipe.
     */
    private final AsyncWaiter mAsyncWaiter;

    /**
     * The {@link MessageReceiver} to which received messages are sent.
     */
    private MessageReceiver mIncomingMessageReceiver;

    /**
     * The Cancellable for the current wait. Is |null| when not currently waiting for new messages.
     */
    private AsyncWaiter.Cancellable mCancellable;

    /**
     * The error handler to notify of errors.
     */
    private ErrorHandler mErrorHandler;

    /**
     * Create a new connector over a |messagePipeHandle|. The created connector will use the default
     * {@link AsyncWaiter} from the {@link Core} implementation of |messagePipeHandle|.
     */
    public Connector(MessagePipeHandle messagePipeHandle) {
        this(messagePipeHandle, getDefaultAsyncWaiterForMessagePipe(messagePipeHandle));
    }

    /**
     * Create a new connector over a |messagePipeHandle| using the given {@link AsyncWaiter} to get
     * notified of changes on the handle.
     */
    public Connector(MessagePipeHandle messagePipeHandle, AsyncWaiter asyncWaiter) {
        mCancellable = null;
        mMessagePipeHandle = messagePipeHandle;
        mAsyncWaiter = asyncWaiter;
    }

    /**
     * Set the {@link MessageReceiver} that will receive message from the owned message pipe.
     */
    public void setIncomingMessageReceiver(MessageReceiver incomingMessageReceiver) {
        mIncomingMessageReceiver = incomingMessageReceiver;
    }

    /**
     * Set the {@link ErrorHandler} that will be notified of errors on the owned message pipe.
     */
    public void setErrorHandler(ErrorHandler errorHandler) {
        mErrorHandler = errorHandler;
    }

    /**
     * Start listening for incoming messages.
     */
    public void start() {
        assert mCancellable == null;
        registerAsyncWaiterForRead();
    }

    /**
     * @see MessageReceiver#accept(Message)
     */
    @Override
    public boolean accept(Message message) {
        try {
            mMessagePipeHandle.writeMessage(message.buffer, message.handles,
                    MessagePipeHandle.WriteFlags.NONE);
            return true;
        } catch (MojoException e) {
            onError(e);
            return false;
        }
    }

    /**
     * Pass the owned handle of the connector. After this, the connector is disconnected. It cannot
     * accept new message and it isn't listening to the handle anymore.
     *
     * @see org.chromium.mojo.bindings.HandleOwner#passHandle()
     */
    @Override
    public MessagePipeHandle passHandle() {
        cancelIfActive();
        return mMessagePipeHandle.pass();
    }

    /**
     * @see java.io.Closeable#close()
     */
    @Override
    public void close() {
        cancelIfActive();
        mMessagePipeHandle.close();
    }

    private static AsyncWaiter getDefaultAsyncWaiterForMessagePipe(
            MessagePipeHandle messagePipeHandle) {
        if (messagePipeHandle.getCore() != null) {
            return messagePipeHandle.getCore().getDefaultAsyncWaiter();
        } else {
            return null;
        }
    }

    private class AsyncWaiterCallback implements AsyncWaiter.Callback {

        /**
         * @see org.chromium.mojo.system.AsyncWaiter.Callback#onResult(int)
         */
        @Override
        public void onResult(int result) {
            Connector.this.onAsyncWaiterResult(result);
        }

        /**
         * @see org.chromium.mojo.system.AsyncWaiter.Callback#onError(MojoException)
         */
        @Override
        public void onError(MojoException exception) {
            Connector.this.onError(exception);
        }

    }

    /**
     * @see org.chromium.mojo.system.AsyncWaiter.Callback#onResult(int)
     */
    private void onAsyncWaiterResult(int result) {
        mCancellable = null;
        if (result == MojoResult.OK) {
            readOutstandingMessages();
        } else {
            onError(new MojoException(result));
        }
    }

    private void onError(MojoException exception) {
        mCancellable = null;
        close();
        if (mErrorHandler != null) {
            mErrorHandler.onError(exception);
        }
    }

    /**
     * Register to be called back when a new message is available on the owned message pipe.
     */
    private void registerAsyncWaiterForRead() {
        assert mCancellable == null;
        if (mAsyncWaiter != null) {
            mCancellable = mAsyncWaiter.asyncWait(mMessagePipeHandle, Core.HandleSignals.READABLE,
                    Core.DEADLINE_INFINITE, mAsyncWaiterCallback);
        } else {
            onError(new MojoException(MojoResult.INVALID_ARGUMENT));
        }
    }

    /**
     * Read all available messages on the owned message pipe.
     */
    private void readOutstandingMessages() {
        int result;
        do {
            try {
                result = Message.readAndDispatchMessage(mMessagePipeHandle,
                        mIncomingMessageReceiver);
            } catch (MojoException e) {
                onError(e);
                return;
            }
        } while (result == MojoResult.OK);
        if (result == MojoResult.SHOULD_WAIT) {
            registerAsyncWaiterForRead();
        } else {
            onError(new MojoException(result));
        }
    }

    private void cancelIfActive() {
        if (mCancellable != null) {
            mCancellable.cancel();
            mCancellable = null;
        }
    }

}
