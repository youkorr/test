#include "webdavbox3.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include <sys/param.h>
#include "esp_http_server.h"

namespace esphome {
namespace webdavbox3 {

static const char *TAG = "webdavbox3";

void WebDAVBox3::setup() {
  ESP_LOGI(TAG, "WebDAVBox3 setup started.");

  // Check if SD card is mounted
  if (!sd_mmc_card::is_mounted()) {
    ESP_LOGE(TAG, "SD card is not mounted.");
    return;
  }

  // Configure HTTP server
  configure_http_server();

  ESP_LOGI(TAG, "WebDAVBox3 setup completed.");
}

void WebDAVBox3::loop() {
  // Empty loop
}

void WebDAVBox3::configure_http_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = port_;
  config.uri_match_fn = httpd_uri_match_wildcard;
  
  // Start the HTTP server
  if (httpd_start(&server_, &config) == ESP_OK) {
    ESP_LOGI(TAG, "HTTP server started.");
    // Register WebDAV handlers
    httpd_uri_t uri = {
        .uri = "/webdav/*",
        .method = HTTP_GET,
        .handler = handle_webdav_get,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &uri);

    uri.method = HTTP_PUT;
    uri.handler = handle_webdav_put;
    httpd_register_uri_handler(server_, &uri);

    uri.method = HTTP_DELETE;
    uri.handler = handle_webdav_delete;
    httpd_register_uri_handler(server_, &uri);
  }
}

bool WebDAVBox3::authenticate(httpd_req_t *req) {
  if (!auth_enabled_)
    return true;  // No authentication needed

  const char *auth = httpd_req_get_hdr_value_str(req, "Authorization");
  if (auth == nullptr) {
    ESP_LOGW(TAG, "Authorization header missing.");
    return false;
  }

  // Extract username and password from header (simple check, base64 not used)
  std::string auth_str(auth);
  if (auth_str.substr(0, 6) == "Basic ") {
    std::string credentials = auth_str.substr(6);
    size_t delimiter = credentials.find(":");
    if (delimiter != std::string::npos) {
      std::string user = credentials.substr(0, delimiter);
      std::string pass = credentials.substr(delimiter + 1);
      if (user == username_ && pass == password_) {
        return true;
      }
    }
  }
  return false;
}

esp_err_t WebDAVBox3::send_auth_required_response(httpd_req_t *req) {
  httpd_resp_set_status(req, HTTPD_401);
  httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"WebDAV\"");
  httpd_resp_send(req, "Unauthorized", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

std::string WebDAVBox3::get_file_path(httpd_req_t *req, const std::string &root_path) {
  std::string uri(req->uri);
  return root_path + uri.substr(url_prefix_.length());
}

bool WebDAVBox3::is_dir(const std::string &path) {
  struct stat st;
  return (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
}

std::vector<std::string> WebDAVBox3::list_dir(const std::string &path) {
  std::vector<std::string> file_list;
  DIR *d = opendir(path.c_str());
  if (d) {
    struct dirent *dir;
    while ((dir = readdir(d)) != nullptr) {
      file_list.push_back(dir->d_name);
    }
    closedir(d);
  }
  return file_list;
}

esp_err_t WebDAVBox3::handle_root(httpd_req_t *req) {
  // Root handler (return home page or list)
  httpd_resp_send(req, "WebDAV Root", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_get(httpd_req_t *req) {
  std::string file_path = get_file_path(req, root_path_);
  ESP_LOGI(TAG, "GET request for file: %s", file_path.c_str());

  // Handle reading the file from SD card and sending it as response
  FILE *file = fopen(file_path.c_str(), "r");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file: %s", file_path.c_str());
    return HTTPD_404_NOT_FOUND;
  }

  // Send file content as HTTP response
  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  rewind(file);
  char *buffer = (char *)malloc(file_size);
  fread(buffer, 1, file_size, file);
  fclose(file);

  httpd_resp_send(req, buffer, file_size);
  free(buffer);

  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_put(httpd_req_t *req) {
  std::string file_path = get_file_path(req, root_path_);
  ESP_LOGI(TAG, "PUT request for file: %s", file_path.c_str());

  // Handle saving the uploaded file to SD card
  FILE *file = fopen(file_path.c_str(), "w");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file: %s", file_path.c_str());
    return HTTPD_500_INTERNAL_SERVER_ERROR;
  }

  size_t received = 0;
  while (received < req->content_len) {
    size_t to_read = req->content_len - received;
    if (to_read > 1024) to_read = 1024;

    char *buffer = (char *)malloc(to_read);
    httpd_req_recv(req, buffer, to_read);
    fwrite(buffer, 1, to_read, file);
    free(buffer);
    received += to_read;
  }

  fclose(file);
  httpd_resp_send(req, "File uploaded", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_delete(httpd_req_t *req) {
  std::string file_path = get_file_path(req, root_path_);
  ESP_LOGI(TAG, "DELETE request for file: %s", file_path.c_str());

  // Handle file deletion
  if (remove(file_path.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to delete file: %s", file_path.c_str());
    return HTTPD_500_INTERNAL_SERVER_ERROR;
  }

  httpd_resp_send(req, "File deleted", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

}  // namespace webdavbox3
}  // namespace esphome





