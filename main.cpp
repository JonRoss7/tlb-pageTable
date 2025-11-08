#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <chrono>
#include <random>
#include <cstdint>
#include <stack>
#include <map>

using namespace std;
using namespace std::chrono;

const int LAB_PART = 4;

const int PAGE_SIZE = 4096;
const int PAGE_OFFSET_BITS = 12;
const int VPN_BITS = 20;
const int L1_BITS = 10, L2_BITS = 10;
const int PD_ENTRIES = 1 << L1_BITS;
const int PT_ENTRIES = 1 << L2_BITS;


const int TLB_SIZE = 32; 

struct PageTableEntry {
    bool valid = false;
    uint32_t frame = 0;
};

struct PageDirectoryEntry {
    bool present = false;
    PageTableEntry* pt = nullptr;
    ~PageDirectoryEntry() { 
        if (present && pt) {
            delete[] pt; 
            pt = nullptr;
        }
    }
};

PageDirectoryEntry page_directory[PD_ENTRIES];

map<uint32_t, PageTableEntry> page_table_part1; 

uint32_t next_free_frame = 0;

const int NUM_FRAMES = 256;
PageTableEntry* frame_to_pte_map[NUM_FRAMES] = {nullptr}; 
uint32_t frame_to_vpn_map[NUM_FRAMES] = {0}; 
stack<uint32_t> free_frames;
static mt19937 rng(random_device{}()); 

struct TLBEntry {
    uint32_t vpn;
    uint32_t pfn;
    uint64_t timestamp = 0;
    bool valid = false;
};

class TLB {
public:
    TLBEntry entries[TLB_SIZE];
    uint64_t clock = 0;
    int hits = 0;
    int misses = 0;

    bool lookup(uint32_t vpn, uint32_t& pfn) {
        if (LAB_PART < 3) return false; 
        
        clock++;
        for (auto& e : entries) {
            if (e.valid && e.vpn == vpn) {
                e.timestamp = clock;
                pfn = e.pfn;
                hits++;
                return true;
            }
        }
        misses++;
        return false;
    }

    void insert(uint32_t vpn, uint32_t pfn) {
        if (LAB_PART < 3) return;
        
        clock++;
        int lru_idx = 0;
        uint64_t oldest = -1;
        
        for(int i = 0; i < TLB_SIZE; i++) {
            if (!entries[i].valid) {
                lru_idx = i;
                goto insert_entry;
            }
            if (entries[i].timestamp < oldest) {
                oldest = entries[i].timestamp;
                lru_idx = i;
            }
        }
        
    insert_entry:
        entries[lru_idx] = {vpn, pfn, clock, true};
    }

    void invalidate(uint32_t vpn_to_invalidate) {
        for (int i = 0; i < TLB_SIZE; i++) {
            if (entries[i].valid && entries[i].vpn == vpn_to_invalidate) {
                entries[i].valid = false;
                return;
            }
        }
    }

    void print_stats() {
        if (LAB_PART < 3) return;
        
        cout << "\n=== TLB Stats ===\n";
        cout << "TLB Hits:   " << hits << ", Misses: " << misses << "\n";
        if (hits + misses > 0) {
            cout << "Hit Rate:   " << fixed << setprecision(2)
                 << (hits * 100.0 / (hits + misses)) << "%\n";
        }
    }
};

TLB tlb;

uint32_t get_L1_index(uint32_t va) { return (va >> 22) & 0x3FF; }
uint32_t get_L2_index(uint32_t va) { return (va >> 12) & 0x3FF; }
uint32_t get_offset(uint32_t va)   { return va & 0xFFF; }
uint32_t get_vpn(uint32_t va)      { return va >> 12; }

bool translate(uint32_t va, uint32_t& pa, bool& fault) {
    uint32_t vpn = get_vpn(va);
    uint32_t offset = get_offset(va);
    uint32_t pfn;

    if (tlb.lookup(vpn, pfn)) {
        pa = (pfn << PAGE_OFFSET_BITS) | offset;
        return true;
    }

    uint32_t l1 = get_L1_index(va);
    uint32_t l2 = get_L2_index(va);

    if (!page_directory[l1].present) {
        page_directory[l1].pt = new PageTableEntry[PT_ENTRIES];
        page_directory[l1].present = true;
    }

    PageTableEntry& pte = page_directory[l1].pt[l2];

    if (!pte.valid) {
        fault = true;

        if (LAB_PART == 4) {
            if (!free_frames.empty()) {
                pfn = free_frames.top();
                free_frames.pop();
            } else {
                uint32_t victim_frame = rng() % NUM_FRAMES;
                pfn = victim_frame;

                PageTableEntry* victim_pte = frame_to_pte_map[victim_frame];
                uint32_t victim_vpn = frame_to_vpn_map[victim_frame];
                
                frame_to_pte_map[victim_frame] = nullptr; 

                if (victim_pte != nullptr) {
                    victim_pte->valid = false;
                }
                
                tlb.invalidate(victim_vpn);

                cout << "PAGE FAULT: VA 0x" << hex << setw(8) << va << " -> evicted frame " << dec << victim_frame << "\n";
            }
            
            frame_to_pte_map[pfn] = &pte;
            frame_to_vpn_map[pfn] = vpn;

        } else {
            pfn = next_free_frame++;
        }
        
        pte.valid = true;
        pte.frame = pfn;

    } else {
        pfn = pte.frame;
    }

    tlb.insert(vpn, pfn);

    pa = (pfn << PAGE_OFFSET_BITS) | offset;
    return true;
}

bool translate_part1(uint32_t va, uint32_t& pa, bool& fault) {
    uint32_t vpn = get_vpn(va);
    uint32_t offset = get_offset(va);
    uint32_t pfn;

    PageTableEntry& pte = page_table_part1[vpn];

    if (!pte.valid) {
        fault = true; 
        pfn = next_free_frame++;
        pte.valid = true;
        pte.frame = pfn;
    } else {
        pfn = pte.frame;
    }

    pa = (pfn << PAGE_OFFSET_BITS) | offset;
    return true;
}

int main() {
    if (LAB_PART == 4) {
        for (int i = 0; i < NUM_FRAMES; i++) {
            free_frames.push(i);
        }
    }

    vector<uint32_t> addresses;
    ifstream fin("addresses.txt");
    uint32_t addr;

    while (fin >> hex >> addr) {
        addresses.push_back(addr);
    }
    fin.close();

    cout << "Starting simulation for Lab Part " << LAB_PART << ": ";
    if (LAB_PART == 1) cout << "Direct-Mapped PT\n\n";
    if (LAB_PART == 2) cout << "2-Level PT\n\n";
    if (LAB_PART == 3) cout << "2-Level PT + TLB\n\n";
    if (LAB_PART == 4) cout << "2-Level PT + TLB + 256 Frames\n\n";

    auto start = high_resolution_clock::now();

    int faults = 0;
    for (uint32_t va : addresses) {
        uint32_t pa;
        bool fault = false;
        bool success;
        
        if (LAB_PART == 1) {
            success = translate_part1(va, pa, fault);
        } else {
            success = translate(va, pa, fault);
        }
        
        if (fault) faults++;

        cout << "VA: 0x" << hex << setw(8) << setfill('0') << va
             << " -> PA: 0x" << setw(8) << setfill('0') << pa << "\n";
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    cout << dec; 

    cout << "\n=== SUMMARY ===\n";
    cout << "Total Addresses:  " << addresses.size() << "\n";
    cout << "Page Faults:      " << faults << "\n"; 
    cout << "Translation Time: " << duration.count() << " \xC2\xB5s\n";
    
    tlb.print_stats();

    return 0;
}