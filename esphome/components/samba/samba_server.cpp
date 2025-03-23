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
  this->base_->add_handler(this);
  this->init_samba_server();
}

void SambaServer::dump_config() {
  ESP_LOGCONFIG(TAG, "Samba Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
  ESP_LOGCONFIG(TAG, "  Share Name: %s", this->share_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Status: %s", this->is_running_ ? "Running" : "Stopped");
}

bool SambaServer::canHandle(web_server_idf::AsyncWebServerRequest *request) {
  return str_startswith(request->url().c_str(), "/samba");
}

void SambaServer::handleRequest(web_server_idf::AsyncWebServerRequest *request) {
  auto path = request->url();

  if (path == "/samba/status") {
    this->handle_status(request);
  } else if (path == "/samba/start" && request->method() == HTTP_POST) {
    this->handle_start(request);
  } else if (path == "/samba/stop" && request->method() == HTTP_POST) {
    this->handle_stop(request);
  } else if (path == "/samba") {
    this->handle_ui(request);
  } else {
    request->send(404);
  }
}

// Définition des méthodes
void SambaServer::handle_status(web_server_idf::AsyncWebServerRequest *request) {
  auto response = request->beginResponseStream("application/json");
  response->printf("{\"running\": %s, \"share_name\": \"%s\", \"root_path\": \"%s\"}",
                   this->is_running_ ? "true" : "false",
                   this->share_name_.c_str(),
                   this->root_path_.c_str());
  request->send(response);
}

void SambaServer::handle_start(web_server_idf::AsyncWebServerRequest *request) {
  if (this->is_running_) {
    request->send(200, "application/json", "{\"status\": \"already_running\"}");
    return;
  }

  if (this->start_samba_server()) {
    request->send(200, "application/json", "{\"status\": \"started\"}");
  } else {
    request->send(500, "application/json", "{\"error\": \"Failed to start\"}");
  }
}

void SambaServer::handle_stop(web_server_idf::AsyncWebServerRequest *request) {
  if (!this->is_running_) {
    request->send(200, "application/json", "{\"status\": \"not_running\"}");
    return;
  }

  if (this->stop_samba_server()) {
    request->send(200, "application/json", "{\"status\": \"stopped\"}");
  } else {
    request->send(500, "application/json", "{\"error\": \"Failed to stop\"}");
  }
}

void SambaServer::handle_ui(web_server_idf::AsyncWebServerRequest *request) {
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

  request->send(200, "text/html", html);
}

void SambaServer::set_share_name(const std::string &name) { this->share_name_ = name; }
void SambaServer::set_root_path(const std::string &path) { this->root_path_ = path; }
void SambaServer::set_sd_mmc_card(sd_mmc_card::SdMmc *card) { this->sd_mmc_card_ = card; }

void SambaServer::init_samba_server() {
  netbiosns_init();
  netbiosns_set_name(this->share_name_.c_str());
  ESP_LOGI(TAG, "Samba server initialized (not running)");
}

bool SambaServer::start_samba_server() {
  if (this->is_running_) {
    ESP_LOGW(TAG, "Already running");
    return true;
  }

  if (xTaskCreate(
      SambaServer::samba_server_task,
      "samba_server",
      8192,
      this,
      5,
      &samba_task_handle) != pdPASS) {
    ESP_LOGE(TAG, "Failed to create task");
    return false;
  }

  this->is_running_ = true;
  return true;
}

bool SambaServer::stop_samba_server() {
  if (!this->is_running_) {
    ESP_LOGW(TAG, "Not running");
    return true;
  }

  if (samba_task_handle) {
    vTaskDelete(samba_task_handle);
    samba_task_handle = NULL;
  }

  this->is_running_ = false;
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
