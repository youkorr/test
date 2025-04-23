#include "webdavbox3.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <string>
#include <cstring>
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include <sys/stat.h>
#include <dirent.h>
#include "mbedtls/base64.h"  // ESP-IDF base64 implementation

namespace esphome {
namespace webdavbox3 {

static const char *TAG = "webdavbox3";

// Define the static instance pointer
WebDAVBox3 *WebDAVBox3::instance_ = nullptr;

void WebDAVBox3::setup() {
  ESP_LOGD(TAG, "Setting up WebDAV server on port %d", this->port_);
  
  if (this->auth_enabled_) {
    ESP_LOGD(TAG, "Authentication enabled with username: %s", this->username_.c_str());
    // Pre-compute base64 auth string if authentication is enabled
    std::string auth_string = this->username_ + ":" + this->password_;
    
    // Use mbedtls base64 encode function
    size_t output_len;
    // First calculate the required output length
    mbedtls_base64_encode(nullptr, 0, &output_len, 
                         (const unsigned char*)auth_string.c_str(), auth_string.length());
                         
    // Allocate buffer with room for null terminator
    unsigned char* base64_buf = new unsigned char[output_len + 1];
    
    // Perform the actual encoding
    mbedtls_base64_encode(base64_buf, output_len, &output_len, 
                         (const unsigned char*)auth_string.c_str(), auth_string.length());
    
    // Null terminate the string
    base64_buf[output_len] = 0;
    
    // Store the base64 encoded string
    this->auth_base64_ = std::string((char*)base64_buf);
    
    // Clean up
    delete[] base64_buf;
  } else {
    ESP_LOGD(TAG, "Authentication disabled");
  }

  this->start_server();
}

void WebDAVBox3::loop() {
  // Nothing to do in loop
}

void WebDAVBox3::start_server() {
  // Set the static instance pointer
  instance_ = this;
  
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = this->port_;
  config.max_uri_handlers = 15;  // Increase handler count
  config.max_resp_headers = 32;  // Increase header count
  config.uri_match_fn = httpd_uri_match_wildcard;
  
  ESP_LOGI(TAG, "Starting WebDAV server on port %d", config.server_port);
  
  if (httpd_start(&this->server_, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start WebDAV server!");
    return;
  }
  
  this->register_uri_handlers();
}

void WebDAVBox3::register_uri_handlers() {
  // Register all the required URI handlers
  httpd_uri_t options_uri = {
    .uri = this->url_prefix_.c_str(),
    .method = HTTP_OPTIONS,
    .handler = handle_webdav_options,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(this->server_, &options_uri);
  
  httpd_uri_t propfind_uri = {
    .uri = this->url_prefix_.c_str(),
    .method = HTTP_PROPFIND,
    .handler = handle_webdav_propfind,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(this->server_, &propfind_uri);
  
  httpd_uri_t get_uri = {
    .uri = this->url_prefix_.c_str(),
    .method = HTTP_GET,
    .handler = handle_webdav_get,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(this->server_, &get_uri);
  
  httpd_uri_t put_uri = {
    .uri = this->url_prefix_.c_str(),
    .method = HTTP_PUT,
    .handler = handle_webdav_put,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(this->server_, &put_uri);
  
  httpd_uri_t delete_uri = {
    .uri = this->url_prefix_.c_str(),
    .method = HTTP_DELETE,
    .handler = handle_webdav_delete,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(this->server_, &delete_uri);
  
  httpd_uri_t mkcol_uri = {
    .uri = this->url_prefix_.c_str(),
    .method = HTTP_MKCOL,
    .handler = handle_webdav_mkcol,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(this->server_, &mkcol_uri);
}

void WebDAVBox3::stop_server() {
  if (this->server_ != nullptr) {
    httpd_stop(this->server_);
    this->server_ = nullptr;
  }
}

bool WebDAVBox3::authenticate(httpd_req_t *req) {
  if (!this->auth_enabled_) {
    return true;  // Authentication disabled, allow access
  }
  
  char *auth_header = nullptr;
  size_t auth_header_len = httpd_req_get_hdr_value_len(req, "Authorization");
  
  if (auth_header_len == 0) {
    ESP_LOGD(TAG, "No Authorization header found");
    return false;
  }
  
  auth_header = new char[auth_header_len + 1];
  esp_err_t err = httpd_req_get_hdr_value_str(req, "Authorization", auth_header, auth_header_len + 1);
  
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get Authorization header");
    delete[] auth_header;
    return false;
  }
  
  std::string expected = "Basic " + this->auth_base64_;
  bool authenticated = (strcmp(auth_header, expected.c_str()) == 0);
  
  delete[] auth_header;
  return authenticated;
}

esp_err_t WebDAVBox3::send_auth_required_response(httpd_req_t *req) {
  httpd_resp_set_status(req, "401 Unauthorized");
  httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"WebDAV Server\"");
  httpd_resp_send(req, "", 0);
  return ESP_OK;
}

std::string WebDAVBox3::get_file_path(httpd_req_t *req, const std::string &root_path) {
  // Get URI from request
  const char *uri = req->uri;
  std::string uri_str(uri);
  
  // Extract the part after the url_prefix
  std::string relative_path = uri_str;
  if (instance_->url_prefix_ != "/") {
    if (uri_str.find(instance_->url_prefix_) == 0) {
      relative_path = uri_str.substr(instance_->url_prefix_.length());
    }
  }
  
  // Ensure the path starts with /
  if (relative_path.empty() || relative_path[0] != '/') {
    relative_path = "/" + relative_path;
  }
  
  // Combine with root path
  std::string file_path = root_path;
  // Make sure root_path ends with / 
  if (!file_path.empty() && file_path.back() != '/') {
    file_path += '/';
  }
  
  // Remove leading / from relative_path if present
  if (!relative_path.empty() && relative_path[0] == '/') {
    relative_path = relative_path.substr(1);
  }
  
  file_path += relative_path;
  
  ESP_LOGD(TAG, "URI: %s -> File path: %s", uri, file_path.c_str());
  return file_path;
}

// Static request handlers

esp_err_t WebDAVBox3::handle_webdav_get(httpd_req_t *req) {
  if (!instance_->authenticate(req)) {
    return instance_->send_auth_required_response(req);
  }
  
  std::string filepath = get_file_path(req, instance_->root_path_);
  ESP_LOGI(TAG, "GET request for: %s", filepath.c_str());

  struct stat st;
  if (stat(filepath.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "File not found: %s", filepath.c_str());
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
  }

  if (S_ISDIR(st.st_mode)) {
    ESP_LOGE(TAG, "Cannot GET a directory: %s", filepath.c_str());
    httpd_resp_set_status(req, "405 Method Not Allowed");
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
  }

  FILE *file = fopen(filepath.c_str(), "rb");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file: %s", filepath.c_str());
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
  }

  // Set appropriate content type
  if (filepath.find(".html") != std::string::npos) {
    httpd_resp_set_type(req, "text/html");
  } else if (filepath.find(".txt") != std::string::npos) {
    httpd_resp_set_type(req, "text/plain");
  } else if (filepath.find(".css") != std::string::npos) {
    httpd_resp_set_type(req, "text/css");
  } else if (filepath.find(".js") != std::string::npos) {
    httpd_resp_set_type(req, "application/javascript");
  } else if (filepath.find(".jpg") != std::string::npos || filepath.find(".jpeg") != std::string::npos) {
    httpd_resp_set_type(req, "image/jpeg");
  } else if (filepath.find(".png") != std::string::npos) {
    httpd_resp_set_type(req, "image/png");
  } else if (filepath.find(".gif") != std::string::npos) {
    httpd_resp_set_type(req, "image/gif");
  } else if (filepath.find(".ico") != std::string::npos) {
    httpd_resp_set_type(req, "image/x-icon");
  } else {
    httpd_resp_set_type(req, "application/octet-stream");
  }

  char buffer[1024];
  size_t bytes_read;
  
  while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
    httpd_resp_send_chunk(req, buffer, bytes_read);
  }
  
  httpd_resp_send_chunk(req, nullptr, 0);
  fclose(file);
  
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_put(httpd_req_t *req) {
  if (!instance_->authenticate(req)) {
    return instance_->send_auth_required_response(req);
  }
  
  std::string filepath = get_file_path(req, instance_->root_path_);
  ESP_LOGI(TAG, "PUT request for: %s", filepath.c_str());
  
  // Ensure parent directory exists
  size_t last_slash = filepath.find_last_of('/');
  if (last_slash != std::string::npos) {
    std::string parent_dir = filepath.substr(0, last_slash);
    if (!parent_dir.empty()) {
      // Create parent directories if needed
      mkdir(parent_dir.c_str(), 0755);
    }
  }
  
  FILE *file = fopen(filepath.c_str(), "wb");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file for writing: %s", filepath.c_str());
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
  }
  
  char buffer[1024];
  int remaining = req->content_len;
  int received;
  
  while (remaining > 0) {
    if ((received = httpd_req_recv(req, buffer, std::min(sizeof(buffer), (size_t)remaining))) <= 0) {
      if (received == HTTPD_SOCK_ERR_TIMEOUT) {
        continue;
      }
      
      ESP_LOGE(TAG, "Socket error during file reception");
      fclose(file);
      httpd_resp_set_status(req, "500 Internal Server Error");
      httpd_resp_send(req, nullptr, 0);
      return ESP_OK;
    }
    
    fwrite(buffer, 1, received, file);
    remaining -= received;
  }
  
  fclose(file);
  httpd_resp_set_status(req, "201 Created");
  httpd_resp_send(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_delete(httpd_req_t *req) {
  if (!instance_->authenticate(req)) {
    return instance_->send_auth_required_response(req);
  }
  
  std::string filepath = get_file_path(req, instance_->root_path_);
  ESP_LOGI(TAG, "DELETE request for: %s", filepath.c_str());
  
  struct stat st;
  if (stat(filepath.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "File not found: %s", filepath.c_str());
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
  }
  
  bool success;
  if (S_ISDIR(st.st_mode)) {
    // Delete directory
    success = (rmdir(filepath.c_str()) == 0);
  } else {
    // Delete file
    success = (unlink(filepath.c_str()) == 0);
  }
  
  if (success) {
    httpd_resp_set_status(req, "204 No Content");
  } else {
    ESP_LOGE(TAG, "Failed to delete: %s", filepath.c_str());
    httpd_resp_set_status(req, "500 Internal Server Error");
  }
  
  httpd_resp_send(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_mkcol(httpd_req_t *req) {
  if (!instance_->authenticate(req)) {
    return instance_->send_auth_required_response(req);
  }
  
  std::string filepath = get_file_path(req, instance_->root_path_);
  ESP_LOGI(TAG, "MKCOL request for: %s", filepath.c_str());
  
  struct stat st;
  if (stat(filepath.c_str(), &st) == 0) {
    // Path already exists
    ESP_LOGE(TAG, "Path already exists: %s", filepath.c_str());
    httpd_resp_set_status(req, "405 Method Not Allowed");
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
  }
  
  // Create the directory
  if (mkdir(filepath.c_str(), 0755) != 0) {
    ESP_LOGE(TAG, "Failed to create directory: %s", filepath.c_str());
    httpd_resp_set_status(req, "500 Internal Server Error");
  } else {
    httpd_resp_set_status(req, "201 Created");
  }
  
  httpd_resp_send(req, nullptr, 0);
  return ESP_OK;
}

// Placeholder for PROPFIND handler
esp_err_t WebDAVBox3::handle_webdav_propfind(httpd_req_t *req) {
  // Minimal implementation for now
  httpd_resp_set_status(req, "207 Multi-Status");
  httpd_resp_set_type(req, "application/xml; charset=utf-8");
  
  // Simple response for testing
  const char *response = "<?xml version=\"1.0\"?>\n"
                        "<d:multistatus xmlns:d=\"DAV:\">\n"
                        "  <d:response>\n"
                        "    <d:href>/</d:href>\n"
                        "    <d:propstat>\n"
                        "      <d:prop>\n"
                        "        <d:resourcetype><d:collection/></d:resourcetype>\n"
                        "      </d:prop>\n"
                        "      <d:status>HTTP/1.1 200 OK</d:status>\n"
                        "    </d:propstat>\n"
                        "  </d:response>\n"
                        "</d:multistatus>";
  
  httpd_resp_send(req, response, strlen(response));
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_options(httpd_req_t *req) {
  httpd_resp_set_hdr(req, "DAV", "1, 2");
  httpd_resp_set_hdr(req, "Allow", "OPTIONS, GET, HEAD, PUT, DELETE, PROPFIND, MKCOL");
  httpd_resp_set_hdr(req, "MS-Author-Via", "DAV");
  httpd_resp_set_hdr(req, "Content-Length", "0");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "OPTIONS, GET, HEAD, PUT, DELETE, PROPFIND, MKCOL");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Authorization, Content-Type, Depth, Destination");
  
  httpd_resp_send(req, nullptr, 0);
  return ESP_OK;
}

bool WebDAVBox3::is_dir(const std::string &path) {
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    return false;
  }
  return S_ISDIR(st.st_mode);
}

std::vector<std::string> WebDAVBox3::list_dir(const std::string &path) {
  std::vector<std::string> files;
  DIR *dir = opendir(path.c_str());
  
  if (dir != nullptr) {
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
      std::string name = entry->d_name;
      if (name != "." && name != "..") {
        files.push_back(name);
      }
    }
    closedir(dir);
  }
  
  return files;
}

// Other handlers would be implemented here

}  // namespace webdavbox3
}  // namespace esphome











































