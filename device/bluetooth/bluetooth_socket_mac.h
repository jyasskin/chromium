// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_MAC_H_

#include <queue>
#include <string>

#include <IOKit/IOReturn.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "device/bluetooth/bluetooth_socket.h"

@class BluetoothRFCOMMChannelDelegate;
@class IOBluetoothRFCOMMChannel;
@class IOBluetoothSDPServiceRecord;

namespace net {
class IOBuffer;
class IOBufferWithSize;
}  // namespace net

namespace device {

class BluetoothServiceRecord;

// Implements the BluetoothSocket class for the Mac OS X platform.
class BluetoothSocketMac : public BluetoothSocket {
 public:
  typedef base::Callback<void(scoped_refptr<BluetoothSocket>)>
      ConnectSuccessCallback;

  // Creates a client socket and connects it to the Bluetooth service |record|.
  // Calls |success_callback|, passing in the created socket, on success.
  // Calls |error_callback| on failure.
  static void Connect(IOBluetoothSDPServiceRecord* record,
                      const ConnectSuccessCallback& success_callback,
                      const ErrorCompletionCallback& error_callback);

  // Creates a server socket to wrap the |rfcomm_channel|, which should be an
  // incoming channel in the process of being opened.
  // Calls |success_callback|, passing in the created socket, on success.
  // Calls |error_callback| on failure.
  static void AcceptConnection(IOBluetoothRFCOMMChannel* rfcomm_channel,
                               const ConnectSuccessCallback& success_callback,
                               const ErrorCompletionCallback& error_callback);

  // BluetoothSocket:
  virtual void Close() OVERRIDE;
  virtual void Disconnect(const base::Closure& callback) OVERRIDE;
  virtual void Receive(
      int /* buffer_size */,
      const ReceiveCompletionCallback& success_callback,
      const ReceiveErrorCompletionCallback& error_callback) OVERRIDE;
  virtual void Send(scoped_refptr<net::IOBuffer> buffer,
                    int buffer_size,
                    const SendCompletionCallback& success_callback,
                    const ErrorCompletionCallback& error_callback) OVERRIDE;
  virtual void Accept(const AcceptCompletionCallback& success_callback,
                      const ErrorCompletionCallback& error_callback) OVERRIDE;


  // Called by BluetoothRFCOMMChannelDelegate.
  void OnChannelOpened(IOBluetoothRFCOMMChannel* rfcomm_channel,
                       IOReturn status);
  void OnChannelClosed(IOBluetoothRFCOMMChannel* rfcomm_channel);
  void OnChannelDataReceived(IOBluetoothRFCOMMChannel* rfcomm_channel,
                             void* data,
                             size_t length);
  void OnChannelWriteComplete(IOBluetoothRFCOMMChannel* rfcomm_channel,
                              void* refcon,
                              IOReturn status);

 private:
  struct SendRequest {
    SendRequest();
    ~SendRequest();
    int buffer_size;
    SendCompletionCallback success_callback;
    ErrorCompletionCallback error_callback;
    IOReturn status;
    int active_async_writes;
    bool error_signaled;
  };

  struct ReceiveCallbacks {
    ReceiveCallbacks();
    ~ReceiveCallbacks();
    ReceiveCompletionCallback success_callback;
    ReceiveErrorCompletionCallback error_callback;
  };

  struct ConnectCallbacks {
    ConnectCallbacks();
    ~ConnectCallbacks();
    base::Closure success_callback;
    ErrorCompletionCallback error_callback;
  };

  BluetoothSocketMac();
  virtual ~BluetoothSocketMac();

  void ReleaseChannel();

  // Connects to the peer device corresponding to |record| and calls
  // |success_callback| when the connection has been established
  // successfully. If an error occurs, calls |error_callback| with a system
  // error message.
  void ConnectImpl(IOBluetoothSDPServiceRecord* record,
                   const ConnectSuccessCallback& success_callback,
                   const ErrorCompletionCallback& error_callback);

  // Accepts a connection from a peer device. The connection is represented as
  // the |rfcomm_channel|, which should be an incoming channel in the process of
  // being opened. Calls |success_callback|, passing in |this|, on success.
  // Calls |error_callback| on failure.
  void AcceptConnectionImpl(IOBluetoothRFCOMMChannel* rfcomm_channel,
                            const ConnectSuccessCallback& success_callback,
                            const ErrorCompletionCallback& error_callback);

  bool connecting() const { return connect_callbacks_; }

  // Used to verify that all methods are called on the same thread.
  base::ThreadChecker thread_checker_;

  // The RFCOMM channel delegate.
  base::scoped_nsobject<BluetoothRFCOMMChannelDelegate> delegate_;

  // The IOBluetooth RFCOMM channel used to issue commands.
  base::scoped_nsobject<IOBluetoothRFCOMMChannel> rfcomm_channel_;

  // Connection callbacks -- when a pending async connection is active.
  scoped_ptr<ConnectCallbacks> connect_callbacks_;

  // Packets received while there is no pending "receive" callback.
  std::queue<scoped_refptr<net::IOBufferWithSize> > receive_queue_;

  // Receive callbacks -- when a receive call is active.
  scoped_ptr<ReceiveCallbacks> receive_callbacks_;

  // Send queue -- one entry per pending send operation.
  std::queue<linked_ptr<SendRequest> > send_queue_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_MAC_H_
