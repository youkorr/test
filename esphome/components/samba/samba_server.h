#pragma once

#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/sd_mmc_card/sd_mmc_card.h"

namespace esphome {
namespace samba {

class SambaServer : public Component, public web_server_base::RequestHandler {
 public:
  SambaServer(web_server_base::WebServerBase *base);

  void setup() override;
  void dump_config() override;

  // Web server request handling
  bool canHandle(AsyncWebServerRequest *request) override;
  void handleRequest(AsyncWebServerRequest *request) override;

  // Configuration
  void set_share_name(std::string const &name);
  void set_root_path(std::string const &path);
  void set_sd_mmc_card(sd_mmc_card::SdMmc *card);

  // Samba server control
  bool start_samba_server();
  bool stop_samba_server();

 protected:
  void init_samba_server();
  void register_web_handlers();
  void handle_ui(AsyncWebServerRequest *request);

  // Static task function for FreeRTOS
  static void samba_server_task(void *pvParameters);

  // Properties
  web_server_base::WebServerBase *base_;
  sd_mmc_card::SdMmc *sd_mmc_card_{nullptr};
  std::string share_name_{"ESP32"};
  std::string root_path_{"/sdcard"};
  bool is_running_{false};
};

}  // namespace samba
}  // namespace esphome
