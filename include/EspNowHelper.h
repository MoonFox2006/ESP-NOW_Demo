#ifndef __ESPNOWHELPER_H
#define __ESPNOWHELPER_H

#include <inttypes.h>

class EspNowGeneric {
public:
  EspNowGeneric(uint8_t channel = 0) : _channel(channel), _sendError(false) {
    _this = this;
  }
  virtual ~EspNowGeneric() {
    end();
  }

  virtual bool begin();
  virtual void end();

  bool setMasterKey(const uint8_t *key, uint8_t keylen);

  uint8_t peerCount() const;
  void clearPeers();
  bool findPeer(const uint8_t *mac);
  bool addPeer(const uint8_t *mac);
  bool addPeerSecure(const uint8_t *mac, const uint8_t *key, uint8_t keylen);
  bool removePeer(const uint8_t *mac);

  bool send(const uint8_t *mac, const uint8_t *data, uint8_t len);
  bool sendAll(const uint8_t *data, uint8_t len);
  bool sendBroadcast(const uint8_t *data, uint8_t len);

  bool sendError() const {
    return _sendError;
  }

protected:
  virtual void onReceive(const uint8_t *mac, const uint8_t *data, uint8_t len) {}
  virtual void onSend(const uint8_t *mac, bool error) {
    _sendError = error;
  }

  static void _onReceive(uint8_t *mac, uint8_t *data, uint8_t len) {
    _this->onReceive(mac, data, len);
  }
  static void _onSend(uint8_t *mac, uint8_t status) {
    _this->onSend(mac, status != 0);
  }

  static EspNowGeneric *_this;
  uint8_t _channel : 7;
  bool _sendError : 1;
};

class EspNowServer : public EspNowGeneric {
public:
  EspNowServer(uint8_t channel = 0) : EspNowGeneric(channel) {}

  bool begin();
  void end();
};

class EspNowClient : public EspNowGeneric {
public:
  EspNowClient(uint8_t channel = 0, const uint8_t *mac = NULL) : EspNowGeneric(channel) {
    if (mac)
      memcpy(_server_mac, mac, sizeof(_server_mac));
  }

  bool begin();

protected:
  uint8_t _server_mac[6];
};

int8_t espNowFindServer(uint8_t *mac = NULL);

#endif
