/*Cross platform POCSAG encoder.  Please feel free to improve if you wish.  DO NOT make malice of this file, and do not insert any secuity flaws.  This program is written for the reasons of it being useful.  */
#include <cstdint>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <algorithm>
#include <cmath>

const uint32_t SYNC_WORD = 0x7CD215D8;
const uint32_t IDLE_WORD = 0x7A89C197;
const uint32_t PREAMBLE_BITS = 576;
const uint32_t BCH_POLY = 0x769;

struct Slot {
    std::vector<uint32_t> rics;
    std::string type;
    std::string message;
    int function;
};

struct Batch {
    int bps = 1200;
    std::vector<Slot> slots;
};

struct IdleConfig {
    double freq = 0.0;
    int duration_ms = 0;
    bool enabled = false;
};

uint32_t encode_bch(uint32_t data_21bit) {
    uint32_t reg = data_21bit << 10;
    for (int i = 30; i >= 10; --i) {
        if (reg & (1U << i)) reg ^= (BCH_POLY << (i - 10));
    }
    uint32_t codeword = (data_21bit << 11) | (reg << 1);

    int count = 0;
    for (int i = 1; i < 32; ++i) if (codeword & (1U << i)) count++;
    if (count % 2 != 0) codeword |= 1;
    return codeword;
}

void write_sample(std::ofstream& out, int32_t val) {
    out.put(static_cast<char>(val & 0xFF));
    out.put(static_cast<char>((val >> 8) & 0xFF));
    out.put(static_cast<char>((val >> 16) & 0xFF));
}

void write_audio(std::ofstream& out, bool bit, int spb) {
    int32_t val = bit ? -8000000 : 8000000;
    for (int s = 0; s < spb; ++s) {
        write_sample(out, val);
    }
}

void write_cw(std::ofstream& out, uint32_t cw, int spb) {
    for (int i = 31; i >= 0; --i) write_audio(out, (cw >> i) & 1, spb);
}

void write_square_wave(std::ofstream& out, double freq, int duration_ms, int sample_rate = 48000) {
    int total_samples = (sample_rate * duration_ms) / 1000;
    int period_samples = static_cast<int>(sample_rate / freq);
    if (period_samples <= 0) period_samples = 1;

    for (int i = 0; i < total_samples; ++i) {
        bool high = (i % period_samples) < (period_samples / 2);
        int32_t val = high ? 8000000 : -8000000;
        write_sample(out, val);
    }
}

void write_silence(std::ofstream& out, int duration_ms, int sample_rate = 48000) {
    int total_samples = (sample_rate * duration_ms) / 1000;
    for (int i = 0; i < total_samples; ++i) {
        write_sample(out, 0);
    }
}

void write_primary_preamble(std::ofstream& out, int bps, int sample_rate = 48000) {
    if (bps == 512) {
        write_square_wave(out, 375.0, 400, sample_rate);
    }
    else {
        // Default 1200 BPS specifications
        write_square_wave(out, 750.0, 200, sample_rate);
    }
}

uint8_t to_bcd(char c) {
    if (isdigit(c)) return c - '0';
    switch (toupper(c)) {
    case '*': return 0xA; case 'U': return 0xB; case ' ': return 0xC;
    case '-': return 0xD; case ')': return 0xE; case '(': return 0xF;
    default: return 0xC;
    }
}

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

// Parses string content into tokens while preserving quoted text blocks
std::vector<std::string> parse_file_arguments(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open script file: " << filepath << "\n";
        return {};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    std::vector<std::string> args;
    std::string current_token;
    bool in_quotes = false;
    char quote_char = '\0';

    for (size_t i = 0; i < content.length(); ++i) {
        char c = content[i];

        if ((c == '"' || c == '\'') && !in_quotes) {
            in_quotes = true;
            quote_char = c;
        }
        else if (in_quotes && c == quote_char) {
            in_quotes = false;
            quote_char = '\0';
        }
        else if (std::isspace(static_cast<unsigned char>(c)) && !in_quotes) {
            if (!current_token.empty()) {
                args.push_back(current_token);
                current_token.clear();
            }
        }
        else {
            current_token += c;
        }
    }

    if (!current_token.empty()) {
        args.push_back(current_token);
    }

    return args;
}

void print_help() {
    std::cout << "Terminal POCSAG Encoder Tool\n\n"
        << "Options:\n"
        << "  --file <path> | @<path> Read large CLI parameters/messages from a txt file to automate messages (bypasses 8191 character limits on some terminals)\n"
        << "  --address <ric>         Single address\n"
        << "  --function (0-3)\n"
        << "  --group \"[10,20-25]\"    Broadcast to multiple RICs inside braces\n"
        << "  --type <alpha|numeric|tone>\n"
        << "  --message \"text\"        Message string to send\n"
        << "  --bps <512|1200|2400>   Transmission speed (Default: 1200)\n"
        << "  --slot                  Start a NEW standalone slot/batch (includes preamble)\n"
        << "  --append                APPEND a message to the CURRENT batch (bypasses preamble)\n"
        << "  --silence <ms>          Inserts silence (ms) before encoding batches\n"
        << "  --idle <freq> <ms>      Square wave tone (emitted at start, between batches, & end)\n"
        << "  --output <file>         Path to 24-bit raw output\n"
        << "example usage:"
        << "PocsagEncoder --idle 37.5 2000 --slot --address 1234567 --function 2 --type alpha --message ""THIS IS A TEST PERIODIC PAGE SEQUENTIAL NUMBER 0001 ""--output ""page1.raw\n"""
        << "PocsagEncoder --idle 37.5 2000 --slot --group [10000,15000,16000-16010] --function 2 --type alpha --message ""group test page from yalek the lembine\n"""
        << "PocsagEncoder --idle 37.5 2000 --slot --address 5000 --function 2 --type alpha --message ""appended test page from yalek the lembine"" --append --address 5500 --function 1 --type alpha --message ""appended test page from yalek the lembine \n"""
        << "PocsagEncoder @YourCLIFile.txt.  Note, only list your commands in that file. DO NOT put the program title inside your txt file! doing so will crash the program!\n"
}

int main(int argc, char* argv[]) {
    if (argc < 2) { print_help(); return 0; }

    // Load arguments, expanding file contents if --file or @filename is used
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--file" && i + 1 < argc) {
            std::vector<std::string> file_args = parse_file_arguments(argv[++i]);
            args.insert(args.end(), file_args.begin(), file_args.end());
        }
        else if (arg.length() > 1 && arg[0] == '@') {
            std::vector<std::string> file_args = parse_file_arguments(arg.substr(1));
            args.insert(args.end(), file_args.begin(), file_args.end());
        }
        else {
            args.push_back(arg);
        }
    }

    std::map<std::string, std::string> current_args;
    std::vector<Batch> batches;
    IdleConfig idle_cfg;
    int silence_ms = 0;

    auto push_slot = [&](std::map<std::string, std::string>& m, bool new_batch) {
        if (!m.count("--address") && !m.count("--group")) return;
        Slot s;
        s.rics = m.count("--group") ? parse_group(m["--group"]) : std::vector<uint32_t>{ (uint32_t)std::stoul(m["--address"]) };
        s.type = m.count("--type") ? m["--type"] : "alpha";
        s.message = m.count("--message") ? m["--message"] : "";
        s.function = m.count("--function") ? std::stoi(m["--function"]) : (s.type == "alpha" ? 3 : (s.type == "numeric" ? 0 : 1));

        int slot_bps = m.count("--bps") ? std::stoi(m["--bps"]) : 1200;

        if (new_batch || batches.empty()) {
            Batch b;
            b.bps = slot_bps;
            b.slots.push_back(s);
            batches.push_back(b);
        }
        else {
            batches.back().slots.push_back(s);
        }
        };

    bool is_appended = false;

    for (size_t i = 0; i < args.size(); ++i) {
        std::string key = args[i];
        if (key == "--slot") {
            push_slot(current_args, !is_appended);
            current_args.erase("--address");
            current_args.erase("--group");
            current_args.erase("--message");
            is_appended = false;
        }
        else if (key == "--append") {
            push_slot(current_args, false);
            current_args.erase("--address");
            current_args.erase("--group");
            current_args.erase("--message");
            is_appended = true;
        }
        else if (key == "--silence" && i + 1 < args.size()) {
            silence_ms = std::stoi(args[++i]);
        }
        else if (key == "--idle" && i + 2 < args.size()) {
            idle_cfg.freq = std::stod(args[++i]);
            idle_cfg.duration_ms = std::stoi(args[++i]);
            idle_cfg.enabled = true;
        }
        else if (key.find("--") == 0 && i + 1 < args.size()) {
            current_args[key] = args[++i];
        }
    }
    push_slot(current_args, !is_appended && batches.empty());

    if (!current_args.count("--output")) { std::cerr << "Error: No output file specified.\n"; return 1; }

    int sample_rate = 48000;
    std::ofstream out(current_args["--output"], std::ios::binary);

    // Initial Silence
    if (silence_ms > 0) {
        write_silence(out, silence_ms, sample_rate);
    }

    // Initial Idle Tone
    if (idle_cfg.enabled && idle_cfg.duration_ms > 0) {
        write_square_wave(out, idle_cfg.freq, idle_cfg.duration_ms, sample_rate);
    }

    // Process Batches
    for (size_t b_idx = 0; b_idx < batches.size(); ++b_idx) {
        auto& batch = batches[b_idx];
        int spb = sample_rate / batch.bps;

        if (b_idx > 0 && idle_cfg.enabled && idle_cfg.duration_ms > 0) {
            write_square_wave(out, idle_cfg.freq, idle_cfg.duration_ms, sample_rate);
        }

        // Primary Preamble
        write_primary_preamble(out, batch.bps, sample_rate);

        // Standard POCSAG Bit Preamble
        for (int i = 0; i < PREAMBLE_BITS; ++i) write_audio(out, i % 2, spb);

        for (auto& s : batch.slots) {
            for (uint32_t ric : s.rics) {
                write_cw(out, SYNC_WORD, spb);
                uint32_t addr_cw = encode_bch(((ric >> 3) << 2) | (s.function & 0x3));

                std::vector<uint32_t> msg_cws;
                if (s.type == "alpha") {
                    uint64_t buf = 0; int b = 0;
                    for (char c : s.message) {
                        uint32_t c7 = (uint32_t)(c & 0x7F);
                        uint32_t rev7 = 0;
                        for (int i = 0; i < 7; ++i) if ((c7 >> i) & 1) rev7 |= (1 << (6 - i));

                        buf = (buf << 7) | rev7;
                        b += 7;
                        while (b >= 20) {
                            uint32_t data_20 = (uint32_t)(buf >> (b - 20));
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
                        uint8_t rev_nib = 0;
                        if (nib & 0x01) rev_nib |= 0x08;
                        if (nib & 0x02) rev_nib |= 0x04;
                        if (nib & 0x04) rev_nib |= 0x02;
                        if (nib & 0x08) rev_nib |= 0x01;

                        bcd_payload |= (uint32_t(rev_nib) << (4 * (4 - nibble_idx)));
                        nibble_idx++;

                        if (nibble_idx == 5) {
                            msg_cws.push_back(encode_bch(0x100000 | bcd_payload));
                            bcd_payload = 0;
                            nibble_idx = 0;
                        }
                    }

                    if (nibble_idx > 0) {
                        uint8_t rev_space = 0x03;
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
    }

    // Idle Tone
    if (idle_cfg.enabled && idle_cfg.duration_ms > 0) {
        write_square_wave(out, idle_cfg.freq, idle_cfg.duration_ms, sample_rate);
    }

    out.close();
    std::cout << "Encoded " << batches.size() << " message(s) to " << current_args["--output"] << "\n";
    return 0;
}
}
