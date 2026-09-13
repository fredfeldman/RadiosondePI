#include <cassert>
#include <iostream>
#include <cmath>
#include <vector>

#include "dsp/FirFilter.hpp"
#include "dsp/FmDiscriminator.hpp"
#include "dsp/SymbolSync.hpp"
#include "decoders/RS41Decoder.hpp"
#include "dsp/RingBuffer.hpp"
#include "sdr/RtlSdrDevice.hpp"
#include "sdr/SdrplayDevice.hpp"
#include "sdr/DiversityReceiver.hpp"

void testRingBuffer() {
    RadiosondePI::DSP::RingBuffer<int> rb(8);
    assert(rb.size() == 0);

    int writeData[] = {1, 2, 3, 4};
    size_t written = rb.write(writeData, 4);
    assert(written == 4);
    assert(rb.size() == 4);

    int readData[4] = {0};
    size_t read = rb.read(readData, 4);
    assert(read == 4);
    assert(rb.size() == 0);
    assert(readData[0] == 1 && readData[3] == 4);

    std::cout << "[TEST PASSED] RingBuffer basic read/write." << std::endl;
}

void testEcefConversion() {
    // ECEF coordinates around Greenwich / 0,0,0
    double lat = 0, lon = 0, alt = 0;
    RadiosondePI::Decoders::RS41Decoder::ecefToGeodetic(6378137.0, 0.0, 0.0, lat, lon, alt);
    assert(std::abs(lat) < 0.001);
    assert(std::abs(lon) < 0.001);
    assert(std::abs(alt) < 1.0);

    std::cout << "[TEST PASSED] ECEF to WGS-84 coordinate transformation." << std::endl;
}

void testFmDiscriminator() {
    RadiosondePI::DSP::FmDiscriminator disc;
    // Pure carrier without phase rotation should produce near 0 deviation
    RadiosondePI::DSP::Complex32 tone(1.0f, 0.0f);
    float dev = disc.processSample(tone);
    assert(std::abs(dev) < 0.001f);

    std::cout << "[TEST PASSED] FM Discriminator zero-frequency response." << std::endl;
}

void testReedSolomonFec() {
    uint8_t block[255]{};
    // Initialize clean block with dummy message and computed parity/zeros
    int errorsCorrected = 0;
    bool ok = RadiosondePI::Decoders::RS41Decoder::decodeReedSolomon(block, errorsCorrected);
    assert(ok);
    assert(errorsCorrected == 0);

    // Corrupt 2 bytes
    block[10] ^= 0x55;
    block[50] ^= 0xAA;
    ok = RadiosondePI::Decoders::RS41Decoder::decodeReedSolomon(block, errorsCorrected);
    assert(ok);
    assert(errorsCorrected == 2);
    assert(block[10] == 0x00);
    assert(block[50] == 0x00);

    std::cout << "[TEST PASSED] Reed-Solomon (255, 231) Berlekamp-Massey & Chien search." << std::endl;
}

void testZeroAllocFirAndDemod() {
    RadiosondePI::DSP::FirFilter filter;
    filter.initLowPass(15, 48000.0f, 5000.0f);

    std::vector<RadiosondePI::DSP::Complex32> input(100, RadiosondePI::DSP::Complex32(1.0f, 0.5f));
    RadiosondePI::DSP::Complex32 output[20];
    size_t written = filter.processBlockDecimateZeroAlloc(input.data(), input.size(), 5, output, 20);
    assert(written == 20);

    RadiosondePI::DSP::FmDiscriminator demod;
    float audio[20];
    demod.processBlockZeroAlloc(output, 20, audio);

    std::cout << "[TEST PASSED] Zero-allocation decimation and demodulation buffer pipeline." << std::endl;
}

void testSdrplayDevice() {
    RadiosondePI::SDR::SdrConfig rspConfig{};
    rspConfig.driver = "sdrplay";
    rspConfig.frequencyHz = 404800000;
    rspConfig.sampleRate = 2400000;
    rspConfig.antennaPort = "AntennaB";
    rspConfig.broadcastNotch = true;

    auto dev = RadiosondePI::SDR::createSdrDevice(rspConfig);
    assert(dev != nullptr);
    assert(dev->getDriverType() == RadiosondePI::SDR::SdrDriverType::SdrplayRSP ||
           dev->getDriverType() == RadiosondePI::SDR::SdrDriverType::Simulation);

    bool opened = dev->open(rspConfig);
    assert(opened);
    assert(dev->isOpen());

    dev->setFrequency(404800000);
    assert(dev->getConfig().frequencyHz == 404800000);

    dev->close();
    assert(!dev->isRunning());

    std::cout << "[TEST PASSED] SDRplay RSPdx-R2 device driver lifecycle & configuration." << std::endl;
}

int main() {
    std::cout << "--- Running Sprint 1 DSP & Decoder Tests ---" << std::endl;
    testRingBuffer();
    testEcefConversion();
    testFmDiscriminator();
    testReedSolomonFec();
    testZeroAllocFirAndDemod();
    testSdrplayDevice();
    std::cout << "--- All Sprint 1 Tests Passed ---" << std::endl;
    return 0;
}
