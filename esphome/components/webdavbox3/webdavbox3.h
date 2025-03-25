#pragma once

#include "esphome/core/component.h"
#include "esphome/components/web_server/web_server.h"
#include "SD_MMC.h"

namespace esphome {
namespace webdavbox3 {

class WebDAVBox3 : public web_server::WebServer {
public:
    void setup() override;
    void loop() override;

protected:
    void handle_webdav_request_();
    
    // Méthodes de gestion des requêtes WebDAV
    void handle_get_file_(const String& path);
    void handle_put_file_(const String& path);
    void handle_delete_file_(const String& path);
    void list_directory_(const String& path);

private:
    // Chemin racine pour les opérations WebDAV
    String root_path_{"/"}; 

public:
    // Setter pour le chemin racine
    void set_root_path(const std::string& path) {
        root_path_ = String(path.c_str());
    }
};

} // namespace webdavbox3
} // namespace esphome
