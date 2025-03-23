#pragma once

#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_types.h"
#include "esp_http_server.h"
#include <string>
#include "../sd_mmc_card/sd_mmc_card.h"

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
  void set_root_path(const std::string &root_path) { root_path_ = root_path; }
  void set_sd_mmc_card(sdmmc_card_t *card) { sd_card_ = card; }

 protected:
  std::string workgroup_{"WORKGROUP"};
  std::string share_name_{"ESP32"};
  std::string username_{"esp32"};
  std::string password_{"password"};
  std::string root_path_{"/sdcard"};

  sdmmc_card_t *sd_card_{nullptr};
  httpd_handle_t server_{nullptr};

  esp_err_t start_http_server();
};

}  // namespace samba_server
}  // namespace esphome







