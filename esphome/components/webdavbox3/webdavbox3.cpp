#include "webdavbox3.h"
#include "../sd_mmc_card/sd_mmc_card.h"  // Ajouté l'inclusion de sd_mmc_card.h

#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <fstream>


namespace esphome {
namespace webdavbox3 {

static const char *TAG = "webdavbox3";

// ======== Setup & Loop ========
void WebDAVBox3::setup() {
  if (!is_sd_mounted()) {
    ESP_LOGE(TAG, "SD card not mounted, WebDAV server not started.");
    return;
  }
  configure_http_server();
  start_server();
  ESP_LOGI(TAG, "WebDAV server started on port %u", port_);
}

void WebDAVBox3::loop() {
  // rien ici pour le moment
}

// ======== Server Config ========
void WebDAVBox3::configure_http_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = port_;
  config.uri_match_fn = httpd_uri_match_wildcard;

  if (httpd_start(&server_, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start WebDAV server");
    return;
  }

  // Register handlers
  httpd_uri_t uri_get = {
      .uri = "/*",
      .method = HTTP_GET,
      .handler = WebDAVBox3::handle_webdav_get,
      .user_ctx = this,
  };
  httpd_register_uri_handler(server_, &uri_get);

  httpd_uri_t uri_put = {
      .uri = "/*",
      .method = HTTP_PUT,
      .handler = WebDAVBox3::handle_webdav_put,
      .user_ctx = this,
  };
  httpd_register_uri_handler(server_, &uri_put);

  httpd_uri_t uri_delete = {
      .uri = "/*",
      .method = HTTP_DELETE,
      .handler = WebDAVBox3::handle_webdav_delete,
      .user_ctx = this,
  };
  httpd_register_uri_handler(server_, &uri_delete);

  httpd_uri_t uri_propfind = {
      .uri = "/*",
      .method = HTTP_PROPFIND,
      .handler = WebDAVBox3::handle_webdav_propfind,
      .user_ctx = this,
  };
  httpd_register_uri_handler(server_, &uri_propfind);
}

void WebDAVBox3::start_server() {
  // déjà démarré dans configure_http_server()
}

void WebDAVBox3::stop_server() {
  if (server_ != nullptr) {
    httpd_stop(server_);
    server_ = nullptr;
  }
}

// ======== Helpers ========
bool WebDAVBox3::is_sd_mounted() {
  sdmmc_card_t *card;
  esp_err_t ret = sdmmc_card_get_slot(card);  // Exemple de vérification
  return ret == ESP_OK;
}

std::string WebDAVBox3::get_file_path(httpd_req_t *req, const std::string &root_path, const std::string &url_prefix) {
  std::string uri = req->uri;
  std::string full_path = root_path + uri.substr(url_prefix.length());
  return full_path;
}

bool WebDAVBox3::is_dir(const std::string &path) {
  struct stat s;
  if (stat(path.c_str(), &s) == 0) {
    return S_ISDIR(s.st_mode);
  }
  return false;
}

std::vector<std::string> WebDAVBox3::list_dir(const std::string &path) {
  std::vector<std::string> files;
  DIR *dir = opendir(path.c_str());
  if (!dir) return files;

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    files.emplace_back(entry->d_name);
  }
  closedir(dir);
  return files;
}

// ======== Authentication ========
bool WebDAVBox3::authenticate(httpd_req_t *req) {
  if (!auth_enabled_) return true;

  const char *auth = httpd_req_get_hdr_value_str(req, "Authorization");
  if (!auth || strncmp(auth, "Basic ", 6) != 0) return false;

  std::string encoded = auth + 6;
  std::string expected = username_ + ":" + password_;
  std::string encoded_expected = base64_encode(expected);  // Utilisation de base64_encode()

  return encoded == encoded_expected;
}

esp_err_t WebDAVBox3::send_auth_required_response(httpd_req_t *req) {
  httpd_resp_set_status(req, "401 Unauthorized");
  httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"WebDAV\"");
  return httpd_resp_send(req, "Authentication required", HTTPD_RESP_USE_STRLEN);
}

// ======== WebDAV Handlers ========
esp_err_t WebDAVBox3::handle_webdav_get(httpd_req_t *req) {
  WebDAVBox3 *self = static_cast<WebDAVBox3 *>(req->user_ctx);
  if (!self->authenticate(req)) return self->send_auth_required_response(req);

  std::string path = get_file_path(req, self->root_path_, self->url_prefix_);

  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) return httpd_resp_send_404(req);

  httpd_resp_set_type(req, "application/octet-stream");

  char buffer[1024];
  while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
    httpd_resp_send_chunk(req, buffer, file.gcount());
  }

  httpd_resp_send_chunk(req, nullptr, 0);  // end
  return ESP_OK;
}

}  // namespace webdavbox3
}  // namespace esphome




