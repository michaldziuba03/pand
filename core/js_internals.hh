/* AUTO GENERATED FILE - DO NOT EDIT IT MANUALLY */
#ifndef JS_MODULES_H
#define JS_MODULES_H

#include <unordered_map>
#include <string>

std::unordered_map<std::string, std::string> js_internals = {
    {"bootstrap", "class Module {\n  constructor(path) {\n    this.path = path;\n    this.exports = {};\n    this.loaded = false;\n  }\n\n  static _cache = new Map();\n\n  static require(path) {\n    if (Module._cache.has(path)) {\n      const module = Module._cache.get(path);\n      return module.exports;\n    }\n\n    const module = new Module(path);\n    const exports = module.exports;\n\n    Module._cache.set(path, module);\n    \n    const { func } = load(path);\n    module.loaded = true;\n\n    // __filename and __dirname will be implemented later\n    func(exports, Module.require, module, path, path);\n    \n    Module._cache.set(path, module);\n\n    return module.exports;\n  }\n}\n\nModule.require(__entryfile);\n"}
};

#endif // JS_MODULES_H
