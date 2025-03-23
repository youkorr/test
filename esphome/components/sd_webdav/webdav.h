#pragma once

#include "esphome.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "../sd_mmc_card/sd_mmc_card.h"

namespace esphome {
namespace sd_webdav {

class SDWebDAVComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  
  void set_sd_card(sd_mmc_card::SDMMCCard *sd_card) { sd_card_ = sd_card; }
  void set_mount_point(const std::string &mount_point) { mount_point_ = mount_point; }
  void set_credentials(const std::string &username, const std::string &password) {
    username_ = username;
    password_ = password;
  }

 protected:
  sd_mmc_card::SDMMCCard *sd_card_;
  std::string mount_point_;
  std::string username_;
  std::string password_;
  web_server_base::WebServerBase *web_server_;
  static const char *const TAG;
};

}  // namespace sd_webdav
}  // namespace esphome





