#include <cassert>
#include <iostream>
#include <cmath>
#include <vector>

#include "decoders/DFMDecoder.hpp"
#include "decoders/M10Decoder.hpp"
#include "dsp/Afc.hpp"
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
    testSpectrumScannerInit();
    std::cout << "--- All Sprint 2 Tests Passed ---" << std::endl;
    return 0;
}
