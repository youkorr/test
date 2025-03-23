#pragma once
#include "esphome.h"
#include <AsyncWebServer_ESP32_SC_W5500.h>
#include <ESPAsyncWebDav.h>
#include "../sd_mmc_card/sd_mmc_card.h"

namespace esphome {
namespace sd_webdav {

class SDWebDAV : public Component {
 public:
  void set_sd_card(sdmc::SDMmcCard *sd_card) { sd_card_ = sd_card; }
  void set_mount_point(const std::string &mount_point) { mount_point_ = mount_point; }
  void set_username(const std::string &username) { username_ = username; }
  void set_password(const std::string &password) { password_ = password; }

  void setup() override;
  void loop() override;

 protected:
  sdmc::SDMmcCard *sd_card_;
  std::string mount_point_;
  std::string username_;
  std::string password_;
  AsyncWebServer *server_{nullptr};
  AsyncWebdav *dav_{nullptr};
};

}  // namespace sd_webdav
}  // namespace esphome

