#pragma once

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <swc_transform.h>
#include <string>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <ada.h>

#include <cassert>
#include "js_internals.h"
#include <v8.h>
#include "v8_utils.cc"

namespace fs = std::filesystem;

// in release build, internal modules will be bundled directly in the C++ code
#define INTERNALS_DIR "./js"
#define RESERVED_PREFIX "std:" // import { dirname } from 'std:path'

namespace runtime
{
  std::unordered_map<int, std::string> absolute_paths;
  std::unordered_map<std::string, v8::Global<v8::Module>> resolve_cache;

  class Loader
  {
  public:
    ~Loader() {
      Loader::clear_resolve_cache();
    }
    // path is always supposed to be absolute
    void execute(v8::Isolate *isolate, v8::Local<v8::Context> context, std::string path) {
      std::string src = load_file(path);

      v8::Local<v8::String> source = v8_value(isolate, src);
      v8::ScriptOrigin origin(v8_value(isolate, path),
                              0,
                              0,
                              true,
                              -1,
                              v8::Local<v8::Value>(),
                              false,
                              false,
                              true);

      v8::ScriptCompiler::Source script_source(source, origin);
      v8::Local<v8::Module> mod = v8::ScriptCompiler::CompileModule(isolate, &script_source).ToLocalChecked();
      absolute_paths.emplace(mod->ScriptId(), path);
      resolve_cache[path].Reset(isolate, mod);

      bool intialized = mod->InstantiateModule(context, Loader::load).IsJust();
      if (!intialized) {
        std::cerr << "error: Unable to run module '" << path << "'" << std::endl;
        exit(1);
      }

      v8::TryCatch try_catch(isolate);
      v8::MaybeLocal<v8::Value> result = mod->Evaluate(context);

      if (result.IsEmpty()) {
          v8::String::Utf8Value error(isolate, try_catch.Exception());
          printf("Exception: %s\n", *error);
          return;
      }

      v8::Local<v8::Value> value = result.ToLocalChecked();
      if (value->IsPromise()) {
          v8::Local<v8::Promise> promise = value.As<v8::Promise>();
          if (promise->State() == v8::Promise::kRejected) {
              v8::String::Utf8Value error(isolate, promise->Result());
              printf("%s\n", *error);
          }
      }
    }

    // implementation for import.meta.resolve()
    static void resolve(const v8::FunctionCallbackInfo<v8::Value> &args) {
      v8::Local<v8::Object> meta = args.Holder();
      v8::Isolate *isolate = args.GetIsolate();
      v8::MaybeLocal<v8::Value> parent = meta->Get(isolate->GetCurrentContext(), v8_symbol(isolate, "filename"));
      if (parent.IsEmpty()) {
        isolate->ThrowException(v8::Exception::ReferenceError(v8::String::NewFromUtf8(isolate, "Unable to get import.meta.dirname").ToLocalChecked()));
      }
      
      v8::Local<v8::Value> path_to_resolve = args[0];
      if (!path_to_resolve->IsString()) {
        isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Resolve path must be a string").ToLocalChecked()));
      }

      v8::String::Utf8Value parent_path(isolate, parent.ToLocalChecked());
      v8::String::Utf8Value path(isolate, path_to_resolve);
      
      std::string path_std(*path);
      if (!path_std.starts_with("/") && !path_std.starts_with("./") && !path_std.starts_with("../")) {
        isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Resolve path must be prefixed with / or ./ or ../").ToLocalChecked()));
      }

      std::string abs_path = Loader::resolve_module_path(fs::path(*parent_path), *path);
      std::string url = Loader::path_to_url(abs_path);
      args.GetReturnValue().Set(v8_value(isolate, url));
    }

    static v8::MaybeLocal<v8::Promise> dynamic_load(
        v8::Local<v8::Context> context,
        v8::Local<v8::Data> host_defined_options,
        v8::Local<v8::Value> resource_name,
        v8::Local<v8::String> specifier,
        v8::Local<v8::FixedArray> import_attributes)
    {
      v8::Isolate *isolate = context->GetIsolate();
      v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(context).ToLocalChecked();

      v8::String::Utf8Value resource(isolate, resource_name);
      v8::String::Utf8Value specifier_str(isolate, specifier);

      std::string parent_path(*resource);
      std::string path = Loader::resolve_module_path(parent_path, *specifier_str);

      v8::MaybeLocal<v8::Module> mod = Loader::create_module(isolate, path);
      if (mod.IsEmpty()) {
          resolver->Reject(context, v8::Exception::Error(v8::String::NewFromUtf8Literal(isolate, "Module not found"))).Check();
      } else {
          v8::Local<v8::Module> module = mod.ToLocalChecked();
          if (module->InstantiateModule(context, Loader::load).IsJust()) {
              v8::Local<v8::Value> val;
              if (module->Evaluate(context).ToLocal(&val)) {
                  v8::Local<v8::Value> namespace_obj = module->GetModuleNamespace();
                  resolver->Resolve(context, namespace_obj).Check();
                  return resolver->GetPromise();
              } else {
                  resolver->Reject(context, v8::Exception::Error(v8::String::NewFromUtf8Literal(isolate, "Error evaluating module"))).Check();
              }
          } else {
              resolver->Reject(context, v8::Exception::Error(v8::String::NewFromUtf8Literal(isolate, "Error instantiating module"))).Check();
          }
      }

      return resolver->GetPromise();
    }

    static void set_meta(v8::Local<v8::Context> context, v8::Local<v8::Module> module, v8::Local<v8::Object> meta) {
      auto result = absolute_paths.find(module->ScriptId());
      if (result != absolute_paths.end()) {
        v8::Isolate *isolate = context->GetIsolate();
        std::string url = Loader::path_to_url(result->second);
        meta->Set(context, v8_symbol(isolate, "url"), v8_value(isolate, url)).Check();
        meta->Set(context, v8_symbol(isolate, "filename"), v8_value(isolate, result->second)).Check();
        meta->Set(context, v8_symbol(isolate, "dirname"), v8_value(isolate, fs::path(result->second).parent_path().string())).Check();
        meta->Set(context, v8_symbol(isolate, "resolve"), v8::Function::New(context, resolve).ToLocalChecked()).Check(); 
      }
    }

    static inline std::string path_to_url(std::string &path) {
      if (path.find('%') == std::string_view::npos) {
        return ada::href_from_file(path);
      }

      std::string escaped_path; // escape % sign
      for (char ch : path) {
          if (ch == '%') {
              escaped_path += "%25";
          } else {
              escaped_path += ch;
          }
      }
      return ada::href_from_file(escaped_path);
    }

    static v8::MaybeLocal<v8::Module> load(
        v8::Local<v8::Context> context,
        v8::Local<v8::String> specifier,
        v8::Local<v8::FixedArray> import_assertions,
        v8::Local<v8::Module> referrer)
    {
      v8::Isolate *isolate = context->GetIsolate();
      v8::String::Utf8Value specifier_utf8(isolate, specifier);
      
      std::string specifier_name(*specifier_utf8);

      if (is_internal(specifier_name)) {
        return create_module(isolate, specifier_name);
      }

      auto parent_path = absolute_paths.find(referrer->ScriptId());
      if (parent_path == absolute_paths.end()) {
        printf("error: Unable to find referrer's path\n");
        exit(1);
        return {};
      }

      std::string path = Loader::resolve_module_path(parent_path->second, specifier_name);
      return create_module(isolate, path);
    }

    static v8::MaybeLocal<v8::Module> create_module(v8::Isolate *isolate, std::string path) {
      // load from cache:
      auto cached_module = resolve_cache.find(path);
      if (cached_module != resolve_cache.end()) {
        return cached_module->second.Get(isolate);
      }
    
      std::string src = load_file(path);

      v8::Local<v8::String> source = v8_value(isolate, src);
      v8::ScriptOrigin origin(v8_value(isolate, path),
                              0,
                              0,
                              true,
                              -1,
                              v8::Local<v8::Value>(),
                              false,
                              false,
                              true);

      v8::ScriptCompiler::Source script_source(source, origin);
      v8::Local<v8::Module> module;

      if (v8::ScriptCompiler::CompileModule(isolate, &script_source).ToLocal(&module)) {
        absolute_paths.emplace(module->ScriptId(), path);
        resolve_cache[path].Reset(isolate, module);

        return module;
      }

      printf("error: Unable to find module '%s'\n", path.c_str());
      exit(1);
      return {};
    }

    static std::string resolve_module_path(fs::path parent, const std::string &specifier) {
      fs::path abs_path = parent.parent_path() / fs::path(specifier);
      abs_path = abs_path.lexically_normal();

      return abs_path.string();
    }

    static inline bool is_internal(std::string specifier) {
      return specifier.starts_with(RESERVED_PREFIX);
    }

    static std::string load_file(std::string path) {
      if (is_internal(path)) {
        auto internal_src = js_internals.find(path);
        if (internal_src == js_internals.end()) {
          printf("error: Invalid std module name: '%s'\n", path.c_str());
          exit(1);
        }

        return std::string(internal_src->second.begin(), internal_src->second.end());
      }

      std::ifstream file(path);
      if (!file.is_open()) {
        printf("error: Unable to load module: '%s'\n", path.c_str());
        exit(1);
      }

      std::stringstream buffer;
      buffer << file.rdbuf();
      return buffer.str();
    }

    static void clear_resolve_cache() {
      for (auto& entry : resolve_cache) {
        entry.second.Reset();
      }
      resolve_cache.clear();
    }
  };
}
