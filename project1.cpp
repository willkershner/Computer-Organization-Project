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


void get_static_memory(std::string line, int &static_memory_address, std::vector<int> &static_memory, std::unordered_map<std::string, int> &label_map) {
    // int current_address = 0;
    int start = line.find(".word") + 6;
    std::string label = "";
    for (int i = start; i < line.size(); i++) {
        if (line[i] == ' ') {
            // If we have a label, we need to add it to static memory
            if (label != "") {
                // Check if label is in label_map
                if (label_map.find(label) != label_map.end()) {
                    static_memory.push_back(label_map[label] * 4);
                }
                else {
                    std::cerr << "Error: label " << label << " not found in label map." << std::endl;
                    exit(1);
                }
                label = "";
            }
            continue;
        }
        // check if line[i] is a digit without using cctype
        else if (isdigit(line[i])) {
            int num = std::stoi(line.substr(i));
            static_memory.push_back(num);
        }
        // Check if line[i] is a character that could be part of a label
        else if (isalpha(line[i]) || line[i] == '_') {
            label += line[i];
        }

    }
}

std::unordered_map<std::string, int> match_labels(std::vector<std::string> instructions) {
    std::unordered_map<std::string, int> label_map;
    bool main_found = false;
    int line_number = 0;
    for (int i = 0; i < instructions.size(); i++) {
        std::string inst = instructions[i];

        if (main_found) {
            if (inst.back() == ':') {
                // This is a label
                std::string label = inst.substr(0, inst.size() - 1);
                label_map[label] = line_number; // Map label to line number
            }
            else {
                line_number++; // Only increment line number for actual instructions
            }
        }

        if (inst == "main:") {
            main_found = true;
        }
    }
    return label_map;
}

std::unordered_map<std::string, int> add_data_labels(std::vector<std::string> instructions) {
    std::unordered_map<std::string, int> label_map;
    int static_memory_address = 0;
    bool in_static_memory = false;
    
    for (int i = 0; i < instructions.size(); i++) {
        std::string inst = instructions[i];

        if (inst == ".data") {
            in_static_memory = true;
            continue;
        }

        if (in_static_memory){

        std::string label = inst.substr(0, inst.find(':') - 1);

        int start = inst.find(".word") + 5;
        for (int j = start; j < inst.size(); j++) {
            if (inst[j] == ' ') {
                static_memory_address += 4;
            }
        }

        label_map[label] = static_memory_address;
    }
        
        if (inst == ".text") {
            // At this point, we are done with static memory and can use our functionality for reading labels
            in_static_memory = false;
            continue;
        }
    }

    return label_map;
}

int main(int argc, char* argv[]) {
    if (argc < 4) // Checks that at least 3 arguments are given in command line
    {
        std::cerr << "Expected Usage:\n ./assemble infile1.asm infile2.asm ... infilek.asm staticmem_outfile.bin instructions_outfile.bin\n" << std::endl;
        exit(1);
    }
    //Prepare output files
    std::ofstream inst_outfile, static_outfile;
    static_outfile.open(argv[argc - 2], std::ios::binary);
    inst_outfile.open(argv[argc - 1], std::ios::binary);
    std::vector<std::string> instructions;
    std::vector<int> static_memory;
    /**
     * Phase 1:
     * Read all instructions, clean them of comments and whitespace DONE
     * TODO: Determine the numbers for all static memory labels
     * (measured in bytes starting at 0)
     * TODO: Determine the line numbers of all instruction line labels
     * (measured in instructions) starting at 0
    */

    //For each input file:
    for (int i = 1; i < argc - 2; i++) {
        std::ifstream infile(argv[i]); //  open the input file for reading
        if (!infile) { // if file can't be opened, need to let the user know
            std::cerr << "Error: could not open file: " << argv[i] << std::endl;
            exit(1);
        }

        std::string str;
        int line_number = 0;
        bool main = false;

        while (getline(infile, str)){ //Read a line from the file
            str = clean(str); // remove comments, leading and trailing whitespace
            if (str == "") { //Ignore empty lines
                continue;
            }
            instructions.push_back(str); // TODO This will need to change for labels

            // if (str == "main:"){
            //     main = true;
            // }
            
        }
        infile.close();
    }

    // map of label to line number
    std::unordered_map<std::string, int> label_map =  match_labels(instructions);
    if (DEBUG){
        for (auto const& pair: label_map) {
        std::cerr << "Label: " << pair.first << " Line Number: " << pair.second << std::endl;
        }
    }
    


    int line_number = 0;
    int static_memory_address = 0;
    bool reading_static_memory = false;

    //For each input file:
    for (int i=0; i < instructions.size(); i++) {
       std::string inst = instructions[i];

       if (inst == ".text") {
            // At this point, we are done with static memory and can use our functionality for reading labels
            reading_static_memory = false;
            continue;
        }

        if (reading_static_memory) {
            get_static_memory(inst, static_memory_address, static_memory, label_map);
            // static_memory.push_back(static_memory_address);
            // static_memory_address += 4;
            continue;
        }
        if (inst == ".data") {
            // We are in static memory section
            reading_static_memory = true;
            continue;
        }
    }

    std::unordered_map<std::string, int> data_label_map = add_data_labels(instructions);

    // delete labels from instructions
    std::vector<std::string> cleaned_instructions;
    bool main_found = false;
    for (std::string inst : instructions) {

        if (inst.back() == ':' && main_found) {
            // This is a label, TODO: Handle labels
            continue;
        }
        if (main_found){
            cleaned_instructions.push_back(inst);
        }
        if (inst == "main:") {
                main_found = true;
        }

    /** Phase 2
     * Process all static memory, output to static memory file
     * TODO: All of this
     */
    if (DEBUG){
        for (int i = 0; i < static_memory.size(); i++) {
            std::cerr << "Static Memory Address: " << i*4 << " Value: " << static_memory[i] << std::endl;
        }
    }
    // Write static memory to file
    for (int data : static_memory) {
        write_binary(data, static_outfile);
    }
    instructions = cleaned_instructions;
    }
    



    /** Phase 3
     * Process all instructions, output to instruction memory file
     * TODO: Almost all of this, it only works for adds
     */
    int current_instruction_number = 0;
    for(std::string inst : cleaned_instructions) {
        std::vector<std::string> terms = split(inst, WHITESPACE+",()");
        std::string inst_type = terms[0];
        if (inst_type == "add") {
            // opcode, rs,rt,rd,shamt, func
            int result = encode_Rtype(0,registers[terms[2]], registers[terms[3]], registers[terms[1]], 0, 32);
            write_binary(encode_Rtype(0,registers[terms[2]], registers[terms[3]], registers[terms[1]], 0, 32),inst_outfile);
        }
        else if (inst_type == "sub"){
            int result = encode_Rtype(0,registers[terms[2]], registers[terms[3]], registers[terms[1]], 0, 34);
            write_binary(encode_Rtype(0,registers[terms[2]], registers[terms[3]], registers[terms[1]], 0, 34),inst_outfile);
        }
        else if (inst_type == "slt"){
            int result = encode_Rtype(0,registers[terms[2]], registers[terms[3]], registers[terms[1]], 0, 42);
            write_binary(encode_Rtype(0,registers[terms[2]], registers[terms[3]], registers[terms[1]], 0, 42),inst_outfile);
        }
        else if (inst_type == "addi"){
            int result = encode_Itype(8,registers[terms[2]], registers[terms[1]], std::stoi(terms[3]));
            write_binary(encode_Itype(8,registers[terms[2]], registers[terms[1]], std::stoi(terms[3])),inst_outfile);
        }
        else if (inst_type == "lw"){
            int result = encode_Itype(35,registers[terms[3]], registers[terms[1]], std::stoi(terms[2]));
            write_binary(encode_Itype(35,registers[terms[3]], registers[terms[1]], std::stoi(terms[2])),inst_outfile);
        }
        else if (inst_type == "sw"){
            int result = encode_Itype(43,registers[terms[3]], registers[terms[1]], std::stoi(terms[2]));
            write_binary(encode_Itype(43,registers[terms[3]], registers[terms[1]], std::stoi(terms[2])),inst_outfile);
        }
        else if (inst_type == "beq"){
            std::string label = terms[3];
            int offset = label_map[label] - (current_instruction_number + 1);
            int result = encode_Itype(4,registers[terms[1]], registers[terms[2]], offset);
            write_binary(encode_Itype(4,registers[terms[1]], registers[terms[2]], offset),inst_outfile);
        }
        else if (inst_type == "bne"){
            std::string label = terms[3];
            int offset = label_map[label] - (current_instruction_number + 1);
            int result = encode_Itype(5,registers[terms[1]], registers[terms[2]], offset);
            write_binary(encode_Itype(5,registers[terms[1]], registers[terms[2]], offset),inst_outfile);
        }
        else if (inst_type == "j"){
            std::string label = terms[1];
            int result = encode_Jtype(2,label_map[label]);
            write_binary(encode_Jtype(2,label_map[label]),inst_outfile);
        }
        else if (inst_type == "jal"){
            std::string label = terms[1];
            int result = encode_Jtype(3,label_map[label]);
            write_binary(encode_Jtype(3,label_map[label]),inst_outfile);
        }
        else if (inst_type == "jr"){
            std::string label = terms[1];
            int result = encode_Rtype(0,registers[terms[1]], 0, 0, 0, 8);
            write_binary(encode_Rtype(0,registers[terms[1]], 0, 0, 0, 8),inst_outfile);
        }
        else if(inst_type == "jalr"){
            int result = encode_Rtype(0,registers[terms[2]], 0, registers[terms[1]], 0, 9);
            write_binary(encode_Rtype(0,registers[terms[2]], registers[terms[1]], 0, 0, 9),inst_outfile);
        }
        else if(inst_type == "syscall"){
            int result = encode_Rtype(0,0,0,0,0,12);
            write_binary(encode_Rtype(0,0,0,0,0,12),inst_outfile);
        }
        else if(inst_type == "mult"){
            int result = encode_Rtype(0,registers[terms[1]], registers[terms[2]], 0, 0, 24);
            write_binary(encode_Rtype(0,registers[terms[1]], registers[terms[2]], 0, 0, 24),inst_outfile);
        }
        else if(inst_type == "div"){
            int result = encode_Rtype(0,registers[terms[1]], registers[terms[2]], 0, 0, 26);
            write_binary(encode_Rtype(0,registers[terms[1]], registers[terms[2]], 0, 0, 26),inst_outfile);
        }
        else if(inst_type == "mfhi"){
            int result = encode_Rtype(0,0,0,registers[terms[1]],0,16);
            write_binary(encode_Rtype(0,0,0,registers[terms[1]],0,16),inst_outfile);
        }
        else if(inst_type == "mflo"){
            int result = encode_Rtype(0,0,0,registers[terms[1]],0,18);
            write_binary(encode_Rtype(0,0,0,registers[terms[1]],0,18),inst_outfile);
        }
        else if (inst_type == "sll"){
            int result = encode_Rtype(0,0, registers[terms[2]], registers[terms[1]], std::stoi(terms[3]), 0);
            write_binary(encode_Rtype(0,0, registers[terms[2]], registers[terms[1]], std::stoi(terms[3]), 0),inst_outfile);
        }
        else if (inst_type == "srl"){
            int result = encode_Rtype(0,0, registers[terms[2]], registers[terms[1]], std::stoi(terms[3]), 2);
            write_binary(encode_Rtype(0,0, registers[terms[2]], registers[terms[1]], std::stoi(terms[3]), 2),inst_outfile);
        }
        else if (inst_type == "la"){
            std::string label = terms[1];
            
            int result = encode_Rtype(0,0, registers[terms[2]], registers[terms[1]], std::stoi(terms[3]), 2);
            write_binary(encode_Rtype(0,0, registers[terms[2]], registers[terms[1]], std::stoi(terms[3]), 2),inst_outfile);
        }
        else {
            std::cerr << "Error: unrecognized instruction: " << inst_type << std::endl;
            exit(1);
        }
        current_instruction_number++;
        
}
}
#endif