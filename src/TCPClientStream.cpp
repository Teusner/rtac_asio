/**
 * Boost Software License - Version 1.0 - August 17th, 2003
 * 
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 * 
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */



#include <rtac_asio/TCPClientStream.h>

#include <rtac_asio/ip_utils.h>

namespace rtac { namespace asio {

using namespace std::placeholders;

TCPClientStream::TCPClientStream(AsyncService::Ptr service,
                                 const std::string& remoteIP,
                                 uint16_t remotePort) :
    StreamInterface(service),
    socket_(nullptr)
{
    this->reset(EndPoint(make_address(remoteIP), remotePort));
}

TCPClientStream::~TCPClientStream()
{
    this->close();
}

TCPClientStream::Ptr TCPClientStream::Create(AsyncService::Ptr service,
                                             const std::string& remoteIP,
                                             uint16_t remotePort)
{
    return Ptr(new TCPClientStream(service, remoteIP, remotePort));
}

void TCPClientStream::close()
{
    if(this->is_open()) {
        std::cout << "Closing connection" << std::endl;
        try {
            ErrorCode err;
            socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, err);
            socket_->cancel();
            if(err) {
                std::cerr << "Error closing socket : '" << err << "'\n";
                return;
            }
            socket_->close();
        }
        catch(const std::exception& e) {
            std::cerr << "Error closing connection : " << e.what() << std::endl;
        }
        socket_ = nullptr;
    }
}

void TCPClientStream::reset(const EndPoint& remote)
{
    remote_ = remote;
    this->reset();
}

void TCPClientStream::reset()
{
    this->close();
    this->flush();
    
    socket_ = std::make_unique<Socket>(this->service()->service());
    socket_->connect(remote_);
}

void TCPClientStream::flush()
{
    // for compatibility with StreamInterface
}

bool TCPClientStream::is_open() const
{
    if(socket_)
        return socket_->is_open();
    return false;
}

void TCPClientStream::async_read_some(std::size_t bufferSize,
                                      uint8_t* buffer,
                                      Callback callback)
{
    socket_->async_read_some(boost::asio::buffer(buffer, bufferSize), callback);
}

void TCPClientStream::async_write_some(std::size_t count,
                                       const uint8_t* data,
                                       Callback callback)
{
    socket_->async_write_some(boost::asio::buffer(data, count), callback);
}

} //namespace asio
} //namespace rtac


