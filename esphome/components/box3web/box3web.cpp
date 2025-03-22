#include "box3web.h"
#include "esphome/core/log.h"
#include <esp_vfs.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>

namespace esphome {
namespace box3web {

static const char *TAG = "box3web";

// Initialize static instance pointer
Box3WebComponent *Box3WebComponent::instance = nullptr;

Box3WebComponent::Box3WebComponent() {
  instance = this;
}

void Box3WebComponent::setup() {
  ESP_LOGD(TAG, "Setting up Box3Web HTTP File Server");
  
  // Check if SD card is mounted
  if (!sd_mmc_card::global_sd_card->is_mounted()) {
    ESP_LOGE(TAG, "SD card is not mounted. HTTP File Server cannot be initialized.");
    this->mark_failed();
    return;
  }
  
  // Setup the HTTP server
  this->setup_server();
}

void Box3WebComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Box3Web HTTP File Server:");
  ESP_LOGCONFIG(TAG, "  URL Prefix: /%s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Enable Deletion: %s", YESNO(this->enable_deletion_));
  ESP_LOGCONFIG(TAG, "  Enable Download: %s", YESNO(this->enable_download_));
  ESP_LOGCONFIG(TAG, "  Enable Upload: %s", YESNO(this->enable_upload_));
}

float Box3WebComponent::get_setup_priority() const {
  return setup_priority::LATE;
}

void Box3WebComponent::setup_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 10;
  config.stack_size = 8192;
  
  // Start the httpd server
  ESP_LOGI(TAG, "Starting HTTP server");
  esp_err_t ret = httpd_start(&this->server_, &config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
    this->mark_failed();
    return;
  }
  
  // Register URI handlers
  this->register_handlers();
  ESP_LOGI(TAG, "HTTP server started successfully");
}

void Box3WebComponent::register_handlers() {
  // Main index page handler
  httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = &Box3WebComponent::index_handler,
    .user_ctx = nullptr
  };
  httpd_register_uri_handler(this->server_, &index_uri);
  
  // File download handler
  httpd_uri_t download_uri = {
    .uri = "/download",
    .method = HTTP_GET,
    .handler = &Box3WebComponent::download_handler,
    .user_ctx = nullptr
  };
  httpd_register_uri_handler(this->server_, &download_uri);
  
  // File delete handler
  httpd_uri_t delete_uri = {
    .uri = "/delete",
    .method = HTTP_POST,
    .handler = &Box3WebComponent::delete_handler,
    .user_ctx = nullptr
  };
  httpd_register_uri_handler(this->server_, &delete_uri);
  
  // File upload handler
  httpd_uri_t upload_uri = {
    .uri = "/upload",
    .method = HTTP_POST,
    .handler = &Box3WebComponent::upload_handler,
    .user_ctx = nullptr
  };
  httpd_register_uri_handler(this->server_, &upload_uri);
}

bool Box3WebComponent::is_valid_path(const std::string &path) {
  if (path.find("..") != std::string::npos) {
    return false;
  }
  return true;
}

std::string Box3WebComponent::get_content_type(const std::string &path) {
  if (path.ends_with(".html")) return "text/html";
  if (path.ends_with(".css")) return "text/css";
  if (path.ends_with(".js")) return "application/javascript";
  if (path.ends_with(".json")) return "application/json";
  if (path.ends_with(".png")) return "image/png";
  if (path.ends_with(".jpg") || path.ends_with(".jpeg")) return "image/jpeg";
  if (path.ends_with(".gif")) return "image/gif";
  if (path.ends_with(".ico")) return "image/x-icon";
  if (path.ends_with(".txt")) return "text/plain";
  if (path.ends_with(".yaml") || path.ends_with(".yml")) return "text/yaml";
  if (path.ends_with(".mp3") || path.ends_with(".wav")) return "audio/mpeg";
  return "application/octet-stream";
}

esp_err_t Box3WebComponent::index_handler(httpd_req_t *req) {
  const char *html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-S3-BOX3 File Server</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f5f5f5;
        }
        h1 {
            color: #333;
        }
        .container {
            background-color: #fff;
            border-radius: 5px;
            padding: 20px;
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.1);
        }
        .file-list {
            margin-top: 20px;
        }
        .file-item {
            display: flex;
            justify-content: space-between;
            padding: 10px;
            border-bottom: 1px solid #eee;
        }
        .file-item:hover {
            background-color: #f9f9f9;
        }
        .upload-form {
            margin-top: 20px;
            padding: 15px;
            background-color: #f9f9f9;
            border-radius: 5px;
        }
        .btn {
            padding: 8px 12px;
            background-color: #4CAF50;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
        }
        .btn-download {
            background-color: #2196F3;
        }
        .btn-delete {
            background-color: #f44336;
        }
        .actions {
            display: flex;
            gap: 5px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32-S3-BOX3 File Server</h1>
        
        <div class="file-list" id="file-list">
            Loading...
        </div>
        
        <div class="upload-form">
            <h3>Upload File</h3>
            <form id="upload-form" enctype="multipart/form-data">
                <input type="file" name="file" id="file-input" required>
                <button type="submit" class="btn">Upload</button>
            </form>
            <div id="upload-status"></div>
        </div>
    </div>
    
    <script>
        function loadFiles() {
            fetch('/list')
                .then(response => response.json())
                .then(data => {
                    const fileListElem = document.getElementById('file-list');
                    fileListElem.innerHTML = '';
                    
                    data.files.forEach(file => {
                        const fileElem = document.createElement('div');
                        fileElem.className = 'file-item';
                        fileElem.innerHTML = `
                            <span>📄 ${file}</span>
                            <div class="actions">
                                <a href="/download?file=${file}" class="btn btn-download">Download</a>
                                <button onclick="deleteFile('${file}')" class="btn btn-delete">Delete</button>
                            </div>
                        `;
                        fileListElem.appendChild(fileElem);
                    });
                })
                .catch(error => {
                    console.error('Error loading files:', error);
                    document.getElementById('file-list').innerHTML = 'Error loading files';
                });
        }
        
        function uploadFile() {
            const fileInput = document.getElementById('file-input');
            const file = fileInput.files[0];
            
            if (!file) {
                document.getElementById('upload-status').textContent = 'Please select a file';
                return;
            }
            
            const formData = new FormData();
            formData.append('file', file);
            
            document.getElementById('upload-status').textContent = 'Uploading...';
            
            fetch('/upload', {
                method: 'POST',
                body: formData
            })
            .then(response => response.text())
            .then(result => {
                document.getElementById('upload-status').textContent = result;
                loadFiles();
                document.getElementById('upload-form').reset();
            })
            .catch(error => {
                console.error('Error uploading file:', error);
                document.getElementById('upload-status').textContent = 'Error uploading file';
            });
        }
        
        function deleteFile(file) {
            if (confirm('Are you sure you want to delete this file?')) {
                fetch('/delete', {
                    method: 'POST',
                    body: JSON.stringify({ file: file }),
                    headers: { 'Content-Type': 'application/json' }
                })
                .then(response => response.text())
                .then(result => {
                    alert(result);
                    loadFiles();
                })
                .catch(error => {
                    console.error('Error deleting file:', error);
                    alert('Error deleting file');
                });
            }
        }
        
        document.addEventListener('DOMContentLoaded', loadFiles);
        document.getElementById('upload-form').addEventListener('submit', function(e) {
            e.preventDefault();
            uploadFile();
        });
    </script>
</body>
</html>
)";

  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, html, strlen(html));
  return ESP_OK;
}

esp_err_t Box3WebComponent::download_handler(httpd_req_t *req) {
  char file[256];
  if (httpd_req_get_url_query_str(req, file, sizeof(file)) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing file parameter");
    return ESP_FAIL;
  }

  std::string filepath = this->root_path_ + "/" + file;
  if (!this->is_valid_path(filepath)) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid file path");
    return ESP_FAIL;
  }

  FILE *f = fopen(filepath.c_str(), "r");
  if (!f) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, this->get_content_type(filepath).c_str());
  char buffer[1024];
  size_t read_size;
  while ((read_size = fread(buffer, 1, sizeof(buffer), f)) {
    httpd_resp_send_chunk(req, buffer, read_size);
  }
  fclose(f);
  httpd_resp_send_chunk(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t Box3WebComponent::delete_handler(httpd_req_t *req) {
  char buffer[256];
  int ret = httpd_req_recv(req, buffer, sizeof(buffer));
  if (ret <= 0) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
    return ESP_FAIL;
  }

  std::string file = buffer;
  std::string filepath = this->root_path_ + "/" + file;
  if (!this->is_valid_path(filepath)) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid file path");
    return ESP_FAIL;
  }

  if (unlink(filepath.c_str())) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to delete file");
    return ESP_FAIL;
  }

  httpd_resp_send(req, "File deleted successfully", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

esp_err_t Box3WebComponent::upload_handler(httpd_req_t *req) {
  char buffer[1024];
  int received = httpd_req_recv(req, buffer, sizeof(buffer));
  if (received <= 0) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive file");
    return ESP_FAIL;
  }

  std::string filepath = this->root_path_ + "/" + req->uri;
  FILE *f = fopen(filepath.c_str(), "w");
  if (!f) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
    return ESP_FAIL;
  }

  fwrite(buffer, 1, received, f);
  fclose(f);
  httpd_resp_send(req, "File uploaded successfully", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

void Box3WebComponent::loop() {
  // Nothing to do in loop for this component
}

}  // namespace box3web
}  // namespace esphome



