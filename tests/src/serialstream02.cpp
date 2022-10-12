#include <iostream>
#include <functional>
using namespace std;
using namespace std::placeholders;

#include <rtac_asio/AsyncService.h>
#include <rtac_asio/SerialStream.h>
#include <rtac_asio/Stream.h>
using namespace rtac::asio;

std::string msg = "Hello there !\n";

void write_callback(const SerialStream::ErrorCode& err,
                    std::size_t writeCount)
{
    std::cout << "Wrote data (" << writeCount << " bytes)." << std::endl;
}

void read_callback(Stream* serial,
                   std::string* data,
                   const SerialStream::ErrorCode& err,
                   std::size_t count)
{
    std::cout << "Got data (" << count
              << " bytes) : " << *data << std::endl;
    //serial->async_read_some(data->size(), (uint8_t*)data->c_str(),
    //                        std::bind(&read_callback, serial, data, _1, _2));
    serial->async_read(msg.size() + 1, (uint8_t*)data->c_str(),
                       std::bind(&read_callback, serial, data, _1, _2),
                       1000);
}

int main()
{
    std::string data(1024, '\0');

    auto service = AsyncService::Create();

    Stream serial(SerialStream::Create(service, "/dev/ttyACM0"));
    //serial.async_read_some(data.size(), (uint8_t*)data.c_str(),
    //                       std::bind(&read_callback, &serial, &data, _1, _2));
    serial.async_read(msg.size() + 1, (uint8_t*)data.c_str(),
                      std::bind(&read_callback, &serial, &data, _1, _2));

    service->start();
    std::cout << "Started" << std::endl;
    
    while(1) {
    //for(int i = 0; i < 5; i++) {
        getchar();
        //serial.async_write_some(msg.size() + 1,
        //                        (const uint8_t*)msg.c_str(),
        //                        &write_callback);
        serial.async_write(msg.size() + 1,
                           (const uint8_t*)msg.c_str(),
                           &write_callback);
        std::cout << "Service running ? : " << !service->stopped() << std::endl;
    }
    serial.async_read(msg.size() + 1, (uint8_t*)data.c_str(),
                      std::bind(&read_callback, &serial, &data, _1, _2));
    
    cout << "There " << endl;
    getchar();

    service->stop();

    return 0;
}
