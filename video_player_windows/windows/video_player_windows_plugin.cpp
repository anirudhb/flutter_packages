#include "include/video_player_windows/video_player_windows_plugin.h"
#include "include/video_player_windows/messages.h"
#include "include/video_player_windows/util.h"
#include "include/video_player_windows/video_player_texture.h"

// This must be included before many other Windows headers.
#include <windows.h>

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

#include <flutter/basic_message_channel.h>
#include <flutter/event_channel.h>
#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_message_codec.h>
#include <flutter/standard_method_codec.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <map>
#include <memory>
#include <sstream>

namespace {

class VideoPlayerWindowsPlugin : public flutter::Plugin {
public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

  VideoPlayerWindowsPlugin(flutter::PluginRegistrarWindows *registrar) : registrar(registrar){};

  virtual ~VideoPlayerWindowsPlugin();

private:
  // // Called when a method is called on this plugin's channel from Dart.
  // void HandleMethodCall(
  //     const flutter::MethodCall<flutter::EncodableValue> &method_call,
  //     std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
  std::vector<std::unique_ptr<flutter::BasicMessageChannel<flutter::EncodableValue>>> channels;
  std::unordered_map<int64_t, std::unique_ptr<VideoPlayerTexture>> textures;
  // int _textureIdCounter = 0;
  flutter::PluginRegistrarWindows *registrar;

  void SetupMethods(flutter::BinaryMessenger *messenger);
  void InitMethod(flutter::BinaryMessenger *messenger, const char *channel_name,
                  flutter::MessageHandler<flutter::EncodableValue> method);
  void HandleInitialize(const flutter::EncodableValue &message,
                        const flutter::MessageReply<flutter::EncodableValue> &reply);
  void HandleCreate(const flutter::EncodableValue &message,
                    const flutter::MessageReply<flutter::EncodableValue> &reply);
  void HandleDispose(const flutter::EncodableValue &message,
                     const flutter::MessageReply<flutter::EncodableValue> &reply);
  void HandlePosition(const flutter::EncodableValue &message,
                      const flutter::MessageReply<flutter::EncodableValue> &reply);
  void TextureOpStub(const flutter::EncodableValue &message,
                     const flutter::MessageReply<flutter::EncodableValue> &reply);
};

void VideoPlayerWindowsPlugin::SetupMethods(flutter::BinaryMessenger *messenger) {
  using std::placeholders::_1;
  using std::placeholders::_2;
  InitMethod(messenger, "dev.flutter.pigeon.VideoPlayerApi.initialize",
             std::bind(&VideoPlayerWindowsPlugin::HandleInitialize, this, _1, _2));
  InitMethod(messenger, "dev.flutter.pigeon.VideoPlayerApi.create",
             std::bind(&VideoPlayerWindowsPlugin::HandleCreate, this, _1, _2));
  InitMethod(messenger, "dev.flutter.pigeon.VideoPlayerApi.dispose",
             std::bind(&VideoPlayerWindowsPlugin::HandleDispose, this, _1, _2));
  InitMethod(messenger, "dev.flutter.pigeon.VideoPlayerApi.play",
             std::bind(&VideoPlayerWindowsPlugin::TextureOpStub, this, _1, _2));
  InitMethod(messenger, "dev.flutter.pigeon.VideoPlayerApi.pause",
             std::bind(&VideoPlayerWindowsPlugin::TextureOpStub, this, _1, _2));
  InitMethod(messenger, "dev.flutter.pigeon.VideoPlayerApi.setPlaybackSpeed",
             std::bind(&VideoPlayerWindowsPlugin::TextureOpStub, this, _1, _2));
  InitMethod(messenger, "dev.flutter.pigeon.VideoPlayerApi.setVolume",
             std::bind(&VideoPlayerWindowsPlugin::TextureOpStub, this, _1, _2));
  InitMethod(messenger, "dev.flutter.pigeon.VideoPlayerApi.setLooping",
             std::bind(&VideoPlayerWindowsPlugin::TextureOpStub, this, _1, _2));
  InitMethod(messenger, "dev.flutter.pigeon.VideoPlayerApi.position",
             std::bind(&VideoPlayerWindowsPlugin::HandlePosition, this, _1, _2));
}

void VideoPlayerWindowsPlugin::InitMethod(flutter::BinaryMessenger *messenger,
                                          const char *channel_name,
                                          flutter::MessageHandler<flutter::EncodableValue> method) {
  auto channel = std::make_unique<flutter::BasicMessageChannel<flutter::EncodableValue>>(
      messenger, channel_name, &flutter::StandardMessageCodec::GetInstance());
  channel->SetMessageHandler(method);
  channels.emplace_back(std::move(channel));
  // auto channel = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(messenger,
  // channel_name, &flutter::StandardMethodCodec::GetInstance());
  // channel->SetMethodCallHandler(method);
}

void VideoPlayerWindowsPlugin::HandleInitialize(
    const flutter::EncodableValue &message,
    const flutter::MessageReply<flutter::EncodableValue> &reply) {
  std::cout << "Debug print of init value" << std::endl;
  DebugPrintValue(message);
  std::cout << "Initialized" << std::endl;
  // std::flush(std::cout);
  reply(WrapResult(std::move(flutter::EncodableValue(std::monostate()))));
  // result->Success(flutter::EncodableValue(std::monostate()));
}

void VideoPlayerWindowsPlugin::HandleCreate(
    const flutter::EncodableValue &message,
    const flutter::MessageReply<flutter::EncodableValue> &reply) {
  std::cout << "Debug print of message:" << std::endl;
  DebugPrintValue(message);
  CreateMessage cm(message);
  // if (!cm.uri.has_value()) {
  //   std::cout << "No value for URI!" << std::endl;
  //   reply(WrapError(std::move(flutter::EncodableValue("No URI"))));
  //   return;
  //   // std::cout << "cm uri = " << *cm.uri << std::endl;
  // }
  // std::cout << "TODO: handle create" << std::endl;
  // result with texture id 5
  // TextureMessage tm;
  // tm.textureId = 5;
  std::unique_ptr<VideoPlayerTexture> tex = std::make_unique<VideoPlayerTexture>(*cm.uri);
  int64_t tid = tex->RegisterWithTextureRegistrar(registrar->texture_registrar());
  tex->SetupEventChannel(registrar->messenger());
  textures.emplace(tid, std::move(tex));
  TextureMessage tm;
  tm.textureId = tid;
  reply(WrapResult(tm.toEncodable()));
  // reply(WrapResult(std::move(tm.toEncodable())));
  // result->Success(flutter::EncodableValue(std::monostate()));
}

void VideoPlayerWindowsPlugin::HandleDispose(
    const flutter::EncodableValue &message,
    const flutter::MessageReply<flutter::EncodableValue> &reply) {
  TextureMessage tm(message);
  textures.erase(tm.textureId);
  reply(WrapResult(std::monostate()));
}

void VideoPlayerWindowsPlugin::HandlePosition(
    const flutter::EncodableValue &message,
    const flutter::MessageReply<flutter::EncodableValue> &reply) {
  TextureMessage tm(message);
  if (textures.find(tm.textureId) == textures.end()) {
    reply(WrapError(flutter::EncodableValue("Texture not found")));
    return;
  }
  std::unique_ptr<VideoPlayerTexture> &tex = textures[tm.textureId];
  PositionMessage pm;
  pm.textureId = tm.textureId;
  pm.position = tex->GetPosition();
  // std::cerr << "Handling position method, position=" << pm.position << "ms" << std::endl;
  reply(WrapResult(pm.toEncodable()));
}

void VideoPlayerWindowsPlugin::TextureOpStub(
    const flutter::EncodableValue &message,
    const flutter::MessageReply<flutter::EncodableValue> &reply) {
  reply(WrapResult(std::monostate()));
}

// static
void VideoPlayerWindowsPlugin::RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar) {
  auto plugin = std::make_unique<VideoPlayerWindowsPlugin>(registrar);
  auto messenger = registrar->messenger();
  plugin->SetupMethods(messenger);
  // plugin->InitMethod(messenger, "dev.flutter.pigeon.VideoPlayerApi.initialize",
  // plugin->HandleInitialize); plugin->InitMethod(messenger,
  // "dev.flutter.pigeon.VideoPlayerApi.create", plugin->HandleCreate);

  registrar->AddPlugin(std::move(plugin));
}

// VideoPlayerWindowsPlugin::VideoPlayerWindowsPlugin() {}

VideoPlayerWindowsPlugin::~VideoPlayerWindowsPlugin() {}
} // namespace

void VideoPlayerWindowsPluginRegisterWithRegistrar(FlutterDesktopPluginRegistrarRef registrar) {
  VideoPlayerWindowsPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()->GetRegistrar<flutter::PluginRegistrarWindows>(
          registrar));
}
