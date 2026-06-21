#include <KSBAsyncNTP.h>

#include <AsyncNtpClient.h>
#include <FirmwareUpdateState.h>

extern boolean ftimesynced;

static AsyncNtpClient ntpClient;
static String activeNtpServer;
static bool ntpClientStarted = false;
static const uint16_t NTP_SYNC_INTERVAL_MINUTES = 5;

static time_t adjustedNtpEpoch()
{
  const uint32_t elapsedMs = millis() - ntpClient.lastSyncMillis();
  return static_cast<time_t>(ntpClient.lastSyncEpoch())
       + static_cast<time_t>(MyConfParam.v_ntptz) * SECS_PER_HOUR
       + static_cast<time_t>(elapsedMs / 1000UL);
}

void ntpBegin()
{
  if (firmwareUpdateInProgress) return;

  String configuredServer = MyConfParam.v_timeserver;
  configuredServer.trim();
  if (configuredServer.length() == 0) {
    configuredServer = F("pool.ntp.org");
  }

  if (ntpClientStarted && configuredServer == activeNtpServer) {
    return;
  }

  activeNtpServer = configuredServer;
  ntpClient.begin(activeNtpServer, NTP_SYNC_INTERVAL_MINUTES, true);
  ntpClientStarted = true;
}

void ntpStopForFirmwareUpdate()
{
  ntpClient.stop();
  ntpClientStarted = false;
  activeNtpServer = "";
}

void ntpLoop()
{
  if (firmwareUpdateInProgress) return;

  ntpBegin();
  ntpClient.update();
}

time_t timeprovider(void)
{
  ntpLoop();
  if (ntpClient.isSynced()) {
    ftimesynced = true;
    setSyncInterval(300);
    return adjustedNtpEpoch();
  }

  setSyncInterval(10);
  return 0;
}
