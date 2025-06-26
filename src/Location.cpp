#include "Location.hpp"

Location::Location() : _autoindex(false) {}

Location::~Location() {}

void Location::setPath(const std::string& path) { _path = path; }
const std::string& Location::getPath() const { return _path; }

void Location::addAllowedMethod(const std::string& method) { _allowed_methods.push_back(method); }
const std::vector<std::string>& Location::getAllowedMethods() const { return _allowed_methods; }

void Location::setRoot(const std::string& root) { _root = root; }
const std::string& Location::getRoot() const { return _root; }

void Location::setAutoIndex(bool autoindex) { _autoindex = autoindex; }
bool Location::getAutoIndex() const { return _autoindex; }

void Location::setIndex(const std::string& index_file) { _index = index_file; }
const std::string& Location::getIndex() const { return _index; }

void Location::setCgiPath(const std::string& extension, const std::string& path) { _cgi_paths[extension] = path; }
const std::string* Location::getCgiPath(const std::string& extension) const {
    /**
     * @brief Gets the CGI interpreter path for a specific file extension.
     * @param extension The file extension.
     * @return A pointer to the CGI interpreter path string, or NULL if not found.
     */
    std::map<std::string, std::string>::const_iterator it = _cgi_paths.find(extension);
    if (it != _cgi_paths.end()) {
        return &(it->second);
    }
    return NULL;
}
