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
  // Set the static instance to this instance
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
    .uri = ("/" + this->url_prefix_).c_str(),
    .method = HTTP_GET,
    .handler = &Box3WebComponent::index_handler,
    .user_ctx = nullptr
  };
  httpd_register_uri_handler(this->server_, &index_uri);
  
  // File listing/browsing handler
  httpd_uri_t file_uri = {
    .uri = ("/" + this->url_prefix_ + "/*").c_str(),
    .method = HTTP_GET,
    .handler = &Box3WebComponent::file_handler,
    .user_ctx = nullptr
  };
  httpd_register_uri_handler(this->server_, &file_uri);
  
  // Upload handler (if enabled)
  if (this->enable_upload_) {
    httpd_uri_t upload_uri = {
      .uri = ("/" + this->url_prefix_ + "/upload").c_str(),
      .method = HTTP_POST,
      .handler = &Box3WebComponent::upload_handler,
      .user_ctx = nullptr
    };
    httpd_register_uri_handler(this->server_, &upload_uri);
  }
  
  // Delete handler (if enabled)
  if (this->enable_deletion_) {
    httpd_uri_t delete_uri = {
      .uri = ("/" + this->url_prefix_ + "/delete").c_str(),
      .method = HTTP_POST,
      .handler = &Box3WebComponent::delete_handler,
      .user_ctx = nullptr
    };
    httpd_register_uri_handler(this->server_, &delete_uri);
  }
  
  // Download handler (if enabled)
  if (this->enable_download_) {
    httpd_uri_t download_uri = {
      .uri = ("/" + this->url_prefix_ + "/download").c_str(),
      .method = HTTP_GET,
      .handler = &Box3WebComponent::download_handler,
      .user_ctx = nullptr
    };
    httpd_register_uri_handler(this->server_, &download_uri);
  }
}

bool Box3WebComponent::is_valid_path(const std::string &path) {
  // Check if path contains ".."
  if (path.find("..") != std::string::npos) {
    return false;
  }
  
  // Other security checks can be added here
  
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
  if (path.ends_with(".pdf")) return "application/pdf";
  
  return "application/octet-stream";
}

// Handler implementations
esp_err_t Box3WebComponent::index_handler(httpd_req_t *req) {
  // Serve the main HTML page with file browser interface
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
        <div id="path-nav">Current path: /</div>
        
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
        let currentPath = '/';
        
        // Load files on page load
        document.addEventListener('DOMContentLoaded', loadFiles);
        
        // Setup upload form
        document.getElementById('upload-form').addEventListener('submit', function(e) {
            e.preventDefault();
            uploadFile();
        });
        
        function loadFiles(path) {
            if (path) currentPath = path;
            
            // Update path navigation
            document.getElementById('path-nav').textContent = 'Current path: ' + currentPath;
            
            fetch('/files/list?path=' + encodeURIComponent(currentPath))
                .then(response => response.json())
                .then(data => {
                    const fileListElem = document.getElementById('file-list');
                    fileListElem.innerHTML = '';
                    
                    // Add parent directory link if not at root
                    if (currentPath !== '/') {
                        const parentPath = currentPath.substring(0, currentPath.lastIndexOf('/'));
                        const parentDir = document.createElement('div');
                        parentDir.className = 'file-item';
                        parentDir.innerHTML = `
                            <span>📁 <a href="#" onclick="loadFiles('${parentPath || '/'}'); return false;">..</a></span>
                        `;
                        fileListElem.appendChild(parentDir);
                    }
                    
                    // Add directories
                    data.directories.forEach(dir => {
                        const dirElem = document.createElement('div');
                        dirElem.className = 'file-item';
                        
                        const dirPath = currentPath === '/' ? 
                            '/' + dir : currentPath + '/' + dir;
                            
                        dirElem.innerHTML = `
                            <span>📁 <a href="#" onclick="loadFiles('${dirPath}'); return false;">${dir}</a></span>
                        `;
                        fileListElem.appendChild(dirElem);
                    });
                    
                    // Add files
                    data.files.forEach(file => {
                        const fileElem = document.createElement('div');
                        fileElem.className = 'file-item';
                        
                        const filePath = currentPath === '/' ? 
                            '/' + file : currentPath + '/' + file;
                            
                        let actions = `
                            <div class="actions">
                                <a href="/files/download?path=${encodeURIComponent(filePath)}" class="btn btn-download">Download</a>
                        `;
                        
                        actions += `
                                <button onclick="deleteFile('${filePath}')" class="btn btn-delete">Delete</button>
                            </div>
                        `;
                        
                        fileElem.innerHTML = `
                            <span>📄 ${file}</span>
                            ${actions}
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
            formData.append('path', currentPath);
            
            document.getElementById('upload-status').textContent = 'Uploading...';
            
            fetch('/files/upload', {
                method: 'POST',
                body: formData
            })
            .then(response => response.text())
            .then(result => {
                document.getElementById('upload-status').textContent = result;
                loadFiles(); // Reload the file list
                document.getElementById('upload-form').reset();
            })
            .catch(error => {
                console.error('Error uploading file:', error);
                document.getElementById('upload-status').textContent = 'Error uploading file';
            });
        }
        
        function deleteFile(path) {
            if (confirm('Are you sure you want to delete this file?')) {
                const formData = new FormData();
                formData.append('path', path);
                
                fetch('/files/delete', {
                    method: 'POST',
                    body: formData
                })
                .then(response => response.text())
                .then(result => {
                    alert(result);
                    loadFiles(); // Reload the file list
                })
                .catch(error => {
                    console.error('Error deleting file:', error);
                    alert('Error deleting file');
                });
            }
        }
    </script>
</body>
</html>
)";

  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, html, strlen(html));
  return ESP_OK;
}

esp_err_t Box3WebComponent::file_handler(httpd_req_t *req) {
  // Get request URI
  char uri[500];
  strlcpy(uri, req->uri, sizeof(uri));
  
  // Get query parameters
  char query[500] = {0};
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    // Check if this is a list request
    char param[128];
    if (httpd_query_key_value(query, "list", param, sizeof(param)) == ESP_OK) {
      // This is a directory listing request
      char path_param[400] = {0};
      if (httpd_query_key_value(query, "path", path_param, sizeof(path_param)) != ESP_OK) {
        // Default to root if no path specified
        strlcpy(path_param, "/", sizeof(path_param));
      }
      
      // Ensure path is valid
      if (!instance->is_valid_path(path_param)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path");
        return ESP_FAIL;
      }
      
      // Open the directory
      DIR *dir = opendir(path_param);
      if (!dir) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory not found");
        return ESP_FAIL;
      }
      
      // Build JSON response for directory listing
      std::string json = "{\n  \"directories\": [";
      std::string files_json = "\n  \"files\": [";
      
      bool has_dirs = false;
      bool has_files = false;
      
      struct dirent *entry;
      while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") {
          continue;
        }
        
        // Construct full path
        std::string full_path = path_param;
        if (full_path.back() != '/') {
          full_path += '/';
        }
        full_path += name;
        
        struct stat st;
        if (stat(full_path.c_str(), &st) != 0) {
          continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
          // This is a directory
          if (has_dirs) {
            json += ",";
          }
          json += "\n    \"" + name + "\"";
          has_dirs = true;
        } else {
          // This is a file
          if (has_files) {
            files_json += ",";
          }
          files_json += "\n    \"" + name + "\"";
          has_files = true;
        }
      }
      
      closedir(dir);
      
      json += "\n  ],";
      json += files_json + "\n  ]\n}";
      
      httpd_resp_set_type(req, "application/json");
      httpd_resp_send(req, json.c_str(), json.length());
      return ESP_OK;
    }
  }
  
  // If not a special request, serve the file
  char *filename = uri + strlen("/" + instance->url_prefix_);
  if (filename[0] == '/') {
    filename++; // Skip the leading slash
  }
  
  std::string filepath = instance->root_path_;
  if (filepath.back() != '/' && filename[0] != '/') {
    filepath += '/';
  }
  filepath += filename;
  
  if (!instance->is_valid_path(filepath)) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path");
    return ESP_FAIL;
  }
  
  struct stat st;
  if (stat(filepath.c_str(), &st) != 0) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    return ESP_FAIL;
  }
  
  FILE *file = fopen(filepath.c_str(), "r");
  if (!file) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
    return ESP_FAIL;
  }
  
  // Set content type based on file extension
  httpd_resp_set_type(req, instance->get_content_type(filepath).c_str());
  
  // Set Content-Disposition header for inline display
  httpd_resp_set_hdr(req, "Content-Disposition", "inline");
  
  // Send file in chunks
  char buffer[1024];
  size_t read_size;
  do {
    read_size = fread(buffer, 1, sizeof(buffer), file);
    if (read_size > 0) {
      if (httpd_resp_send_chunk(req, buffer, read_size) != ESP_OK) {
        fclose(file);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
        return ESP_FAIL;
      }
    }
  } while (read_size == sizeof(buffer));
  
  // Send empty chunk to signal the end of response
  httpd_resp_send_chunk(req, nullptr, 0);
  fclose(file);
  
  return ESP_OK;
}

esp_err_t Box3WebComponent::upload_handler(httpd_req_t *req) {
  if (!instance->enable_upload_) {
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Upload is disabled");
    return ESP_FAIL;
  }
  
  // Get content length
  int content_length = req->content_len;
  if (content_length <= 0) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty request");
    return ESP_FAIL;
  }
  
  // Buffer for reading multipart form data boundaries
  char boundary[100];
  char filename[200] = {0};
  char path_param[400] = "/";
  
  // Process multipart form data
  bool found_boundary = false;
  bool found_filename = false;
  bool found_path = false;
  bool reading_file_data = false;
  FILE *file = nullptr;
  char buf[1024];
  int received = 0;
  
  while (received < content_length) {
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) {
      if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
        continue;
      }
      
      if (file) {
        fclose(file);
      }
      
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
      return ESP_FAIL;
    }
    
    received += ret;
    
    // If we're already processing file data
    if (reading_file_data && file) {
      fwrite(buf, 1, ret, file);
      continue;
    }
    
    // Look for boundary, filename, and data start
    if (!found_boundary) {
      // Find boundary
      char *boundary_start = strstr(buf, "------WebKitFormBoundary");
      if (boundary_start) {
        int boundary_len = 0;
        while (boundary_start[boundary_len] != '\r' && boundary_len < sizeof(boundary) - 1) {
          boundary[boundary_len] = boundary_start[boundary_len];
          boundary_len++;
        }
        boundary[boundary_len] = '\0';
        found_boundary = true;
      }
    }
    
    if (!found_filename) {
      // Look for content disposition and extract filename
      char *filename_start = strstr(buf, "filename=\"");
      if (filename_start) {
        filename_start += 10; // Skip "filename=\""
        int filename_len = 0;
        while (filename_start[filename_len] != '\"' && filename_len < sizeof(filename) - 1) {
          filename[filename_len] = filename_start[filename_len];
          filename_len++;
        }
        filename[filename_len] = '\0';
        found_filename = true;
      }
    }
    
    if (!found_path) {
      // Look for path parameter
      char *path_start = strstr(buf, "name=\"path\"");
      if (path_start) {
        // Find the path value after the headers
        char *path_value = strstr(path_start, "\r\n\r\n");
        if (path_value) {
          path_value += 4; // Skip "\r\n\r\n"
          int path_len = 0;
          while (path_value[path_len] != '\r' && path_len < sizeof(path_param) - 1) {
            path_param[path_len] = path_value[path_len];
            path_len++;
          }
          path_param[path_len] = '\0';
          found_path = true;
        }
      }
    }
    
    if (found_filename && !reading_file_data) {
      // Find the start of file data (after \r\n\r\n)
      char *data_start = strstr(buf, "\r\n\r\n");
      if (data_start) {
        // Skip the headers
        data_start += 4;
        
        // Ensure path is valid
        if (!instance->is_valid_path(path_param)) {
          httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path");
          return ESP_FAIL;
        }
        
        // Create directory if it doesn't exist
        std::string dir_path = path_param;
        if (dir_path.back() != '/') {
          dir_path += '/';
        }
        
        // Create target filepath
        std::string filepath = dir_path + filename;
        
        // Open the file for writing
        file = fopen(filepath.c_str(), "w");
        if (!file) {
          httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
          return ESP_FAIL;
        }
        
        // Write the beginning of file data
        int data_len = ret - (data_start - buf);
        fwrite(data_start, 1, data_len, file);
        
        reading_file_data = true;
      }
    }
  }
  
  if (file) {
    fclose(file);
  }
  
  if (!found_filename || !file) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid upload request");
    return ESP_FAIL;
  }
  
  // Send response
  const char *resp_str = "File uploaded successfully";
  httpd_resp_send(req, resp_str, strlen(resp_str));
  
  return ESP_OK;
}

esp_err_t Box3WebComponent::delete_handler(httpd_req_t *req) {
  if (!instance->enable_deletion_) {
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Deletion is disabled");
    return ESP_FAIL;
  }
  
  // Get content length
  int content_length = req->content_len;
  if (content_length <= 0) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty request");
    return ESP_FAIL;
  }
  
  // Buffer for form data
  char *buf = (char *)malloc(content_length + 1);
  if (!buf) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal server error");
// Completion of delete_handler method
esp_err_t Box3WebComponent::delete_handler(httpd_req_t *req) {
  if (!instance->enable_deletion_) {
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Deletion is disabled");
    return ESP_FAIL;
  }
  
  // Get content length
  int content_length = req->content_len;
  if (content_length <= 0) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty request");
    return ESP_FAIL;
  }
  
  // Buffer for form data
  char *buf = (char *)malloc(content_length + 1);
  if (!buf) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal server error");
    return ESP_FAIL;
  }
  
  // Receive data
  int ret = httpd_req_recv(req, buf, content_length);
  if (ret <= 0) {
    free(buf);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
    return ESP_FAIL;
  }
  
  buf[content_length] = '\0';
  
  // Parse form data to get path
  char path_param[400] = {0};
  
  // Simple parsing for "path=..." in the form data
  char *path_start = strstr(buf, "path=");
  if (path_start) {
    path_start += 5; // Skip "path="
    
    // Find the end of path (& or end of string)
    char *path_end = strchr(path_start, '&');
    if (path_end) {
      *path_end = '\0';
    }
    
    // URL decode the path
    instance->url_decode(path_start, path_param, sizeof(path_param));
  }
  
  free(buf);
  
  if (path_param[0] == '\0') {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Path parameter missing");
    return ESP_FAIL;
  }
  
  // Ensure path is valid
  if (!instance->is_valid_path(path_param)) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path");
    return ESP_FAIL;
  }
  
  // Delete the file
  if (unlink(path_param) != 0) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to delete file");
    return ESP_FAIL;
  }
  
  // Send response
  const char *resp_str = "File deleted successfully";
  httpd_resp_send(req, resp_str, strlen(resp_str));
  
  return ESP_OK;
}

esp_err_t Box3WebComponent::download_handler(httpd_req_t *req) {
  if (!instance->enable_download_) {
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Download is disabled");
    return ESP_FAIL;
  }
  
  // Get query parameters
  char query[500] = {0};
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No query parameters");
    return ESP_FAIL;
  }
  
  // Get path parameter
  char path_param[400] = {0};
  if (httpd_query_key_value(query, "path", path_param, sizeof(path_param)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Path parameter missing");
    return ESP_FAIL;
  }
  
  // Ensure path is valid
  if (!instance->is_valid_path(path_param)) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path");
    return ESP_FAIL;
  }
  
  // Check if file exists
  struct stat st;
  if (stat(path_param, &st) != 0) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    return ESP_FAIL;
  }
  
  // Open the file
  FILE *file = fopen(path_param, "r");
  if (!file) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
    return ESP_FAIL;
  }
  
  // Get filename from path
  const char *filename = strrchr(path_param, '/');
  if (filename) {
    filename++; // Skip the '/'
  } else {
    filename = path_param;
  }
  
  // Set content type
  httpd_resp_set_type(req, "application/octet-stream");
  
  // Set Content-Disposition header for download
  char disposition[500];
  snprintf(disposition, sizeof(disposition), "attachment; filename=\"%s\"", filename);
  httpd_resp_set_hdr(req, "Content-Disposition", disposition);
  
  // Send file in chunks
  char buffer[1024];
  size_t read_size;
  do {
    read_size = fread(buffer, 1, sizeof(buffer), file);
    if (read_size > 0) {
      if (httpd_resp_send_chunk(req, buffer, read_size) != ESP_OK) {
        fclose(file);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
        return ESP_FAIL;
      }
    }
  } while (read_size == sizeof(buffer));
  
  // Send empty chunk to signal the end of response
  httpd_resp_send_chunk(req, nullptr, 0);
  fclose(file);
  
  return ESP_OK;
}

// Helper method for URL decoding
void Box3WebComponent::url_decode(const char *src, char *dst, size_t dst_size) {
  size_t src_len = strlen(src);
  size_t i = 0, j = 0;
  
  while (i < src_len && j < dst_size - 1) {
    if (src[i] == '%' && i + 2 < src_len) {
      // Handle percent-encoded characters
      char hex[3] = {src[i+1], src[i+2], 0};
      dst[j++] = (char)strtol(hex, nullptr, 16);
      i += 3;
    } else if (src[i] == '+') {
      // Handle plus as space
      dst[j++] = ' ';
      i++;
    } else {
      // Copy character as-is
      dst[j++] = src[i++];
    }
  }
  
  dst[j] = '\0';
}

void Box3WebComponent::loop() {
  // Nothing to do in loop for this component
}

}  // namespace box3web
}  // namespace esphome	
