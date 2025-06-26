#ifndef LOCATION_HPP
#define LOCATION_HPP

#include <string>
#include <vector>
#include <map>

class Location {
public:
    Location();
    ~Location();

    void setPath(const std::string& path);
    const std::string& getPath() const;

    void addAllowedMethod(const std::string& method);
    const std::vector<std::string>& getAllowedMethods() const;

    void setRoot(const std::string& root);
    const std::string& getRoot() const;

    void setAutoIndex(bool autoindex);
    bool getAutoIndex() const;

    void setIndex(const std::string& index_file);
    const std::string& getIndex() const;

    void setCgiPath(const std::string& extension, const std::string& path);
    const std::string* getCgiPath(const std::string& extension) const;

private:
    std::string _path;
    std::vector<std::string> _allowed_methods;
    std::string _root;
    bool _autoindex;
    std::string _index;
    std::map<std::string, std::string> _cgi_paths;
};

#endif
