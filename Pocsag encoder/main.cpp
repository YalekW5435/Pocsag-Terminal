/*Cross platform POCSAG encoder.  Please feel free to improve if you wish.  DO NOT make malice of this file, and do not insert any secuity flaws.  This program is written for the reasons of it being useful.  */
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <algorithm>

const uint32_t SYNC_WORD = 0x7CD215D8;
const uint32_t IDLE_WORD = 0x7A89C197;
const uint32_t PREAMBLE_BITS = 576;//"01010101010101010"
const uint32_t BCH_POLY = 0x769;//the default set so it causes 0 BCHchecksum errors.  Any other value causes checkum errors and "ghost" addresses.

struct Slot {
    std::vector<uint32_t> rics;
    std::string type;
    std::string message;
    int function;
};

uint32_t encode_bch(uint32_t data_21bit) {
    uint32_t reg = data_21bit << 10;
    for (int i = 30; i >= 10; --i) {
        if (reg & (1U << i)) reg ^= (BCH_POLY << (i - 10));
    }
    // Result: Data (21 bits) + Checkbits (10 bits) + Placeholder Parity (1 bit)
    uint32_t codeword = (data_21bit << 11) | (reg << 1);

    // Even Parity Calculation
    int count = 0;
    for (int i = 1; i < 32; ++i) if (codeword & (1U << i)) count++;
    if (count % 2 != 0) codeword |= 1;
    return codeword;
}
//write raw pcm audio from no external libs.  
void write_audio(std::ofstream& out, bool bit, int spb) {
    int32_t val = bit ? -8000000 : 8000000;
    for (int s = 0; s < spb; ++s) {
        out.put(static_cast<char>(val & 0xFF));
        out.put(static_cast<char>((val >> 8) & 0xFF));
        out.put(static_cast<char>((val >> 16) & 0xFF));
    }
}

void write_cw(std::ofstream& out, uint32_t cw, int spb) {
    for (int i = 31; i >= 0; --i) write_audio(out, (cw >> i) & 1, spb);
}

// BCD Mapping per ITU-R M.584-2
uint8_t to_bcd(char c) {
    if (isdigit(c)) return c - '0';
    switch (toupper(c)) {
    case '*': return 0xA; case 'U': return 0xB; case ' ': return 0xC;
    case '-': return 0xD; case ')': return 0xE; case '(': return 0xF;
    default: return 0xC;


    }
}

// Parses group format: [100,200-205]
std::vector<uint32_t> parse_group(std::string input) {
    std::vector<uint32_t> results;
    input.erase(std::remove(input.begin(), input.end(), '['), input.end());
    input.erase(std::remove(input.begin(), input.end(), ']'), input.end());
    std::stringstream ss(input);
    std::string segment;
    while (std::getline(ss, segment, ',')) {
        size_t dash = segment.find('-');
        if (dash != std::string::npos) {
            uint32_t start = std::stoul(segment.substr(0, dash));
            uint32_t end = std::stoul(segment.substr(dash + 1));
            for (uint32_t i = start; i <= end; ++i) results.push_back(i);
        }
        else {
            results.push_back(std::stoul(segment));
        }
    }
    return results;
}

void print_help() {
    std::cout << "Terminal POCSAG Encoder Tool that dumps POCSAG pager messages to encode to a .raw file for further analysis.\n"
      
        << "Options:\n"
        << "  --address <ric>       Single address (0-2097151, anything >> 2097151 gets reset to 0 and recounts)\n"
        << "  --function (0-3)\n"
        << "  --group \"[10,20-25]\"  Broadcast to multiple RICs inside braces\n"
        << "  --type <alpha|numeric|tone>\n"
        << "  --message \"text\"      Message string to send\n"
        << "  --bps <512|1200|2400> Transmission speed (Default: 1200)\n"
        << "  --slot                create a batch of separate messages with different addresses.  When using this, ensure the --slot parameter comes *first* before \n anything.  Once your message is completed, you can move on to the next --slot.\n\n"
        << "  --output <file>       Path to 24-bit raw output\n";
        << "  Recommended usage:\n";
        << "  PocsagEncoder.exe --address --function --bps --type --message ""  --output\n"
        << "  PocsagEncoder.exe --group[, or - to repeat addresses sequentially] --function --bps --type --message "" --output ""page1.raw""\n"
        << "  PocsagEncoder.exe --slot --address --function --bps --type --message ""  --slot --address --function --bps --type --message ""  --output ""page1.raw""\n\n"	
        << "  I would highly recommend using the ""required"" use to get a feel of how this functionality works.\n"
    }

int main(int argc, char* argv[]) {
    if (argc < 2) { print_help(); return 0; }
    std::map<std::string, std::string> current_args;
    std::vector<Slot> slots;

    auto push_slot = [&](std::map<std::string, std::string>& m) {
        if (!m.count("--address") && !m.count("--group")) return;
        Slot s;
        s.rics = m.count("--group") ? parse_group(m["--group"]) : std::vector<uint32_t>{ (uint32_t)std::stoul(m["--address"]) };
        s.type = m.count("--type") ? m["--type"] : "alpha";
        s.message = m.count("--message") ? m["--message"] : "";
        s.function = m.count("--function") ? std::stoi(m["--function"]) : (s.type == "alpha" ? 3 : (s.type == "numeric" ? 0 : 1));
        slots.push_back(s);
        };

    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if (key == "--slot") { push_slot(current_args); current_args.erase("--address"); current_args.erase("--group"); current_args.erase("--message"); }
        else if (key.find("--") == 0 && i + 1 < argc) { current_args[key] = argv[++i]; }
    }
    push_slot(current_args);

    if (!current_args.count("--output")) { std::cerr << "Error: No output file.\n"; return 1; }
    int bps = std::stoi(current_args.count("--bps") ? current_args["--bps"] : "1200");
    int spb = 48000 / bps;
    std::ofstream out(current_args["--output"], std::ios::binary);

    for (int i = 0; i < PREAMBLE_BITS; ++i) write_audio(out, i % 2, spb);

    for (auto& s : slots) {
        for (uint32_t ric : s.rics) {
            write_cw(out, SYNC_WORD, spb);
            uint32_t addr_cw = encode_bch(((ric >> 3) << 2) | (s.function & 0x3));

            std::vector<uint32_t> msg_cws;
            if (s.type == "alpha") {
                uint64_t buf = 0; int b = 0;
                for (char c : s.message) {
                    uint32_t c7 = (uint32_t)(c & 0x7F);
                    // Pack 7-bit characters into the buffer
                    // Note: Standard POCSAG reverses bits within the 7-bit char for transmission
                    uint32_t rev7 = 0;
                    for (int i = 0; i < 7; ++i) if ((c7 >> i) & 1) rev7 |= (1 << (6 - i));

                    buf = (buf << 7) | rev7;
                    b += 7;
                    while (b >= 20) {
                        uint32_t data_20 = (uint32_t)(buf >> (b - 20));
                        // Bit 31 must be 1 for message codewords
                        msg_cws.push_back(encode_bch((1U << 20) | data_20));
                        b -= 20;
                        buf &= (1ULL << b) - 1;
                    }
                }
                if (b > 0) msg_cws.push_back(encode_bch((1U << 20) | (uint32_t)(buf << (20 - b))));
            }
            else if (s.type == "numeric") {
                uint32_t bcd_payload = 0;
                int nibble_idx = 0;

                for (char c : s.message) {
                    uint8_t nib = to_bcd(c) & 0x0F;

                    // Reverse bits within the 4-bit nibble (Standard for many POCSAG decoders)
                    uint8_t rev_nib = 0;
                    if (nib & 0x01) rev_nib |= 0x08;
                    if (nib & 0x02) rev_nib |= 0x04;
                    if (nib & 0x04) rev_nib |= 0x02;
                    if (nib & 0x08) rev_nib |= 0x01;

                    // Pack into 20-bit area: First digit goes into the highest 4 bits
                    // Bits 19-16, then 15-12, 11-8, 7-4, 3-0
                    bcd_payload |= (uint32_t(rev_nib) << (4 * (4 - nibble_idx)));
                    nibble_idx++;

                    if (nibble_idx == 5) {
                        msg_cws.push_back(encode_bch(0x100000 | bcd_payload));
                        bcd_payload = 0;
                        nibble_idx = 0;
                    }
                }

                // Pad remaining nibbles with 'Space' (0xC), also bit-reversed
                if (nibble_idx > 0) {
                    uint8_t rev_space = 0x03; // Bit-reversed 0xC (1100 -> 0011)
                    while (nibble_idx < 5) {
                        bcd_payload |= (uint32_t(rev_space) << (4 * (4 - nibble_idx)));
                        nibble_idx++;
                    }
                    msg_cws.push_back(encode_bch(0x100000 | bcd_payload));
                }
            }
            int current_cw = 0;
            int frame_idx = ric % 8;
            for (int i = 0; i < frame_idx * 2; ++i) { write_cw(out, IDLE_WORD, spb); current_cw++; }
            write_cw(out, addr_cw, spb); current_cw++;
            for (uint32_t mcw : msg_cws) {
                if (current_cw == 16) { write_cw(out, SYNC_WORD, spb); current_cw = 0; }
                write_cw(out, mcw, spb); current_cw++;
            }
            while (current_cw < 16) { write_cw(out, IDLE_WORD, spb); current_cw++; }
        }
    }
    out.close();
    std::cout << "Encoded " << slots.size() << " slot(s) to " << current_args["--output"] << "\n";
    return 0;
}
