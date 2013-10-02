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

#include <assert.h>

////////////////////////////////////////////////////////
///  ACE context ops
////////////////////////////////////////////////////////
/**
 * Wrapper for xio handlers
 */
class XIO_Event_Handler : public ACE_Event_Handler
{
public:
  XIO_Event_Handler (ACE_HANDLE fd, xio_ev_handler_t handler, void* data)
  : fd_ (fd)
  , handler_ (handler)
  , data_ (data)
  {
  }

  /// Get the I/O handle.
  virtual ACE_HANDLE get_handle(void) const
  {
    return this->fd_;
  }

  /// Called when input events occur (e.g., connection or data).
  virtual int handle_input(ACE_HANDLE fd)
  {
    this->handler_ (fd, 0, this->data_);
    return 0;
  }
  /// Called when output events are possible (e.g., when flow control
  /// abates or non-blocking connection completes).
  virtual int handle_output(ACE_HANDLE fd)
  {
    this->handler_ (fd, 0, this->data_);
    return 0;
  }

private:
  ACE_HANDLE fd_;
  xio_ev_handler_t handler_;
  void* data_;
};

/// Register a handler
static int static_add_xio_handler(void* void_reactor,
                                  int fd,
                                  int events,
                                  xio_ev_handler_t handler,
                                  void *data)
{
  ACE_Reactor* reactor = reinterpret_cast <ACE_Reactor*> (void_reactor);

  ACE_Reactor_Mask mask = 0;
  if (events & XIO_POLLIN)
  {
    mask |= ACE_Event_Handler::READ_MASK;
  }
  if (events & XIO_POLLOUT)
  {
    mask |= ACE_Event_Handler::WRITE_MASK;
  }

  return reactor->register_handler (new XIO_Event_Handler (fd, handler, data), mask);
}

/// Remove a handler
static int static_remove_xio_handler(void* void_reactor, int fd)
{
  ACE_Reactor* reactor = reinterpret_cast <ACE_Reactor*> (void_reactor);
  return reactor->remove_handler (fd, ACE_Event_Handler::ALL_EVENTS_MASK);
}

static struct xio_loop_ops ACE_REACTOR_LOOP_OPS = { static_add_xio_handler, static_remove_xio_handler };


struct xio_context *xio_ace_ctx_open(ACE_Reactor *reactor,
                                     int polling_timeout_us)
{
  return xio_ctx_open (&ACE_REACTOR_LOOP_OPS, reactor, polling_timeout_us);
}


////////////////////////////////////////////////////////
///  Static callbacks
////////////////////////////////////////////////////////
template <class T>
static int
static_on_msg (xio_session* session,
               xio_msg* msg,
               int more_in_batch,
               void* cb_user_context)
{
  T* obj = reinterpret_cast <T*> (cb_user_context);
  if (obj == NULL)
  {
    assert (obj != NULL);
    return -1;
  }
  return obj->on_msg (session, msg, more_in_batch);
}

template <class T>
static int
static_on_msg_delivered (struct xio_session *session,
                         struct xio_msg *msg,
                         int more_in_batch,
                         void *conn_user_context)
{
  T* obj = reinterpret_cast <T*> (conn_user_context);
  if (obj == NULL)
  {
    assert (obj != NULL);
    return -1;
  }
  return obj->on_msg_delivered (session, msg, more_in_batch);
}

template <class T>
static int
static_on_msg_error (xio_session* session,
                     xio_status error,
                     xio_msg* msg,
                     void* cb_user_context)
{
  T* obj = reinterpret_cast <T*> (cb_user_context);
  if (obj == NULL)
  {
    assert (obj != NULL);
    return -1;
  }
  return obj->on_msg_error (session, error, msg);
}

template <class T>
static int
static_on_msg_send_complete (xio_session* session,
                             xio_msg* msg,
                             void* cb_user_context)
{
  T* obj = reinterpret_cast <T*> (cb_user_context);
  if (obj == NULL)
  {
    assert (obj != NULL);
    return -1;
  }
  return obj->on_msg_send_complete (session, msg);
}

template <class T>
static int
static_assign_data_in_buf (xio_msg* msg, void* cb_user_context)
{
  T* obj = reinterpret_cast <T*> (cb_user_context);
  if (obj == NULL)
  {
    assert (obj != NULL);
    return -1;
  }
  return obj->assign_data_in_buf (msg);
}

template <class T>
static int
static_on_session_established (xio_session* session,
                               xio_new_session_rsp* rsp,
                               void* cb_user_context)
{
  T* obj = reinterpret_cast <T*> (cb_user_context);
  if (obj == NULL)
  {
    assert (obj != NULL);
    return -1;
  }
  return obj->on_session_established (session, rsp);
}

template <class T>
static int
static_on_session_event (xio_session* session, xio_session_event_data* data, void* cb_user_context)
{
  T* obj = reinterpret_cast <T*> (cb_user_context);
  if (obj == NULL)
  {
    assert (obj != NULL);
    return -1;
  }
  return obj->on_session_event (session, data);
}

template <class T>
static int
static_on_new_session (xio_session* session, xio_new_session_req* req, void* cb_user_context)
{
  T* obj = reinterpret_cast <T*> (cb_user_context);
  if (obj == NULL)
  {
    assert (obj != NULL);
    return -1;
  }
  return obj->on_new_session (session, req);
}

////////////////////////////////////////////////////////
///  XIO_Callback_Implementor
////////////////////////////////////////////////////////
XIO_Callback_Implementor::XIO_Callback_Implementor (Callback implemented_callbacks)
: implemented_callbacks_ (implemented_callbacks)
{
}

XIO_Callback_Implementor::~XIO_Callback_Implementor ()
{
}

int XIO_Callback_Implementor::assign_data_in_buf (xio_msg* msg)
{
  return 0;
}
int XIO_Callback_Implementor::on_msg (xio_session* session, xio_msg* msg, int more_in_batch)
{
  return 0;
}
int XIO_Callback_Implementor::on_msg_error (xio_session* session, xio_status error, xio_msg* msg)
{
  return 0;
}
int XIO_Callback_Implementor::on_msg_delivered (struct xio_session *session, struct xio_msg *msg, int more_in_batch)
{
  return 0;
}
int XIO_Callback_Implementor::on_msg_send_complete (xio_session* session, xio_msg* msg)
{
  return 0;
}
int XIO_Callback_Implementor::on_new_session (xio_session* session, xio_new_session_req* req)
{
  return 0;
}
int XIO_Callback_Implementor::on_session_established (struct xio_session *session, struct xio_new_session_rsp *rsp)
{
  return 0;
}
int XIO_Callback_Implementor::on_session_event (xio_session* session, xio_session_event_data* data)
{
  return 0;
}

bool XIO_Callback_Implementor::is_implemented (Callback cb)
{
  return (this->implemented_callbacks_ & cb) != 0;
}

void XIO_Callback_Implementor::fill_callbacks (xio_session_ops& ses_ops)
{
  memset (&ses_ops, 0, sizeof (ses_ops));
  if (this->is_implemented (XIO_CB_ASSIGN_DATA_IN_BUF))
  {
    ses_ops.assign_data_in_buf = static_assign_data_in_buf <XIO_Callback_Implementor>;
  }
  if (this->is_implemented (XIO_CB_ON_MSG))
  {
    ses_ops.on_msg = static_on_msg <XIO_Callback_Implementor>;
  }
  if (this->is_implemented (XIO_CB_ON_MSG_DELIVERED))
  {
    ses_ops.on_msg_delivered = static_on_msg_delivered <XIO_Callback_Implementor>;
  }
  if (this->is_implemented (XIO_CB_ON_MSG_ERROR))
  {
    ses_ops.on_msg_error = static_on_msg_error <XIO_Callback_Implementor>;
  }
  if (this->is_implemented (XIO_CB_ON_MSG_SEND_COMPLETE))
  {
    ses_ops.on_msg_send_complete = static_on_msg_send_complete <XIO_Callback_Implementor>;
  }
  if (this->is_implemented (XIO_CB_ON_NEW_SESSION))
  {
    ses_ops.on_new_session = static_on_new_session <XIO_Callback_Implementor>;
  }
  if (this->is_implemented (XIO_CB_ON_SESSION_ESTABLISHED))
  {
    ses_ops.on_session_established = static_on_session_established <XIO_Callback_Implementor>;
  }
  if (this->is_implemented (XIO_CB_ON_SESSION_EVENT))
  {
    ses_ops.on_session_event = static_on_session_event <XIO_Callback_Implementor>;
  }
}


////////////////////////////////////////////////////////
///  XIO_Server
////////////////////////////////////////////////////////
XIO_Server::XIO_Server (Callback implemented_callbacks)
: XIO_Callback_Implementor (implemented_callbacks)
, server_ (NULL)
{
}

XIO_Server::~XIO_Server ()
{
  if (this->server_)
  {
    this->close ();
  }
}

struct xio_server*
XIO_Server::open (struct xio_context *ctx,
                  const char *uri,
                  uint16_t *src_port,
                  int32_t flags)
{
  // Create session ops
  xio_session_ops ops;
  this->fill_callbacks (ops);

  // Create server
  this->server_ = xio_bind (ctx, &ops, uri, src_port, flags, this);

  return this->server_;
}

int
XIO_Server::close ()
{
  if (this->server_ == NULL)
  {
    return 0;
  }

  int retval = xio_unbind (this->server_);
  if (retval == 0)
  {
    this->server_ = NULL;
  }
  return retval;
}

struct xio_server*
XIO_Server::server ()
{
  return this->server_;
}


////////////////////////////////////////////////////////
///  XIO_Reqeust_Session
////////////////////////////////////////////////////////
XIO_Reqeust_Session::XIO_Reqeust_Session (Callback implemented_callbacks)
: XIO_Callback_Implementor (implemented_callbacks)
, session_ (NULL)
{
}

XIO_Reqeust_Session::~XIO_Reqeust_Session ()
{
  if (this->session_)
  {
    this->close ();
  }
}

struct xio_session*
XIO_Reqeust_Session::open (const char *uri,
                           uint32_t initial_sn,
                           uint32_t flags,
                           void* user_context,
                           size_t user_context_len)
{
  // Create session ops
  xio_session_ops ops;
  this->fill_callbacks (ops);

  // Create session attributes
  xio_session_attr attr;
  memset (&attr, 0, sizeof (attr));
  attr.ses_ops = &ops;
  attr.user_context = user_context;
  attr.user_context_len = user_context_len;

  // Create session
  this->session_ = xio_session_open (XIO_SESSION_REQ, &attr, uri, initial_sn, flags, this);
  return this->session_;
}

int
XIO_Reqeust_Session::close ()
{
  if (this->session_ == NULL)
  {
    return 0;
  }

  int retval = xio_session_close (this->session_);
  if (retval == 0)
  {
    this->session_ = NULL;
  }
  return retval;
}

struct xio_session*
XIO_Reqeust_Session::session ()
{
  return this->session_;
}


////////////////////////////////////////////////////////
///  XIO_Connection
////////////////////////////////////////////////////////
XIO_Connection::XIO_Connection ()
: XIO_Callback_Implementor (XIO_CB_NONE)
, session_ (NULL)
, connection_ (NULL)
{
}

XIO_Connection::~XIO_Connection ()
{
  if (this->connection_)
  {
    this->close ();
  }
}

struct xio_connection*
XIO_Connection::open (XIO_Reqeust_Session *session,
                      struct xio_context *ctx,
                      int conn_idx)
{
  if (this->connection_)
  {
    // Cannot create a new connection when already connected
    return NULL;
  }

  // Connect
  this->session_ = session;
  this->connection_ = xio_connect (this->session_->session (), ctx, conn_idx, this);
  return this->connection_;
}

void
XIO_Connection::close ()
{
  this->session_ = NULL;
  this->connection_ = NULL;
}

XIO_Reqeust_Session*
XIO_Connection::session ()
{
  return this->session_;
}

struct xio_connection*
XIO_Connection::connection ()
{
  return this->connection_;
}

