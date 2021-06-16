#include "include/video_player_windows/video_player_windows_plugin.h"
#include "include/video_player_windows/logging.h"
#include "include/video_player_windows/messages.h"
#include "include/video_player_windows/util.h"
#include "include/video_player_windows/video_player_texture.h"

// This must be included before many other Windows headers.
#include <windows.h>

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
  std::vector<std::unique_ptr<flutter::BasicMessageChannel<flutter::EncodableValue>>> channels;
  std::unordered_map<int64_t, std::unique_ptr<VideoPlayerTexture>> textures;
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
  void HandlePlay(const flutter::EncodableValue &message,
                  const flutter::MessageReply<flutter::EncodableValue> &reply);
  void HandlePause(const flutter::EncodableValue &message,
                   const flutter::MessageReply<flutter::EncodableValue> &reply);
  void HandleSeek(const flutter::EncodableValue &message,
                  const flutter::MessageReply<flutter::EncodableValue> &reply);
  void HandleSetVolume(const flutter::EncodableValue &message,
                       const flutter::MessageReply<flutter::EncodableValue> &reply);
  void HandleSetSpeed(const flutter::EncodableValue &message,
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
             std::bind(&VideoPlayerWindowsPlugin::HandlePlay, this, _1, _2));
  InitMethod(messenger, "dev.flutter.pigeon.VideoPlayerApi.pause",
             std::bind(&VideoPlayerWindowsPlugin::HandlePause, this, _1, _2));
  InitMethod(messenger, "dev.flutter.pigeon.VideoPlayerApi.setPlaybackSpeed",
             std::bind(&VideoPlayerWindowsPlugin::TextureOpStub, this, _1, _2));
  InitMethod(messenger, "dev.flutter.pigeon.VideoPlayerApi.setVolume",
             std::bind(&VideoPlayerWindowsPlugin::HandleSetVolume, this, _1, _2));
  InitMethod(messenger, "dev.flutter.pigeon.VideoPlayerApi.setLooping",
             std::bind(&VideoPlayerWindowsPlugin::TextureOpStub, this, _1, _2));
  InitMethod(messenger, "dev.flutter.pigeon.VideoPlayerApi.position",
             std::bind(&VideoPlayerWindowsPlugin::HandlePosition, this, _1, _2));
  InitMethod(messenger, "dev.flutter.pigeon.VideoPlayerApi.seekTo",
             std::bind(&VideoPlayerWindowsPlugin::HandleSeek, this, _1, _2));
}

void VideoPlayerWindowsPlugin::InitMethod(flutter::BinaryMessenger *messenger,
                                          const char *channel_name,
                                          flutter::MessageHandler<flutter::EncodableValue> method) {
  auto channel = std::make_unique<flutter::BasicMessageChannel<flutter::EncodableValue>>(
      messenger, channel_name, &flutter::StandardMessageCodec::GetInstance());
  channel->SetMessageHandler(method);
  channels.emplace_back(std::move(channel));
}

void VideoPlayerWindowsPlugin::HandleInitialize(
    const flutter::EncodableValue &message,
    const flutter::MessageReply<flutter::EncodableValue> &reply) {
  reply(WrapResult(std::move(flutter::EncodableValue(std::monostate()))));
}

void VideoPlayerWindowsPlugin::HandleCreate(
    const flutter::EncodableValue &message,
    const flutter::MessageReply<flutter::EncodableValue> &reply) {
  debug_log << "Debug print of message:" << std::endl;
  debug_log << DebugPrintValue(message) << std::endl;
  CreateMessage cm(message);
  if (!cm.uri.has_value()) {
    reply(WrapError(flutter::EncodableValue("Only URIs are supported")));
    return;
  }
  std::unique_ptr<VideoPlayerTexture> tex = std::make_unique<VideoPlayerTexture>();
  int64_t tid = tex->RegisterWithTextureRegistrar(registrar->texture_registrar());
  tex->SetupEventChannel(registrar->messenger());
  tex->InitAsync(*cm.uri);
  textures.emplace(tid, std::move(tex));
  TextureMessage tm;
  tm.textureId = tid;
  reply(WrapResult(tm.toEncodable()));
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
  reply(WrapResult(pm.toEncodable()));
}

void VideoPlayerWindowsPlugin::HandlePause(
    const flutter::EncodableValue &message,
    const flutter::MessageReply<flutter::EncodableValue> &reply) {
  TextureMessage tm(message);
  if (textures.find(tm.textureId) == textures.end()) {
    reply(WrapError(flutter::EncodableValue("Texture not found")));
    return;
  }
  std::unique_ptr<VideoPlayerTexture> &tex = textures[tm.textureId];
  tex->Pause();
  reply(WrapResult(std::monostate()));
}

void VideoPlayerWindowsPlugin::HandlePlay(
    const flutter::EncodableValue &message,
    const flutter::MessageReply<flutter::EncodableValue> &reply) {
  TextureMessage tm(message);
  if (textures.find(tm.textureId) == textures.end()) {
    reply(WrapError(flutter::EncodableValue("Texture not found")));
    return;
  }
  std::unique_ptr<VideoPlayerTexture> &tex = textures[tm.textureId];
  tex->Play();
  reply(WrapResult(std::monostate()));
}

void VideoPlayerWindowsPlugin::HandleSeek(
    const flutter::EncodableValue &message,
    const flutter::MessageReply<flutter::EncodableValue> &reply) {
  PositionMessage pm(message);
  if (textures.find(pm.textureId) == textures.end()) {
    reply(WrapError(flutter::EncodableValue("Texture not found")));
    return;
  }
  std::unique_ptr<VideoPlayerTexture> &tex = textures[pm.textureId];
  tex->Seek(pm.position);
  reply(WrapResult(std::monostate()));
}

void VideoPlayerWindowsPlugin::HandleSetVolume(
    const flutter::EncodableValue &message,
    const flutter::MessageReply<flutter::EncodableValue> &reply) {
  VolumeMessage vm(message);
  if (textures.find(vm.textureId) == textures.end()) {
    reply(WrapError(flutter::EncodableValue("Texture not found")));
    return;
  }
  std::unique_ptr<VideoPlayerTexture> &tex = textures[vm.textureId];
  tex->SetVolume(vm.volume);
  info_log << "Volume set to " << vm.volume << std::endl;
  reply(WrapResult(std::monostate()));
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
  registrar->AddPlugin(std::move(plugin));
}

VideoPlayerWindowsPlugin::~VideoPlayerWindowsPlugin() {}
} // namespace

void VideoPlayerWindowsPluginRegisterWithRegistrar(FlutterDesktopPluginRegistrarRef registrar) {
  VideoPlayerWindowsPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()->GetRegistrar<flutter::PluginRegistrarWindows>(
          registrar));
}
