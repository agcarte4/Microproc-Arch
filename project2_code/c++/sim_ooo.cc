#include "sim_ooo.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <iomanip>
#include <map>

using namespace std;

//used for debugging purposes
static const char *stage_names[NUM_STAGES] = {"ISSUE", "EXE", "WR", "COMMIT"};
static const char *instr_names[NUM_OPCODES] = {"LW", "SW", "ADD", "ADDI", "SUB", "SUBI", "XOR", "AND", "MULT", "DIV", "BEQZ", "BNEZ", "BLTZ", "BGTZ", "BLEZ", "BGEZ", "JUMP", "EOP", "LWS", "SWS", "ADDS", "SUBS", "MULTS", "DIVS"};
static const char *res_station_names[5]={"Int", "Add", "Mult", "Load"};

/* =============================================================

   HELPER FUNCTIONS (misc)

   ============================================================= */


/* convert a float into an unsigned */
inline unsigned float2unsigned(float value){
	unsigned result;
	memcpy(&result, &value, sizeof value);
	return result;
}

/* convert an unsigned into a float */
inline float unsigned2float(unsigned value){
	float result;
	memcpy(&result, &value, sizeof value);
	return result;
}

/* convert integer into array of unsigned char - little indian */
inline void unsigned2char(unsigned value, unsigned char *buffer){
        buffer[0] = value & 0xFF;
        buffer[1] = (value >> 8) & 0xFF;
        buffer[2] = (value >> 16) & 0xFF;
        buffer[3] = (value >> 24) & 0xFF;
}

/* convert array of char into integer - little indian */
inline unsigned char2unsigned(unsigned char *buffer){
       return buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
}

/* the following six functions return the kind of the considered opcode */

bool is_branch(opcode_t opcode){
        return (opcode == BEQZ || opcode == BNEZ || opcode == BLTZ || opcode == BLEZ || opcode == BGTZ || opcode == BGEZ || opcode == JUMP);
}

bool is_memory(opcode_t opcode){
        return (opcode == LW || opcode == SW || opcode == LWS || opcode == SWS);
}

bool is_int_r(opcode_t opcode){
        return (opcode == ADD || opcode == SUB || opcode == XOR || opcode == AND);
}

bool is_int_imm(opcode_t opcode){
        return (opcode == ADDI || opcode == SUBI );
}

bool is_int(opcode_t opcode){
        return (is_int_r(opcode) || is_int_imm(opcode));
}

bool is_fp_alu(opcode_t opcode){
        return (opcode == ADDS || opcode == SUBS || opcode == MULTS || opcode == DIVS);
}

/* clears a ROB entry */
void clean_rob(rob_entry_t *entry){
        entry->ready=false;
        entry->pc=UNDEFINED;
        entry->state=ISSUE;
        entry->destination=UNDEFINED;
        entry->value=UNDEFINED;
}

// get free ROB entry - returns UNDEFINED if no free entry else returns ROB index
unsigned sim_ooo::get_free_ROB(){
	int index = UNDEFINED;
	for(unsigned i = 0; i <rob.num_entries; i++){
		if(rob.entries[i].pc == UNDEFINED || rob.entries[i].state == (stage_t) COMMIT){
			index = i;
			return index;
		}
	}
	return index;
}

// get the ROB entry
unsigned sim_ooo::get_ROB(unsigned pc){
	for(int i = 0; i < rob.num_entries; i++){
		if(rob.entries[i].pc == pc) return i;
	}
	return UNDEFINED;
}



/* clears a reservation station */
void clean_res_station(res_station_entry_t *entry){
        entry->pc=UNDEFINED;
        entry->value1=UNDEFINED;
        entry->value2=UNDEFINED;
        entry->tag1=UNDEFINED;
        entry->tag2=UNDEFINED;
        entry->destination=UNDEFINED;
        entry->address=UNDEFINED;
}

// get free RS entry - returns UNDEFINED if no free entry else returns RS index
unsigned sim_ooo::get_free_RS(opcode_t op){
	// {INTEGER_RS, ADD_RS, MULT_RS, LOAD_B} res_station_t;
	res_station_t rs_type;

	switch (op)
	{
		case LW:
		case SW:
			rs_type = LOAD_B;
			break;
		case ADD:
		case ADDI:
		case SUB: 
		case SUBI:
		case XOR:
		case AND: 
		case MULT:
		case DIV:
		case BEQZ: 
		case BNEZ: 
		case BLTZ: 
		case BGTZ: 
		case BLEZ: 
		case BGEZ: 
		case JUMP:
			rs_type = INTEGER_RS;
			break; 
		case LWS:
		case SWS:
			rs_type = LOAD_B;
			break;
		case ADDS: 
		case SUBS:
			rs_type = ADD_RS;
			break;
		case MULTS: 
		case DIVS:
			rs_type = MULT_RS;
			break;
		case EOP:
			return UNDEFINED;
	}

	for (unsigned n=0; n<reservation_stations.num_entries; n++){
		if(	reservation_stations.entries[n].type == rs_type){
			if(reservation_stations.entries[n].pc == UNDEFINED){
				return n;
			}
		}
	}
	return UNDEFINED;
}

// return index of RS with pc
unsigned sim_ooo::get_RS(unsigned pc){
	for(int i = 0; i < reservation_stations.num_entries; i++){
		if(reservation_stations.entries[i].pc == pc)	return i;
	}
	return UNDEFINED;
}

void sim_ooo::RS_add_inst(unsigned pc, unsigned rs, instruction_t inst){
	
	//reservation_stations;
	reservation_stations.entries[rs].pc = pc;

	switch (inst.opcode)
	{
		case LW:
		case SW:
			reservation_stations.entries[rs].address = inst.immediate;
			break;
		case ADD:
		case ADDI:
		case SUB: 
		case SUBI:
		case XOR:
		case AND: 
		case MULT:
		case DIV:
		case BEQZ: 
		case BNEZ: 
		case BLTZ: 
		case BGTZ: 
		case BLEZ: 
		case BGEZ: 
		case JUMP:
			
			break; 
		case LWS:
			reservation_stations.entries[rs].address = inst.immediate;

			break;
		case SWS:
			break;
		case ADDS: 
		case SUBS:
			
			break;
		case MULTS: 
		case DIVS:
			
			break;
		case EOP:
			break;
	}
	return;
}


/* clears an entry if the instruction window */
void clean_instr_window(instr_window_entry_t *entry){
        entry->pc=UNDEFINED;
        entry->issue=UNDEFINED;
        entry->exe=UNDEFINED;
        entry->wr=UNDEFINED;
        entry->commit=UNDEFINED;
}

// push inst to queue 
void sim_ooo::IQ_push(unsigned pc){
	pending_instructions.entries[IQ_tail].pc = pc;
	IQ_tail = (IQ_tail + 1) % pending_instructions.num_entries;	
	return;
}

// pop inst from head of queue
void sim_ooo::IQ_pop(unsigned pc){
	clean_instr_window(&pending_instructions.entries[IQ_find(pc)]);
	IQ_head = (IQ_head + 1) % pending_instructions.num_entries;	
	return;
	
}

// finds instruction in queue - returns UNDEFINED if not found else returns IQ index
unsigned sim_ooo::IQ_find(unsigned pc){
	for(unsigned i = 0; i < pending_instructions.num_entries; i++){
		if(pending_instructions.entries[i].pc == pc){
			return i;
		}
	}
	return UNDEFINED;
	
}

// check if IQ is full
bool sim_ooo::IQ_full(){
	for(unsigned i = 0; i < pending_instructions.num_entries; i++){
		if(pending_instructions.entries[i].pc == UNDEFINED){
			return false;
		}
	}
	return true;
}


/* implements the ALU operation 
   NOTE: this function does not cover LOADS and STORES!
*/
unsigned alu(opcode_t opcode, unsigned value1, unsigned value2, unsigned immediate, unsigned pc){
	unsigned result;
	switch(opcode){
			case ADD:
			case ADDI:
				result = value1+value2;
				break;
			case SUB:
			case SUBI:
				result = value1-value2;
				break;
			case XOR:
				result = value1 ^ value2;
				break;
			case AND:
				result = value1 & value2;
				break;
			case MULT:
				result = value1 * value2;
				break;
			case DIV:
				result = value1 / value2;
				break;
			case ADDS:
				result = float2unsigned(unsigned2float(value1) + unsigned2float(value2));
				break;
			case SUBS:
				result = float2unsigned(unsigned2float(value1) - unsigned2float(value2));
				break;
			case MULTS:
				result = float2unsigned(unsigned2float(value1) * unsigned2float(value2));
				break;
			case DIVS:
				result = float2unsigned(unsigned2float(value1) / unsigned2float(value2));
				break;
			case JUMP:
				result = pc + 4 + immediate;
				break;
			default: //branches
				int reg = (int) value1;
				bool condition = ((opcode == BEQZ && reg==0) ||
				(opcode == BNEZ && reg!=0) ||
  				(opcode == BGEZ && reg>=0) ||
  				(opcode == BLEZ && reg<=0) ||      
  				(opcode == BGTZ && reg>0) ||       
  				(opcode == BLTZ && reg<0));
				if (condition)
	 				result = pc+4+immediate;
				else 
					result = pc+4;
				break;
	}
	return 	result;
}

/* writes the data memory at the specified address */
void sim_ooo::write_memory(unsigned address, unsigned value){
	unsigned2char(value,data_memory+address);
}

/* =============================================================

   Handling of FUNCTIONAL UNITS

   ============================================================= */

/* initializes an execution unit */
void sim_ooo::init_exec_unit(exe_unit_t exec_unit, unsigned latency, unsigned instances){
        for (unsigned i=0; i<instances; i++){
                exec_units[num_units].type = exec_unit;
                exec_units[num_units].latency = latency;
                exec_units[num_units].busy = 0;
                exec_units[num_units].pc = UNDEFINED;
                num_units++;
        }
}

/* returns a free unit for that particular operation or UNDEFINED if no unit is currently available */
unsigned sim_ooo::get_free_unit(opcode_t opcode){
	if (num_units == 0){
		cout << "ERROR:: simulator does not have any execution units!\n";
		exit(-1);
	}
	for (unsigned u=0; u<num_units; u++){
		switch(opcode){
			//Integer unit
			case ADD:
			case ADDI:
			case SUB:
			case SUBI:
			case XOR:
			case AND:
			case BEQZ:
			case BNEZ:
			case BLTZ:
			case BGTZ:
			case BLEZ:
			case BGEZ:
			case JUMP:
				if (exec_units[u].type==INTEGER && exec_units[u].busy==0 && exec_units[u].pc==UNDEFINED) return u;
				break;
			//memory unit
			case LW:
			case SW:
			case LWS: 
			case SWS:
				if (exec_units[u].type==MEMORY && exec_units[u].busy==0 && exec_units[u].pc==UNDEFINED) return u;
				break;
			// FP adder
			case ADDS:
			case SUBS:
				if (exec_units[u].type==ADDER && exec_units[u].busy==0 && exec_units[u].pc==UNDEFINED) return u;
				break;
			// Multiplier
			case MULT:
			case MULTS:
				if (exec_units[u].type==MULTIPLIER && exec_units[u].busy==0 && exec_units[u].pc==UNDEFINED) return u;
				break;
			// Divider
			case DIV:
			case DIVS:
				if (exec_units[u].type==DIVIDER && exec_units[u].busy==0 && exec_units[u].pc==UNDEFINED) return u;
				break;
			default:
				cout << "ERROR:: operations not requiring exec unit!\n";
				exit(-1);
		}
	}
	return UNDEFINED;
}

// get exe unit
unsigned sim_ooo::get_exe_unit(unsigned pc){
	for(unsigned i = 0; i < num_units; i++){
		if(exec_units[i].pc == pc){
			return i;
		}
	}
	return UNDEFINED;
}


/* ============================================================================

   Primitives used to print out the state of each component of the processor:
   	- registers
	- data memory
	- instruction window
        - reservation stations and load buffers
        - (cycle-by-cycle) execution log
	- execution statistics (CPI, # instructions executed, # clock cycles) 

   =========================================================================== */
 

/* prints the content of the data memory */
void sim_ooo::print_memory(unsigned start_address, unsigned end_address){
	cout << "DATA MEMORY[0x" << hex << setw(8) << setfill('0') << start_address << ":0x" << hex << setw(8) << setfill('0') <<  end_address << "]" << endl;
	for (unsigned i=start_address; i<end_address; i++){
		if (i%4 == 0) cout << "0x" << hex << setw(8) << setfill('0') << i << ": "; 
		cout << hex << setw(2) << setfill('0') << int(data_memory[i]) << " ";
		if (i%4 == 3){
			cout << endl;
		}
	} 
}

/* prints the value of the registers */
void sim_ooo::print_registers(){
        unsigned i;
	cout << "GENERAL PURPOSE REGISTERS" << endl;
	cout << setfill(' ') << setw(8) << "Register" << setw(22) << "Value" << setw(5) << "ROB" << endl;
        for (i=0; i< NUM_GP_REGISTERS; i++){
                if (get_int_register_tag(i)!=UNDEFINED) 
			cout << setfill(' ') << setw(7) << "R" << dec << i << setw(22) << "-" << setw(5) << get_int_register_tag(i) << endl;
                else if (get_int_register(i)!=(int)UNDEFINED) 
			cout << setfill(' ') << setw(7) << "R" << dec << i << setw(11) << get_int_register(i) << hex << "/0x" << setw(8) << setfill('0') << get_int_register(i) << setfill(' ') << setw(5) << "-" << endl;
        }
	for (i=0; i< NUM_GP_REGISTERS; i++){
                if (get_fp_register_tag(i)!=UNDEFINED) 
			cout << setfill(' ') << setw(7) << "F" << dec << i << setw(22) << "-" << setw(5) << get_fp_register_tag(i) << endl;
                else if (get_fp_register(i)!=UNDEFINED) 
			cout << setfill(' ') << setw(7) << "F" << dec << i << setw(11) << get_fp_register(i) << hex << "/0x" << setw(8) << setfill('0') << float2unsigned(get_fp_register(i)) << setfill(' ') << setw(5) << "-" << endl;
	}
	cout << endl;
}

/* prints the content of the ROB */
void sim_ooo::print_rob(){
	cout << "REORDER BUFFER" << endl;
	cout << setfill(' ') << setw(5) << "Entry" << setw(6) << "Busy" << setw(7) << "Ready" << setw(12) << "PC" << setw(10) << "State" << setw(6) << "Dest" << setw(12) << "Value" << endl;
	for(unsigned i=0; i< rob.num_entries;i++){
		rob_entry_t entry = rob.entries[i];
		instruction_t instruction;
		if (entry.pc != UNDEFINED) instruction = instr_memory[(entry.pc-instr_base_address)>>2]; 
		cout << setfill(' ');
		cout << setw(5) << i;
		cout << setw(6);
		if (entry.pc==UNDEFINED) cout << "no"; else cout << "yes";
		cout << setw(7);
		if (entry.ready) cout << "yes"; else cout << "no";	
		if (entry.pc!= UNDEFINED ) cout << "  0x" << hex << setfill('0') << setw(8) << entry.pc;
		else	cout << setw(12) << "-";
		cout << setfill(' ') << setw(10);
		if (entry.pc==UNDEFINED) cout << "-";		
		else cout << stage_names[entry.state];
		if (entry.destination==UNDEFINED) cout << setw(6) << "-";
		else{
			if (instruction.opcode == SW || instruction.opcode == SWS)
				cout << setw(6) << dec << entry.destination; 
			else if (entry.destination < NUM_GP_REGISTERS)
				cout << setw(5) << "R" << dec << entry.destination;
			else
				cout << setw(5) << "F" << dec << entry.destination-NUM_GP_REGISTERS;
		}
		if (entry.value!=UNDEFINED) cout << "  0x" << hex << setw(8) << setfill('0') << entry.value << endl;	
		else cout << setw(12) << setfill(' ') << "-" << endl;
	}
	cout << endl;
}

/* prints the content of the reservation stations */
void sim_ooo::print_reservation_stations(){
	cout << "RESERVATION STATIONS" << endl;
	cout  << setfill(' ');
	cout << setw(7) << "Name" << setw(6) << "Busy" << setw(12) << "PC" << setw(12) << "Vj" << setw(12) << "Vk" << setw(6) << "Qj" << setw(6) << "Qk" << setw(6) << "Dest" << setw(12) << "Address" << endl; 
	for(unsigned i=0; i< reservation_stations.num_entries;i++){
		res_station_entry_t entry = reservation_stations.entries[i];
	 	cout  << setfill(' ');
		cout << setw(6); 
		cout << res_station_names[entry.type];
		cout << entry.name + 1;
		cout << setw(6);
		if (entry.pc==UNDEFINED) cout << "no"; else cout << "yes";
		if (entry.pc!= UNDEFINED ) cout << setw(4) << "  0x" << hex << setfill('0') << setw(8) << entry.pc;
		else	cout << setfill(' ') << setw(12) <<  "-";			
		if (entry.value1!= UNDEFINED ) cout << "  0x" << setfill('0') << setw(8) << hex << entry.value1;
		else	cout << setfill(' ') << setw(12) << "-";			
		if (entry.value2!= UNDEFINED ) cout << "  0x" << setfill('0') << setw(8) << hex << entry.value2;
		else	cout << setfill(' ') << setw(12) << "-";			
		cout << setfill(' ');
		cout <<setw(6);
		if (entry.tag1!= UNDEFINED ) cout << dec << entry.tag1;
		else	cout << "-";			
		cout <<setw(6);
		if (entry.tag2!= UNDEFINED ) cout << dec << entry.tag2;
		else	cout << "-";			
		cout <<setw(6);
		if (entry.destination!= UNDEFINED ) cout << dec << entry.destination;
		else	cout << "-";			
		if (entry.address != UNDEFINED ) cout <<setw(4) << "  0x" << setfill('0') << setw(8) << hex << entry.address;
		else	cout << setfill(' ') << setw(12) <<  "-";			
		cout << endl;	
	}
	cout << endl;
}

/* prints the state of the pending instructions */
void sim_ooo::print_pending_instructions(){
	cout << "PENDING INSTRUCTIONS STATUS" << endl;
	cout << setfill(' ');
	cout << setw(10) << "PC" << setw(7) << "Issue" << setw(7) << "Exe" << setw(7) << "WR" << setw(7) << "Commit";
	cout << endl;
	for(unsigned i=0; i< pending_instructions.num_entries;i++){
		instr_window_entry_t entry = pending_instructions.entries[i];
		if (entry.pc!= UNDEFINED ) cout << "0x" << setfill('0') << setw(8) << hex << entry.pc;
		else	cout << setfill(' ') << setw(10)  << "-";
		cout << setfill(' ');
		cout << setw(7);			
		if (entry.issue!= UNDEFINED ) cout << dec << entry.issue;
		else	cout << "-";			
		cout << setw(7);			
		if (entry.exe!= UNDEFINED ) cout << dec << entry.exe;
		else	cout << "-";			
		cout << setw(7);			
		if (entry.wr!= UNDEFINED ) cout << dec << entry.wr;
		else	cout << "-";			
		cout << setw(7);			
		if (entry.commit!= UNDEFINED ) cout << dec << entry.commit;
		else	cout << "-";
		cout << endl;			
	}
	cout << endl;
}


/* initializes the execution log */
void sim_ooo::init_log(){
	log << "EXECUTION LOG" << endl;
	log << setfill(' ');
	log << setw(10) << "PC" << setw(7) << "Issue" << setw(7) << "Exe" << setw(7) << "WR" << setw(7) << "Commit";
	log << endl;
}

/* adds an instruction to the log */
void sim_ooo::commit_to_log(instr_window_entry_t entry){
                if (entry.pc!= UNDEFINED ) log << "0x" << setfill('0') << setw(8) << hex << entry.pc;
                else    log << setfill(' ') << setw(10)  << "-";
                log << setfill(' ');
                log << setw(7);
                if (entry.issue!= UNDEFINED ) log << dec << entry.issue;
                else    log << "-";
                log << setw(7);
                if (entry.exe!= UNDEFINED ) log << dec << entry.exe;
                else    log << "-";
                log << setw(7);
                if (entry.wr!= UNDEFINED ) log << dec << entry.wr;
                else    log << "-";
                log << setw(7);
                if (entry.commit!= UNDEFINED ) log << dec << entry.commit;
                else    log << "-";
                log << endl;
}

/* prints the content of the log */
void sim_ooo::print_log(){
	cout << log.str();
}

/* prints the state of the pending instruction, the content of the ROB, the content of the reservation stations and of the registers */
void sim_ooo::print_status(){
	print_pending_instructions();
	print_rob();
	print_reservation_stations();
	print_registers();
}

/* execution statistics */

float sim_ooo::get_IPC(){return (float)instructions_executed/clock_cycles;}

unsigned sim_ooo::get_instructions_executed(){return instructions_executed;}

unsigned sim_ooo::get_clock_cycles(){return clock_cycles;}



/* ============================================================================

   PARSER

   =========================================================================== */


void sim_ooo::load_program(const char *filename, unsigned base_address){

   /* initializing the base instruction address */
   instr_base_address = base_address;

   /* creating a map with the valid opcodes and with the valid labels */
   map<string, opcode_t> opcodes; //for opcodes
   map<string, unsigned> labels;  //for branches
   for (int i=0; i<NUM_OPCODES; i++)
	 opcodes[string(instr_names[i])]=(opcode_t)i;

   /* opening the assembly file */
   ifstream fin(filename, ios::in | ios::binary);
   if (!fin.is_open()) {
      cerr << "error: open file " << filename << " failed!" << endl;
      exit(-1);
   }

   /* parsing the assembly file line by line */
   string line;
   unsigned instruction_nr = 0;
   while (getline(fin,line)){
	
	// set the instruction field
	char *str = const_cast<char*>(line.c_str());

  	// tokenize the instruction
	char *token = strtok (str," \t");
	map<string, opcode_t>::iterator search = opcodes.find(token);
        if (search == opcodes.end()){
		// this is a label for a branch - extract it and save it in the labels map
		string label = string(token).substr(0, string(token).length() - 1);
		labels[label]=instruction_nr;
		// move to next token, which must be the instruction opcode
		token = strtok (NULL, " \t");
		search = opcodes.find(token);
		if (search == opcodes.end()) cout << "ERROR: invalid opcode: " << token << " !" << endl;
	}

	instr_memory[instruction_nr].opcode = search->second;

	//reading remaining parameters
	char *par1;
	char *par2;
	char *par3;
	switch(instr_memory[instruction_nr].opcode){
		case ADD:
		case SUB:
		case XOR:
		case AND:
		case MULT:
		case DIV:
		case ADDS:
		case SUBS:
		case MULTS:
		case DIVS:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			par3 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "RF"));
			instr_memory[instruction_nr].src1 = atoi(strtok(par2, "RF"));
			instr_memory[instruction_nr].src2 = atoi(strtok(par3, "RF"));
			break;
		case ADDI:
		case SUBI:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			par3 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].src1 = atoi(strtok(par2, "R"));
			instr_memory[instruction_nr].immediate = strtoul (par3, NULL, 0); 
			break;
		case LW:
		case LWS:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "RF"));
			instr_memory[instruction_nr].immediate = strtoul(strtok(par2, "()"), NULL, 0);
			instr_memory[instruction_nr].src1 = atoi(strtok(NULL, "R"));
			break;
		case SW:
		case SWS:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].src1 = atoi(strtok(par1, "RF"));
			instr_memory[instruction_nr].immediate = strtoul(strtok(par2, "()"), NULL, 0);
			instr_memory[instruction_nr].src2 = atoi(strtok(NULL, "R"));
			break;
		case BEQZ:
		case BNEZ:
		case BLTZ:
		case BGTZ:
		case BLEZ:
		case BGEZ:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].src1 = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].label = par2;
			break;
		case JUMP:
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].label = par2;
		default:
			break;

	} 

	/* increment instruction number before moving to next line */
	instruction_nr++;
   }
   //reconstructing the labels of the branch operations
   int i = 0;
   while(true){
   	instruction_t instr = instr_memory[i];
	if (instr.opcode == EOP) break;
	if (instr.opcode == BLTZ || instr.opcode == BNEZ ||
            instr.opcode == BGTZ || instr.opcode == BEQZ ||
            instr.opcode == BGEZ || instr.opcode == BLEZ ||
            instr.opcode == JUMP
	 ){
		instr_memory[i].immediate = (labels[instr.label] - i - 1) << 2;
	}
        i++;
   }

}

/* ============================================================================

   Simulator creation, initialization and deallocation 

   =========================================================================== */

sim_ooo::sim_ooo(unsigned mem_size,
                unsigned rob_size,
                unsigned num_int_res_stations,
                unsigned num_add_res_stations,
                unsigned num_mul_res_stations,
                unsigned num_load_res_stations,
		unsigned max_issue){
	//memory
	data_memory_size = mem_size;
	data_memory = new unsigned char[data_memory_size];

	//issue width
	issue_width = max_issue;

	//rob, instruction window, reservation stations
	rob.num_entries=rob_size;
	pending_instructions.num_entries=rob_size;
	reservation_stations.num_entries= num_int_res_stations+num_load_res_stations+num_add_res_stations+num_mul_res_stations;
	rob.entries = new rob_entry_t[rob_size];
	pending_instructions.entries = new instr_window_entry_t[rob_size];
	reservation_stations.entries = new res_station_entry_t[reservation_stations.num_entries];
	unsigned n=0;
	for (unsigned i=0; i<num_int_res_stations; i++,n++){
		reservation_stations.entries[n].type=INTEGER_RS;
		reservation_stations.entries[n].name=i;
	}
	for (unsigned i=0; i<num_load_res_stations; i++,n++){
		reservation_stations.entries[n].type=LOAD_B;
		reservation_stations.entries[n].name=i;
	}
	for (unsigned i=0; i<num_add_res_stations; i++,n++){
		reservation_stations.entries[n].type=ADD_RS;
		reservation_stations.entries[n].name=i;
	}
	for (unsigned i=0; i<num_mul_res_stations; i++,n++){
		reservation_stations.entries[n].type=MULT_RS;
		reservation_stations.entries[n].name=i;
	}
	//execution units
	num_units = 0;
	reset();
}
	
sim_ooo::~sim_ooo(){
	delete [] data_memory;
	delete [] rob.entries;
	delete [] pending_instructions.entries;
	delete [] reservation_stations.entries;
}

/* =============================================================

   CODE TO BE COMPLETED

	1 32 int reg and 32 fp regs
	2 instruction and data mem are seperated
		- 	configurable mem latency
	3 exe units configurable num & latency
		-	init_exec_unit
		- 	only 1 data unit
	4 Res Stations (3 kinds)
		-	INTEGER for integer units
		-	ADD for fp adders
		-	MULT for for mult & div
	5 Load buffer for mem instructions
	6 Load - load data from mem in exe stage, send result to ROB thru CDB in wr stage
	7 Store - write data to mem in commint stage
	8 Hanlde RAW for load/store that access same address
		-	the value that will be written to memory by the pending store is bypassed to the load
		-	when ld/st is issued, res station filled with value of imm, update add field when inst is in exe
		-	consider insts in program order (moving from issue to exe stage)
				-- Ld inst not allowed to exe if preceded by potentially conflicting str insts
		- 	Str inst are allowed in the exe stage only if the values of both their input registers are available
		-   Str inss update the destination field of the ROB with the effective memory address when they
			enter the execution stage, and they update the value field of the ROB with the result that they will later
			write to memory in the write-result stage.
		-	In the presence of a pending store to the same address, data bypassing from the store to the load instruction
			is done when the load enters the execution stage. In this stage, the load will save the value bypassed in the
			value2 field of its reservation station. In the write-result stage, the load will write this value in the reorder
			buffer.
		- 	Bypassing happens through the ROB
		- 	Store instructions spend only one clock cycle in execution stage, and a number of clock cycles equal to the
		  	memory latency in the commit stage. In the presence of store-to-load data bypassing, load instructions
		  	spend only one clock cycle in the execution stage (since in this case they don’t use the memory unit).
		- 	Note that data bypassing does not modify the timing of store instructions, but it only speeds up the
			processing of dependent load instructions
	9 ROB entries become available in cycle following when an instruction commits
	10 If an instruction I uses an execution unit in the execution stage, such execution unit is freed when I writes the
	 	result and is available to a different instruction starting from the next clock cycle
	11 If an instruction I uses a reservation station or a load buffer, such reservation station or load buffer is freed
	   	when I writes the result, and is available to a different instruction starting from the next clock cycle.
	12 If an instruction J depends on instruction I, the data written by instruction I will be available to J in the
		clock cycle when I writes the result (say t), and (in the absence of data hazard), instruction J will be able to
		execute in the following clock cycle (say t+1)
	13 In the write result stage branch instructions write their target address in the result field of the corresponding
		ROB entry. If the branch is mispredicted, the target instruction of the branch is fetched in the clock cycle
		after the branch instruction commits
	14 Memory address calculation 
		-	Rather than explicitly computing the memory address in the execution stage, assume that address computation is 
			performed before entering that stage. In addition, assume that the address information is used to determine 
			whether the execution stage can be entered by the instruction
	15 The EOP instruction should be excluded from the computation of the CPI

   ============================================================= */

/* core of the simulator */
void sim_ooo::run(unsigned cycles){	
	// temp variable
	unsigned i = 0;

	// temp inst variable
	instruction_t inst;
	
	// temp pc variable
	unsigned pc;

	//  RS pointer
	unsigned rs ;

	// ROB pointer
	unsigned rb;

	// ROB  head pointer
	unsigned h;

	while((i < cycles) || (cycles == (unsigned) NULL)){

		/* 4 - Commit
		*	Commit instruction at head of ROB
			-	If ALU/store and result available in ROB, store result in the real register (for ALU) or 
				memory (for store) and delete ROB entry.
			-	If branch with correct prediction, do nothing and delete ROB entry
			-	If branch with incorrect prediction, flush the ROB and restart with correct target address (do it ASAP)
		*/


		/* 3 - Write Result
			*	Broadcast on CDB the value and ROB tag
			*	Res stations that match tag will grab the value
		*/
		for(unsigned i=0; i<pending_instructions.num_entries; i++){
			if((pending_instructions.entries[i].pc != UNDEFINED)){
				pc = pending_instructions.entries[i].pc;
				inst = instr_memory[((pc - instr_base_address) / (unsigned) 4)];

				unsigned rs_e = get_RS(pc);
				unsigned rb_e = get_ROB(pc);

				rb_e = reservation_stations.entries[rs_e].destination;
				//reservation_stations.entries[rs_e].busy = false;
				for(unsigned x = 0; x < reservation_stations.num_entries; x++){
					if(reservation_stations.entries[x].tag1 == rb_e){
						reservation_stations.entries[x].value1 = rob.entries[rb_e].value;
						reservation_stations.entries[x].tag1 = 0;
					}
					if(reservation_stations.entries[x].tag2 == rb_e){
						reservation_stations.entries[x].value2 = rob.entries[rb_e].value;
						reservation_stations.entries[x].tag2 = 0;		
					}
				}

				rob.entries[rb_e].ready = true;
				pending_instructions.entries[i].wr = clock_cycles;


			/* Handle store executions */	
			}
		}

		
		/* 2 - Execute
		*	If 1 or more operands not ready, watch CDB for broadcast of result
			-	see 12
		* 	When both operands have values and a UNIT, execute
		*/
		// Iterate over pending instructions
		for (unsigned i=0; i<pending_instructions.num_entries; i++){
			// if inst has entered execution, check and update exe unit values
			if(pending_instructions.entries[i].exe != UNDEFINED){
				pc = pending_instructions.entries[i].pc;
				unsigned u = get_exe_unit(pc);

				// check if exe unit is done
				if(exec_units[u].busy == 1){ //1 or 0??
					// move to write back
					pending_instructions.entries[i].wr = clock_cycles;
					rob.entries[i].state = (stage_t) WRITE_RESULT;
				}
				// decrease counter 
				else exec_units[u].latency--;
			}

			// if inst has been issued, check if theres an available exe unit
			else if (pending_instructions.entries[i].issue != UNDEFINED){
				pc = pending_instructions.entries[i].pc;
				inst = instr_memory[((pc - instr_base_address) / (unsigned) 4)];

				unsigned unit = get_free_unit(inst.opcode);
				unsigned rs_e = get_RS(pc);
				unsigned rb_e = get_ROB(pc);

				// if inst is not a memory operation
				if(!is_memory(inst.opcode)){
					// are regs ready?
					if((reservation_stations.entries[rs_e].tag1 == 0) && (reservation_stations.entries[rs_e].tag1 == 0)){
						rob.entries[rb_e].value = alu(inst.opcode,
						 							reservation_stations.entries[rs_e].value1,
													reservation_stations.entries[rs_e].value2,
													inst.immediate,
													pc );
					}
				}
				// if inst is a load 
				if(is_memory(inst.opcode) && ((inst.opcode == LW) || (inst.opcode == LWS))){
					if(reservation_stations.entries[rs_e].tag1 == 0){
						reservation_stations.entries[rs_e].address = reservation_stations.entries[rs_e].tag1 + reservation_stations.entries[rs_e].address;
						// load the data from mem
						/* code */
					}
				}

			} 
		}


		/*	0 - Queue Instructions
			*	Add instructions to queue
			each cycle add new instruction to the queue. if queue is full, dont add anything until there is space		
		*/
		if(!IQ_full()){ // check that the instruction window isnt full
			IQ_push(IQ_pc);

			inst = instr_memory[((IQ_pc - instr_base_address) / (unsigned) 4)];

			if(instr_memory[((IQ_pc - instr_base_address) / (unsigned) 4)].opcode == EOP)
				return;
			
			IQ_pc += 4;	//increment IQ_PC 
		}

		/* 1 - Issue 
				* 	Check for structural hazards (no free res station, no free load-store buffer, ROB full)	
					- 	Stall
				* 	If possible, send to res station
					-	send values if available or ROB tag
				*	Allocate entry in ROB and send number to res station 
		*/ 
		for(unsigned i = 0; i < pending_instructions.num_entries; i++){
			if((pending_instructions.entries[i].pc != UNDEFINED) && (pending_instructions.entries[i].issue == UNDEFINED)){
				//read inst at front of queue
				unsigned pc = pending_instructions.entries[i].pc;
				inst = instr_memory[((pc - instr_base_address) / (unsigned) 4)];

				// check for free RS entry
				unsigned rs = get_free_RS(inst.opcode);

				// check for free ROB entry
				unsigned rb = get_free_ROB();
				
				// Check for Structural Hazards
				if(rs != UNDEFINED && rb != UNDEFINED){
					// set ISSUE clk
					pending_instructions.entries[IQ_head].issue = clock_cycles;

					// FIGURE out how to deal with the instruction queue
					//IQ_head = (IQ_head + 1) % pending_instructions.num_entries;

					unsigned type = get_reg_type(inst.opcode);
					/* issue all instructions */
					// for fp regs
					if(type == 1 ){
						if(fp_regs_stat[inst.src1].busy){ /* in-flight inst writes rs */
							h = fp_regs_stat[inst.src1].reorder;
							if(rob.entries[h].ready){ /* Inst completed already */
								reservation_stations.entries[rs].value1 = rob.entries[h].value;
								reservation_stations.entries[rs].tag1 = 0;
							}
							else{ /* wait for instruction */
								//reservation_stations.entries[rs].tag1 = h;
							}
						}
						else{	/* */
							reservation_stations.entries[rs].value1 = get_fp_register(inst.src1);
							//reservation_stations.entries[rs].tag1 = 0;
						}
						//FIX ME -> reservation_stations.entries[rs].busy = true;
						reservation_stations.entries[rs].destination = rb;
						reservation_stations.entries[rs].pc = pc;
						rob.entries[rb].pc = pc;
						rob.entries[rb].destination = inst.dest;
						rob.entries[rb].ready = false;
						rob.entries[rb].state = (stage_t) ISSUE;
					}
					// for int regs
					else{
						if(int_regs_stat[inst.src1].busy){ /* in-flight inst writes rs */
							h <- int_regs_stat[inst.src1].reorder;
							if(rob.entries[h].ready){ /* Inst completed already */
								reservation_stations.entries[rs].value1 = rob.entries[h].value;
								reservation_stations.entries[rs].tag1 = 0;
							}
							else{ /* wait for instruction */
								reservation_stations.entries[rs].tag1 = h;
							}
						}
						else{	/* */
							reservation_stations.entries[rs].value1 = get_int_register(inst.src1);
							reservation_stations.entries[rs].tag1 = 0;
						}
						//FIX ME -> reservation_stations.entries[rs].busy = true;
						reservation_stations.entries[rs].destination = rb;
						reservation_stations.entries[rs].pc = pc;
						rob.entries[rb].pc = pc;
						rob.entries[rb].destination = inst.dest;
						rob.entries[rb].ready = false;
						rob.entries[rb].state = (stage_t) ISSUE;
					}

					/* FP operations and stores */
					if(is_fp_alu(inst.opcode)){
						if(fp_regs_stat[inst.src2].busy){ /* in-flight inst writes rt */
							h = fp_regs_stat[inst.src2].reorder;
							if(rob.entries[h].ready){ /* inst completed already */
								reservation_stations.entries[rs].value2 = rob.entries[h].value;
								reservation_stations.entries[rs].tag2 = 0;
							}
							else { /* wait for instruction */
								reservation_stations.entries[rs].tag2 = h;
							}
						}
						else {	
							reservation_stations.entries[rs].value2 = get_fp_register(inst.src2);
							reservation_stations.entries[rs].tag2 = 0;
						}
					}

					/* FP operations */
					if(get_reg_type(inst.opcode) && (inst.opcode != (opcode_t) LWS ) && (inst.opcode != (opcode_t) SWS )){
						fp_regs_stat[inst.dest].reorder = rb;
						fp_regs_stat[inst.dest].busy = true;
						rob.entries[rb].destination = inst.dest + NUM_GP_REGISTERS;
					}

					/* Loads */
					if(inst.opcode == (opcode_t) LW){
						reservation_stations.entries[rs].address = inst.immediate;
						int_regs_stat[inst.dest].reorder = rb;
						int_regs_stat[inst.dest].busy = true;
						rob.entries[rb].destination = inst.dest;
					} else if (inst.opcode == (opcode_t) LWS){
						reservation_stations.entries[rs].address = inst.immediate;
						fp_regs_stat[inst.dest].reorder = rb;
						fp_regs_stat[inst.dest].busy = true;
						rob.entries[rb].destination = inst.dest + NUM_GP_REGISTERS;
					}

					/* Stores */
					if((inst.opcode == (opcode_t) SW) || (inst.opcode == (opcode_t) SWS)){
						reservation_stations.entries[rs].address = inst.immediate;
					}
				}
			}
		}
		
		// increment counters
		i += 1;
		clock_cycles += 1;
	}

}

//reset the state of the simulator - please complete
void sim_ooo::reset(){

	//init instruction log
	init_log();	

	// data memory
	for (unsigned i=0; i<data_memory_size; i++) data_memory[i]=0xFF;
	
	//instr memory
	for (int i=0; i<PROGRAM_SIZE;i++){
		instr_memory[i].opcode=(opcode_t)EOP;
		instr_memory[i].src1=UNDEFINED;
		instr_memory[i].src2=UNDEFINED;
		instr_memory[i].dest=UNDEFINED;
		instr_memory[i].immediate=UNDEFINED;
	}

	//general purpose registers
	for(int i = 0; i < NUM_GP_REGISTERS; i++){
		int_regs[i] = UNDEFINED;
		int_regs_tags[i] = UNDEFINED;
		int_regs_stat[i] = {UNDEFINED, false};
		fp_regs_stat[i] = {UNDEFINED, false};
		fp_regs[i] = UNDEFINED;
		fp_regs_tags[i] = UNDEFINED;
	}

	//pending_instructions
	for(unsigned i=0; i< pending_instructions.num_entries; i++){
		pending_instructions.entries[i].pc = UNDEFINED;
		pending_instructions.entries[i].issue = UNDEFINED;
		pending_instructions.entries[i].exe = UNDEFINED;
		pending_instructions.entries[i].wr = UNDEFINED;
		pending_instructions.entries[i].commit = UNDEFINED;
	}

	//rob
	for(unsigned i=0; i< rob.num_entries; i++){
		rob.entries[i].ready = false;
		rob.entries[i].pc = UNDEFINED;
		rob.entries[i].state = (stage_t)ISSUE;
		rob.entries[i].destination = UNDEFINED;
		rob.entries[i].value = UNDEFINED;
	}

	//reservation_stations
	for(unsigned i=0; i< reservation_stations.num_entries; i++){
		reservation_stations.entries[i].pc = UNDEFINED;
		reservation_stations.entries[i].value1 = UNDEFINED;
		reservation_stations.entries[i].value2 = UNDEFINED;
		reservation_stations.entries[i].tag1 = UNDEFINED;
		reservation_stations.entries[i].tag2 = UNDEFINED;
		reservation_stations.entries[i].destination = UNDEFINED;
		reservation_stations.entries[i].address = UNDEFINED;
	}

	//execution statistics
	clock_cycles = 0;
	instructions_executed = 0;

	//other required initializations
	inst_ctr = 0;
	rob_head = 0;

	IQ_head = 0;
	IQ_tail = 0;
	IQ_pc = instr_base_address;

}

/* Additional functions */

/* registers related */

char sim_ooo::get_reg_type(opcode_t op){
	switch (op)
	{
		case LW:
		case SW:
		case ADD:
		case ADDI:
		case SUB: 
		case SUBI:
		case XOR:
		case AND: 
		case MULT:
		case DIV:
		case BEQZ: 
		case BNEZ: 
		case BLTZ: 
		case BGTZ: 
		case BLEZ: 
		case BGEZ: 
		case JUMP:
			return 0; 
		case LWS:
		case SWS:
		case ADDS: 
		case SUBS:
		case MULTS: 
		case DIVS:
			return 1;
		case EOP:
			return 2;
	}
}

char sim_ooo::check_op_FPstr(opcode_t op){
	switch (op)
	{
		case LW:
			return 0;
		case SW:
			return 1;
		case ADD:
		case ADDI:
		case SUB: 
		case SUBI:
		case XOR:
		case AND: 
		case MULT:
		case DIV:
		case BEQZ: 
		case BNEZ: 
		case BLTZ: 
		case BGTZ: 
		case BLEZ: 
		case BGEZ: 
		case JUMP:
		case LWS:
			return 0; 
		case SWS:
		case ADDS: 
		case SUBS:
		case MULTS: 
		case DIVS:
			return 1;
		case EOP:
			return 2;
	}
}


int sim_ooo::get_int_register(unsigned reg){
	return int_regs[reg]; 
}

void sim_ooo::set_int_register(unsigned reg, int value){
	int_regs[reg] = value;
}

float sim_ooo::get_fp_register(unsigned reg){
	return fp_regs[reg]; 
}

void sim_ooo::set_fp_register(unsigned reg, float value){
	fp_regs[reg] = value;
}

unsigned sim_ooo::get_int_register_tag(unsigned reg){
	return int_regs_stat[reg].reorder;
}

unsigned sim_ooo::get_fp_register_tag(unsigned reg){
	return fp_regs_stat[reg].reorder;
}


