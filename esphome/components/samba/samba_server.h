#pragma once

#include "esphome/core/component.h"
#include "../sd_mmc_card/sd_mmc_card.h"
#include <string>
#include <vector>

namespace esphome {
namespace samba {

class SambaComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;

  // Setters pour les options de configuration
  void set_workgroup(const std::string &workgroup) { workgroup_ = workgroup; }
  void set_root_path(const std::string &root_path) { root_path_ = root_path; }
  void set_local_master(bool local_master) { local_master_ = local_master; }
  void set_username(const std::string &username) { username_ = username; }
  void set_password(const std::string &password) { password_ = password; }
  void add_enabled_share(const std::string &share) { enabled_shares_.push_back(share); }
  void add_allowed_host(const std::string &host) { allow_hosts_.push_back(host); }

  // Configuration de la carte SD
  void set_sd_mmc_card(sd_mmc_card::SdMmc *card) { sd_card_ = card; }

 private:
  std::string workgroup_;
  std::string root_path_;
  bool local_master_;
  std::string username_;
  std::string password_;
  std::vector<std::string> enabled_shares_;
  std::vector<std::string> allow_hosts_;

  sd_mmc_card::SdMmc *sd_card_;

  // Méthodes internes
  void configure_samba();
};

}  // namespace samba
}  // namespace esphome


