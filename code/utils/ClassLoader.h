#ifndef CLASS_LOADER_H
#define CLASS_LOADER_H

#include <dlfcn.h>

#include <iostream>

#define SUCCESS             0
#define CASE_HANDLE_ERR     -1
#define CASE_INIT_ERR       -2
#define CASE_DEST_ERR       -3

namespace fs_testing {
namespace utils {

template<class T>
class ClassLoader {
 public:
  T *get_instance() {
    return instance;
  }

  // TODO(ashmrtn): Make this load the handles only. Since each constructor can
  // be different, make just a method that returns the factory and have the user
  // provide the instance on unload. The other option is to restrict all default
  // loaded classes to having a default constructor. Then the class must also
  // provide a method to load data if needed, but that seems like the cleanest
  // solution here...
  template<typename F>
  int load_class(const char *path, const char *factory_name,
      const char *defactory_name) {
    const char* dl_error = NULL;

    loader_handle = dlopen(path, RTLD_LAZY);
    if (loader_handle == NULL) {
      std::cerr << "Error loading class " << path << std::endl
        << dlerror() << std::endl;
      return CASE_HANDLE_ERR;
    }

    // Get needed methods from loaded class.
    factory = dlsym(loader_handle, factory_name);
    dl_error = dlerror();
    if (dl_error) {
      std::cerr << "Error gettig factory method " << dl_error << std::endl;
      dlclose(loader_handle);
      factory = NULL;
      defactory = NULL;
      loader_handle = NULL;
      return CASE_INIT_ERR;
    }
    defactory = dlsym(loader_handle, defactory_name);
    dl_error = dlerror();
    if (dl_error) {
      std::cerr << "Error gettig deleter method " << dl_error << std::endl;
      dlclose(loader_handle);
      factory = NULL;
      defactory = NULL;
      loader_handle = NULL;
      return CASE_DEST_ERR;
    }
    instance = ((F)(factory))();
    return SUCCESS;
  };

  template<typename DF>
  void unload_class() {
    if (loader_handle != NULL && instance != NULL) {
      ((DF)(defactory))(instance);
      dlclose(loader_handle);
      factory = NULL;
      defactory = NULL;
      loader_handle = NULL;
      instance = NULL;
    }
  };

 private:
  void *loader_handle = NULL;
  void *factory = NULL;
  void *defactory = NULL;
  T *instance = NULL;
};

}  // namespace utils
}  // namespace fs_testing
#endif
