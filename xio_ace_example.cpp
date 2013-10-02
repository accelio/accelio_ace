/*
 * Copyright (c) 2013 Fabrix Systems. All rights reserved.
 * Copyright (c) 2013 Mellanox Technologies®. All rights reserved.
 *
 * This software is available to you under a choice of one of two licenses.
 * You may choose to be licensed under the terms of the GNU General Public
 * License (GPL) Version 2, available from the file COPYING in the main
 * directory of this source tree, or the Mellanox Technologies® BSD license
 * below:
 *
 *      - Redistribution and use in source and binary forms, with or without
 *        modification, are permitted provided that the following conditions
 *        are met:
 *
 *      - Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 *      - Neither the name of the Mellanox Technologies® nor the names of its
 *        contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "xio_ace_session.h"
#include <signal.h>

class Example_Server : public XIO_Server
{
public:
  static const XIO_Callback_Implementor::Callback cbs =
    (XIO_Callback_Implementor::Callback) (XIO_Callback_Implementor::XIO_CB_ON_NEW_SESSION |
                                          XIO_Callback_Implementor::XIO_CB_ON_SESSION_EVENT |
                                          XIO_Callback_Implementor::XIO_CB_ON_MSG |
                                          XIO_Callback_Implementor::XIO_CB_ON_MSG_ERROR |
                                          XIO_Callback_Implementor::XIO_CB_ON_MSG_SEND_COMPLETE);

  Example_Server ()
  : XIO_Server(cbs)
  {
  }

  virtual int on_new_session(xio_session* session, xio_new_session_req* req)
  {
    printf("Example_Server::%s called\n", __FUNCTION__);
    xio_accept (session, NULL, 0, NULL, 0);
    return 0;
  }

  virtual int on_session_event(xio_session* session, xio_session_event_data* data)
  {
    printf("Example_Server::%s called, event=%s\n", __FUNCTION__, xio_session_event_str (data->event));
    return 0;
  }

  virtual int on_msg(xio_session* session, xio_msg* msg, int more_in_batch)
  {
    printf("Example_Server::%s called\n", __FUNCTION__);
    // Create response
    xio_msg* response = new xio_msg;
    memset (response, 0, sizeof (xio_msg));
    response->request = msg;
    // Send response
    xio_send_response (response);
    return 0;
  }

  virtual int on_msg_error(xio_session* session, xio_status error, xio_msg* msg)
  {
    printf("Example_Server::%s called\n", __FUNCTION__);
    // Delete dynamically allocated response
    delete msg;
  }

  virtual int on_msg_send_complete(xio_session* session, xio_msg* msg)
  {
    printf("Example_Server::%s called\n", __FUNCTION__);
    // Delete dynamically allocated response
    delete msg;
  }
};

class Example_Session : public XIO_Reqeust_Session
{
public:
  static const XIO_Callback_Implementor::Callback cbs =
    (XIO_Callback_Implementor::Callback) (XIO_Callback_Implementor::XIO_CB_ON_SESSION_ESTABLISHED |
                                          XIO_Callback_Implementor::XIO_CB_ON_SESSION_EVENT |
                                          XIO_Callback_Implementor::XIO_CB_ON_MSG);

  Example_Session (ACE_Reactor* reactor)
  : XIO_Reqeust_Session (cbs)
  , reactor_ (reactor)
  {
  }

  virtual int on_session_established(struct xio_session* session, struct xio_new_session_rsp* rsp)
  {
    printf("Example_Session::%s called\n", __FUNCTION__);
    return 0;
  }
  virtual int on_session_event(xio_session* session, xio_session_event_data* data)
  {
    printf("Example_Session::%s called, event=%s\n", __FUNCTION__, xio_session_event_str (data->event));
    switch (data->event)
    {
    case XIO_SESSION_CONNECTION_DISCONNECTED_EVENT:
      xio_disconnect (data->conn);
      break;
    case XIO_SESSION_CONNECTION_CLOSED_EVENT:
      {
        // Close the connection
        XIO_Connection* connection = reinterpret_cast <XIO_Connection*> (data->conn_user_context);
        if (connection)
        {
          connection->close ();
        }
      }
      break;
    case XIO_SESSION_TEARDOWN_EVENT:
      this->close ();
      // Stop the program
      this->reactor_->end_event_loop ();
      break;
    }

    return 0;
  }

private:
  ACE_Reactor* reactor_;
};


class Example_Connection : public XIO_Connection
{
public:
  Example_Connection (int num_messages)
  : num_messages_ (num_messages)
  {
  }

  virtual int on_msg(xio_session* session, xio_msg* msg, int more_in_batch)
  {
    printf("Example_Connection::%s called\n", __FUNCTION__);
    xio_release_response (msg);
    if (--num_messages_ == 0)
    {
      xio_disconnect (this->connection ());
    }
    return 0;
  }

private:
  int num_messages_;
};


void handle_signal(int sig)
{
  // End Main Event Loop
  ACE_Reactor::end_event_loop();
}

int main (int argc, char* argv[])
{
  if (argc == 1 || argc > 3)
  {
    printf("Usage (server): %s <port>\n", argv[0]);
    printf("Usage (client): %s <target ip> <port>\n", argv[0]);
    return -1;
  }

  // Create context
  ACE_Reactor* reactor = ACE_Reactor::instance ();
  xio_context* ctx = xio_ace_ctx_open (reactor, 0);

  if (argc == 2)
  {
    // Setup signal handler
    signal (SIGINT, &handle_signal);

    // Server
    char listen_uri[256];
    snprintf (listen_uri, 256, "rdma://0.0.0.0:%s", argv[1]);

    Example_Server server;
    server.open (ctx, listen_uri, NULL, 0);

    reactor->restart (1);
    reactor->run_event_loop ();

    server.close ();
  }
  else
  {
    // Client
    char connect_uri[256];
    snprintf (connect_uri, 256, "rdma://%s:%s", argv[1], argv[2]);

    Example_Session session (reactor);
    if (session.open (connect_uri, 0, 0, NULL, 0) == NULL)
    {
      printf("Failed to open session\n");
      return -1;
    }

    printf("connecting to %s\n", connect_uri);
    static const int NUM_MSG = 8;
    Example_Connection conn (NUM_MSG);
    if (conn.open(&session, ctx, 0) == NULL)
    {
      printf("Failed to open connection\n");
      return -1;
    }

    struct xio_msg req_msgs [NUM_MSG];
    memset (&req_msgs[0], 0, sizeof(req_msgs));
    for (int i = 0; i < NUM_MSG; ++i)
    {
      xio_send_request (conn.connection (), &req_msgs[i]);
    }

    reactor->restart (1);
    reactor->run_event_loop ();
  }

  xio_ctx_close (ctx);
  return 0;
}
