#pragma once

#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "../sd_mmc_card/sd_mmc_card.h"

namespace esphome {
namespace webdav {

class WebDAVComponent : public Component, public AsyncWebHandler {
 public:
  void set_sd_card(sdcard::SDCard *sd_card) { sd_card_ = sd_card; }
  void set_mount_point(const std::string &mount_point) { mount_point_ = mount_point; }
  void set_username(const std::string &username) { username_ = username; }
  void set_password(const std::string &password) { password_ = password; }

  void setup() override;
  void loop() override {}

  bool canHandle(AsyncWebServerRequest *request) override;
  void handleRequest(AsyncWebServerRequest *request) override;

 protected:
  sdcard::SDCard *sd_card_{nullptr};
  std::string mount_point_;
  std::string username_;
  std::string password_;

  bool authenticate(AsyncWebServerRequest *request);
  void handlePropfind(AsyncWebServerRequest *request);
  void handleGet(AsyncWebServerRequest *request);
  void handlePut(AsyncWebServerRequest *request);
  void handleDelete(AsyncWebServerRequest *request);
  void handleMkcol(AsyncWebServerRequest *request);
};

}  // namespace webdav
}  // namespace esphome


