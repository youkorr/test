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
  
  // Register the web interface handler
  this->register_web_handlers();
  
  // Initialize but don't start the server yet
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
  // Register the control interface URLs
  this->base_->get_server()->on("/samba/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->printf("{\"running\": %s, \"share_name\": \"%s\", \"root_path\": \"%s\"}", 
                    this->is_running_ ? "true" : "false", 
                    this->share_name_.c_str(), 
                    this->root_path_.c_str());
    request->send(response);
  });

  this->base_->get_server()->on("/samba/start", HTTP_POST, [this](AsyncWebServerRequest *request) {
    if (!this->is_running_) {
      if (this->start_samba_server()) {
        request->send(200, "application/json", "{\"status\": \"started\"}");
      } else {
        request->send(500, "application/json", "{\"error\": \"Failed to start Samba server\"}");
      }
    } else {
      request->send(200, "application/json", "{\"status\": \"already_running\"}");
    }
  });

  this->base_->get_server()->on("/samba/stop", HTTP_POST, [this](AsyncWebServerRequest *request) {
    if (this->is_running_) {
      if (this->stop_samba_server()) {
        request->send(200, "application/json", "{\"status\": \"stopped\"}");
      } else {
        request->send(500, "application/json", "{\"error\": \"Failed to stop Samba server\"}");
      }
    } else {
      request->send(200, "application/json", "{\"status\": \"not_running\"}");
    }
  });
}

bool SambaServer::canHandle(AsyncWebServerRequest *request) {
  return str_startswith(std::string(request->url().c_str()), "/samba");
}

void SambaServer::handleRequest(AsyncWebServerRequest *request) {
  // The actual handling is done by the registered handlers
}

void SambaServer::set_share_name(std::string const &name) { 
  this->share_name_ = name; 
}

void SambaServer::set_root_path(std::string const &path) { 
  this->root_path_ = path; 
}

void SambaServer::set_sd_mmc_card(sd_mmc_card::SdMmc *card) { 
  this->sd_mmc_card_ = card; 
}

void SambaServer::init_samba_server() {
  // Initialize the NetBIOS name service for Windows network discovery
  netbiosns_init();
  netbiosns_set_name(this->share_name_.c_str());
  
  // Set up initial configuration but don't start the task yet
  ESP_LOGI(TAG, "Samba server initialized (not running)");
}

bool SambaServer::start_samba_server() {
  if (this->is_running_) {
    ESP_LOGW(TAG, "Samba server already running");
    return true;
  }

  // Create the Samba server task
  BaseType_t xReturned = xTaskCreate(
    SambaServer::samba_server_task,
    "samba_server",
    8192,  // Stack size
    this,  // Pass this instance as parameter
    5,     // Priority
    &samba_task_handle
  );

  if (xReturned != pdPASS) {
    ESP_LOGE(TAG, "Failed to create Samba server task");
    return false;
  }

  this->is_running_ = true;
  ESP_LOGI(TAG, "Samba server started. Share name: %s, Root path: %s", 
           this->share_name_.c_str(), this->root_path_.c_str());
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

// Static task function that will be run by FreeRTOS
void SambaServer::samba_server_task(void *pvParameters) {
  SambaServer *server = static_cast<SambaServer *>(pvParameters);
  
  ESP_LOGI(TAG, "Samba server task started");

  // Basic SMB/CIFS implementation
  // Note: This is a simplified example. A full Samba implementation would require
  // more complex handling of SMB protocols, authentication, etc.
  
  // Main server loop
  while (true) {
    // Your actual Samba protocol implementation would go here
    // For ESP-IDF, you might use a lightweight SMB implementation
    
    // For the purpose of this example, we'll just keep the task alive
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// HTML UI for Samba controls
void SambaServer::handle_ui(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  
  response->print(F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=UTF-8>"
                    "<meta name=viewport content=\"width=device-width, initial-scale=1,user-scalable=no\">"
                    "<title>Samba Server Control</title></head><body>"
                    "<h1>Samba Server Control</h1>"));
  
  response->print(F("<div id=\"status\">Checking status...</div>"));
  
  response->print(F("<button id=\"startBtn\" onclick=\"startServer()\">Start Server</button>"));
  response->print(F("<button id=\"stopBtn\" onclick=\"stopServer()\">Stop Server</button>"));
  
  response->print(F("<script>"
                    "function updateStatus() {"
                    "  fetch('/samba/status').then(response => response.json())"
                    "  .then(data => {"
                    "    document.getElementById('status').innerHTML = "
                    "      'Status: ' + (data.running ? 'Running' : 'Stopped') + "
                    "      '<br>Share Name: ' + data.share_name + "
                    "      '<br>Root Path: ' + data.root_path;"
                    "    document.getElementById('startBtn').disabled = data.running;"
                    "    document.getElementById('stopBtn').disabled = !data.running;"
                    "  });"
                    "}"
                    
                    "function startServer() {"
                    "  fetch('/samba/start', {method: 'POST'})"
                    "  .then(response => response.json())"
                    "  .then(data => { updateStatus(); });"
                    "}"
                    
                    "function stopServer() {"
                    "  fetch('/samba/stop', {method: 'POST'})"
                    "  .then(response => response.json())"
                    "  .then(data => { updateStatus(); });"
                    "}"
                    
                    "// Initial status check"
                    "updateStatus();"
                    "// Refresh status every 5 seconds"
                    "setInterval(updateStatus, 5000);"
                    "</script>"
                    "</body></html>"));

  request->send(response);
}

} // namespace samba
} // namespace esphome
