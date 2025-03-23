#pragma once

#include "esphome/core/component.h"
#include <ESPAsyncWebServer.h>

namespace esphome {
namespace samba_server {

class WebServer {
 public:
  void setup();
  void loop();

 protected:
  AsyncWebServer server_{80};
  
  void handle_list_directory(AsyncWebServerRequest *request);
  void handle_download_file(AsyncWebServerRequest *request);
  void handle_upload_file(AsyncWebServerRequest *request);
  void handle_delete_file(AsyncWebServerRequest *request);
  void handle_rename_file(AsyncWebServerRequest *request);
};

}  // namespace samba_server
}  // namespace esphome

