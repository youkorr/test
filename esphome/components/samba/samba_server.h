#pragma once

#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/web_server_idf/web_server_idf.h"
#include "../sd_mmc_card/sd_mmc_card.h"

namespace esphome {
namespace samba {

class SambaComponent : public Component, public web_server_idf::AsyncWebHandler {
 public:
  explicit SambaComponent(web_server_base::WebServerBase *base);

  void setup() override;
  void dump_config() override;

  bool canHandle(web_server_idf::AsyncWebServerRequest *request) override;
  void handleRequest(web_server_idf::AsyncWebServerRequest *request) override;

  void handle_status(web_server_idf::AsyncWebServerRequest *request);
  void handle_start(web_server_idf::AsyncWebServerRequest *request);
  void handle_stop(web_server_idf::AsyncWebServerRequest *request);
  void handle_ui(web_server_idf::AsyncWebServerRequest *request);

  void set_web_server(web_server_base::WebServerBase *server) { this->base_ = server; }
  void set_sd_mmc_card(sd_mmc_card::SdMmc *card);
  void set_share_name(const std::string &name);
  void set_root_path(const std::string &path);

  void init_samba_server();
  bool start_samba_server();
  bool stop_samba_server();

  static void samba_server_task(void *pvParameters);

 private:
  web_server_base::WebServerBase *base_;
  std::string share_name_;
  std::string root_path_;
  bool is_running_;
  sd_mmc_card::SdMmc *sd_mmc_card_;
};

} // namespace samba
} // namespace esphome
