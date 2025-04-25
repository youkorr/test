#include "web.h"
#include "webdavbox3.h"
#include <string>

namespace esphome {
namespace webdavbox3 {

std::string WebDAVBox3::get_web_interface_html(WebDAVBox3* instance) {
  return R"(<!DOCTYPE html>
<html>
<head>
  <title>WebDAV File Manager</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; }
    h1 { color: #333; }
    table { width: 100%; border-collapse: collapse; margin-top: 20px; }
    th, td { padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }
    th { background-color: #f2f2f2; }
    a { color: #0066cc; text-decoration: none; }
    a:hover { text-decoration: underline; }
  </style>
</head>
<body>
  <h1>WebDAV File Manager</h1>
  <div id="content">
    <p>Loading file list...</p>
  </div>
  <script>
    fetch('/webdav', {
      method: 'PROPFIND',
      headers: { 'Depth': '1' }
    })
    .then(response => response.text())
    .then(data => {
      // Simple parsing du XML (à améliorer)
      const parser = new DOMParser();
      const xmlDoc = parser.parseFromString(data, "text/xml");
      const responses = xmlDoc.getElementsByTagName('d:response');
      let html = '<table><tr><th>Name</th><th>Size</th><th>Type</th></tr>';
      
      for (let i = 1; i < responses.length; i++) {
        const href = responses[i].getElementsByTagName('d:href')[0].textContent;
        const isCollection = responses[i].getElementsByTagName('d:collection').length > 0;
        const displayName = href.split('/').pop();
        
        html += `<tr>
          <td><a href="${href}">${displayName}</a></td>
          <td>${isCollection ? '-' : ''}</td>
          <td>${isCollection ? 'Directory' : 'File'}</td>
        </tr>`;
      }
      
      html += '</table>';
      document.getElementById('content').innerHTML = html;
    })
    .catch(error => {
      document.getElementById('content').innerHTML = 
        `<p style="color: red">Error loading file list: ${error.message}</p>`;
    });
  </script>
</body>
</html>
)";
}

}  // namespace webdavbox3
}  // namespace esphome
