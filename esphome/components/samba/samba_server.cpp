// samba_server.cpp
#include "samba_server.h"
#include "esphome/core/log.h"
#include "SmbServer.h"  // Bibliothèque SmbServer de tobozo

namespace esphome {
namespace samba {

static const char *TAG = "samba";
SmbServer smbServer;  // Instance globale du serveur SMB

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
  ESP_LOGI(TAG, "Configuring Samba server...");
  ESP_LOGI(TAG, "Workgroup: %s", this->workgroup_.c_str());
  ESP_LOGI(TAG, "Root path: %s", this->root_path_.c_str());
  
  // Configurer le serveur SMB
  SmbServerConfig config;
  config.hostname = "ESPHOME";
  config.workgroup = this->workgroup_;
  config.username = this->username_;
  config.password = this->password_;
  
  // Initialiser le serveur SMB
  bool success = smbServer.begin(config);
  if (!success) {
    ESP_LOGE(TAG, "Failed to initialize SMB server");
    return;
  }
  
  // Ajouter les partages
  if (this->enabled_shares_.empty()) {
    // Si aucun partage n'est spécifié, partager la racine de la carte SD
    ESP_LOGI(TAG, "Adding default share for root path: %s", this->root_path_.c_str());
    success = smbServer.addShare("SDCard", this->root_path_, false);
    if (!success) {
      ESP_LOGE(TAG, "Failed to add default share");
    }
  } else {
    // Ajouter les partages spécifiés
    for (const auto &share_name : this->enabled_shares_) {
      std::string share_path = this->root_path_ + "/" + share_name;
      ESP_LOGI(TAG, "Adding share: %s at path: %s", share_name.c_str(), share_path.c_str());
      success = smbServer.addShare(share_name, share_path, false);
      if (!success) {
        ESP_LOGE(TAG, "Failed to add share %s", share_name.c_str());
      }
    }
  }
  
  ESP_LOGI(TAG, "Samba server started successfully");
}

void SambaComponent::dump_config() {
  // Identique à l'implémentation précédente
}

}  // namespace samba
}  // namespace esphome








