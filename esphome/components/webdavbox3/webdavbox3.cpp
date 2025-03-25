#include "webdavbox3.h"
#include "esphome/core/log.h"

namespace esphome {
namespace webdavbox3 {

static const char* TAG = "webdavbox3";

void WebDAVBox3::setup() {
    // Initialisation du WebServer parent
    web_server::WebServer::setup();
    
    // Initialisation de la carte SD
    if (!SD_MMC.begin()) {
        ESP_LOGE(TAG, "Erreur d'initialisation de la carte SD");
        return;
    }
    
    // Enregistrer les gestionnaires de requêtes WebDAV
    this->server_.on("^\\/webdav\\/.*$", HTTP_GET, std::bind(&WebDAVBox3::handle_webdav_request_, this));
    this->server_.on("^\\/webdav\\/.*$", HTTP_PUT, std::bind(&WebDAVBox3::handle_webdav_request_, this));
    this->server_.on("^\\/webdav\\/.*$", HTTP_DELETE, std::bind(&WebDAVBox3::handle_webdav_request_, this));
}

void WebDAVBox3::loop() {
    // Appel du loop du parent
    web_server::WebServer::loop();
}

void WebDAVBox3::handle_webdav_request_() {
    String uri = this->server_.uri();
    
    // Supprimer le préfixe /webdav/
    uri.replace("/webdav/", "");
    uri = root_path_ + uri;
    
    switch (this->server_.method()) {
        case HTTP_GET:
            if (uri.endsWith("/")) {
                list_directory_(uri);
            } else {
                handle_get_file_(uri);
            }
            break;
        
        case HTTP_PUT:
            handle_put_file_(uri);
            break;
        
        case HTTP_DELETE:
            handle_delete_file_(uri);
            break;
        
        default:
            this->server_.send(405, "text/plain", "Méthode non autorisée");
            break;
    }
}

void WebDAVBox3::handle_get_file_(const String& path) {
    if (!SD_MMC.exists(path)) {
        this->server_.send(404, "text/plain", "Fichier non trouvé");
        return;
    }
    
    File file = SD_MMC.open(path, FILE_READ);
    if (!file) {
        this->server_.send(500, "text/plain", "Impossible d'ouvrir le fichier");
        return;
    }
    
    this->server_.streamFile(file, "application/octet-stream");
    file.close();
}

void WebDAVBox3::handle_put_file_(const String& path) {
    File file = SD_MMC.open(path, FILE_WRITE);
    if (!file) {
        this->server_.send(500, "text/plain", "Impossible de créer le fichier");
        return;
    }
    
    // Écriture du contenu du fichier
    size_t bytesWritten = file.write(
        (uint8_t*)this->server_.arg("plain").c_str(), 
        this->server_.arg("plain").length()
    );
    file.close();
    
    if (bytesWritten == this->server_.arg("plain").length()) {
        this->server_.send(200, "text/plain", "Fichier uploadé avec succès");
    } else {
        this->server_.send(500, "text/plain", "Erreur lors de l'upload");
    }
}

void WebDAVBox3::handle_delete_file_(const String& path) {
    if (SD_MMC.remove(path)) {
        this->server_.send(200, "text/plain", "Fichier supprimé avec succès");
    } else {
        this->server_.send(500, "text/plain", "Impossible de supprimer le fichier");
    }
}

void WebDAVBox3::list_directory_(const String& path) {
    File root = SD_MMC.open(path);
    if (!root || !root.isDirectory()) {
        this->server_.send(404, "text/plain", "Répertoire non trouvé");
        return;
    }
    
    String fileList = "";
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            fileList += file.name();
            fileList += "\n";
        }
        file = root.openNextFile();
    }
    
    this->server_.send(200, "text/plain", fileList);
}

} // namespace webdavbox3
} // namespace esphome
