#ifndef API_REPORTER_H
#define API_REPORTER_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DeauthDetector.h"
#include "WifiIDSDetector.h"
#include "Config.h"

class APIReporter {
public:
    APIReporter(APIConfig& config);
    bool sendBatch(const std::vector<DeauthEvent>& events);
    bool sendIdsBatch(const std::vector<WifiIDSEvent>& events);

private:
    APIConfig& apiConfig;
    String buildPayload(const std::vector<DeauthEvent>& events);
    String buildIdsPayload(const std::vector<WifiIDSEvent>& events);
};

#endif
