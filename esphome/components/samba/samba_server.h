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
};

}  // namespace samba
}  // namespace esphome









