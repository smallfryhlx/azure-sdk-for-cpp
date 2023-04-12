// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-Licence-Identifier: MIT

#include "azure/core/amqp/network/transport.hpp"
#include "azure/core/amqp/common/completion_operation.hpp"
#include "azure/core/amqp/common/global_state.hpp"
#include "azure/core/amqp/network/private/transport_impl.hpp"
#include <azure_c_shared_utility/platform.h>
#include <azure_c_shared_utility/xio.h>
#include <cassert>

namespace Azure { namespace Core { namespace Amqp { namespace Network { namespace _internal {
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

  Transport::Transport(TransportEvents* eventHandler)
      : m_impl{std::make_shared<_detail::TransportImpl>(eventHandler)}
  {
  }

  Transport::Transport(XIO_HANDLE xio, TransportEvents* eventHandler)
      : m_impl{std::make_shared<_detail::TransportImpl>(xio, eventHandler)}
  {
  }
  Transport::~Transport() {}

  bool Transport::Open() { return m_impl->Open(); }
  bool Transport::Close(TransportCloseCompleteFn callback) { return m_impl->Close(callback); }

  bool Transport::Send(uint8_t* buffer, size_t size, TransportSendCompleteFn callback) const
  {
    return m_impl->Send(buffer, size, callback);
  }
  void Transport::Poll() const { return m_impl->Poll(); }

  void Transport::SetInstance(XIO_HANDLE handle) { m_impl->SetInstance(handle); }

  namespace _detail {

    TransportImpl::TransportImpl(TransportEvents* eventHandler)
        : m_xioInstance(nullptr), m_eventHandler{eventHandler}
    {
      EnsureGlobalStateInitialized();
    }
    TransportImpl::TransportImpl(XIO_HANDLE handle, TransportEvents* eventHandler)
        : m_xioInstance{handle}, m_eventHandler{eventHandler}
    {
    }

    void TransportImpl::SetInstance(XIO_HANDLE handle)
    {
      assert(m_xioInstance == nullptr);
      m_xioInstance = handle;

      EnsureGlobalStateInitialized();
    }

    TransportImpl::~TransportImpl()
    {
      if (m_xioInstance)
      {
        xio_destroy(m_xioInstance);
        m_xioInstance = nullptr;
      }
    }

    template <typename CompleteFn> struct CloseCallbackWrapper
    {
      static void OnOperation(CompleteFn onComplete) { onComplete(); }
    };

    bool TransportImpl::Close(TransportCloseCompleteFn onCloseComplete)
    {
      if (!m_isOpen)
      {
        throw std::logic_error("Cannot close an unopened transport.");
      }
      auto closeOperation
          = std::make_unique<Azure::Core::Amqp::Common::_internal::CompletionOperation<
              decltype(onCloseComplete),
              CloseCallbackWrapper<decltype(onCloseComplete)>>>(onCloseComplete);
      if (m_xioInstance)
      {
        if (xio_close(
                m_xioInstance,
                std::remove_pointer<decltype(closeOperation.get())>::type::OnOperationFn,
                closeOperation.release()))
        {
          return false;
        }
        m_xioInstance = nullptr;
      }
      return true;
    }

    void TransportImpl::OnOpenCompleteFn(void* context, IO_OPEN_RESULT ioOpenResult)
    {
      TransportImpl* transport = reinterpret_cast<TransportImpl*>(context);
      if (transport->m_eventHandler)
      {
        TransportOpenResult openResult{TransportOpenResult::Error};
        switch (ioOpenResult)
        {
          case IO_OPEN_RESULT_INVALID: // LCOV_EXCL_LINE
            openResult = TransportOpenResult::Invalid; // LCOV_EXCL_LINE
            break;
          case IO_OPEN_OK:
            openResult = TransportOpenResult::Ok;
            break;
          case IO_OPEN_CANCELLED: // LCOV_EXCL_LINE
            openResult = TransportOpenResult::Cancelled; // LCOV_EXCL_LINE
            break; // LCOV_EXCL_LINE
          case IO_OPEN_ERROR: // LCOV_EXCL_LINE
            openResult = TransportOpenResult::Error; // LCOV_EXCL_LINE
            break; // LCOV_EXCL_LINE
        }
        transport->m_eventHandler->OnOpenComplete(openResult);
      }
    }

    void TransportImpl::OnBytesReceivedFn(void* context, unsigned char const* buffer, size_t size)
    {
      TransportImpl* transport = reinterpret_cast<TransportImpl*>(context);
      if (transport->m_eventHandler)
      {
        transport->m_eventHandler->OnBytesReceived(transport->shared_from_this(), buffer, size);
      }
    }

    // LCOV_EXCL_START
    void TransportImpl::OnIoErrorFn(void* context)
    {
      TransportImpl* transport = reinterpret_cast<TransportImpl*>(context);
      if (transport->m_eventHandler)
      {

        transport->m_eventHandler->OnIoError();
      }
    }
    // LCOV_EXCL_STOP

    bool TransportImpl::Open()
    {
      if (m_isOpen)
      {
        throw std::logic_error("Cannot open an opened transport.");
      }

      if (xio_open(
              m_xioInstance, OnOpenCompleteFn, this, OnBytesReceivedFn, this, OnIoErrorFn, this))
      {
        return false;
      }
      m_isOpen = true;
      return true;
    }

    template <typename CompleteFn> struct SendCallbackRewriter
    {
      static void OnOperation(CompleteFn onComplete, IO_SEND_RESULT sendResult)
      {
        TransportSendResult result{TransportSendResult::Ok};
        switch (sendResult)
        {
          case IO_SEND_RESULT_INVALID: // LCOV_EXCL_LINE
            result = TransportSendResult::Invalid; // LCOV_EXCL_LINE
            break;
          case IO_SEND_OK:
            result = TransportSendResult::Ok;
            break;
          case IO_SEND_CANCELLED: // LCOV_EXCL_LINE
            result = TransportSendResult::Cancelled; // LCOV_EXCL_LINE
            break; // LCOV_EXCL_LINE
          case IO_SEND_ERROR: // LCOV_EXCL_LINE
            result = TransportSendResult::Error; // LCOV_EXCL_LINE
            break; // LCOV_EXCL_LINE
        }
        onComplete(result);
      }
    };

    bool TransportImpl::Send(
        unsigned char* buffer,
        size_t size,
        TransportSendCompleteFn sendComplete) const
    {
      auto operation{std::make_unique<Azure::Core::Amqp::Common::_internal::CompletionOperation<
          decltype(sendComplete),
          SendCallbackRewriter<decltype(sendComplete)>>>(sendComplete)};
      if (xio_send(
              m_xioInstance,
              buffer,
              size,
              std::remove_pointer<decltype(operation)::element_type>::type::OnOperationFn,
              operation.release()))
      {
        return false;
      }
      return true;
    }

    void TransportImpl::Poll() const
    {
      if (m_xioInstance)
      {
        xio_dowork(m_xioInstance);
      }
    }
  } // namespace _detail
}}}}} // namespace Azure::Core::Amqp::Network::_internal