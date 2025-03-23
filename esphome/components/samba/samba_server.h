#pragma once

#include "esphome/core/component.h"
#include "../sd_mmc_card/sd_mmc_card.h"
#include "web_server.h"

namespace esphome {
namespace samba_server {

class SambaServer : public Component {
 public:
  void setup() override;
  void loop() override;
  
  void set_workgroup(const std::string &workgroup) { workgroup_ = workgroup; }
  void set_share_name(const std::string &share_name) { share_name_ = share_name; }
  void set_username(const std::string &username) { username_ = username; }
  void set_password(const std::string &password) { password_ = password; }
  void set_sd_mmc_card(sd_mmc_card::SdMmc *card) { sd_mmc_card_ = card; }

 protected:
  std::string workgroup_{"WORKGROUP"};
  std::string share_name_{"ESP32"};
  std::string username_{"esp32"};
  std::string password_{"password"};
  
  sd_mmc_card::SdMmc *sd_mmc_card_{nullptr};
  std::unique_ptr<WebServer> web_server_;
};

}  // namespace samba_server
}  // namespace esphome





