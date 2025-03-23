#pragma once

#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/web_server_idf/web_server_idf.h"
#include "../sd_mmc_card/sd_mmc_card.h"

namespace esphome {
namespace samba {

class SambaServer : public Component, public web_server_idf::AsyncWebHandler {
 public:
  SambaServer(web_server_base::WebServerBase *base);
  
  void setup() override;
  void dump_config() override;
  
  // Méthodes requises par AsyncWebHandler
  bool canHandle(web_server_idf::AsyncWebServerRequest *request) override;
  void handleRequest(web_server_idf::AsyncWebServerRequest *request) override;
  
  // Méthodes d'accès aux membres
  bool is_running() const { return is_running_; }
  const std::string& get_share_name() const { return share_name_; }
  const std::string& get_root_path() const { return root_path_; }
  
  // Configuration
  void set_share_name(const std::string &name);
  void set_root_path(const std::string &path);
  void set_sd_mmc_card(sd_mmc_card::SdMmc *card);
  
  // Gestion des handlers web
  void register_web_handlers();
  void handle_ui(web_server_idf::AsyncWebServerRequest *request);
  
  // Gestion du serveur Samba
  void init_samba_server();
  bool start_samba_server();
  bool stop_samba_server();
  
  // Tâche FreeRTOS
  static void samba_server_task(void *pvParameters);
  
 private:
  web_server_base::WebServerBase *base_;
  std::string share_name_{"ESPHome"};
  std::string root_path_{"/sdcard"};
  bool is_running_{false};
  sd_mmc_card::SdMmc *sd_mmc_card_{nullptr};
};

} // namespace samba
} // namespace esphome
