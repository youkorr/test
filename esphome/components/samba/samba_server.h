#pragma once

#include "esphome/core/component.h"
#include "../sd_mmc_card/sd_mmc_card.h"
#include <vector>
#include <string>

namespace esphome {
namespace samba {

class SambaComponent : public Component {
 public:
  void setup() override;
  void set_sd_mmc_card(sd_mmc_card::SdMmc *);
  void add_allowed_host(const std::string &host) { this->allow_hosts_.push_back(host); }
  
  // Ajout des déclarations de méthodes manquantes
  void handle_connections();
  void handle_smb_request(int client_socket);

 protected:
  void configure_samba();
  sd_mmc_card::SdMmc *sd_mmc_card_;
  std::vector<std::string> allow_hosts_{};
  // Déclarations des méthodes manquantes
  void handle_connections();
  void handle_smb_request(int client_socket);
  
  // Ajout des nouvelles méthodes demandées
  void set_workgroup(const std::string &workgroup) { this->workgroup_ = workgroup; }
  void set_root_path(const std::string &root_path) { this->root_path_ = root_path; }
  void set_local_master(bool local_master) { this->local_master_ = local_master; }
  void set_username(const std::string &username) { this->username_ = username; }
  void set_password(const std::string &password) { this->password_ = password; }
  void add_enabled_share(const std::string &share_name, const std::string &share_path) {
    this->enabled_shares_[share_name] = share_path;
  }

 protected:
  void configure_samba();
  sd_card::SDCard *sd_card_{nullptr};
  std::vector<std::string> allow_hosts_{};
  
  // Nouveaux attributs pour les méthodes ajoutées
  std::string workgroup_{"WORKGROUP"};
  std::string root_path_{"/sdcard"};
  bool local_master_{true};
  std::string username_{"esp32"};
  std::string password_{""}; // Par défaut, pas de mot de passe
  std::map<std::string, std::string> enabled_shares_{};
};

}  // namespace samba
}  // namespace esphome









