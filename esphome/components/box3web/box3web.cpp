// box3web.cpp
#include "box3web.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace box3web {

static const char *TAG = "box3web";

Box3Web::Box3Web(web_server_base::WebServerBase *base) : base_(base) {}

void Box3Web::setup() { 
    this->base_->add_handler(this); 
}

void Box3Web::dump_config() {
    ESP_LOGCONFIG(TAG, "Box3Web:");
    ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
    ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_.c_str());
    ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
    ESP_LOGCONFIG(TAG, "  Enabled Deletion : %s", TRUEFALSE(this->enable_deletion_));
    ESP_LOGCONFIG(TAG, "  Enabled Download : %s", TRUEFALSE(this->enable_download_));
    ESP_LOGCONFIG(TAG, "  Enabled Upload : %s", TRUEFALSE(this->enable_upload_));
}

bool Box3Web::canHandle(AsyncWebServerRequest *request) {
    return str_startswith(std::string(request->url().c_str()), this->build_prefix());
}

void Box3Web::handleRequest(AsyncWebServerRequest *request) {
    if (str_startswith(std::string(request->url().c_str()), this->build_prefix())) {
        if (request->method() == HTTP_GET) {
            this->handle_get(request);
            return;
        }
        if (request->method() == HTTP_DELETE) {
            this->handle_delete(request);
            return;
        }
    }
}

void Box3Web::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                            size_t len, bool final) {
    if (!this->upload_enabled_) {
        request->send(401, "application/json", "{ \"error\": \"file upload is disabled\" }");
        return;
    }

    std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
    std::string path = this->build_absolute_path(extracted);

    // Validate upload folder
    if (index == 0 && !this->sd_mmc_card_->is_directory(path)) {
        request->send(401, "application/json", "{ \"error\": \"invalid upload folder\" }");
        return;
    }

    std::string full_path = Path::join(path, std::string(filename.c_str()));
    
    // New upload or continuing upload
    if (index == 0) {
        // First chunk - create or overwrite file
        ESP_LOGD(TAG, "Uploading file %s to %s", filename.c_str(), full_path.c_str());
        this->sd_mmc_card_->write_file(full_path.c_str(), data, len);
    } else {
        // Continuing upload - append data
        this->sd_mmc_card_->append_file(full_path.c_str(), data, len);
    }

    // Final chunk
    if (final) {
        auto response = request->beginResponse(201, "text/html", "upload success");
        response->addHeader("Connection", "close");
        request->send(response);
    }
}

void Box3Web::handle_get(AsyncWebServerRequest *request) const {
    std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
    std::string path = this->build_absolute_path(extracted);

    if (!this->sd_mmc_card_->is_directory(path)) {
        handle_download(request, path);
        return;
    }

    handle_index(request, path);
}

void Box3Web::write_row(AsyncResponseStream *response, sd_mmc_card::FileInfo const &info) const {
    std::string uri = "/" + Path::join(this->url_prefix_, Path::remove_root_path(info.path, this->root_path_));
    std::string file_name = Path::file_name(info.path);
    
    response->print("<tr><td>");
    
    if (info.is_directory) {
        response->printf("<a href=\"%s\">%s</a>", uri.c_str(), file_name.c_str());
    } else {
        response->print(file_name.c_str());
    }
    
    response->print("</td><td>");
    
    if (!info.is_directory) {
        if (this->download_enabled_) {
            response->printf("<button onClick=\"download_file('%s','%s')\">Download</button>", 
                             uri.c_str(), file_name.c_str());
        }
        
        if (this->deletion_enabled_) {
            response->printf("<button onClick=\"delete_file('%s')\">Delete</button>", uri.c_str());
        }
    }
    
    response->print("</td></tr>");
}

void Box3Web::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    
    // HTML Header and Basic Structure
    response->print(F("<!DOCTYPE html><html lang=\"en\"><head>"
                      "<meta charset=UTF-8>"
                      "<meta name=viewport content=\"width=device-width, initial-scale=1,user-scalable=no\">"
                      "<title>SD Card Content</title>"
                      "<style>"
                      "body { font-family: Arial, sans-serif; margin: 20px; }"
                      "table { width: 100%; border-collapse: collapse; }"
                      "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }"
                      "thead { background-color: #f2f2f2; }"
                      "</style>"
                      "</head><body>"));
    
    response->print(F("<h1>SD Card Content</h1>"));
    response->printf("<h2>Folder %s</h2>", path.c_str());

    // Upload Form
    if (this->upload_enabled_) {
        response->print(F("<form method=\"POST\" enctype=\"multipart/form-data\">"
                          "<input type=\"file\" name=\"file\">"
                          "<input type=\"submit\" value=\"Upload\">"
                          "</form>"));
    }

    // Navigation
    response->printf(F("<a href=\"/%s\">Home</a><br><br>"), this->url_prefix_.c_str());

    // File List Table
    response->print(F("<table id=\"files\">"
                      "<thead><tr><th>Name</th><th>Actions</th></tr></thead>"
                      "<tbody>"));

    // List directory contents
    auto entries = this->sd_mmc_card_->list_directory_file_info(path, 0);
    for (auto const &entry : entries) {
        write_row(response, entry);
    }

    response->print(F("</tbody></table>"));

    // JavaScript for file actions
    response->print(F("<script>"
                      "function delete_file(path) {"
                      "  fetch(path, {method: 'DELETE'})"
                      "    .then(response => location.reload())"
                      "    .catch(console.error);"
                      "}"
                      "function download_file(path, filename) {"
                      "  fetch(path)"
                      "    .then(response => response.blob())"
                      "    .then(blob => {"
                      "      const link = document.createElement('a');"
                      "      link.href = URL.createObjectURL(blob);"
                      "      link.download = filename;"
                      "      link.click();"
                      "    })"
                      "    .catch(console.error);"
                      "}"
                      "</script>"
                      "</body></html>"));

    request->send(response);
}

void Box3Web::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
    if (!this->download_enabled_) {
        request->send(401, "application/json", "{ \"error\": \"file download is disabled\" }");
        return;
    }

    auto file = this->sd_mmc_card_->read_file(path);
    if (file.size() == 0) {
        request->send(404, "application/json", "{ \"error\": \"failed to read file\" }");
        return;
    }

    // Get filename for Content-Disposition
    std::string filename = Path::file_name(path);

    // Correction : utiliser la méthode beginResponse avec un buffer
    auto *response = request->beginResponseStream("application/octet-stream");
    response->print(std::string(file.begin(), file.end()));
    
    // Add filename suggestion in download header
    char content_disposition[256];
    snprintf(content_disposition, sizeof(content_disposition), 
             "attachment; filename=\"%s\"", filename.c_str());
    response->addHeader("Content-Disposition", content_disposition);

    request->send(response);
}

void Box3Web::handle_delete(AsyncWebServerRequest *request) {
    if (!this->deletion_enabled_) {
        request->send(401, "application/json", "{ \"error\": \"file deletion is disabled\" }");
        return;
    }

    std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
    std::string path = this->build_absolute_path(extracted);

    if (this->sd_mmc_card_->is_directory(path)) {
        request->send(401, "application/json", "{ \"error\": \"cannot delete a directory\" }");
        return;
    }

    if (this->sd_mmc_card_->delete_file(path)) {
        request->send(204, "application/json", "{}");
        return;
    }

    request->send(401, "application/json", "{ \"error\": \"failed to delete file\" }");
}

// Utility method implementations
std::string Box3Web::build_prefix() const {
    if (this->url_prefix_.empty() || this->url_prefix_[0] != '/')
        return "/" + this->url_prefix_;
    return this->url_prefix_;
}

std::string Box3Web::extract_path_from_url(std::string const &url) const {
    std::string prefix = this->build_prefix();
    return url.substr(prefix.size());
}

std::string Box3Web::build_absolute_path(std::string relative_path) const {
    if (relative_path.empty())
        return this->root_path_;

    return Path::join(this->root_path_, relative_path);
}

// Path utility method implementations
std::string Path::file_name(std::string const &path) {
    size_t pos = path.rfind(Path::separator);
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

bool Path::is_absolute(std::string const &path) { 
    return !path.empty() && path[0] == separator; 
}

bool Path::trailing_slash(std::string const &path) { 
    return !path.empty() && path[path.length() - 1] == separator; 
}

std::string Path::join(std::string const &first, std::string const &second) {
    std::string result = first;
    if (!trailing_slash(first) && !is_absolute(second)) {
        result.push_back(separator);
    }
    if (trailing_slash(first) && is_absolute(second)) {
        result.pop_back();
    }
    result.append(second);
    return result;
}

std::string Path::remove_root_path(std::string path, std::string const &root) {
    if (!str_startswith(path, root))
        return path;
    if (path.size() == root.size() || path.size() < 2)
        return "/";
    return path.erase(0, root.size());
}

} // namespace box3web
} // namespace esphome






