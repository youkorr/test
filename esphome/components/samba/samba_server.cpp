#include "samba_server.h"
#include "esphome/core/log.h"

namespace esphome {
namespace samba {

static const char *TAG = "samba";

void SambaComponent::setup() {
  ESP_LOGI(TAG, "Setting up Samba server...");
  
  if (sd_card_ == nullptr) {
    ESP_LOGE(TAG, "SD card is not initialized!");
    return;
  }

  // Configure Samba avec les paramètres fournis
  this->configure_samba();
}

void SambaComponent::configure_samba() {
  ESP_LOGI(TAG, "Workgroup: %s", this->workgroup_.c_str());
  ESP_LOGI(TAG, "Root path: %s", this->root_path_.c_str());
  ESP_LOGI(TAG, "Local master: %s", this->local_master_ ? "true" : "false");
  
  // Exemple de configuration des partages activés
  for (const auto &share : this->enabled_shares_) {
    ESP_LOGI(TAG, "Enabled share: %s", share.c_str());
    // Ajoutez ici la logique pour configurer chaque partage Samba.
  }

  // Exemple de configuration des hôtes autorisés
  for (const auto &host : this->allow_hosts_) {
    ESP_LOGI(TAG, "Allowed host: %s", host.c_str());
    // Ajoutez ici la logique pour autoriser chaque hôte.
  }

  ESP_LOGI(TAG, "Samba server configured successfully.");
}

void SambaComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Samba Component:");
  ESP_LOGCONFIG(TAG, "Workgroup: %s", this->workgroup_.c_str());
  ESP_LOGCONFIG(TAG, "Root path: %s", this->root_path_.c_str());
}

}  // namespace samba
}  // namespace esphome

