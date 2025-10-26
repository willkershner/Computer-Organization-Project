#ifndef __PROJECT1_CPP__
#define __PROJECT1_CPP__
#define DEBUG false

#include "project1.h"

#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cctype>

/**
 * Parse a single .data line that may contain a ".word" directive.
 * Extracts comma/whitespace–separated tokens after ".word" and appends each
 * resolved value into static_memory. For each appended word, advances
 * static_memory_address by 4 bytes.
 */
void get_static_memory(std::string line, int& static_memory_address, std::vector<int>& static_memory, const std::unordered_map<std::string, int>& text_label_map, const std::unordered_map<std::string, int>& data_label_map) 
{
    // Normalize tabs to spaces for simpler scanning.
    for (char& c : line) 
    {
        if (c == '\t') c = ' ';
    }
    // Find the ".word" directive on this line; return if missing.
    std::size_t pos = line.find(".word");
    if (pos == std::string::npos) return;
    pos += 5;
    std::string token;

    // Process an accumulated token: resolve to an int, push to static_memory,
    auto flush = [&](const std::string& t) 
    {
        if (t.empty()) return;

        // Try to parse as integer.
        try 
        {
            int v = std::stoi(t);
            static_memory.push_back(v);
        } catch (...) 
        {
            // If is not a number then try data label.
            auto d = data_label_map.find(t);
            if (d != data_label_map.end()) 
            {
                static_memory.push_back(d->second);
            } else 
            {
                // Else try text label and convert instruction index to byte address.
                auto it = text_label_map.find(t);
                if (it != text_label_map.end()) 
                {
                    static_memory.push_back(it->second * 4);
                } else 
                {
                    static_memory.push_back(0);
                }
            }
        }

        static_memory_address += 4;
    };

    // Scan the remainder of the line; split by commas and whitespace.
    for (std::size_t i = pos; i <= line.size(); ++i) 
    {
        char ch = (i < line.size() ? line[i] : ',');
        if (ch == ',' || std::isspace(static_cast<unsigned char>(ch)) || i == line.size()) 
        {
            if (!token.empty()) 
            { flush(token); token.clear(); }
        } else 
        {
            token.push_back(ch);
        }
    }
}

/**
 * Build a map of text labels to instruction indices.
 * Counts only real instructions.
 */
std::unordered_map<std::string, int> match_labels(std::vector<std::string> instructions) 
{
    std::unordered_map<std::string, int> label_map;
    bool main_found = false;
    int line_number = 0;

    for (const std::string& inst : instructions) 
    {
        if (!main_found) 
        {
            if (inst == "main:") main_found = true;
            continue;
        }

        // Skip assembler directives like .data/.text/.globl.
        if (inst.empty() || inst[0] == '.') continue;

        // Label-only line: "label:"
        if (!inst.empty() && inst.back() == ':') 
        {
            std::string label = inst.substr(0, inst.size() - 1);
            label_map[label] = line_number;
            continue;
        }
        line_number++;
    }

    return label_map;
}

/**
 * Build a map of data labels to byte offsets.
 * Scans all lines and every ".word" count advances the current byte offset by 4 per entry.
 */
std::unordered_map<std::string, int> add_data_labels(std::vector<std::string> instructions) 
{
    std::unordered_map<std::string, int> data_label_map;
    int byte_offset = 0;

    for (std::string raw : instructions) 
    {
        for (char& c : raw) 
        {
            if (c == '\t') c = ' ';
        }
        // If the line has a label at the start, capture it as pointing to current byte_offset.
        std::size_t colon = raw.find(':');
        if (colon != std::string::npos) 
        {
            std::string label = raw.substr(0, colon);
            while (!label.empty() && std::isspace(static_cast<unsigned char>(label.back()))) 
            {
                label.pop_back();
            }
            if (!label.empty() && label[0] != '.') 
            {
                data_label_map[label] = byte_offset;
            }
        }
        // If the line has ".word", count the number of entries to advance the offset.
        std::size_t pos = raw.find(".word");
        if (pos == std::string::npos) continue;
        pos += 5;

        std::string tok;
        int count_words = 0;
        // Count tokens after ".word" separated by commas/whitespace.
        for (std::size_t i = pos; i <= raw.size(); ++i) 
        {
            char ch = (i < raw.size() ? raw[i] : ',');
            if (ch == ',' || std::isspace(static_cast<unsigned char>(ch)) || i == raw.size()) 
            {
                if (!tok.empty()) 
                { ++count_words; tok.clear(); }
            } else 
            {
                tok.push_back(ch);
            }
        }
        byte_offset += 4 * count_words;
    }

    return data_label_map;
}

int main(int argc, char* argv[]) 
{
    if (argc < 4) 
    {
        std::cerr
            << "Expected Usage:\n"
            << "  ./assemble infile1.asm infile2.asm ... infilek.asm "
            << "staticmem_outfile.bin instructions_outfile.bin\n";
        exit(1);
    }

    // Prepare output files.
    std::ofstream inst_outfile;
    std::ofstream static_outfile;
    static_outfile.open(argv[argc - 2], std::ios::binary);
    inst_outfile.open(argv[argc - 1], std::ios::binary);

    std::vector<std::string> instructions; // cleaned lines from all input files
    std::vector<int> static_memory;        // assembled .data words

    /**
     * Phase 1:
     * Read all instructions, clean them of comments and whitespace DONE
     * TODO: Determine the numbers for all static memory labels
     * (measured in bytes starting at 0)
     * TODO: Determine the line numbers of all instruction line labels
     * (measured in instructions) starting at 0
    */
    for (int i = 1; i < argc - 2; i++) 
    {
        std::ifstream infile(argv[i]);
        if (!infile) 
        {
            std::cerr << "Error: could not open file: " << argv[i] << std::endl;
            exit(1);
        }

        std::string line;
        while (std::getline(infile, line)) 
        {
            line = clean(line);
            if (line.empty()) continue;
            instructions.push_back(line);
        }
        infile.close();
    }

    // Build maps for labels.
    std::unordered_map<std::string, int> label_map = match_labels(instructions);
    if (DEBUG) 
    {
        for (const auto& p : label_map) 
        {
            std::cerr << "Label: " << p.first << "  Line: " << p.second << std::endl;
        }
    }
    std::unordered_map<std::string, int> data_label_map = add_data_labels(instructions);

    int static_memory_address = 0;
    bool reading_static_memory = false;

    for (std::size_t i = 0; i < instructions.size(); ++i) 
    {
        std::string inst = instructions[i];

        if (inst == ".text") 
        {
            // Stop treating lines as .data; resume normal parsing.
            reading_static_memory = false;
            continue;
        }

        if (reading_static_memory) 
        {
            // Try to parse a ".word" list on this line and append to static_memory.
            get_static_memory(inst, static_memory_address, static_memory, label_map, data_label_map);
            continue;
        }

        if (inst == ".data") 
        {
            // Start treating subsequent lines as data section until the next ".text".
            reading_static_memory = true;
            continue;
        }
    }

    std::vector<std::string> cleaned_instructions;
    bool main_found = false;

    for (const std::string& inst : instructions) 
    {
        if (!main_found) 
        {
            if (inst == "main:") main_found = true;
            continue;
        }
        // Skip label-only lines after main.
        if (!inst.empty() && inst.back() == ':') continue;

        // Skip assembler directives entirely.
        if (!inst.empty() && inst[0] == '.') continue;

        cleaned_instructions.push_back(inst);
    }

     /** Phase 2
     * Process all static memory, output to static memory file
     * TODO: All of this
     */
    if (DEBUG) {
        for (std::size_t i = 0; i < static_memory.size(); ++i) {
            std::cerr << "Static [addr " << (i * 4) << "]: " << static_memory[i] << std::endl;
        }
    }

    for (int data : static_memory) {
        write_binary(data, static_outfile);
    }

    /** Phase 3
     * Process all instructions, output to instruction memory file
     * TODO: Almost all of this, it only works for adds
     */
    int current_instruction_number = 0;

    for (const std::string& inst : cleaned_instructions) 
    {
        // Tokenize the instruction line by whitespace, commas, and parentheses.
        std::vector<std::string> terms = split(inst, WHITESPACE + ",()");
        if (terms.empty()) continue;
        const std::string inst_type = terms[0];
        if (inst_type == "add") 
        {
            write_binary(encode_Rtype(0, registers[terms[2]], registers[terms[3]], registers[terms[1]], 0, 32), inst_outfile);
        }
        else if (inst_type == "sub") 
        {
            write_binary(encode_Rtype(0, registers[terms[2]], registers[terms[3]], registers[terms[1]], 0, 34), inst_outfile);
        }
        else if (inst_type == "slt") 
        {
            write_binary(encode_Rtype(0, registers[terms[2]], registers[terms[3]], registers[terms[1]], 0, 42), inst_outfile);
        }
        else if (inst_type == "addi") 
        {
            write_binary(encode_Itype(8,  registers[terms[2]], registers[terms[1]], std::stoi(terms[3])), inst_outfile);
        }
        else if (inst_type == "lw") 
        {
            write_binary(encode_Itype(35, registers[terms[3]], registers[terms[1]], std::stoi(terms[2])), inst_outfile);
        }
        else if (inst_type == "sw") 
        {
            write_binary(encode_Itype(43, registers[terms[3]], registers[terms[1]], std::stoi(terms[2])), inst_outfile);
        }
        else if (inst_type == "beq") 
        {
            const std::string& label = terms[3];
            int offset = label_map[label] - (current_instruction_number + 1);
            write_binary(encode_Itype(4, registers[terms[1]], registers[terms[2]], offset), inst_outfile);
        }
        else if (inst_type == "bne") 
        {
            const std::string& label = terms[3];
            int offset = label_map[label] - (current_instruction_number + 1);
            write_binary(encode_Itype(5, registers[terms[1]], registers[terms[2]], offset), inst_outfile);
        }
        else if (inst_type == "j") 
        {
            const std::string& label = terms[1];
            write_binary(encode_Jtype(2, label_map[label]), inst_outfile);
        }
        else if (inst_type == "jal") 
        {
            const std::string& label = terms[1];
            write_binary(encode_Jtype(3, label_map[label]), inst_outfile);
        }
        else if (inst_type == "jr") 
        {
            int rs = registers[terms[1]];
            write_binary(encode_Rtype(0, rs, 0, 0, 0, 8), inst_outfile);
        }
        else if (inst_type == "jalr") 
        {
            int rd, rs;
            if (terms.size() == 2) 
            { rs = registers[terms[1]]; rd = 31; }
            else 
            { rd = registers[terms[1]]; rs = registers[terms[2]]; }
            write_binary(encode_Rtype(0, rs, 0, rd, 0, 9), inst_outfile);
        }
        else if (inst_type == "syscall") 
        {
            write_binary(encode_Rtype(0, 0, 0, 0, 0, 12), inst_outfile);
        }
        else if (inst_type == "mult") 
        {
            write_binary(encode_Rtype(0, registers[terms[1]], registers[terms[2]], 0, 0, 24), inst_outfile);
        }
        else if (inst_type == "div") 
        {
            write_binary(encode_Rtype(0, registers[terms[1]], registers[terms[2]], 0, 0, 26), inst_outfile);
        }
        else if (inst_type == "mfhi") 
        {
            write_binary(encode_Rtype(0, 0, 0, registers[terms[1]], 0, 16), inst_outfile);
        }
        else if (inst_type == "mflo") 
        {
            write_binary(encode_Rtype(0, 0, 0, registers[terms[1]], 0, 18), inst_outfile);
        }
        else if (inst_type == "sll") 
        {
            write_binary(encode_Rtype(0, 0, registers[terms[2]], registers[terms[1]], std::stoi(terms[3]), 0), inst_outfile);
        }
        else if (inst_type == "srl") 
        {
            write_binary(encode_Rtype(0, 0, registers[terms[2]], registers[terms[1]], std::stoi(terms[3]), 2), inst_outfile);
        }
        else if (inst_type == "la") 
        {
            int rd = registers[terms[1]];
            const std::string& sym = terms[2];
            int addr = 0;
            if (label_map.count(sym))          addr = label_map[sym] * 4;
            else if (data_label_map.count(sym)) addr = data_label_map[sym];
            int hi = (addr >> 16) & 0xFFFF;
            int lo =  addr        & 0xFFFF;
            write_binary(encode_Itype(15, 0, 1,  hi), inst_outfile);
            write_binary(encode_Itype(13, 1, rd, lo), inst_outfile);
        }
        else if (inst_type == "sgt")
        {
            write_binary(encode_Rtype(0, registers[terms[3]], registers[terms[2]], registers[terms[1]], 0, 42), inst_outfile);
        }
        else if (inst_type == "sge")
        {
            int rd = registers[terms[1]], rs = registers[terms[2]], rt = registers[terms[3]];
            write_binary(encode_Rtype(0, rs, rt, rd, 0, 42), inst_outfile);
            write_binary(encode_Itype(8, 0, 1, 1), inst_outfile);
            write_binary(encode_Rtype(0, rd, 1, rd, 0, 42), inst_outfile);
        }
        else if (inst_type == "sle")
        {
            int rd = registers[terms[1]], rs = registers[terms[2]], rt = registers[terms[3]];
            write_binary(encode_Rtype(0, rt, rs, rd, 0, 42), inst_outfile);
            write_binary(encode_Itype(8, 0, 1, 1), inst_outfile);
            write_binary(encode_Rtype(0, rd, 1, rd, 0, 42), inst_outfile);
        }
        else if (inst_type == "seq")
        {
            int rd = registers[terms[1]], rs = registers[terms[2]], rt = registers[terms[3]];
            write_binary(encode_Rtype(0, rs, rt, rd, 0, 42), inst_outfile);
            write_binary(encode_Rtype(0, rt, rs, 1, 0, 42), inst_outfile);
            write_binary(encode_Rtype(0, rd, 1, rd, 0, 32), inst_outfile);
            write_binary(encode_Rtype(0, 0, rd, rd, 0, 42), inst_outfile);
            write_binary(encode_Itype(8, 0, 1, 1), inst_outfile);
            write_binary(encode_Rtype(0, rd, 1, rd, 0, 42), inst_outfile);
        }
        else if (inst_type == "sne")
        {
            int rd = registers[terms[1]], rs = registers[terms[2]], rt = registers[terms[3]];
            write_binary(encode_Rtype(0, rs, rt, rd, 0, 42), inst_outfile);
            write_binary(encode_Rtype(0, rt, rs, 1, 0, 42), inst_outfile);
            write_binary(encode_Rtype(0, rd, 1, rd, 0, 32), inst_outfile);
            write_binary(encode_Rtype(0, 0, rd, rd, 0, 42), inst_outfile);
        }
        else if (inst_type == "bge")
        {
            int rs = registers[terms[1]], rt = registers[terms[2]];
            const std::string& label = terms[3];
            int offset = label_map[label] - (current_instruction_number + 1);
            write_binary(encode_Rtype(0, rs, rt, 1, 0, 42), inst_outfile);
            write_binary(encode_Itype(4, 1, 0, offset), inst_outfile);
        }
        else if (inst_type == "bgt")
        {
            int rs = registers[terms[1]], rt = registers[terms[2]];
            const std::string& label = terms[3];
            int offset = label_map[label] - (current_instruction_number + 1);
            write_binary(encode_Rtype(0, rt, rs, 1, 0, 42), inst_outfile);
            write_binary(encode_Itype(5, 1, 0, offset), inst_outfile);
        }
        else if (inst_type == "ble")
        {
            int rs = registers[terms[1]], rt = registers[terms[2]];
            const std::string& label = terms[3];
            int offset = label_map[label] - (current_instruction_number + 1);
            write_binary(encode_Rtype(0, rt, rs, 1, 0, 42), inst_outfile);
            write_binary(encode_Itype(4, 1, 0, offset), inst_outfile);
        }
        else if (inst_type == "blt")
        {
            int rs = registers[terms[1]], rt = registers[terms[2]];
            const std::string& label = terms[3];
            int offset = label_map[label] - (current_instruction_number + 1);
            write_binary(encode_Rtype(0, rs, rt, 1, 0, 42), inst_outfile);
            write_binary(encode_Itype(5, 1, 0, offset), inst_outfile);
        }
        else if (inst_type == "abs")
        {
            int rd = registers[terms[1]], rs = registers[terms[2]];
            write_binary(encode_Rtype(0, 0, rs, 1, 31, 3), inst_outfile);
            write_binary(encode_Rtype(0, rs, 1, rd, 0, 38), inst_outfile);
            write_binary(encode_Rtype(0, rd, 1, rd, 0, 34), inst_outfile);
        }

        else 
        {
            std::cerr << "Error: unrecognized instruction: " << inst_type << std::endl;
            exit(1);
        }

        current_instruction_number++;
    }

    return 0;
}

#endif 

