#ifndef TICKCOUNTER_H
#define TICKCOUNTER_H

//Tick count that takes longer to roll over
class TickCounter
{
  private:
  
    unsigned long long _tickCounter = 0;
    unsigned long _lastCpuTickCount = 0;

  public:
  
    //The tick counter will roll over in 53 seconds, this must be called at least every 26 sec
    unsigned long long getTicks()
    {
      unsigned long ticks = ESP.getCycleCount();
      // Use 0xFFFFFFFF (32-bit) mask — 0x7FFFFFFF (31-bit) discarded the MSB,
      // causing incorrect deltas whenever the MSB was set (~half of all wraps).
      unsigned long delta = (ticks - _lastCpuTickCount) & 0xFFFFFFFF;
      _lastCpuTickCount = ticks;
      _tickCounter += delta;
      return _tickCounter;
    }

    unsigned long long getMillis()
    {
      #ifndef ESP32
      return getTicks() / (ESP8266_CLOCK / 1000);
      #else
      // Use the actual CPU frequency — hardcoded 40 MHz was wrong at 80/160/240 MHz.
      return getTicks() / ((unsigned long long)ESP.getCpuFreqMHz() * 1000ULL);
      #endif
    }

    unsigned long getSeconds()
    {
      #ifndef ESP32
      return getTicks() / ESP8266_CLOCK;
      #else
      return getTicks() / ((unsigned long long)ESP.getCpuFreqMHz() * 1000000ULL);
      #endif
    }
};

class PollDelay
{
  private:
    
    unsigned long long _startMillis;
    TickCounter* _tickCounter;

  public:
    
    PollDelay(TickCounter& tickCounter)
    {
      _tickCounter = &tickCounter;
      _startMillis = _tickCounter->getMillis();
    }
  
    void reset()
    {
      _startMillis = _tickCounter->getMillis();
    }

    //Call this once every 26 seconds or it'll roll over
    int compare(unsigned int millisSinceStart)
    {
      return (int)( _tickCounter->getMillis() - (_startMillis + millisSinceStart) );
    }
};

#endif
