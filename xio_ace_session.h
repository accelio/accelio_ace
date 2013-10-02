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

#ifndef XIO_ACE_SESSION_H
#define XIO_ACE_SESSION_H

#include <libxio.h>
#include <ace/Reactor.h>

/**
 * creates xio context - a context is mapped internaly to
 *		   a cpu core.
 *
 * @param reactor: The reactor use for events
 * @param polling_timeout: polling timeout in microsecs - 0 ignore
 *
 * @note The reactor's run_event_loop method should be called on the
 *       same thread where this context is created
 * @return xio context handle, or NULL upon error.
 */
struct xio_context* xio_ace_ctx_open(ACE_Reactor *reactor,
                                     int polling_timeout_us);


/**
 * Base class for all classes that implement xio callbacks
 */
class XIO_Callback_Implementor
{
public:
  /// Flags that indicate which callbacks are implemented
  enum Callback
  {
    XIO_CB_NONE = 0,
    XIO_CB_ASSIGN_DATA_IN_BUF = 0x1,
    XIO_CB_ON_MSG = 0x2,
    XIO_CB_ON_MSG_DELIVERED = 0x4,
    XIO_CB_ON_MSG_ERROR = 0x8,
    XIO_CB_ON_MSG_SEND_COMPLETE = 0x10,
    XIO_CB_ON_NEW_SESSION = 0x20,
    XIO_CB_ON_SESSION_ESTABLISHED = 0x40,
    XIO_CB_ON_SESSION_EVENT = 0x80,
  };

  /**
   * Initialize a callback implementor
   *
   * @param implemented_callbacks Flags indicating which callbacks are
   *                              implemented
   */
  XIO_Callback_Implementor (Callback implemented_callbacks);
  virtual ~XIO_Callback_Implementor ();

    /// Callbacks
  virtual int assign_data_in_buf (xio_msg* msg);
  virtual int on_msg (xio_session* session, xio_msg* msg, int more_in_batch);
  virtual int on_msg_error (xio_session* session, xio_status error, xio_msg* msg);
  virtual int on_msg_delivered (struct xio_session *session, struct xio_msg *msg, int more_in_batch);
  virtual int on_msg_send_complete (xio_session* session, xio_msg* msg);
  virtual int on_new_session (xio_session* session, xio_new_session_req* req);
  virtual int on_session_established (struct xio_session *session, struct xio_new_session_rsp *rsp);
  virtual int on_session_event (xio_session* session, xio_session_event_data* data);

protected:
  /**
   * Check whether a callback is implemented
   *
   * @param cb The callback to check
   * @return Whether the callback is implemented
   */
  bool is_implemented (Callback cb);

  /// Fill the session ops according to the implemented callbacks
  void fill_callbacks (xio_session_ops& ses_ops);

private:
  /// The implemented callbacks
  Callback implemented_callbacks_;
};


/**
 * A server instance.
 * Can be bound to a URI and accept new connections
 */
class XIO_Server : public XIO_Callback_Implementor
{
public:
  /**
   * Create a new server
   *
   * @param implemented_callbacks Flags indicating which callbacks where
   *                              implemented
   */
  XIO_Server (Callback implemented_callbacks);
  virtual ~XIO_Server ();

  /**
   * Bind server to a uri
   *
   * @param ctx Context handle
   * @param uri The uri to listen on
   * @param src_port returned listen port in host order, can be NULL if
   *                 not needed
   * @param flags message related flags as defined in xio_msg_flags
   *
   * @return xio server context, or NULL upon error.
   */
  struct xio_server* open (struct xio_context *ctx,
                           const char *uri,
                           uint16_t *src_port,
                           int32_t flags);

  /**
   * Unbind and close the server
   *
   * @note This will not close sessions spawned by this server
   */
  int close ();

  /// Accessor to the server handle
  struct xio_server* server ();

protected:
  /// The server handle (NULL before open is called)
  struct xio_server *server_;
};

/**
 * A request session.
 *
 * New connections can be created after the session is opened.
 * Note that the implemented callbacks passed to the session include
 * the callbacks implemented by the session as well as callbacks
 * implemented by the connection objects that will be used...
 */
class XIO_Reqeust_Session : public XIO_Callback_Implementor
{
public:
  /**
   * Create a new session
   *
   * @param implemented_callbacks Flags indicating which callbacks where
   *                              implemented
   * @note The implemented callbacks passed to the session include the
   *       callbacks implemented by the session as well as callbacks
   *       implemented by the connection objects that will be used.
   */
  XIO_Reqeust_Session (Callback implemented_callbacks);
  virtual ~XIO_Reqeust_Session ();

  /**
   * Open a new request session.
   *
   * @param uri The uri to connect
   * @param initial_sn initial serial number to start with
   * @param flags message related flags as defined in xio_msg_flags
   * @param user_context sent to server upon new session
   * @param user_context_len size of the user_context
   */
  struct xio_session* open (const char *uri,
                            uint32_t initial_sn,
                            uint32_t flags,
                            void* user_context,
                            size_t user_context_len);

  /**
   * Close the session.
   * This should only be called if no connections were made or if a
   * session teardown event was received
   */
  int close ();

  /// Accessor to the session handle
  struct xio_session* session ();

private:
  /// The session (NULL before open is called)
  struct xio_session* session_;
};


/**
 * A connection.
 *
 * This is opened on the client side
 */
class XIO_Connection : public XIO_Callback_Implementor
{
public:
  /**
   * Create a new empty connection
   */
  XIO_Connection ();
  virtual ~XIO_Connection ();

  /**
   * Open the connection
   *
   * @param session The session object the new connection belongs to
   * @param ctx Context handle
   * @param conn_idx Index of this connection in the session (0 means
   *                 auto)
   */
  struct xio_connection* open (XIO_Reqeust_Session *session,
                               struct xio_context *ctx,
                               int conn_idx);

  /**
   * Mark the connection as closed
   * This should only be called after getting
   * XIO_SESSION_CONNECTION_CLOSED_EVENT for this connection
   */
  void close ();

  /// Accessor to the session
  XIO_Reqeust_Session* session ();
  /// Accessor to the connection handle
  struct xio_connection* connection ();

private:
  /// The session this connection belongs to
  XIO_Reqeust_Session* session_;
  /// The connection (NULL before open is called)
  struct xio_connection* connection_;
};

#endif // XIO_ACE_SESSION_H
