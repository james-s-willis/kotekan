#ifndef PROCESS_FACTORY_HPP
#define PROCESS_FACTORY_HPP

#include <string>
#include <map>

#include "json.hpp"
#include "Config.hpp"
#include "bufferContainer.hpp"
//#include "KotekanProcess.hpp"

// Name space includes.
using json = nlohmann::json;
using std::string;
using std::map;


class KotekanProcess;

class kotekanProcessMaker{
public:
    virtual KotekanProcess *create(Config &config, const string &unique_name,
                bufferContainer &host_buffers) const = 0;
 };

class processFactory {

public:

    // One processFactory should be created for each set of config and buffer_container
    processFactory(Config& config, bufferContainer &buffer_container);
    ~processFactory();

    // Creates all the processes listed in the config file, and returns them
    // as a vector of KotekanProcess pointers.
    // This should only be called once.
    map<string, KotekanProcess *> build_processes();

private:

    void build_from_tree(map<string, KotekanProcess *> &processes, json &config_tree, const string &path);
//    KotekanProcess * new_process(const string &name, const string &location);

    Config &config;
    bufferContainer &buffer_container;


    KotekanProcess *create(const string &name,
                       Config& config,
                       const string &unique_name,
                       bufferContainer &host_buffers) const;
};

class processFactoryRegistry {
public:
    static processFactoryRegistry& Instance();
    std::map<std::string, kotekanProcessMaker*> _kotekan_processes;

    //Add the process to the registry.
    void kotekanRegisterProcess(const std::string& key, kotekanProcessMaker* cmd);

private:
    processFactoryRegistry();
};

template<typename T>
class kotekanProcessMakerTemplate : public kotekanProcessMaker
{
    public:
        kotekanProcessMakerTemplate(const std::string& key)
        {
            printf("Registering a KotekanProcess! %s\n",key.c_str());
            processFactoryRegistry::Instance().kotekanRegisterProcess(key, this);
        }
        virtual KotekanProcess *create(Config &config, const string &unique_name,
                    bufferContainer &host_buffers) const
        {
            return new T(config, unique_name, host_buffers);
        }
}; 
#define REGISTER_KOTEKAN_PROCESS(T) static kotekanProcessMakerTemplate<T> maker(#T);


#endif /* PROCESS_FACTORY_HPP */