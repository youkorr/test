// samba_server.cpp
#include "samba_server.h"
#include "esphome/core/log.h"
#include "esp_smb.h"  // Bibliothèque ESP-SMB
#include "esp_vfs_fat.h"

namespace esphome {
namespace samba {

static const char *TAG = "samba";

void SambaComponent::setup() {
  ESP_LOGI(TAG, "Setting up Samba server...");
  
  if (sd_card_ == nullptr) {
    ESP_LOGE(TAG, "SD card is not initialized!");
    return;
  }
  
  // Vérifier si la carte SD est montée
  if (!sd_card_->is_mounted()) {
    ESP_LOGE(TAG, "Failed to mount SD card");
    return;
  }
  
  // Obtenir le chemin de montage de la carte SD
  std::string mount_point = sd_card_->get_mount_point();
  if (this->root_path_.empty()) {
    this->root_path_ = mount_point;
  }
  
  ESP_LOGI(TAG, "SD card mount point: %s", mount_point.c_str());
  
  // Configure Samba avec les paramètres fournis
  this->configure_samba();
}

void SambaComponent::configure_samba() {
  ESP_LOGI(TAG, "Configuring Samba server...");
  ESP_LOGI(TAG, "Workgroup: %s", this->workgroup_.c_str());
  ESP_LOGI(TAG, "Root path: %s", this->root_path_.c_str());
  ESP_LOGI(TAG, "Local master: %s", this->local_master_ ? "true" : "false");
  
  // Configuration du serveur SMB
  smb_config_t smb_config = {
    .hostname = "ESPHOME",
    .workgroup = this->workgroup_.c_str(),
    .server_description = "ESPHome Samba Server",
    .username = this->username_.c_str(),
    .password = this->password_.c_str()
  };
  
  // Initialiser le serveur SMB
  esp_err_t ret = esp_smb_init(&smb_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SMB server: %d", ret);
    return;
  }
  
  // Ajouter les partages
  if (this->enabled_shares_.empty()) {
    // Si aucun partage n'est spécifié, partager la racine de la carte SD
    ESP_LOGI(TAG, "Adding default share for root path: %s", this->root_path_.c_str());
    
    smb_share_config_t share_config = {
      .name = "SDCard",
      .path = this->root_path_.c_str(),
      .read_only = false
    };
    
    ret = esp_smb_add_share(&share_config);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to add default share: %d", ret);
    }
  } else {
    // Ajouter les partages spécifiés
    for (const auto &share_name : this->enabled_shares_) {
      std::string share_path = this->root_path_ + "/" + share_name;
      ESP_LOGI(TAG, "Adding share: %s at path: %s", share_name.c_str(), share_path.c_str());
      
      smb_share_config_t share_config = {
        .name = share_name.c_str(),
        .path = share_path.c_str(),
        .read_only = false
      };
      
      ret = esp_smb_add_share(&share_config);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add share %s: %d", share_name.c_str(), ret);
      }
    }
  }
  
  // Configurer les restrictions d'hôtes si nécessaire
  if (!this->allow_hosts_.empty()) {
    for (const auto &host : this->allow_hosts_) {
      ESP_LOGI(TAG, "Adding allowed host: %s", host.c_str());
      ret = esp_smb_add_allowed_host(host.c_str());
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add allowed host %s: %d", host.c_str(), ret);
      }
    }
  }
  
  // Démarrer le serveur SMB
  ret = esp_smb_start();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start SMB server: %d", ret);
    return;
  }
  
  ESP_LOGI(TAG, "Samba server started successfully");
}

void SambaComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Samba Component:");
  ESP_LOGCONFIG(TAG, "  Workgroup: %s", this->workgroup_.c_str());
  ESP_LOGCONFIG(TAG, "  Root path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Username: %s", this->username_.c_str());
  ESP_LOGCONFIG(TAG, "  Password: %s", this->password_.empty() ? "not set" : "set");
  ESP_LOGCONFIG(TAG, "  Local master: %s", this->local_master_ ? "yes" : "no");
  
  if (!this->enabled_shares_.empty()) {
    ESP_LOGCONFIG(TAG, "  Enabled shares:");
    for (const auto &share : this->enabled_shares_) {
      ESP_LOGCONFIG(TAG, "    - %s", share.c_str());
    }
  }
  
  if (!this->allow_hosts_.empty()) {
    ESP_LOGCONFIG(TAG, "  Allowed hosts:");
    for (const auto &host : this->allow_hosts_) {
      ESP_LOGCONFIG(TAG, "    - %s", host.c_str());
    }
  }
}

}  // namespace samba
}  // namespace esphome








