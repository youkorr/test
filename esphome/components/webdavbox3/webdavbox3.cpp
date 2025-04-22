#include "webdavbox3.h"
#include "esphome/core/log.h"
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>
#include <sstream>
#include <ctime>
#include <cstring>

namespace esphome {
namespace webdavbox3 {

static const char *TAG = "webdavbox3";

void WebDAVBox3::setup() {
  ESP_LOGI(TAG, "Setting up WebDAV server on port %d", this->port_);
  
  // Ensure root path exists
  mkdir(this->root_path_.c_str(), 0755);
  
  // Configure and start HTTP server
  this->configure_http_server();
  this->start_server();
}

void WebDAVBox3::loop() {
  // Nothing to do in loop
}

void WebDAVBox3::configure_http_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = this->port_;
  config.max_uri_handlers = 15;
  config.stack_size = 8192;
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.max_resp_headers = 32;  // Augmenté pour gérer plus d'en-têtes

  if (httpd_start(&this->server_, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return;
  }

  // Register URI handlers
  httpd_uri_t uri_handlers[] = {
    {
      .uri = "/*",
      .method = HTTP_GET,
      .handler = handle_webdav_get,
      .user_ctx = nullptr
    },
    {
      .uri = "/*",
      .method = HTTP_PUT,
      .handler = handle_webdav_put,
      .user_ctx = nullptr
    },
    {
      .uri = "/*",
      .method = HTTP_DELETE,
      .handler = handle_webdav_delete,
      .user_ctx = nullptr
    },
    {
      .uri = "/*",
      .method = HTTP_PROPFIND,
      .handler = handle_webdav_propfind,
      .user_ctx = nullptr
    },
    {
      .uri = "/*",
      .method = HTTP_PROPPATCH,
      .handler = handle_webdav_proppatch,
      .user_ctx = nullptr
    },
    {
      .uri = "/*",
      .method = HTTP_MKCOL,
      .handler = handle_webdav_mkcol,
      .user_ctx = nullptr
    },
    {
      .uri = "/*",
      .method = HTTP_COPY,
      .handler = handle_webdav_copy,
      .user_ctx = nullptr
    },
    {
      .uri = "/*",
      .method = HTTP_MOVE,
      .handler = handle_webdav_move,
      .user_ctx = nullptr
    },
    {
      .uri = "/*",
      .method = HTTP_OPTIONS,
      .handler = handle_webdav_options,
      .user_ctx = nullptr
    },
    {
      .uri = "/*",
      .method = HTTP_LOCK,
      .handler = handle_webdav_lock,
      .user_ctx = nullptr
    },
    {
      .uri = "/*",
      .method = HTTP_UNLOCK,
      .handler = handle_webdav_unlock,
      .user_ctx = nullptr
    }
  };

  for (const auto &handler : uri_handlers) {
    if (httpd_register_uri_handler(this->server_, &handler) != ESP_OK) {
      ESP_LOGE(TAG, "Failed to register handler for %s", handler.uri);
    }
  }
}

void WebDAVBox3::start_server() {
  if (this->server_ == nullptr) {
    ESP_LOGE(TAG, "Server not configured");
    return;
  }
  ESP_LOGI(TAG, "WebDAV server started successfully");
}

void WebDAVBox3::stop_server() {
  if (this->server_ != nullptr) {
    httpd_stop(this->server_);
    this->server_ = nullptr;
  }
}

std::string WebDAVBox3::get_file_path(httpd_req_t *req, const std::string &root_path) {
  std::string uri(req->uri);
  if (uri == "/") return root_path;
  return root_path + uri;
}

bool WebDAVBox3::authenticate(httpd_req_t *req) {
  if (!this->auth_enabled_) {
    return true;
  }

  char *auth_header = nullptr;
  size_t auth_header_len = httpd_req_get_hdr_value_len(req, "Authorization");
  
  if (auth_header_len > 0) {
    auth_header = (char*)malloc(auth_header_len + 1);
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, auth_header_len + 1) == ESP_OK) {
      // Basic auth validation
      std::string auth_str = this->username_ + ":" + this->password_;
      std::string expected_auth = "Basic " + esphome::base64_encode((const uint8_t*)auth_str.c_str(), auth_str.length());
      bool authenticated = (strcmp(auth_header, expected_auth.c_str()) == 0);
      free(auth_header);
      return authenticated;
    }
    free(auth_header);
  }
  
  return false;
}

esp_err_t WebDAVBox3::send_auth_required_response(httpd_req_t *req) {
  httpd_resp_set_status(req, "401 Unauthorized");
  httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"WebDAV Server\"");
  return httpd_resp_send(req, nullptr, 0);
}

esp_err_t WebDAVBox3::handle_webdav_options(httpd_req_t *req) {
  httpd_resp_set_hdr(req, "DAV", "1,2");
  httpd_resp_set_hdr(req, "Allow", "OPTIONS,PROPFIND,GET,HEAD,PUT,DELETE,MKCOL,MOVE,COPY,PROPPATCH,LOCK,UNLOCK");
  httpd_resp_set_hdr(req, "Content-Length", "0");
  httpd_resp_set_hdr(req, "MS-Author-Via", "DAV");
  httpd_resp_set_type(req, "text/plain");
  
  return httpd_resp_send(req, nullptr, 0);
}

esp_err_t WebDAVBox3::handle_webdav_propfind(httpd_req_t *req) {
  std::string path = get_file_path(req, "/sdcard");
  struct stat st;
  
  if (stat(path.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Path not found: %s", path.c_str());
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
  }

  httpd_resp_set_type(req, "application/xml; charset=utf-8");
  httpd_resp_set_status(req, "207 Multi-Status");
  httpd_resp_set_hdr(req, "DAV", "1,2");

  std::stringstream xml;
  xml << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
      << "<D:multistatus xmlns:D=\"DAV:\">\n";

  // Add current resource
  xml << "<D:response>\n"
      << "  <D:href>" << req->uri << "</D:href>\n"
      << "  <D:propstat>\n"
      << "    <D:prop>\n"
      << "      <D:resourcetype>";
  
  if (S_ISDIR(st.st_mode)) {
    xml << "<D:collection/>";
  }
  
  xml << "</D:resourcetype>\n"
      << "      <D:getcontentlength>" << st.st_size << "</D:getcontentlength>\n"
      << "      <D:getlastmodified>" << ctime(&st.st_mtime) << "</D:getlastmodified>\n"
      << "    </D:prop>\n"
      << "    <D:status>HTTP/1.1 200 OK</D:status>\n"
      << "  </D:propstat>\n"
      << "</D:response>\n";

  // If it's a directory, list its contents
  if (S_ISDIR(st.st_mode)) {
    DIR *dir = opendir(path.c_str());
    if (dir != nullptr) {
      struct dirent *entry;
      while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string fullPath = path + "/" + name;
        struct stat entry_st;
        if (stat(fullPath.c_str(), &entry_st) == 0) {
          std::string href = std::string(req->uri) + (req->uri[strlen(req->uri)-1] == '/' ? "" : "/") + name;
          
          xml << "<D:response>\n"
              << "  <D:href>" << href << "</D:href>\n"
              << "  <D:propstat>\n"
              << "    <D:prop>\n"
              << "      <D:resourcetype>";
          
          if (S_ISDIR(entry_st.st_mode)) {
            xml << "<D:collection/>";
          }
          
          xml << "</D:resourcetype>\n"
              << "      <D:getcontentlength>" << entry_st.st_size << "</D:getcontentlength>\n"
              << "      <D:getlastmodified>" << ctime(&entry_st.st_mtime) << "</D:getlastmodified>\n"
              << "    </D:prop>\n"
              << "    <D:status>HTTP/1.1 200 OK</D:status>\n"
              << "  </D:propstat>\n"
              << "</D:response>\n";
        }
      }
      closedir(dir);
    }
  }

  xml << "</D:multistatus>";
  
  return httpd_resp_send(req, xml.str().c_str(), xml.str().length());
}

esp_err_t WebDAVBox3::handle_webdav_get(httpd_req_t *req) {
  std::string path = get_file_path(req, "/sdcard");
  struct stat st;
  
  if (stat(path.c_str(), &st) != 0) {
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
  }

  if (S_ISDIR(st.st_mode)) {
    return handle_webdav_propfind(req);
  }

  FILE* file = fopen(path.c_str(), "rb");
  if (!file) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
  }

  httpd_resp_set_type(req, "application/octet-stream");
  char contentLength[32];
  snprintf(contentLength, sizeof(contentLength), "%ld", st.st_size);
  httpd_resp_set_hdr(req, "Content-Length", contentLength);

  char buffer[1024];
  size_t bytesRead;
  while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
    if (httpd_resp_send_chunk(req, buffer, bytesRead) != ESP_OK) {
      fclose(file);
      return ESP_FAIL;
    }
  }

  fclose(file);
  return httpd_resp_send_chunk(req, nullptr, 0);
}

esp_err_t WebDAVBox3::handle_webdav_put(httpd_req_t *req) {
  std::string path = get_file_path(req, "/sdcard");
  
  // Créer les répertoires parents si nécessaire
  size_t pos = 0;
  while ((pos = path.find('/', pos + 1)) != std::string::npos) {
    std::string dir = path.substr(0, pos);
    mkdir(dir.c_str(), 0755);
  }
  
  FILE* file = fopen(path.c_str(), "wb");
  if (!file) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
  }

  char buffer[1024];
  int remaining = req->content_len;
  
  while (remaining > 0) {
    int toRead = std::min(remaining, (int)sizeof(buffer));
    int received = httpd_req_recv(req, buffer, toRead);
    
    if (received <= 0) {
      fclose(file);
      return ESP_FAIL;
    }
    
    fwrite(buffer, 1, received, file);
    remaining -= received;
  }

  fclose(file);
  httpd_resp_set_status(req, "201 Created");
  return httpd_resp_send(req, nullptr, 0);
}

esp_err_t WebDAVBox3::handle_webdav_mkcol(httpd_req_t *req) {
  std::string path = get_file_path(req, "/sdcard");
  
  // Créer les répertoires parents si nécessaire
  size_t pos = 0;
  while ((pos = path.find('/', pos + 1)) != std::string::npos) {
    std::string dir = path.substr(0, pos);
    mkdir(dir.c_str(), 0755);
  }
  
  if (mkdir(path.c_str(), 0755) != 0) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create directory");
  }

  httpd_resp_set_status(req, "201 Created");
  return httpd_resp_send(req, nullptr, 0);
}

esp_err_t WebDAVBox3::handle_webdav_delete(httpd_req_t *req) {
  std::string path = get_file_path(req, "/sdcard");
  struct stat st;
  
  if (stat(path.c_str(), &st) != 0) {
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
  }

  if (S_ISDIR(st.st_mode)) {
    if (rmdir(path.c_str()) != 0) {
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to delete directory");
    }
  } else {
    if (remove(path.c_str()) != 0) {
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to delete file");
    }
  }

  httpd_resp_set_status(req, "204 No Content");
  return httpd_resp_send(req, nullptr, 0);
}

esp_err_t WebDAVBox3::handle_webdav_move(httpd_req_t *req) {
  char *destination = nullptr;
  size_t dest_len = httpd_req_get_hdr_value_len(req, "Destination");
  
  if (dest_len <= 0) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Destination header required");
  }

  destination = (char*)malloc(dest_len + 1);
  if (httpd_req_get_hdr_value_str(req, "Destination", destination, dest_len + 1) != ESP_OK) {
    free(destination);
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid destination");
  }

  std::string src_path = get_file_path(req, "/sdcard");
  std::string dest_path = get_file_path(req, "/sdcard");  // Adapter selon l'URL de destination

  if (rename(src_path.c_str(), dest_path.c_str()) != 0) {
    free(destination);
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to move resource");
  }

  free(destination);
  httpd_resp_set_status(req, "201 Created");
  return httpd_resp_send(req, nullptr, 0);
}

esp_err_t WebDAVBox3::handle_webdav_copy(httpd_req_t *req) {
  // Similaire à MOVE mais avec copie au lieu de déplacement
  return handle_webdav_move(req);  // Pour l'instant, même comportement que MOVE
}

esp_err_t WebDAVBox3::handle_webdav_lock(httpd_req_t *req) {
  // Réponse simple pour LOCK (pas de vraie implémentation de verrouillage)
  const char* response = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                        "<D:prop xmlns:D=\"DAV:\">\n"
                        "  <D:lockdiscovery>\n"
                        "    <D:activelock>\n"
                        "      <D:locktype><D:write/></D:locktype>\n"
                        "      <D:lockscope><D:exclusive/></D:lockscope>\n"
                        "      <D:depth>0</D:depth>\n"
                        "      <D:timeout>Second-3600</D:timeout>\n"
                        "    </D:activelock>\n"
                        "  </D:lockdiscovery>\n"
                        "</D:prop>";

  httpd_resp_set_type(req, "application/xml; charset=utf-8");
  httpd_resp_set_status(req, "200 OK");
  return httpd_resp_send(req, response, strlen(response));
}

esp_err_t WebDAVBox3::handle_webdav_unlock(httpd_req_t *req) {
  // Réponse simple pour UNLOCK
  httpd_resp_set_status(req, "204 No Content");
  return httpd_resp_send(req, nullptr, 0);
}

esp_err_t WebDAVBox3::handle_webdav_proppatch(httpd_req_t *req) {
  httpd_resp_set_status(req, "207 Multi-Status");
  httpd_resp_set_type(req, "application/xml; charset=utf-8");
  
  const char* response = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                        "<D:multistatus xmlns:D=\"DAV:\">\n"
                        "  <D:response>\n"
                        "    <D:href>/</D:href>\n"
                        "    <D:propstat>\n"
                        "      <D:status>HTTP/1.1 200 OK</D:status>\n"
                        "    </D:propstat>\n"
                        "  </D:response>\n"
                        "</D:multistatus>";
  
  return httpd_resp_send(req, response, strlen(response));
}

}  // namespace webdavbox3
}  // namespace esphome




















