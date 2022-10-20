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



#include <rtac_asio/StreamWriter.h>

using namespace std::placeholders;

namespace rtac { namespace asio {

StreamWriter::StreamWriter(StreamInterface::Ptr stream) :
    stream_(stream),
    writeCounter_(0),
    writeId_(0),
    timer_(stream_->service()->service())
{}

StreamWriter::Ptr StreamWriter::Create(StreamInterface::Ptr stream)
{
    return Ptr(new StreamWriter(stream));
}

void StreamWriter::flush()
{
    stream_->flush();
}

void StreamWriter::reset()
{
    stream_->reset();
}

void StreamWriter::async_write_some(std::size_t count,
                                    const uint8_t* data,
                                    Callback callback)
{
    stream_->async_write_some(count, data, callback);
    if(!stream_->service()->is_running()) {
        stream_->service()->start();
    }
}

void StreamWriter::async_write(std::size_t count, const uint8_t* data,
                               Callback callback, unsigned int timeoutMillis)
{
    if(writeId_ != 0) {
        throw std::runtime_error("Another write was in progress : cannot call another write");
    }

    writeCounter_++;
    writeId_        = writeCounter_;
    requestedSize_ = count;
    processed_     = 0;
    src_           = data;
    callback_      = callback;
    
    if(timeoutMillis > 0) {
        timer_.expires_from_now(Millis(timeoutMillis));
        timer_.async_wait(std::bind(&StreamWriter::timeout_reached, this, writeId_, _1));
    }

    this->async_write_some(requestedSize_, src_,
        std::bind(&StreamWriter::async_write_continue, this, writeId_, _1, _2));
}

void StreamWriter::async_write_continue(unsigned int writeId,
                                        const ErrorCode& err,
                                        std::size_t writtenCount)
{
    if(writeId_ != writeId) {
        // probably a timeout was reached
        return;
    }

    processed_ += writtenCount;
    if(!err && processed_ < requestedSize_) {
        this->async_write_some(requestedSize_ - processed_, src_ + processed_,
            std::bind(&StreamWriter::async_write_continue, this, writeId_, _1, _2));
    }
    else {
        writeId_ = 0;
        timer_.cancel();
        //callback_(err, processed_);
        stream_->service()->post(std::bind(callback_, err, processed_));
    }
}

void StreamWriter::timeout_reached(unsigned int writeId, const ErrorCode& err)
{
    if(writeId != writeId_) {
        // timer abort probably failed
        return;
    }
    
    waiterNotified_ = true;
    waiter_.notify_all();
    writeId_ = 0;
    //callback_(err, processed_);
    stream_->service()->post(std::bind(callback_, err, processed_));
}

std::size_t StreamWriter::write(std::size_t count, const uint8_t* data,
                                unsigned int timeoutMillis)
{
    std::unique_lock<std::mutex> lock(mutex_); // will release mutex when out of scope

    this->async_write(count, data, std::bind(&StreamWriter::write_callback, this, _1, _2),
                     timeoutMillis);

    waiterNotified_ = false; // this protects against spurious wakeups.
    waiter_.wait(lock, [&]{ return waiterNotified_; });

    return processed_;
}

void StreamWriter::write_callback(const ErrorCode& err, std::size_t writtenCount)
{
    waiterNotified_ = true;
    waiter_.notify_all();
}

} //namespace asio
} //namespace rtac


