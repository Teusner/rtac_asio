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



#include <rtac_asio/StreamReader.h>

using namespace std::placeholders;

namespace rtac { namespace asio {

StreamReader::StreamReader(StreamInterface::Ptr stream) :
    stream_(stream),
    readCounter_(0),
    readId_(0),
    timer_(stream_->service()->service())
{}

StreamReader::~StreamReader()
{
    stream_->service()->stop();
}

StreamReader::Ptr StreamReader::Create(StreamInterface::Ptr stream)
{
    return Ptr(new StreamReader(stream));
}

void StreamReader::flush()
{
    stream_->flush();
}

void StreamReader::reset()
{
    stream_->reset();
}

void StreamReader::async_read_some(std::size_t count,
                                   uint8_t* data,
                                   Callback callback)
{
    if(readBuffer_.size() > 0) {
        std::istream is(&readBuffer_);
        char c = is.get();
        int readCount = 0;
        while(!is.eof() && readCount < count) {
            data[readCount] = c;
            readCount++;
            c = is.get();
        }
        readBuffer_.consume(readCount);
        if(readCount >= count) {
            // readBuffer emptied.
            // Defering callback execution to avoid deadlock
            stream_->service()->post(
                std::bind(callback, ErrorCode(), readCount));
        }
    }
    else {
        stream_->async_read_some(count, data, callback);
        if(!stream_->service()->is_running()) {
            stream_->service()->start();
        }
    }
}

void StreamReader::async_read(std::size_t count, uint8_t* data,
                              Callback callback, unsigned int timeoutMillis)
{
    if(readId_ != 0) {
        throw std::runtime_error("Another read was in progress : cannot call another read");
    }

    readCounter_++;
    readId_        = readCounter_;
    requestedSize_ = count;
    processed_     = 0;
    dst_           = data;
    callback_      = callback;
    
    if(timeoutMillis > 0) {
        timer_.expires_from_now(Millis(timeoutMillis));
        timer_.async_wait(std::bind(&StreamReader::timeout_reached, this, readId_, _1));
    }

    this->async_read_some(requestedSize_, dst_,
        std::bind(&StreamReader::async_read_continue, this, readId_, _1, _2));
}

void StreamReader::async_read_continue(unsigned int readId,
                                       const ErrorCode& err,
                                       std::size_t readCount)
{
    if(readId_ != readId) {
        // probably a timeout was reached
        return;
    }

    processed_ += readCount;
    if(!err && processed_ < requestedSize_) {
        this->async_read_some(requestedSize_ - processed_, dst_ + processed_,
            std::bind(&StreamReader::async_read_continue, this, readId_, _1, _2));
    }
    else {
        readId_ = 0;
        timer_.cancel();
        //callback_(err, processed_);
        stream_->service()->post(std::bind(callback_, err, processed_));
    }
}

void StreamReader::timeout_reached(unsigned int readId, const ErrorCode& err)
{
    if(readId != readId_) {
        // timer abort probably failed
        return;
    }
    
    waiterNotified_ = true;
    waiter_.notify_all();
    readId_ = 0;
    //callback_(err, processed_);
    stream_->service()->post(std::bind(callback_, err, processed_));
}

std::size_t StreamReader::read(std::size_t count, uint8_t* data,
                               unsigned int timeoutMillis)
{
    std::unique_lock<std::mutex> lock(mutex_); // will release mutex when out of scope

    this->async_read(count, data, std::bind(&StreamReader::read_callback, this, _1, _2),
                     timeoutMillis);

    waiterNotified_ = false; // this protects against spurious wakeups.
    waiter_.wait(lock, [&]{ return waiterNotified_; });

    return processed_;
}

void StreamReader::read_callback(const ErrorCode& err, std::size_t readCount)
{
    waiterNotified_ = true;
    waiter_.notify_all();
}

void StreamReader::async_read_until(std::size_t maxSize, uint8_t* data, char delimiter,
                                    Callback callback, unsigned int timeoutMillis)
{
    if(readId_ != 0) {
        throw std::runtime_error("Another read was in progress : cannot call another read");
    }

    readCounter_++;
    readId_        = readCounter_;
    requestedSize_ = maxSize;
    processed_     = 0;
    dst_           = data;
    callback_      = callback;
    
    if(timeoutMillis > 0) {
        timer_.expires_from_now(Millis(timeoutMillis));
        timer_.async_wait(std::bind(&StreamReader::timeout_reached, this, readId_, _1));
    }
    
    this->async_read_some(requestedSize_, dst_,
        std::bind(&StreamReader::async_read_until_continue, this,
                  readId_, delimiter, _1, _2));
}

void StreamReader::async_read_until_continue(unsigned int readId,
                                             char delimiter,
                                             const ErrorCode& err,
                                             std::size_t readCount)
{
    if(readId_ != readId) {
        // probably a timeout was reached
        return;
    }
    
    const uint8_t* data = dst_ + processed_;
    int i;
    for(i = 0; i < readCount; i++) {
        if(data[i] == delimiter) {
            i++;
            int remaining = readCount - i;
            std::ostream os(&readBuffer_);
            for(; i < readCount; i++) {
                os << data[i];
            }
            readBuffer_.commit(remaining);
            break;
        }
    }

    processed_ += i;
    if(!err && processed_ < requestedSize_ && dst_[processed_ - 1] != delimiter) {
        this->async_read_some(requestedSize_ - processed_, dst_ + processed_,
            std::bind(&StreamReader::async_read_until_continue, this,
                      readId_, delimiter, _1, _2));
    }
    else {
        readId_ = 0;
        timer_.cancel();
        //callback_(err, processed_);
        stream_->service()->post(std::bind(callback_, err, processed_));
    }
}

std::size_t StreamReader::read_until(std::size_t maxSize, uint8_t* data,
                                     char delimiter, unsigned int timeoutMillis)
{
    std::unique_lock<std::mutex> lock(mutex_); // will release mutex when out of scope

    this->async_read_until(maxSize, data, delimiter,
                           std::bind(&StreamReader::read_callback, this, _1, _2),
                           timeoutMillis);

    waiterNotified_ = false; // this protects against spurious wakeups.
    waiter_.wait(lock, [&]{ return waiterNotified_; });

    return processed_;
}

} //namespace asio
} //namespace rtac


