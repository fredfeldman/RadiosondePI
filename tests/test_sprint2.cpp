#include <cassert>
#include <iostream>
#include <cmath>
#include <vector>

#include "decoders/DFMDecoder.hpp"
#include "decoders/M10Decoder.hpp"
#include "dsp/Afc.hpp"
#include "dsp/DiversityCombiner.hpp"
#include "scanner/SpectrumScanner.hpp"

void testAfc() {
    RadiosondePI::DSP::Afc afc;
    afc.init(48000.0f, 0.1f);
    assert(std::abs(afc.getFrequencyOffsetHz()) < 0.001f);

    // Simulate positive frequency offset
    for (int i = 0; i < 20; ++i) {
        afc.updateOffsetEstimate(0.1f); // 0.1 normalized offset = +2400 Hz
    }
    assert(afc.getFrequencyOffsetHz() > 1500.0f);

    std::cout << "[TEST PASSED] AFC frequency tracking & convergence." << std::endl;
}

void testDiversityCombining() {
    RadiosondePI::DSP::DiversityCombiner combiner(RadiosondePI::DSP::DiversityMode::MaximalRatioCombining, 2);

    // Channel 0: Clean carrier
    std::vector<RadiosondePI::DSP::Complex32> ch0(256, RadiosondePI::DSP::Complex32(1.0f, 0.0f));
    // Channel 1: Carrier with 90 degree phase lag (0.0 + 1.0j)
    std::vector<RadiosondePI::DSP::Complex32> ch1(256, RadiosondePI::DSP::Complex32(0.0f, 1.0f));

    const RadiosondePI::DSP::Complex32* channels[2] = { ch0.data(), ch1.data() };
    size_t counts[2] = { 256, 256 };

    std::vector<RadiosondePI::DSP::Complex32> output(256);
    size_t combined = combiner.combineBlocks(channels, counts, 2, output.data(), output.size());

    assert(combined == 256);
    // After phase-alignment and MRC combination, real part should be reinforced
    assert(output[0].real() > 0.8f);

    std::cout << "[TEST PASSED] Multi-channel diversity combining & phase alignment." << std::endl;
}

void testSpectrumScannerInit() {
    RadiosondePI::Scanner::ScannerConfig cfg{};
    cfg.minFrequencyHz = 400050000;
    cfg.maxFrequencyHz = 406000000;
    RadiosondePI::Scanner::SpectrumScanner scanner(cfg);

    std::vector<RadiosondePI::DSP::Complex32> testSamples(2048, RadiosondePI::DSP::Complex32(0.0f, 0.0f));
    auto peaks = scanner.analyzeBlock(testSamples.data(), testSamples.size(), 403000000, 2400000.0f);
    assert(peaks.empty()); // Pure zeros should not trigger false detection

    std::cout << "[TEST PASSED] Spectrum Scanner baseline analysis." << std::endl;
}

int main() {
    std::cout << "--- Running Sprint 2 Multi-Decoder & Scanner Tests ---" << std::endl;
    testAfc();
    testDiversityCombining();
    testSpectrumScannerInit();
    std::cout << "--- All Sprint 2 Tests Passed ---" << std::endl;
    return 0;
}
