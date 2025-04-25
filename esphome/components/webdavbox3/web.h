#ifndef WEB_H
#define WEB_H

#include "webdavbox3.h"
#include <string>

namespace esphome {
namespace webdavbox3 {

class WebInterface {
public:
    static std::string get_web_interface_html(WebDAVBox3* instance) {
        std::string html = R"(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WebDAV File Manager</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f5f5f5;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            border-bottom: 1px solid #eee;
            padding-bottom: 10px;
        }
        .file-list {
            margin-top: 20px;
        }
        table {
            width: 100%;
            border-collapse: collapse;
        }
        th, td {
            padding: 12px 15px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }
        th {
            background-color: #f8f8f8;
            font-weight: bold;
        }
        tr:hover {
            background-color: #f9f9f9;
        }
        .actions {
            display: flex;
            gap: 10px;
        }
        button, .btn {
            padding: 8px 12px;
            background-color: #4CAF50;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            text-decoration: none;
            font-size: 14px;
        }
        button:hover, .btn:hover {
            background-color: #45a049;
        }
        .btn-danger {
            background-color: #f44336;
        }
        .btn-danger:hover {
            background-color: #d32f2f;
        }
        .btn-primary {
            background-color: #2196F3;
        }
        .btn-primary:hover {
            background-color: #0b7dda;
        }
        .upload-form {
            margin: 20px 0;
            padding: 20px;
            background: #f8f8f8;
            border-radius: 4px;
        }
        .path-navigation {
            margin-bottom: 20px;
            font-size: 16px;
        }
        .path-navigation a {
            color: #2196F3;
            text-decoration: none;
        }
        .path-navigation a:hover {
            text-decoration: underline;
        }
        .status {
            padding: 10px;
            margin: 10px 0;
            border-radius: 4px;
        }
        .status.success {
            background-color: #dff0d8;
            color: #3c763d;
        }
        .status.error {
            background-color: #f2dede;
            color: #a94442;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>WebDAV File Manager</h1>
        
        <div class="path-navigation" id="path-nav">
            <a href="/web">Root</a>
        </div>

        <div class="upload-form">
            <h3>Upload File</h3>
            <form id="uploadForm" enctype="multipart/form-data">
                <input type="file" name="file" id="fileInput" required>
                <button type="submit">Upload</button>
            </form>
        </div>

        <div class="file-list">
            <h3>Files</h3>
            <table>
                <thead>
                    <tr>
                        <th>Name</th>
                        <th>Size</th>
                        <th>Modified</th>
                        <th>Actions</th>
                    </tr>
                </thead>
                <tbody id="fileTable">
                    <!-- Files will be loaded here by JavaScript -->
                </tbody>
            </table>
        </div>

        <div class="create-folder">
            <h3>Create Folder</h3>
            <form id="createFolderForm">
                <input type="text" name="folderName" id="folderNameInput" placeholder="Folder name" required>
                <button type="submit">Create</button>
            </form>
        </div>
    </div>

    <script>
        let currentPath = '/';
        
        // Load files when page loads
        document.addEventListener('DOMContentLoaded', function() {
            loadFiles(currentPath);
        });

        // Function to load files for a given path
        function loadFiles(path) {
            fetch('/webdav' + path, {
                method: 'PROPFIND',
                headers: {
                    'Depth': '1',
                    'Content-Type': 'application/xml'
                },
                body: '<?xml version="1.0"?><d:propfind xmlns:d="DAV:"><d:prop><d:getlastmodified/><d:getcontentlength/><d:resourcetype/></d:prop></d:propfind>'
            })
            .then(response => response.text())
            .then(str => (new window.DOMParser()).parseFromString(str, "text/xml"))
            .then(xml => {
                const responses = xml.getElementsByTagName('d:response');
                const fileTable = document.getElementById('fileTable');
                fileTable.innerHTML = '';
                
                // Update path navigation
                updatePathNavigation(path);
                
                // Skip the first response (the directory itself)
                for (let i = 1; i < responses.length; i++) {
                    const href = responses[i].getElementsByTagName('d:href')[0].textContent;
                    const propstat = responses[i].getElementsByTagName('d:propstat')[0];
                    const prop = propstat.getElementsByTagName('d:prop')[0];
                    
                    const lastModified = prop.getElementsByTagName('d:getlastmodified')[0]?.textContent || '';
                    const contentLength = prop.getElementsByTagName('d:getcontentlength')[0]?.textContent || '0';
                    const resourceType = prop.getElementsByTagName('d:resourcetype')[0];
                    const isDirectory = resourceType.getElementsByTagName('d:collection').length > 0;
                    
                    const fileName = decodeURIComponent(href.split('/').pop());
                    const filePath = href.replace('/webdav', '');
                    
                    const row = document.createElement('tr');
                    
                    // Name column
                    const nameCell = document.createElement('td');
                    if (isDirectory) {
                        const folderLink = document.createElement('a');
                        folderLink.href = '#';
                        folderLink.textContent = fileName + '/';
                        folderLink.onclick = function(e) {
                            e.preventDefault();
                            currentPath = filePath;
                            loadFiles(currentPath);
                        };
                        nameCell.appendChild(folderLink);
                    } else {
                        nameCell.textContent = fileName;
                    }
                    row.appendChild(nameCell);
                    
                    // Size column
                    const sizeCell = document.createElement('td');
                    sizeCell.textContent = isDirectory ? '-' : formatFileSize(parseInt(contentLength));
                    row.appendChild(sizeCell);
                    
                    // Modified column
                    const modifiedCell = document.createElement('td');
                    modifiedCell.textContent = formatDate(lastModified);
                    row.appendChild(modifiedCell);
                    
                    // Actions column
                    const actionsCell = document.createElement('td');
                    actionsCell.className = 'actions';
                    
                    if (!isDirectory) {
                        const downloadBtn = document.createElement('a');
                        downloadBtn.className = 'btn btn-primary';
                        downloadBtn.href = '/webdav' + filePath;
                        downloadBtn.textContent = 'Download';
                        downloadBtn.download = fileName;
                        actionsCell.appendChild(downloadBtn);
                    }
                    
                    const deleteBtn = document.createElement('button');
                    deleteBtn.className = 'btn btn-danger';
                    deleteBtn.textContent = 'Delete';
                    deleteBtn.onclick = function() {
                        if (confirm('Are you sure you want to delete ' + fileName + '?')) {
                            deleteFile(filePath, isDirectory);
                        }
                    };
                    actionsCell.appendChild(deleteBtn);
                    
                    row.appendChild(actionsCell);
                    fileTable.appendChild(row);
                }
            })
            .catch(error => {
                console.error('Error loading files:', error);
                showStatus('Error loading files: ' + error.message, 'error');
            });
        }
        
        // Update path navigation breadcrumbs
        function updatePathNavigation(path) {
            const pathNav = document.getElementById('path-nav');
            const parts = path.split('/').filter(part => part.length > 0);
            
            let html = '<a href="#" onclick="loadFiles(\'/\')">Root</a>';
            let currentPath = '';
            
            for (let i = 0; i < parts.length; i++) {
                currentPath += '/' + parts[i];
                html += ' / <a href="#" onclick="loadFiles(\'' + currentPath + '\')">' + parts[i] + '</a>';
            }
            
            pathNav.innerHTML = html;
        }
        
        // Format file size
        function formatFileSize(bytes) {
            if (bytes === 0) return '0 Bytes';
            const k = 1024;
            const sizes = ['Bytes', 'KB', 'MB', 'GB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
        }
        
        // Format date
        function formatDate(dateString) {
            if (!dateString) return '';
            const date = new Date(dateString);
            return date.toLocaleString();
        }
        
        // Delete file or directory
        function deleteFile(path, isDirectory) {
            fetch('/webdav' + path, {
                method: 'DELETE'
            })
            .then(response => {
                if (response.ok) {
                    showStatus('Successfully deleted', 'success');
                    loadFiles(currentPath);
                } else {
                    throw new Error('Failed to delete');
                }
            })
            .catch(error => {
                console.error('Error deleting:', error);
                showStatus('Error deleting: ' + error.message, 'error');
            });
        }
        
        // Handle file upload
        document.getElementById('uploadForm').addEventListener('submit', function(e) {
            e.preventDefault();
            const fileInput = document.getElementById('fileInput');
            const file = fileInput.files[0];
            
            if (!file) return;
            
            const formData = new FormData();
            formData.append('file', file);
            
            fetch('/webdav' + currentPath + encodeURIComponent(file.name), {
                method: 'PUT',
                body: file
            })
            .then(response => {
                if (response.ok) {
                    showStatus('File uploaded successfully', 'success');
                    loadFiles(currentPath);
                    fileInput.value = '';
                } else {
                    throw new Error('Upload failed');
                }
            })
            .catch(error => {
                console.error('Upload error:', error);
                showStatus('Upload error: ' + error.message, 'error');
            });
        });
        
        // Handle folder creation
        document.getElementById('createFolderForm').addEventListener('submit', function(e) {
            e.preventDefault();
            const folderName = document.getElementById('folderNameInput').value.trim();
            
            if (!folderName) return;
            
            fetch('/webdav' + currentPath + encodeURIComponent(folderName), {
                method: 'MKCOL'
            })
            .then(response => {
                if (response.ok) {
                    showStatus('Folder created successfully', 'success');
                    loadFiles(currentPath);
                    document.getElementById('folderNameInput').value = '';
                } else {
                    throw new Error('Failed to create folder');
                }
            })
            .catch(error => {
                console.error('Error creating folder:', error);
                showStatus('Error creating folder: ' + error.message, 'error');
            });
        });
        
        // Show status message
        function showStatus(message, type) {
            const statusDiv = document.createElement('div');
            statusDiv.className = 'status ' + type;
            statusDiv.textContent = message;
            
            const container = document.querySelector('.container');
            container.insertBefore(statusDiv, container.firstChild);
            
            setTimeout(() => {
                statusDiv.remove();
            }, 5000);
        }
    </script>
</body>
</html>
)";
        return html;
    }
};

}  // namespace webdavbox3
}  // namespace esphome

#endif  // WEB_H
