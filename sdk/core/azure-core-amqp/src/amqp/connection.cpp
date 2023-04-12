// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-Licence-Identifier: MIT

#include "azure/core/amqp/connection.hpp"
#include "azure/core/amqp/common/global_state.hpp"
#include "azure/core/amqp/network/private/transport_impl.hpp"
#include "azure/core/amqp/network/socket_transport.hpp"
#include "azure/core/amqp/network/tls_transport.hpp"
#include "azure/core/amqp/private/connection_impl.hpp"
#include <azure/core/url.hpp>
#include <azure/core/uuid.hpp>
#include <memory>

#include <azure_uamqp_c/connection.h>

namespace Azure { namespace Core { namespace Amqp {
  namespace _internal {

    // Create a connection with an existing networking Transport.
    Connection::Connection(
        std::shared_ptr<Network::_internal::Transport> transport,
        ConnectionOptions const& options,
        ConnectionEvents* eventHandler)
        : m_impl{std::make_shared<Azure::Core::Amqp::_detail::ConnectionImpl>(
            transport,
            options,
            eventHandler)}
    {
      m_impl->FinishConstruction();
    }

    // Create a connection with a request URI and options.
    Connection::Connection(
        std::string const& requestUri,
        ConnectionOptions const& options,
        ConnectionEvents* eventHandler)
        : m_impl{std::make_shared<Azure::Core::Amqp::_detail::ConnectionImpl>(
            requestUri,
            options,
            eventHandler)}
    {
      m_impl->FinishConstruction();
    }

    // Connection::Connection(ConnectionEvents* eventHandler, ConnectionOptions const& options)
    //     : m_impl{std::make_shared<_detail::ConnectionImpl>(eventHandler, options)}
    //{
    //   m_impl->FinishConstruction();
    // }

    Connection::~Connection() {}

    void Connection::Poll() const { m_impl->Poll(); }

    void Connection::Listen() { m_impl->Listen(); }
    void Connection::SetTrace(bool enableTrace) { m_impl->SetTrace(enableTrace); }
    void Connection::Open() { m_impl->Open(); }
    void Connection::Close(
        std::string const& condition,
        std::string const& description,
        Azure::Core::Amqp::Models::AmqpValue value)
    {
      m_impl->Close(condition, description, value);
    }
    uint32_t Connection::GetMaxFrameSize() const { return m_impl->GetMaxFrameSize(); }
    void Connection::SetMaxFrameSize(uint32_t maxFrameSize)
    {
      m_impl->SetMaxFrameSize(maxFrameSize);
    }
    uint32_t Connection::GetRemoteMaxFrameSize() const { return m_impl->GetRemoteMaxFrameSize(); }
    uint16_t Connection::GetMaxChannel() const { return m_impl->GetMaxChannel(); }
    void Connection::SetMaxChannel(uint16_t channel) { m_impl->SetMaxChannel(channel); }
    std::chrono::milliseconds Connection::GetIdleTimeout() const
    {
      return m_impl->GetIdleTimeout();
    }
    void Connection::SetIdleTimeout(std::chrono::milliseconds timeout)
    {
      m_impl->SetIdleTimeout(timeout);
    }
    void Connection::SetRemoteIdleTimeoutEmptyFrameSendRatio(double idleTimeoutEmptyFrameSendRatio)
    {
      return m_impl->SetRemoteIdleTimeoutEmptyFrameSendRatio(idleTimeoutEmptyFrameSendRatio);
    }

    void Connection::SetProperties(Azure::Core::Amqp::Models::AmqpValue properties)
    {
      m_impl->SetProperties(properties);
    }
    Azure::Core::Amqp::Models::AmqpValue Connection::GetProperties() const
    {
      return m_impl->GetProperties();
    }
  } // namespace _internal
  namespace _detail {

    namespace {
      void EnsureGlobalStateInitialized()
      {
        // Force the global instance to exist. This is required to ensure that uAMQP and
        // azure-c-shared-utility is
        auto globalInstance
            = Azure::Core::Amqp::Common::_detail::GlobalStateHolder::GlobalStateInstance();
        (void)globalInstance;
      }
    } // namespace

    // Create a connection with an existing networking Transport.
    ConnectionImpl::ConnectionImpl(
        std::shared_ptr<Network::_internal::Transport> transport,
        _internal::ConnectionOptions const& options,
        _internal::ConnectionEvents* eventHandler)
        : m_hostName{options.HostName}, m_options{options}, m_eventHandler{eventHandler}
    {
      if (options.SaslCredentials)
      {
        throw std::runtime_error("Sasl Credentials should not be provided with a transport.");
      }
      EnsureGlobalStateInitialized();
      m_transport = transport;
    }

    // Create a connection with a request URI and options.
    ConnectionImpl::ConnectionImpl(
        std::string const& requestUri,
        _internal::ConnectionOptions const& options,
        _internal::ConnectionEvents* eventHandler)
        : m_options{options}, m_eventHandler{eventHandler}
    {
      EnsureGlobalStateInitialized();

      if (options.SaslCredentials)
      {
        throw std::runtime_error("Sasl Credentials should not be provided with a request URI.");
      }
      Azure::Core::Url requestUrl(requestUri);
      std::shared_ptr<Network::_internal::Transport> requestTransport;
      if (requestUrl.GetScheme() == "amqp")
      {
        m_transport = std::make_shared<Azure::Core::Amqp::Network::_internal::SocketTransport>(
            requestUrl.GetHost(), requestUrl.GetPort() ? requestUrl.GetPort() : 5672);
      }
      else if (requestUrl.GetScheme() == "amqps")
      {
        m_transport = std::make_shared<Azure::Core::Amqp::Network::_internal::TlsTransport>(
            requestUrl.GetHost(), requestUrl.GetPort() ? requestUrl.GetPort() : 5671);
      }
      m_hostName = requestUrl.GetHost();
    }

    ConnectionImpl::~ConnectionImpl()
    {
      // If the connection is going away, we don't want to generate any more events on it.
      if (m_eventHandler)
      {
        m_eventHandler = nullptr;
      }
      if (m_connection)
      {
        connection_destroy(m_connection);
        m_connection = nullptr;
      }
    }

    void ConnectionImpl::FinishConstruction()
    {
      std::string containerId{m_options.ContainerId};
      if (containerId.empty())
      {
        containerId = Azure::Core::Uuid::CreateUuid().ToString();
      }

      m_connection = connection_create2(
          *m_transport->GetImpl(),
          m_hostName.c_str(),
          containerId.c_str(),
          OnNewEndpointFn,
          this,
          OnConnectionStateChangedFn,
          this,
          OnIoErrorFn,
          this);
      SetTrace(m_options.EnableTrace);
      //    SetIdleTimeout(options.IdleTimeout);

      //    SetMaxFrameSize(options.MaxFrameSize);
      //    SetMaxChannel(options.MaxSessions);
      //    SetProperties(options.Properties);

      //    std::string SASLType; // Not a string - fill in later.
      //    std::chrono::seconds Timeout{0};
    }

    void ConnectionImpl::Poll() const
    {
      if (m_transport)
      {
        //      xio_dowork(*m_transport);
        connection_dowork(m_connection);
      }
    }

    _internal::ConnectionState ConnectionStateFromCONNECTION_STATE(CONNECTION_STATE state)
    {
      switch (state)
      {
        case CONNECTION_STATE_START:
          return _internal::ConnectionState::Start;
        case CONNECTION_STATE_CLOSE_PIPE: // LCOV_EXCL_LINE
          return _internal::ConnectionState::ClosePipe; // LCOV_EXCL_LINE
        case CONNECTION_STATE_CLOSE_RCVD: // LCOV_EXCL_LINE
          return _internal::ConnectionState::CloseReceived; // LCOV_EXCL_LINE
        case CONNECTION_STATE_END:
          return _internal::ConnectionState::End;
        case CONNECTION_STATE_HDR_RCVD: // LCOV_EXCL_LINE
          return _internal::ConnectionState::HeaderReceived; // LCOV_EXCL_LINE
        case CONNECTION_STATE_HDR_SENT:
          return _internal::ConnectionState::HeaderSent;
        case CONNECTION_STATE_HDR_EXCH:
          return _internal::ConnectionState::HeaderExchanged;
        case CONNECTION_STATE_OPEN_PIPE: // LCOV_EXCL_LINE
          return _internal::ConnectionState::OpenPipe; // LCOV_EXCL_LINE
        case CONNECTION_STATE_OC_PIPE: // LCOV_EXCL_LINE
          return _internal::ConnectionState::OcPipe; // LCOV_EXCL_LINE
        case CONNECTION_STATE_OPEN_RCVD: // LCOV_EXCL_LINE
          return _internal::ConnectionState::OpenReceived; // LCOV_EXCL_LINE
        case CONNECTION_STATE_OPEN_SENT:
          return _internal::ConnectionState::OpenSent;
        case CONNECTION_STATE_OPENED:
          return _internal::ConnectionState::Opened;
        case CONNECTION_STATE_CLOSE_SENT: // LCOV_EXCL_LINE
          return _internal::ConnectionState::CloseSent; // LCOV_EXCL_LINE
        case CONNECTION_STATE_DISCARDING:
          return _internal::ConnectionState::Discarding;
        case CONNECTION_STATE_ERROR: // LCOV_EXCL_LINE
          return _internal::ConnectionState::Error; // LCOV_EXCL_LINE
        default: // LCOV_EXCL_LINE
          throw std::logic_error("Unknown connection state"); // LCOV_EXCL_LINE
      }
    }

    void ConnectionImpl::OnConnectionStateChangedFn(
        void* context,
        CONNECTION_STATE newState,
        CONNECTION_STATE oldState)
    {
      ConnectionImpl* connection = static_cast<ConnectionImpl*>(context);

      if (connection->m_eventHandler)
      {
        connection->m_eventHandler->OnConnectionStateChanged(
            connection->shared_from_this(),
            ConnectionStateFromCONNECTION_STATE(newState),
            ConnectionStateFromCONNECTION_STATE(oldState));
      }
    }

    bool ConnectionImpl::OnNewEndpointFn(void* context, ENDPOINT_HANDLE newEndpoint)
    {
      ConnectionImpl* cn = static_cast<ConnectionImpl*>(context);
      _internal::Endpoint endpoint(newEndpoint);
      if (cn->m_eventHandler)
      {
        return cn->m_eventHandler->OnNewEndpoint(cn->shared_from_this(), endpoint);
      }
      return false; // LCOV_EXCL_LINE
    }

    void ConnectionImpl::OnIoErrorFn(void* context) // LCOV_EXCL_LINE
    { // LCOV_EXCL_LINE
      ConnectionImpl* cn = static_cast<ConnectionImpl*>(context); // LCOV_EXCL_LINE
      if (cn->m_eventHandler) // LCOV_EXCL_LINE
      { // LCOV_EXCL_LINE
        return cn->m_eventHandler->OnIoError(cn->shared_from_this()); // LCOV_EXCL_LINE
      } // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    void ConnectionImpl::Open()
    {
      if (connection_open(m_connection))
      {
        throw std::runtime_error("Could not open connection."); // LCOV_EXCL_LINE
      }
    }

    void ConnectionImpl::Listen()
    {
      if (connection_listen(m_connection))
      {
        throw std::runtime_error("Could not listen on connection."); // LCOV_EXCL_LINE
      }
    }

    void ConnectionImpl::SetTrace(bool setTrace) { connection_set_trace(m_connection, setTrace); }

    void ConnectionImpl::Close(
        const std::string& condition,
        const std::string& description,
        Azure::Core::Amqp::Models::AmqpValue info)
    {
      if (!m_connection)
      {
        throw std::logic_error("Connection already closed."); // LCOV_EXCL_LINE
      }

      if (connection_close(
              m_connection,
              (condition.empty() ? nullptr : condition.c_str()),
              (description.empty() ? nullptr : description.c_str()),
              info))
      {
        throw std::runtime_error("Could not close connection.");
      }
    }

    void ConnectionImpl::SetMaxFrameSize(uint32_t maxSize)
    {
      if (connection_set_max_frame_size(m_connection, maxSize))
      {
        throw std::runtime_error("COuld not set max frame size."); // LCOV_EXCL_LINE
      }
    }
    uint32_t ConnectionImpl::GetMaxFrameSize() const
    {
      uint32_t maxSize;
      if (connection_get_max_frame_size(m_connection, &maxSize))
      {
        throw std::runtime_error("COuld not get max frame size."); // LCOV_EXCL_LINE
      }
      return maxSize;
    }

    void ConnectionImpl::SetMaxChannel(uint16_t maxChannel)
    {
      if (connection_set_channel_max(m_connection, maxChannel))
      {
        throw std::runtime_error("COuld not set channel max."); // LCOV_EXCL_LINE
      }
    }
    uint16_t ConnectionImpl::GetMaxChannel() const
    {
      uint16_t maxChannel;
      if (connection_get_channel_max(m_connection, &maxChannel))
      {
        throw std::runtime_error("COuld not get channel max."); // LCOV_EXCL_LINE
      }
      return maxChannel;
    }

    void ConnectionImpl::SetIdleTimeout(std::chrono::milliseconds idleTimeout)
    {
      if (connection_set_idle_timeout(m_connection, static_cast<milliseconds>(idleTimeout.count())))
      {
        throw std::runtime_error("COuld not set idle timeout."); // LCOV_EXCL_LINE
      }
    }
    std::chrono::milliseconds ConnectionImpl::GetIdleTimeout() const
    {
      milliseconds ms;

      if (connection_get_idle_timeout(m_connection, &ms))
      {
        throw std::runtime_error("COuld not set max frame size."); // LCOV_EXCL_LINE
      }
      return std::chrono::milliseconds(ms);
    }

    void ConnectionImpl::SetProperties(Azure::Core::Amqp::Models::AmqpValue value)
    {
      if (connection_set_properties(m_connection, value))
      {
        throw std::runtime_error("COuld not set properties."); // LCOV_EXCL_LINE
      }
    }
    Azure::Core::Amqp::Models::AmqpValue ConnectionImpl::GetProperties() const
    {
      AMQP_VALUE value;
      if (connection_get_properties(m_connection, &value))
      {
        throw std::runtime_error("COuld not get properties."); // LCOV_EXCL_LINE
      }
      return value;
    }

    uint32_t ConnectionImpl::GetRemoteMaxFrameSize() const
    {
      uint32_t maxFrameSize;
      if (connection_get_remote_max_frame_size(m_connection, &maxFrameSize))
      {
        throw std::runtime_error("Could not get remote max frame size."); // LCOV_EXCL_LINE
      }
      return maxFrameSize;
    }
    void ConnectionImpl::SetRemoteIdleTimeoutEmptyFrameSendRatio(double ratio)
    {
      if (connection_set_remote_idle_timeout_empty_frame_send_ratio(m_connection, ratio))
      {
        throw std::runtime_error(
            "Could not set remote idle timeout send frame ratio."); // LCOV_EXCL_LINE
      }
    }

  } // namespace _detail
}}} // namespace Azure::Core::Amqp