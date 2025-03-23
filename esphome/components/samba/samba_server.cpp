#include "samba_server.h"
#include "esp_log.h"

namespace esphome {
namespace samba_server {

static const char *TAG = "samba_server";

void SambaServer::setup() {
  ESP_LOGI(TAG, "Setting up Samba Server...");

  // Initialisation de la carte SD
  if (!this->initialize_sd_card()) {
    ESP_LOGE(TAG, "Failed to initialize SD card");
    return;
  }

  // Démarrage du serveur HTTP
  esp_err_t err = this->start_http_server();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
    return;
  }

  ESP_LOGI(TAG, "Samba Server setup complete.");
}

void SambaServer::loop() {
  // Pas de logique spécifique requise dans la boucle principale
}

bool SambaServer::initialize_sd_card() {
  ESP_LOGI(TAG, "Initializing SD card...");

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

  // Configuration du montage FATFS
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024,
  };

  // Pointeur pour stocker les informations sur la carte
  sdmmc_card_t *card;
  
  esp_err_t ret = esp_vfs_fat_sdmmc_mount(this->root_path_.c_str(), &host, &slot_config, &mount_config, &card);
  
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount filesystem. Error: %s", esp_err_to_name(ret));
    return false;
  }

  this->sd_card_ = card; // Stocker la carte pour un usage ultérieur
  sdmmc_card_print_info(stdout, card);

  ESP_LOGI(TAG, "SD card initialized successfully.");
  return true;
}

esp_err_t SambaServer::start_http_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  // Démarrer le serveur HTTP
  if (httpd_start(&server_, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return ESP_FAIL;
  }

  // Enregistrer les gestionnaires d'URI
  httpd_uri_t list_handler = {
      .uri       = "/list",
      .method    = HTTP_GET,
      .handler   = [](httpd_req_t *req) -> esp_err_t {
        httpd_resp_send(req, "Directory listing not implemented", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
      },
      .user_ctx = nullptr};

  httpd_register_uri_handler(server_, &list_handler);

  httpd_uri_t upload_handler = {
      .uri       = "/upload",
      .method    = HTTP_POST,
      .handler   = [](httpd_req_t *req) -> esp_err_t {
        httpd_resp_send(req, "File upload not implemented", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
      },
      .user_ctx = nullptr};

  httpd_register_uri_handler(server_, &upload_handler);

  ESP_LOGI(TAG, "HTTP server started successfully.");
  
  return ESP_OK;
}

} // namespace samba_server
} // namespace esphome






