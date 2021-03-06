#ifndef __RADGIGASTAR_H_
#define __RADGIGASTAR_H_

#ifdef __cplusplus

#include "icsneo/device/device.h"
#include "icsneo/device/devicetype.h"
#include "icsneo/device/tree/radgigastar/radgigastarsettings.h"

namespace icsneo {

class RADGigastar : public Device {
public:
	static constexpr DeviceType::Enum DEVICE_TYPE = DeviceType::RADGigastar;
	static constexpr const char* SERIAL_START = "GS";

protected:
	RADGigastar(neodevice_t neodevice) : Device(neodevice) {
		getWritableNeoDevice().type = DEVICE_TYPE;
	}

	void setupPacketizer(Packetizer& packetizer) override {
		Device::setupPacketizer(packetizer);
		packetizer.disableChecksum = true;
		packetizer.align16bit = false;
	}

	void setupDecoder(Decoder& decoder) override {
		Device::setupDecoder(decoder);
		decoder.timestampResolution = 10; // Timestamps are in 10ns increments instead of the usual 25ns
	}

	void setupEncoder(Encoder& encoder) override {
		Device::setupEncoder(encoder);
		encoder.supportCANFD = true;
	}

	void setupSupportedRXNetworks(std::vector<Network>& rxNetworks) override {
		static std::vector<Network> supportedRxNetworks = {
			Network::NetID::HSCAN,
			Network::NetID::MSCAN,
			Network::NetID::HSCAN2,
			Network::NetID::HSCAN3,
			Network::NetID::HSCAN4,
			Network::NetID::HSCAN5,

			Network::NetID::Ethernet,
			Network::NetID::Ethernet2,

			Network::NetID::OP_Ethernet1,
			Network::NetID::OP_Ethernet2,

			Network::NetID::LIN,

			Network::NetID::FlexRay
		};
		rxNetworks.insert(rxNetworks.end(), supportedRxNetworks.begin(), supportedRxNetworks.end());
	}

	void setupSupportedTXNetworks(std::vector<Network>& txNetworks) override {
		static std::vector<Network> supportedTxNetworks = {
			Network::NetID::HSCAN,
			Network::NetID::MSCAN,
			Network::NetID::HSCAN2,
			Network::NetID::HSCAN3,
			Network::NetID::HSCAN4,
			Network::NetID::HSCAN5,

			Network::NetID::Ethernet,
			Network::NetID::Ethernet2,

			Network::NetID::OP_Ethernet1,
			Network::NetID::OP_Ethernet2,

			Network::NetID::LIN

			// FlexRay is Receive Only
		};
		txNetworks.insert(txNetworks.end(), supportedTxNetworks.begin(), supportedTxNetworks.end());
	}
};

}

#endif // __cplusplus

#endif