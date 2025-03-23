#include "samba_server.h"
#include "esp_log.h"

namespace esphome {
namespace samba_server {

static const char *TAG = "samba_server";

void SambaServer::setup() {
  ESP_LOGI(TAG, "Setting up Samba Server...");

  if (sd_card_ == nullptr) {
    ESP_LOGE(TAG, "SD card not initialized!");
    return;
  }

  // Start HTTP server
  esp_err_t err = this->start_http_server();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
    return;
  }

  ESP_LOGI(TAG, "Samba Server setup complete.");
}

void SambaServer::loop() {
  // No specific loop logic required for HTTP server
}

esp_err_t SambaServer::start_http_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  
  // Start the HTTP server
  if (httpd_start(&server_, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return ESP_FAIL;
  }

  // Register URI handlers
  httpd_uri_t list_handler = {
      .uri       = "/list",
      .method    = HTTP_GET,
      .handler   = [](httpd_req_t *req) -> esp_err_t {
        // TODO: Implement directory listing logic here
        httpd_resp_send(req, "Directory listing not implemented", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
      },
      .user_ctx = nullptr};
  
  httpd_register_uri_handler(server_, &list_handler);

  httpd_uri_t upload_handler = {
      .uri       = "/upload",
      .method    = HTTP_POST,
      .handler   = [](httpd_req_t *req) -> esp_err_t {
        // TODO: Implement file upload logic here
        httpd_resp_send(req, "File upload not implemented", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
      },
      .user_ctx = nullptr};

  httpd_register_uri_handler(server_, &upload_handler);

  ESP_LOGI(TAG, "HTTP server started successfully.");
  
  return ESP_OK;
}

}  // namespace samba_server
}  // namespace esphome







