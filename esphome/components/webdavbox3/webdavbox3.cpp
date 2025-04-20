#include "webdavbox3.h"
#include "esphome/core/log.h"
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>

namespace esphome {
namespace webdavbox3 {

static const char* TAG = "webdavbox3";

esp_err_t WebDAVBox3::handle_root(httpd_req_t *req) {
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
    
    char response[256];
    snprintf(response, sizeof(response), 
        "<html><body><h1>WebDAV Server</h1><p>IP: " IPSTR ":%d</p><p>Prefix: %s</p></body></html>", 
        IP2STR(&ip_info.ip), 
        static_cast<WebDAVBox3*>(req->user_ctx)->port_,
        static_cast<WebDAVBox3*>(req->user_ctx)->url_prefix_.c_str());
    
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, response, strlen(response));
}

esp_err_t WebDAVBox3::handle_webdav_list(httpd_req_t *req) {
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    
    DIR *dir = opendir(instance->root_path_.c_str());
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory %s", instance->root_path_.c_str());
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    std::string fileList = "Files in " + instance->root_path_ + ":\n";
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) {
            fileList += entry->d_name;
            fileList += "\n";
        }
    }
    closedir(dir);

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, fileList.c_str(), fileList.size());
    return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_get(httpd_req_t *req) {
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    std::string uri(req->uri);
    std::string filepath = instance->root_path_ + uri.substr(instance->url_prefix_.size());

    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
        ESP_LOGE(TAG, "File not found: %s", filepath.c_str());
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    FILE *file = fopen(filepath.c_str(), "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file %s", filepath.c_str());
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        httpd_resp_send_chunk(req, buffer, bytes_read);
    }
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_put(httpd_req_t *req) {
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    std::string uri(req->uri);
    std::string filepath = instance->root_path_ + uri.substr(instance->url_prefix_.size());

    FILE *file = fopen(filepath.c_str(), "wb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to create file %s", filepath.c_str());
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char buffer[1024];
    int received;
    while ((received = httpd_req_recv(req, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, received, file);
    }
    fclose(file);
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_delete(httpd_req_t *req) {
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    std::string uri(req->uri);
    std::string filepath = instance->root_path_ + uri.substr(instance->url_prefix_.size());

    if (remove(filepath.c_str()) == 0) {
        httpd_resp_sendstr(req, "File deleted successfully");
    } else {
        ESP_LOGE(TAG, "Failed to delete file %s", filepath.c_str());
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    return ESP_OK;
}

void WebDAVBox3::configure_http_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.server_port = port_;
    
    if (httpd_start(&server_, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server on port %d", port_);
        return;
    }

    httpd_uri_t uris[] = {
        {.uri = (url_prefix_ + "/").c_str(), .method = HTTP_GET, .handler = handle_root, .user_ctx = this},
        {.uri = (url_prefix_ + "/list").c_str(), .method = HTTP_GET, .handler = handle_webdav_list, .user_ctx = this},
        {.uri = (url_prefix_ + "/*").c_str(), .method = HTTP_GET, .handler = handle_webdav_get, .user_ctx = this},
        {.uri = (url_prefix_ + "/*").c_str(), .method = HTTP_PUT, .handler = handle_webdav_put, .user_ctx = this},
        {.uri = (url_prefix_ + "/*").c_str(), .method = HTTP_DELETE, .handler = handle_webdav_delete, .user_ctx = this}
    };

    for (auto& uri : uris) {
        if (httpd_register_uri_handler(server_, &uri) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register URI handler for %s", uri.uri);
        }
    }

    ESP_LOGI(TAG, "WebDAV server started on port %d with prefix %s", port_, url_prefix_.c_str());
}

void WebDAVBox3::start_server() {
    // Configurer et démarrer le serveur HTTP
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.server_port = port_;  // Port de ton serveur HTTP

    esp_err_t ret = httpd_start(&server_, &config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Server started on port %d", port_);
    } else {
        ESP_LOGE(TAG, "Failed to start server on port %d: %s", port_, esp_err_to_name(ret));
    }
}

void WebDAVBox3::setup() {
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024};

  sdmmc_card_t *card;
  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

  if (ret != ESP_OK) {
    ESP_LOGE("webdav", "Failed to mount SD card: %s", esp_err_to_name(ret));
    return;
  }

  ESP_LOGI("webdav", "SD card mounted successfully");

  configure_http_server();
  start_server();  // Démarre le serveur WebDAV après la configuration
}

void WebDAVBox3::loop() {
    // Nothing to do here for now
}

} // namespace webdavbox3
} // namespace esphome






