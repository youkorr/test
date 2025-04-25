#include "webdavbox3.h"
#include <esp_http_client.h>
#include "mbedtls/base64.h" 
#include <string.h>
#include "esphome/core/log.h"
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fstream>
#include <errno.h>
#include "esp_err.h"
#include "esp_netif.h"
#include <ctime>
#include <chrono>
#include <sys/stat.h>
#include <fcntl.h>
#include "web.h"

namespace esphome {
namespace webdavbox3 {

static const char *const TAG = "webdavbox3";

std::string normalize_path(const std::string& base_path, const std::string& path) {
  std::string result;
  
  if (path.empty() || path == ".") {
    return base_path;
  }
  
  if (path[0] == '/') {
    if (path == "/") {
      return base_path;
    }
    std::string clean_path = path;
    if (base_path.back() == '/' && path[0] == '/') {
      clean_path = path.substr(1);
    }
    result = base_path + clean_path;
  } else {
    if (base_path.back() == '/') {
      result = base_path + path;
    } else {
      result = base_path + "/" + path;
    }
  }
  
  ESP_LOGD(TAG, "Normalized path: %s (from base: %s, request: %s)", 
           result.c_str(), base_path.c_str(), path.c_str());
  
  return result;
}

std::string url_decode(const std::string &src) {
  std::string result;
  char ch;
  int i, j;
  for (i = 0; i < src.length(); i++) {
    if (src[i] == '%' && i + 2 < src.length()) {
      sscanf(src.substr(i + 1, 2).c_str(), "%x", &j);
      ch = static_cast<char>(j);
      result += ch;
      i += 2;
    } else if (src[i] == '+') {
      result += ' ';
    } else {
      result += src[i];
    }
  }
  return result;
}

void WebDAVBox3::setup() {
    struct stat st;
    
    // Vérification du point de montage
    if (stat(root_path_.c_str(), &st) != 0) {
        ESP_LOGI(TAG, "Creating root directory: %s", root_path_.c_str());
        if (mkdir(root_path_.c_str(), 0755) != 0) {
            ESP_LOGE(TAG, "Failed to create root directory: %s (errno: %d)", root_path_.c_str(), errno);
            return;
        }
    } 
    else if (!S_ISDIR(st.st_mode)) {
        ESP_LOGE(TAG, "Root path is not a directory: %s", root_path_.c_str());
        return;
    }

    // Vérification des permissions
    if (access(root_path_.c_str(), R_OK | W_OK | X_OK) != 0) {
        ESP_LOGE(TAG, "Insufficient permissions on root directory: %s (errno: %d)", root_path_.c_str(), errno);
        if (rmdir(root_path_.c_str()) == 0) {
            if (mkdir(root_path_.c_str(), 0755) != 0) {
                ESP_LOGE(TAG, "Failed to recreate directory: %s", root_path_.c_str());
                return;
            }
        } else {
            ESP_LOGE(TAG, "Failed to remove directory: %s", root_path_.c_str());
            return;
        }
    }

    ESP_LOGI(TAG, "Successfully initialized SD mount at %s", root_path_.c_str());
    this->configure_http_server();
    this->start_server();
}


void WebDAVBox3::loop() {
  // Nothing to do in loop
}

void WebDAVBox3::configure_http_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = port_;
  config.ctrl_port = port_ + 1000;
  config.max_uri_handlers = 16;  // Augmenter si nécessaire
  config.lru_purge_enable = true;
  config.recv_wait_timeout = 30;
  config.send_wait_timeout = 30;
  
  if (server_ != nullptr) {
    ESP_LOGW(TAG, "Server already started, stopping previous instance");
    httpd_stop(server_);
    server_ = nullptr;
  }
  
  if (httpd_start(&server_, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start server on port %d", port_);
    server_ = nullptr;
    return;
  }
  ESP_LOGI(TAG, "WebDAV Server started on port %d", port_);
    httpd_uri_t web_interface = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_web_interface, 
        .user_ctx = this
    };
  httpd_register_uri_handler(server_, &web_interface);

  // Handler WebDAV pour la racine
  httpd_uri_t webdav_root = {
      .uri = "/webdav",
      .method = HTTP_PROPFIND,
      .handler = handle_webdav_propfind,
      .user_ctx = this
  };
  httpd_register_uri_handler(server_, &webdav_root);

  // Handler WebDAV pour tous les chemins sous /webdav/
  httpd_uri_t webdav_all = {
      .uri = "/webdav/*",
      .method = HTTP_PROPFIND,
      .handler = handle_webdav_propfind,
      .user_ctx = this
  };
  httpd_register_uri_handler(server_, &webdav_all);
  httpd_uri_t root_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = handle_root,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &root_uri);

  httpd_uri_t webdav = {
    .uri = "/webdav/*",
    .method = HTTP_PROPFIND,
    .handler = handle_webdav_propfind,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &webdav);

  // 1. RACINE (GET et OPTIONS)
  httpd_uri_t root_get_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = handle_root,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &root_get_uri);
  
  httpd_uri_t root_options_uri = {
    .uri = "/",
    .method = HTTP_OPTIONS,
    .handler = handle_webdav_options,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &root_options_uri);
  
  // 2. PROPFIND (pour la racine spécifiquement)
  httpd_uri_t propfind_root_uri = {
    .uri = "/",
    .method = HTTP_PROPFIND,
    .handler = handle_webdav_propfind,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &propfind_root_uri);
  
  // 3. Autres méthodes pour la racine
  httpd_uri_t mkcol_root_uri = {
    .uri = "/",
    .method = HTTP_MKCOL,
    .handler = handle_webdav_mkcol,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &mkcol_root_uri);
  
  httpd_uri_t put_root_uri = {
    .uri = "/",
    .method = HTTP_PUT,
    .handler = handle_webdav_put,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &put_root_uri);
  
  httpd_uri_t delete_root_uri = {
    .uri = "/",
    .method = HTTP_DELETE,
    .handler = handle_webdav_delete,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &delete_root_uri);
  
  httpd_uri_t proppatch_root_uri = {
    .uri = "/",
    .method = HTTP_PROPPATCH,
    .handler = handle_webdav_proppatch,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &proppatch_root_uri);
  
  httpd_uri_t lock_root_uri = {
    .uri = "/",
    .method = HTTP_LOCK,
    .handler = handle_webdav_lock,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &lock_root_uri);
  
  httpd_uri_t unlock_root_uri = {
    .uri = "/",
    .method = HTTP_UNLOCK,
    .handler = handle_webdav_unlock,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &unlock_root_uri);
  
  httpd_uri_t copy_root_uri = {
    .uri = "/",
    .method = HTTP_COPY,
    .handler = handle_webdav_copy,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &copy_root_uri);
  
  httpd_uri_t move_root_uri = {
    .uri = "/",
    .method = HTTP_MOVE,
    .handler = handle_webdav_move,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &move_root_uri);
  
  // 4. AUTRES CHEMINS (pour uri = /*)
  httpd_uri_t options_uri = {
    .uri = "/*",
    .method = HTTP_OPTIONS,
    .handler = handle_webdav_options,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &options_uri);
  
  httpd_uri_t propfind_uri = {
    .uri = "/*",
    .method = HTTP_PROPFIND,
    .handler = handle_webdav_propfind,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &propfind_uri);
  
  httpd_uri_t get_uri = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = handle_webdav_get,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &get_uri);
  
  httpd_uri_t head_uri = {
    .uri = "/*",
    .method = HTTP_HEAD,
    .handler = handle_webdav_get,  // Souvent HEAD utilise le même handler que GET
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &head_uri);
  
  httpd_uri_t put_uri = {
    .uri = "/*",
    .method = HTTP_PUT,
    .handler = handle_webdav_put,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &put_uri);
  
  httpd_uri_t mkcol_uri = {
    .uri = "/*",
    .method = HTTP_MKCOL,
    .handler = handle_webdav_mkcol,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &mkcol_uri);
  
  httpd_uri_t delete_uri = {
    .uri = "/*",
    .method = HTTP_DELETE,
    .handler = handle_webdav_delete,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &delete_uri);
  
  httpd_uri_t copy_uri = {
    .uri = "/*",
    .method = HTTP_COPY,
    .handler = handle_webdav_copy,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &copy_uri);
  
  httpd_uri_t move_uri = {
    .uri = "/*",
    .method = HTTP_MOVE,
    .handler = handle_webdav_move,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &move_uri);
  
  httpd_uri_t proppatch_uri = {
    .uri = "/*",
    .method = HTTP_PROPPATCH,
    .handler = handle_webdav_proppatch,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &proppatch_uri);
  
  httpd_uri_t lock_uri = {
    .uri = "/*",
    .method = HTTP_LOCK,
    .handler = handle_webdav_lock,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &lock_uri);
  
  httpd_uri_t unlock_uri = {
    .uri = "/*",
    .method = HTTP_UNLOCK,
    .handler = handle_webdav_unlock,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &unlock_uri);
}

void WebDAVBox3::start_server() {
  if (server_ != nullptr)
    return;
  configure_http_server();
}

void WebDAVBox3::stop_server() {
  if (server_ != nullptr) {
    httpd_stop(server_);
    server_ = nullptr;
  }
}
esp_err_t WebDAVBox3::handle_root(httpd_req_t *req) {
    // Serve a simple HTML page or redirect to WebDAV interface
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    
    if (instance->auth_enabled_ && !instance->check_auth(req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"WebDAV\"");
        return httpd_resp_send(req, "Authentication required", HTTPD_RESP_USE_STRLEN);
    }

    // Simple HTML or redirect
    const char* html = "<html><head><title>WebDAV Server</title></head>"
                      "<body><h1>WebDAV Server</h1>"
                      "<p>Connect to <a href=\"/webdav/\">/webdav/</a> to access files.</p>"
                      "</body></html>";
                      
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, strlen(html));
}
esp_err_t WebDAVBox3::handle_web_interface(httpd_req_t *req) {
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    
    if (instance->auth_enabled_ && !instance->check_auth(req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"WebDAV\"");
        return httpd_resp_send(req, "Authentication required", HTTPD_RESP_USE_STRLEN);
    }

    std::string html = instance->get_web_interface_html(instance);
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html.c_str(), html.length());
}


bool WebDAVBox3::check_auth(httpd_req_t *req) {
    // Récupérer l'instance depuis le contexte
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    
    // Si l'authentification est désactivée
    if (!instance->auth_enabled_) {
        return true;
    }

    // Vérifier l'en-tête Authorization
    size_t auth_len = httpd_req_get_hdr_value_len(req, "Authorization");
    if (auth_len == 0) {
        return false;
    }

    // Allouer un buffer pour l'en-tête
    char* auth_header = (char*)malloc(auth_len + 1);
    if (auth_header == nullptr) {
        return false;
    }

    // Récupérer l'en-tête
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, auth_len + 1) != ESP_OK) {
        free(auth_header);
        return false;
    }

    // Vérifier le type Basic Auth
    if (strncmp(auth_header, "Basic ", 6) != 0) {
        free(auth_header);
        return false;
    }

    // Décoder les credentials Base64 avec mbedTLS
    char* encoded_credentials = auth_header + 6;
    size_t encoded_len = strlen(encoded_credentials);
    size_t max_decoded_len = encoded_len * 3 / 4 + 1;
    unsigned char* decoded = (unsigned char*)malloc(max_decoded_len);
    size_t decoded_len = 0;

    int ret = mbedtls_base64_decode(decoded, max_decoded_len, &decoded_len,
                                  (const unsigned char*)encoded_credentials, encoded_len);
    
    if (ret != 0 || decoded_len == 0) {
        free(auth_header);
        free(decoded);
        return false;
    }

    // Vérifier username:password
    std::string provided_credentials((char*)decoded, decoded_len);
    std::string expected_credentials = instance->username_ + ":" + instance->password_;
    
    free(auth_header);
    free(decoded);
    
    return provided_credentials == expected_credentials;
}


static size_t base64_decode(const uint8_t* src, size_t src_len, uint8_t* dest) {
    const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i, j;
    uint32_t a = 0;
    uint32_t b = 0;
    
    for (i = j = 0; i < src_len; i += 4) {
        a = 0;
        for (size_t k = 0; k < 4; k++) {
            if (i + k >= src_len) break;
            const char* p = strchr(b64, src[i + k]);
            a = (a << 6) | (p ? p - b64 : 0);
        }
        
        dest[j++] = (a >> 16) & 0xFF;
        if (src[i + 2] != '=') dest[j++] = (a >> 8) & 0xFF;
        if (src[i + 3] != '=') dest[j++] = a & 0xFF;
    }
    
    return j;
}

esp_err_t WebDAVBox3::handle_webdav_options(httpd_req_t *req) {
  ESP_LOGD(TAG, "OPTIONS request for path: %s", req->uri);
  
  // En-têtes importants pour WebDAV
  httpd_resp_set_hdr(req, "DAV", "1,2");
  httpd_resp_set_hdr(req, "Allow", "OPTIONS,PROPFIND,GET,HEAD,PUT,DELETE,COPY,MOVE,MKCOL,LOCK,UNLOCK,PROPPATCH");
  httpd_resp_set_hdr(req, "Content-Length", "0");
  httpd_resp_set_hdr(req, "MS-Author-Via", "DAV"); // Pour le support de Windows
  
  // Pour le support CORS si nécessaire
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "OPTIONS,PROPFIND,GET,HEAD,PUT,DELETE,COPY,MOVE,MKCOL,LOCK,UNLOCK,PROPPATCH");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Depth,Destination,Overwrite,Content-Type,Lock-Token");
  
  httpd_resp_send(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_propfind(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = normalize_path(inst->root_path_, req->uri);
  
  ESP_LOGD(TAG, "PROPFIND request for path: %s", path.c_str());
  
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Path not found: %s (errno: %d)", path.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
  }
  
  std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                    "<D:multistatus xmlns:D=\"DAV:\">\n";
  
  // Add entry for the requested path
  xml += generate_propfind_response(req->uri, st);
  
  // If it's a directory, add entries for its contents
  if (S_ISDIR(st.st_mode)) {
    DIR *dir = opendir(path.c_str());
    if (dir != nullptr) {
      struct dirent *entry;
      while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
          continue;
          
        std::string child_path = path + "/" + entry->d_name;
        if (stat(child_path.c_str(), &st) == 0) {
          std::string uri = std::string(req->uri);
          if (uri.back() != '/') uri += '/';
          uri += entry->d_name;
          xml += generate_propfind_response(uri, st);
        }
      }
      closedir(dir);
    }
  }
  
  xml += "</D:multistatus>";
  
  httpd_resp_set_type(req, "application/xml; charset=utf-8");
  httpd_resp_set_status(req, "207 Multi-Status");
  httpd_resp_send(req, xml.c_str(), xml.length());
  
  return ESP_OK;
}

std::string WebDAVBox3::generate_propfind_response(const std::string& uri, const struct stat& st) {
  char time_buf[32];
  strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&st.st_mtime));
  
  std::string response = "  <D:response>\n"
                        "    <D:href>" + uri + "</D:href>\n"
                        "    <D:propstat>\n"
                        "      <D:prop>\n"
                        "        <D:getlastmodified>" + time_buf + "</D:getlastmodified>\n"
                        "        <D:getcontentlength>" + std::to_string(st.st_size) + "</D:getcontentlength>\n"
                        "        <D:resourcetype>";
  
  if (S_ISDIR(st.st_mode)) {
    response += "<D:collection/>";
  }
  
  response += "</D:resourcetype>\n"
              "      </D:prop>\n"
              "      <D:status>HTTP/1.1 200 OK</D:status>\n"
              "    </D:propstat>\n"
              "  </D:response>\n";
  
  return response;
}

esp_err_t WebDAVBox3::handle_webdav_get(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = normalize_path(inst->root_path_, req->uri);
  
  ESP_LOGD(TAG, "GET request for path: %s", path.c_str());
  
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "File not found: %s (errno: %d)", path.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
  }
  
  if (S_ISDIR(st.st_mode)) {
    return handle_webdav_propfind(req);
  }
  
  FILE *file = fopen(path.c_str(), "rb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file: %s (errno: %d)", path.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
  }
  
  const char* ext = strrchr(path.c_str(), '.');
  if (ext != nullptr) {
    ext++; // Skip the dot
    if (strcasecmp(ext, "txt") == 0) {
      httpd_resp_set_type(req, "text/plain");
    } else if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) {
      httpd_resp_set_type(req, "text/html");
    } else if (strcasecmp(ext, "pdf") == 0) {
      httpd_resp_set_type(req, "application/pdf");
    } else if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) {
      httpd_resp_set_type(req, "image/jpeg");
    } else if (strcasecmp(ext, "png") == 0) {
      httpd_resp_set_type(req, "image/png");
    } else {
      httpd_resp_set_type(req, "application/octet-stream");
    }
  }
  
  httpd_resp_set_hdr(req, "Content-Length", std::to_string(st.st_size).c_str());
  
  char buffer[2048];
  size_t read_bytes;
  while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
    if (httpd_resp_send_chunk(req, buffer, read_bytes) != ESP_OK) {
      fclose(file);
      ESP_LOGE(TAG, "Failed to send file chunk");
      return ESP_FAIL;
    }
  }
  
  fclose(file);
  httpd_resp_send_chunk(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_put(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = normalize_path(inst->root_path_, req->uri);
  
  ESP_LOGD(TAG, "PUT request for path: %s", path.c_str());
  
  size_t last_slash = path.find_last_of('/');
  if (last_slash != std::string::npos) {
    std::string dir_path = path.substr(0, last_slash);
    if (!dir_path.empty()) {
      struct stat st;
      if (stat(dir_path.c_str(), &st) != 0) {
        if (mkdir(dir_path.c_str(), 0755) != 0) {
          ESP_LOGE(TAG, "Failed to create directory: %s (errno: %d)", dir_path.c_str(), errno);
          return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create directory");
        }
      }
    }
  }
  
  int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    ESP_LOGE(TAG, "Failed to create file: %s (errno: %d)", path.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
  }
  
  char buffer[2048];
  int received;
  while ((received = httpd_req_recv(req, buffer, sizeof(buffer))) > 0) {
    if (write(fd, buffer, received) != received) {
      close(fd);
      ESP_LOGE(TAG, "Failed to write to file: %s (errno: %d)", path.c_str(), errno);
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file");
    }
  }
  
  close(fd);
  httpd_resp_set_status(req, "201 Created");
  httpd_resp_send(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_mkcol(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = normalize_path(inst->root_path_, req->uri);
  
  ESP_LOGD(TAG, "MKCOL request for path: %s", path.c_str());
  
  struct stat st;
  if (stat(path.c_str(), &st) == 0) {
    return httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Directory already exists");
  }
  
  if (mkdir(path.c_str(), 0755) != 0) {
    ESP_LOGE(TAG, "Failed to create directory: %s (errno: %d)", path.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create directory");
  }
  
  httpd_resp_set_status(req, "201 Created");
  httpd_resp_send(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_delete(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = normalize_path(inst->root_path_, req->uri);
  
  ESP_LOGD(TAG, "DELETE request for path: %s", path.c_str());
  
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
  }
  
  if (S_ISDIR(st.st_mode)) {
    if (rmdir(path.c_str()) != 0) {
      ESP_LOGE(TAG, "Failed to delete directory: %s (errno: %d)", path.c_str(), errno);
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to delete directory");
    }
  } else {
    if (unlink(path.c_str()) != 0) {
      ESP_LOGE(TAG, "Failed to delete file: %s (errno: %d)", path.c_str(), errno);
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to delete file");
    }
  }
  
  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_send(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_copy(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string src_path = normalize_path(inst->root_path_, req->uri);
  
  char *destination = nullptr;
  size_t dest_len = httpd_req_get_hdr_value_len(req, "Destination") + 1;
  if (dest_len <= 1) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Destination header missing");
  }
  
  destination = (char*)malloc(dest_len);
  if (httpd_req_get_hdr_value_str(req, "Destination", destination, dest_len) != ESP_OK) {
    free(destination);
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Destination header");
  }
  
  const char *dest_path_start = strstr(destination, inst->root_path_.c_str());
  if (!dest_path_start) {
    dest_path_start = strchr(destination, '/');
  }
  if (!dest_path_start) {
    free(destination);
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Destination path");
  }
  
  std::string dest_path = normalize_path(inst->root_path_, dest_path_start);
  free(destination);
  
  ESP_LOGD(TAG, "COPY from %s to %s", src_path.c_str(), dest_path.c_str());
  
  struct stat st;
  if (stat(src_path.c_str(), &st) != 0) {
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Source not found");
  }
  
  if (S_ISDIR(st.st_mode)) {
    return httpd_resp_send_err(req, HTTPD_501_METHOD_NOT_IMPLEMENTED, "Directory copy not implemented");
  }
  
  std::ifstream src(src_path, std::ios::binary);
  std::ofstream dst(dest_path, std::ios::binary);
  
  if (!src || !dst) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Copy failed");
  }
  
  dst << src.rdbuf();
  
  httpd_resp_set_status(req, "201 Created");
  httpd_resp_send(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_move(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string src_path = normalize_path(inst->root_path_, req->uri);
  
  char *destination = nullptr;
  size_t dest_len = httpd_req_get_hdr_value_len(req, "Destination") + 1;
  if (dest_len <= 1) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Destination header missing");
  }
  
  destination = (char*)malloc(dest_len);
  if (httpd_req_get_hdr_value_str(req, "Destination", destination, dest_len) != ESP_OK) {
    free(destination);
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Destination header");
  }
  
  const char *dest_path_start = strstr(destination, inst->root_path_.c_str());
  if (!dest_path_start) {
    dest_path_start = strchr(destination, '/');
  }
  if (!dest_path_start) {
    free(destination);
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Destination path");
  }
  
  std::string dest_path = normalize_path(inst->root_path_, dest_path_start);
  free(destination);
  
  ESP_LOGD(TAG, "MOVE from %s to %s", src_path.c_str(), dest_path.c_str());
  
  if (rename(src_path.c_str(), dest_path.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to move: %s -> %s (errno: %d)", src_path.c_str(), dest_path.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Move failed");
  }
  
  httpd_resp_set_status(req, "201 Created");
  httpd_resp_send(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_lock(httpd_req_t *req) {
  ESP_LOGD(TAG, "LOCK request received");
  
  std::string response = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
                        "<D:prop xmlns:D=\"DAV:\">\n"
                        "  <D:lockdiscovery>\n"
                        "    <D:activelock>\n"
                        "      <D:locktype><D:write/></D:locktype>\n"
                        "      <D:lockscope><D:exclusive/></D:lockscope>\n"
                        "      <D:depth>0</D:depth>\n"
                        "      <D:timeout>Second-600</D:timeout>\n"
                        "    </D:activelock>\n"
                        "  </D:lockdiscovery>\n"
                        "</D:prop>";
  
  httpd_resp_set_type(req, "application/xml");
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_send(req, response.c_str(), response.length());
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_unlock(httpd_req_t *req) {
  ESP_LOGD(TAG, "UNLOCK request received");
  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_send(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_proppatch(httpd_req_t *req) {
  ESP_LOGD(TAG, "PROPPATCH request received");
  
  std::string response = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<D:multistatus xmlns:D=\"DAV:\">\n"
                        "  <D:response>\n"
                        "    <D:href>" + std::string(req->uri) + "</D:href>\n"
                        "    <D:propstat>\n"
                        "      <D:prop></D:prop>\n"
                        "      <D:status>HTTP/1.1 200 OK</D:status>\n"
                        "    </D:propstat>\n"
                        "  </D:response>\n"
                        "</D:multistatus>";
  
  httpd_resp_set_type(req, "application/xml");
  httpd_resp_set_status(req, "207 Multi-Status");
  httpd_resp_send(req, response.c_str(), response.length());
  return ESP_OK;
}

}  // namespace webdavbox3
}  // namespace esphome




















































































































































