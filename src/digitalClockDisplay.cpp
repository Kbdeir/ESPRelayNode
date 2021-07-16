#include <TimeLib.h>
#include <digitalClockDisplay.h>


String digitalClockDisplay()
{
    String t =  String(hour())+":"+printDigits(minute()) +":"+printDigits(second())+" "+String(day())+"-"+String(month())+"-"+String(year());
return t;

}

String printDigits(int digits)
{
    return digits < 10 ? "0"  + String(digits) : String(digits);
}
