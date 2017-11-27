#ifndef FK_RTC_H_INCLUDED
#define FK_RTC_H_INCLUDED

#include <RTCZero.h>
#include <RTClib.h>

class FkCoreRTC {
private:
    RTCZero rtc;

public:
    void begin() {
        rtc.begin();
    }

    void setTime(uint32_t unix) {
        DateTime dt(unix);
        rtc.setYear(dt.year() - 2000);
        rtc.setMonth(dt.month());
        rtc.setDay(dt.day());
        rtc.setHours(dt.hour());
        rtc.setMinutes(dt.minute());
        rtc.setSeconds(dt.second());
    }

    uint32_t getTime() {
        DateTime dt(rtc.getYear(), rtc.getMonth(), rtc.getDay(),
                    rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());
        return dt.unixtime();
    }
};

#endif
