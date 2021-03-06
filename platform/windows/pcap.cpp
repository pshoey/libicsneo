#include "icsneo/platform/windows/pcap.h"
#include "icsneo/communication/network.h"
#include "icsneo/communication/communication.h"
#include "icsneo/communication/packetizer.h"
#include "icsneo/communication/ethernetpacketizer.h"
#include <pcap.h>
#include <iphlpapi.h>
#pragma comment(lib, "IPHLPAPI.lib")
#include <codecvt>
#include <chrono>
#include <iostream>

using namespace icsneo;

static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
static const uint8_t BROADCAST_MAC[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static const uint8_t ICS_UNSET_MAC[6] = { 0x00, 0xFC, 0x70, 0xFF, 0xFF, 0xFF };

std::vector<PCAP::NetworkInterface> PCAP::knownInterfaces;

std::vector<PCAP::PCAPFoundDevice> PCAP::FindAll() {
	std::vector<PCAPFoundDevice> foundDevices;
	const PCAPDLL& pcap = PCAPDLL::getInstance();
	if(!pcap.ok()) {
		EventManager::GetInstance().add(APIEvent::Type::PCAPCouldNotStart, APIEvent::Severity::Error);
		return std::vector<PCAPFoundDevice>();
	}

	// First we ask WinPCAP to give us all of the devices
	pcap_if_t* alldevs;
	char errbuf[PCAP_ERRBUF_SIZE] = { 0 };
	bool success = false;
	// Calling pcap.findalldevs_ex too quickly can cause various errors. Retry a few times in this case.
	for(auto retry = 0; retry < 10; retry++) {
		auto ret = pcap.findalldevs_ex(PCAP_SRC_IF_STRING, nullptr, &alldevs, errbuf);
		if(ret == 0) {
			success = true;
			break;
		}
	}

	if(!success) {
		EventManager::GetInstance().add(APIEvent::Type::PCAPCouldNotFindDevices, APIEvent::Severity::Error);
		return std::vector<PCAPFoundDevice>();
	}

	std::vector<NetworkInterface> interfaces;
	for(pcap_if_t* dev = alldevs; dev != nullptr; dev = dev->next) {
		NetworkInterface netif;
		netif.nameFromWinPCAP = dev->name;
		netif.descriptionFromWinPCAP = dev->description;
		interfaces.push_back(netif);
	}

	pcap.freealldevs(alldevs);

	// Now we're going to ask Win32 for the information as well
	ULONG size = 0;
	if(GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW) {
		EventManager::GetInstance().add(APIEvent::Type::PCAPCouldNotFindDevices, APIEvent::Severity::Error);
		return std::vector<PCAPFoundDevice>();
	}
	std::vector<uint8_t> adapterAddressBuffer;
	adapterAddressBuffer.resize(size);
	if(GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, (IP_ADAPTER_ADDRESSES*)adapterAddressBuffer.data(), &size) != ERROR_SUCCESS) {
		EventManager::GetInstance().add(APIEvent::Type::PCAPCouldNotFindDevices, APIEvent::Severity::Error);
		return std::vector<PCAPFoundDevice>();
	}
	
	// aa->AdapterName constains a unique name of the interface like "{3B1D2791-435A-456F-8A7B-9CB0EEE5DAB3}"
	// interface.nameFromWinPCAP has "rpcap://\Device\NPF_{3B1D2791-435A-456F-8A7B-9CB0EEE5DAB3}"
	// We're comparing strings to match the Win32 info with the WinPCAP info
	for(IP_ADAPTER_ADDRESSES* aa = (IP_ADAPTER_ADDRESSES*)adapterAddressBuffer.data(); aa != nullptr; aa = aa->Next) {
		for(auto& interface : interfaces) {
			if(interface.nameFromWinPCAP.find(aa->AdapterName) == std::string::npos)
				continue; // This is not the interface that corresponds

			memcpy(interface.macAddress, aa->PhysicalAddress, sizeof(interface.macAddress));
			interface.nameFromWin32API = aa->AdapterName;
			interface.descriptionFromWin32API = converter.to_bytes(aa->Description);
			interface.friendlyNameFromWin32API = converter.to_bytes(aa->FriendlyName);
			if(interface.descriptionFromWin32API.find("LAN9512/LAN9514") != std::string::npos) {
				// This is an Ethernet EVB device
				interface.fullName = "Intrepid Ethernet EVB ( " + interface.friendlyNameFromWin32API + " : " + interface.descriptionFromWin32API + " )";
			} else {
				interface.fullName = interface.friendlyNameFromWin32API + " : " + interface.descriptionFromWin32API;
			}
		}
	}

	for(auto& interface : interfaces) {
		bool exists = false;
		for(auto& known : knownInterfaces)
			if(memcmp(interface.macAddress, known.macAddress, sizeof(interface.macAddress)) == 0)
				exists = true;
		if(!exists)
			knownInterfaces.emplace_back(interface);
	}

	constexpr auto openflags = (PCAP_OPENFLAG_MAX_RESPONSIVENESS | PCAP_OPENFLAG_NOCAPTURE_LOCAL);
	for(size_t i = 0; i < knownInterfaces.size(); i++) {
		auto& interface = knownInterfaces[i];
		if(interface.fullName.length() == 0)
			continue; // Win32 did not find this interface in the previous step

		interface.fp = pcap.open(interface.nameFromWinPCAP.c_str(), 1518, openflags, 1, nullptr, errbuf);

		if(interface.fp == nullptr)
			continue; // Could not open the interface

		EthernetPacketizer::EthernetPacket requestPacket;
		memcpy(requestPacket.srcMAC, interface.macAddress, sizeof(requestPacket.srcMAC));
		requestPacket.payload.reserve(4);
		requestPacket.payload = {
			((1 << 4) | (uint8_t)Network::NetID::Main51), // Packet size of 1 on NETID_MAIN51
			(uint8_t)Command::RequestSerialNumber
		};
		requestPacket.payload.push_back(Packetizer::ICSChecksum(requestPacket.payload));
		requestPacket.payload.insert(requestPacket.payload.begin(), 0xAA);

		auto bs = requestPacket.getBytestream();
		pcap.sendpacket(interface.fp, bs.data(), (int)bs.size());

		auto timeout = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(5);
		while(std::chrono::high_resolution_clock::now() <= timeout) { // Wait up to 5ms for the response
			struct pcap_pkthdr* header;
			const uint8_t* data;
			auto res = pcap.next_ex(interface.fp, &header, &data);
			if(res < 0) {
				//std::cout << "pcapnextex failed with " << res << std::endl;
				break;
			}
			if(res == 0)
				continue; // Keep waiting for that packet
			
			EthernetPacketizer::EthernetPacket packet(data, header->caplen);
			// Is this an ICS response packet (0xCAB2) from an ICS MAC, either to broadcast or directly to us?
			if(packet.etherType == 0xCAB2 && packet.srcMAC[0] == 0x00 && packet.srcMAC[1] == 0xFC && packet.srcMAC[2] == 0x70 && (
				memcmp(packet.destMAC, interface.macAddress, sizeof(packet.destMAC)) == 0 ||
				memcmp(packet.destMAC, BROADCAST_MAC, sizeof(packet.destMAC)) == 0
			)) {
				/* We have received a packet from a device. We don't know if this is the device we're
				 * looking for, we don't know if it's actually a response to our RequestSerialNumber
				 * or not, we just know we got something.
				 *
				 * Unlike most transport layers, we can't get the serial number here as we actually
				 * need to parse this message that has been returned. Some devices parse messages
				 * differently, so we need to use their communication layer. We could technically
				 * create a communication layer to parse the packet we have in `payload` here, but
				 * we'd need to be given a packetizer and decoder for the device. I'm intentionally
				 * avoiding passing that information down here for code quality's sake. Instead, pass
				 * the packet we received back up so the device can handle it.
				 */
				neodevice_handle_t handle = (neodevice_handle_t)((i << 24) | (packet.srcMAC[3] << 16) | (packet.srcMAC[4] << 8) | (packet.srcMAC[5]));
				PCAPFoundDevice* alreadyExists = nullptr;
				for(auto& dev : foundDevices)
					if(dev.device.handle == handle)
						alreadyExists = &dev;

				if(alreadyExists == nullptr) {
					PCAPFoundDevice foundDevice;
					foundDevice.device.handle = handle;
					foundDevice.discoveryPackets.push_back(std::move(packet.payload));
					foundDevices.push_back(foundDevice);
				} else {
					alreadyExists->discoveryPackets.push_back(std::move(packet.payload));
				}
			}
		}

		pcap.close(interface.fp);
		interface.fp = nullptr;
	}

	return foundDevices;
}

bool PCAP::IsHandleValid(neodevice_handle_t handle) {
	uint8_t netifIndex = (uint8_t)(handle >> 24);
	return (netifIndex < knownInterfaces.size());
}

PCAP::PCAP(const device_eventhandler_t& err, neodevice_t& forDevice) : Driver(err), device(forDevice), pcap(PCAPDLL::getInstance()), ethPacketizer(err) {
	if(IsHandleValid(device.handle)) {
		interface = knownInterfaces[(device.handle >> 24) & 0xFF];
		interface.fp = nullptr; // We're going to open our own connection to the interface. This should already be nullptr but just in case.

		deviceMAC[0] = 0x00;
		deviceMAC[1] = 0xFC;
		deviceMAC[2] = 0x70;
		deviceMAC[3] = (device.handle >> 16) & 0xFF;
		deviceMAC[4] = (device.handle >> 8) & 0xFF;
		deviceMAC[5] = device.handle & 0xFF;
		memcpy(ethPacketizer.deviceMAC, deviceMAC, 6);
		memcpy(ethPacketizer.hostMAC, interface.macAddress, 6);
	} else {
		openable = false;
	}
}

bool PCAP::open() {
	if(!openable) {
		report(APIEvent::Type::InvalidNeoDevice, APIEvent::Severity::Error);
		return false;
	}

	if(!pcap.ok()) {
		report(APIEvent::Type::DriverFailedToOpen, APIEvent::Severity::Error);
		return false;
	}

	if(isOpen()) {
		report(APIEvent::Type::DeviceCurrentlyOpen, APIEvent::Severity::Error);
		return false;
	}	

	// Open the interface
	interface.fp = pcap.open(interface.nameFromWinPCAP.c_str(), 65536, PCAP_OPENFLAG_MAX_RESPONSIVENESS | PCAP_OPENFLAG_NOCAPTURE_LOCAL, 50, nullptr, errbuf);
	if(interface.fp == nullptr) {
		report(APIEvent::Type::DriverFailedToOpen, APIEvent::Severity::Error);
		return false;
	}

	// Create threads
	readThread = std::thread(&PCAP::readTask, this);
	writeThread = std::thread(&PCAP::writeTask, this);
	transmitThread = std::thread(&PCAP::transmitTask, this);
	
	return true;
}

bool PCAP::isOpen() {
	return interface.fp != nullptr;
}

bool PCAP::close() {
	if(!isOpen()) {
		report(APIEvent::Type::DeviceCurrentlyClosed, APIEvent::Severity::Error);
		return false;
	}

	closing = true; // Signal the threads that we are closing
	readThread.join();
	writeThread.join();
	transmitThread.join();
	closing = false;

	pcap.close(interface.fp);
	interface.fp = nullptr;

	uint8_t flush;
	WriteOperation flushop;
	while(readQueue.try_dequeue(flush)) {}
	while(writeQueue.try_dequeue(flushop)) {}
	transmitQueue = nullptr;

	return true;
}

void PCAP::readTask() {
	struct pcap_pkthdr* header;
	const uint8_t* data;
	EventManager::GetInstance().downgradeErrorsOnCurrentThread();
	while(!closing) {
		auto readBytes = pcap.next_ex(interface.fp, &header, &data);
		if(readBytes < 0) {
			report(APIEvent::Type::FailedToRead, APIEvent::Severity::Error);
			break;
		}
		if(readBytes == 0)
			continue; // Keep waiting for that packet

		if(ethPacketizer.inputUp({data, data + header->caplen})) {
			const auto bytes = ethPacketizer.outputUp();
			readQueue.enqueue_bulk(bytes.data(), bytes.size());
		}
	}
}

void PCAP::writeTask() {
	WriteOperation writeOp;
	EventManager::GetInstance().downgradeErrorsOnCurrentThread();

	pcap_send_queue* queue1 = pcap.sendqueue_alloc(128000);
	pcap_send_queue* queue2 = pcap.sendqueue_alloc(128000);
	pcap_send_queue* queue = queue1;
	std::vector<uint8_t> extraData;

	while(!closing) {
		if(!writeQueue.wait_dequeue_timed(writeOp, std::chrono::milliseconds(100)))
			continue;

		unsigned int i = 0;
		do {
			ethPacketizer.inputDown(std::move(writeOp.bytes));
		} while(writeQueue.try_dequeue(writeOp) && i++ < (queue->maxlen - queue->len) / 1518 / 3);

		for(const auto& data : ethPacketizer.outputDown()) {
			pcap_pkthdr header = {};
			header.caplen = header.len = bpf_u_int32(data.size());
			if(pcap.sendqueue_queue(queue, &header, data.data()) == -1)
				report(APIEvent::Type::FailedToWrite, APIEvent::Severity::EventWarning);
		}
		
		std::unique_lock<std::mutex> lk(transmitQueueMutex);
		if(!transmitQueue || queue->len + (1518*2) >= queue->maxlen) { // Checking if we want to swap sendqueues with the transmitTask
			if(transmitQueue) // Need to wait for the queue to become available
				transmitQueueCV.wait(lk, [this] { return !transmitQueue; });
			
			// Time to swap
			transmitQueue = queue;
			lk.unlock();
			transmitQueueCV.notify_one();

			// Set up our next queue
			if(queue == queue1) {
				pcap.sendqueue_destroy(queue2);
				queue = queue2 = pcap.sendqueue_alloc(128000);
			} else {
				pcap.sendqueue_destroy(queue1);
				queue = queue1 = pcap.sendqueue_alloc(128000);
			}
		}
	}

	pcap.sendqueue_destroy(queue1);
	pcap.sendqueue_destroy(queue2);
}

void PCAP::transmitTask() {
	while(!closing) {
		std::unique_lock<std::mutex> lk(transmitQueueMutex);
		if(transmitQueueCV.wait_for(lk, std::chrono::milliseconds(100), [this] { return !!transmitQueue; }) && !closing && transmitQueue) {
			pcap_send_queue* current = transmitQueue;
			lk.unlock();
			pcap.sendqueue_transmit(interface.fp, current, 0);
			{
				std::lock_guard<std::mutex> lk2(transmitQueueMutex);
				transmitQueue = nullptr;
			}
			transmitQueueCV.notify_one();
		}
	}
}
