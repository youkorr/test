#pragma once

#include <AsyncWebServer.h>
#include <SD.h>
#include <SPI.h>
#include <esphome/core/component.h>
#include <esphome/components/sdcard/sdcard.h>

namespace webdav {

class WebDAV : public esphome::Component {
 public:
  WebDAV(esphome::sdcard::SDCardComponent *sd_card, const char *mount_point, const char *username, const char *password)
      : sd_card_(sd_card), mount_point_(mount_point), username_(username), password_(password) {}

  static WebDAV *create(esphome::sdcard::SDCardComponent *sd_card, const char *mount_point, const char *username, const char *password) {
    WebDAV *webdav = new WebDAV(sd_card, mount_point, username, password);
    webdav->setup();
    return webdav;
  }

  void setup() override {
    AsyncWebServer server(80);
    server.on("/sdcard/*", HTTP_GET, [this](AsyncWebServerRequest *request) {
      // Implémentation de la requête GET
      request->send(200, "text/plain", "WebDAV not fully implemented yet");
    });
    server.begin();
  }

 protected:
  esphome::sdcard::SDCardComponent *sd_card_;
  const char *mount_point_;
  const char *username_;
  const char *password_;
};

}  // namespace webdav
