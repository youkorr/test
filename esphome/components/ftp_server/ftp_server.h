#include "esphome.h"
#include "SD.h"
#include "AsyncTCP.h"
#include "ESPAsyncWebServer.h"

class HttpServer : public Component {
 public:
  void setup() override;

 private:
  AsyncWebServer server = AsyncWebServer(8123);
};



