#include "samba_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "lwip/apps/netbiosns.h"

namespace esphome {
namespace samba {

static const char *TAG = "samba";
static TaskHandle_t samba_task_handle = NULL;

SambaServer::SambaServer(web_server_base::WebServerBase *base) : base_(base) {}

void SambaServer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Samba Server...");
  this->base_->add_handler(static_cast<web_server_idf::AsyncWebHandler*>(this));
  this->register_web_handlers();
  this->init_samba_server();
}

void SambaServer::dump_config() {
  ESP_LOGCONFIG(TAG, "Samba Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
  ESP_LOGCONFIG(TAG, "  Share Name: %s", this->share_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Status: %s", this->is_running_ ? "Running" : "Stopped");
}

void SambaServer::register_web_handlers() {
  auto server = this->base_->get_server();

  // Définition des handlers pour chaque endpoint
  httpd_uri_t status_uri = {
    .uri       = "/samba/status",
    .method    = HTTP_GET,
    .handler   = status_handler,
    .user_ctx  = this
  };
  httpd_register_uri_handler(server->get_httpd(), &status_uri);

  httpd_uri_t start_uri = {
    .uri       = "/samba/start",
    .method    = HTTP_POST,
    .handler   = start_handler,
    .user_ctx  = this
  };
  httpd_register_uri_handler(server->get_httpd(), &start_uri);

  httpd_uri_t stop_uri = {
    .uri       = "/samba/stop",
    .method    = HTTP_POST,
    .handler   = stop_handler,
    .user_ctx  = this
  };
  httpd_register_uri_handler(server->get_httpd(), &stop_uri);

  httpd_uri_t samba_uri = {
    .uri       = "/samba",
    .method    = HTTP_GET,
    .handler   = samba_ui_handler,
    .user_ctx  = this
  };
  httpd_register_uri_handler(server->get_httpd(), &samba_uri);
}

// Handler pour "/samba/status"
esp_err_t status_handler(httpd_req_t *req) {
  SambaServer *server = (SambaServer*) req->user_ctx;
  char buffer[256];
  snprintf(buffer, sizeof(buffer), "{\"running\": %s, \"share_name\": \"%s\", \"root_path\": \"%s\"}",
           server->is_running_ ? "true" : "false",
           server->share_name_.c_str(),
           server->root_path_.c_str());
  httpd_resp_send(req, buffer, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

// Handler pour "/samba/start"
esp_err_t start_handler(httpd_req_t *req) {
  SambaServer *server = (SambaServer*) req->user_ctx;
  if (server->is_running_) {
    httpd_resp_send(req, "{\"status\": \"already_running\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  if (server->start_samba_server()) {
    httpd_resp_send(req, "{\"status\": \"started\"}", HTTPD_RESP_USE_STRLEN);
  } else {
    httpd_resp_send_err(req, HTTP_INTERNAL_SERVER_ERROR, "Failed to start Samba server");
  }
  return ESP_OK;
}

// Handler pour "/samba/stop"
esp_err_t stop_handler(httpd_req_t *req) {
  SambaServer *server = (SambaServer*) req->user_ctx;
  if (!server->is_running_) {
    httpd_resp_send(req, "{\"status\": \"not_running\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  if (server->stop_samba_server()) {
    httpd_resp_send(req, "{\"status\": \"stopped\"}", HTTPD_RESP_USE_STR_LEN);
  } else {
    httpd_resp_send_err(req, HTTP_INTERNAL_SERVER_ERROR, "Failed to stop Samba server");
  }
  return ESP_OK;
}

// Handler pour l'interface utilisateur
esp_err_t samba_ui_handler(httpd_req_t *req) {
  SambaServer *server = (SambaServer*) req->user_ctx;
  const char *html = R"HTML(
    <!DOCTYPE html><html lang="en">
    <head><meta charset=UTF-8>
    <meta name=viewport content="width=device-width, initial-scale=1,user-scalable=no">
    <title>Samba Server Control</title></head>
    <body><h1>Samba Server Control</h1>
    <div id="status">Checking status...</div>
    <button id="startBtn" onclick="startServer()">Start Server</button>
    <button id="stopBtn" onclick="stopServer()">Stop Server</button>
    <script>
    function updateStatus() {
      fetch('/samba/status').then(r => r.json()).then(data => {
        document.getElementById('status').innerHTML = 
          'Status: ' + (data.running ? 'Running' : 'Stopped') + 
          '<br>Share Name: ' + data.share_name + 
          '<br>Root Path: ' + data.root_path;
        document.getElementById('startBtn').disabled = data.running;
        document.getElementById('stopBtn').disabled = !data.running;
      });
    }
    function startServer() { fetch('/samba/start', {method: 'POST'}).then(updateStatus); }
    function stopServer() { fetch('/samba/stop', {method: 'POST'}).then(updateStatus); }
    updateStatus(); setInterval(updateStatus, 5000);
    </script>
    </body></html>
  )HTML";

  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, html, strlen(html));
  return ESP_OK;
}

bool SambaServer::canHandle(web_server_idf::AsyncWebServerRequest *request) {
  return str_startswith(request->url().c_str(), "/samba");
}

void SambaServer::handleRequest(web_server_idf::AsyncWebServerRequest *request) {
  // Les handlers sont gérés via httpd_uri_t, donc cette méthode reste vide
}

void SambaServer::set_share_name(const std::string &name) { 
  this->share_name_ = name; 
}

void SambaServer::set_root_path(const std::string &path) { 
  this->root_path_ = path; 
}

void SambaServer::set_sd_mmc_card(sd_mmc_card::SdMmc *card) { 
  this->sd_mmc_card_ = card; 
}

void SambaServer::init_samba_server() {
  netbiosns_init();
  netbiosns_set_name(this->share_name_.c_str());
  ESP_LOGI(TAG, "Samba server initialized (not running)");
}

bool SambaServer::start_samba_server() {
  if (this->is_running_) {
    ESP_LOGW(TAG, "Samba server already running");
    return true;
  }

  if (xTaskCreate(
    SambaServer::samba_server_task,
    "samba_server",
    8192,
    this,
    5,
    &samba_task_handle) != pdPASS) {
    ESP_LOGE(TAG, "Failed to create Samba server task");
    return false;
  }

  this->is_running_ = true;
  ESP_LOGI(TAG, "Samba server started");
  return true;
}

bool SambaServer::stop_samba_server() {
  if (!this->is_running_) {
    ESP_LOGW(TAG, "Samba server not running");
    return true;
  }

  if (samba_task_handle != NULL) {
    vTaskDelete(samba_task_handle);
    samba_task_handle = NULL;
  }

  this->is_running_ = false;
  ESP_LOGI(TAG, "Samba server stopped");
  return true;
}

void SambaServer::samba_server_task(void *pvParameters) {
  SambaServer *server = static_cast<SambaServer *>(pvParameters);
  ESP_LOGI(TAG, "Samba server task started");

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

} // namespace samba
} // namespace esphome
