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



#include <rtac_asio/Stream.h>

using namespace std::placeholders;

namespace rtac { namespace asio {

Stream::Stream(StreamInterface::Ptr stream) :
    reader_(stream),
    writer_(stream)
{}

Stream::Ptr Stream::Create(StreamInterface::Ptr stream)
{
    return Ptr(new Stream(stream));
}

Stream::Ptr Stream::CreateSerial(const std::string& device,
                                 const SerialStream::Parameters& params)
{
    return Ptr(new Stream(SerialStream::Create(AsyncService::Create(),
                                               device, params)));
}

Stream::Ptr Stream::CreateUDPClient(const std::string& remoteIP,
                                    uint16_t remotePort)
{
    return Ptr(new Stream(UDPClientStream::Create(AsyncService::Create(),
                                                  remoteIP, remotePort)));
}

void Stream::flush()
{
    reader_.flush();
}

void Stream::reset()
{
    reader_.reset();
}

void Stream::async_read_some(std::size_t count, uint8_t* data,
                     Callback callback)
{
    reader_.async_read_some(count, data, callback);
}

void Stream::async_write_some(std::size_t count, const uint8_t* data,
                              Callback callback)
{
    writer_.async_write_some(count, data, callback);
}

void Stream::async_read(std::size_t count, uint8_t* data, Callback callback,
                        unsigned int timeoutMillis)
{
    reader_.async_read(count, data, callback, timeoutMillis);
}

void Stream::async_write(std::size_t count, const uint8_t* data,
                         Callback callback, unsigned int timeoutMillis)
{
    writer_.async_write(count, data, callback, timeoutMillis);
}

std::size_t Stream::read(std::size_t count, uint8_t* data,
                         unsigned int timeoutMillis)
{
    return reader_.read(count, data, timeoutMillis);
}

std::size_t Stream::write(std::size_t count, const uint8_t* data,
                          unsigned int timeoutMillis)
{
    return writer_.write(count, data, timeoutMillis);
}

void Stream::async_read_until(std::size_t maxSize, uint8_t* data, char delimiter,
                              Callback callback, unsigned int timeoutMillis)
{
    reader_.async_read_until(maxSize, data, delimiter, callback, timeoutMillis);
}

std::size_t Stream::read_until(std::size_t maxSize, uint8_t* data,
                               char delimiter, unsigned int timeoutMillis)
{
    return reader_.read_until(maxSize, data, delimiter, timeoutMillis);
}

} //namespace asio
} //namespace rtac
