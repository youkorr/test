#pragma once

#include "esphome.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "../sd_mmc_card/sd_mmc_card.h"

namespace esphome {
namespace webdav {

class WebDAVComponent : public esphome::Component, public esphome::web_server_base::WebServerHandler {
 public:
  void setup() override;
  bool canHandle(AsyncWebServerRequest *request) override;
  void handleRequest(AsyncWebServerRequest *request) override;

  void set_sd_card(esphome::sd_mmc_card::SDMMCCard *sd_card) { this->sd_card_ = sd_card; }
  void set_mount_point(const std::string &mount_point) { this->mount_point_ = mount_point; }
  void set_username(const std::string &username) { this->username_ = username; }
  void set_password(const std::string &password) { this->password_ = password; }

 protected:
  esphome::sd_mmc_card::SDMMCCard *sd_card_{nullptr};
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



