/**
 * Ideal Indirection
 * CS 241 - Spring 2020
 */
#include "mmu.h"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// partner: nehap2

mmu *mmu_create() {
    mmu *my_mmu = calloc(1, sizeof(mmu));
    my_mmu->tlb = tlb_create();
    return my_mmu;
}

// 
void* no_switch(mmu *this, addr32 virtual_address, 
    size_t pid, int writing);

void* goto_main_mem(mmu *this, addr32 virtual_address,
    size_t pid, int writing);

page_table_entry* entry_request_frm_kernel(mmu *this, addr32 virtual_address, 
    page_table_entry* entry, vm_segmentation* segment);

void directory_kernel_request(mmu* this, page_directory_entry* entry);


// helper functions
void* goto_main_mem(mmu *this, addr32 virtual_address,
                                   size_t pid, int writing) {
                                       //printf("entering goto\n");
    uint32_t index1 = (virtual_address & 0xffc00000) >> 22; // bytes 22 - 31
    uint32_t index2 = (virtual_address & 0x3ff000) >> 12; // bytes 12 - 21
    uint32_t offset = virtual_address & 0xfff; // bytes 0 - 11

    vm_segmentations* segments = (this->segmentations)[pid];
    vm_segmentation* segment = find_segment(segments, virtual_address);

    page_directory* page_direc = this->page_directories[pid];
    page_directory_entry* page_dir_ent = &(page_direc->entries)[index1];

    int dirPresent = page_dir_ent->present;

    if (dirPresent == 0) { // ask from kernel
        directory_kernel_request(this, page_dir_ent);
    }

    page_table* pageTable = get_system_pointer_from_pde(page_dir_ent);
    page_table_entry* page_tab_entry = &(pageTable->entries)[index2];
    int tablePresent = page_tab_entry->present;

    if (tablePresent == 0) {
        //printf("table entry not present \n");
        page_tab_entry = entry_request_frm_kernel(this, virtual_address, page_tab_entry, segment);
    } else {
        tlb_add_pte(&(this->tlb), virtual_address, page_tab_entry);
    }
    
    // return page_tab_entry
    addr32 base = page_tab_entry->base_addr << 12;
    page_tab_entry->accessed = true;
    if (writing) {
        page_tab_entry->dirty = true;
        if ((segment->permissions & WRITE) == 0) { // no writing permission!
            mmu_raise_segmentation_fault(this);
        }
    }

    void* desired = get_system_pointer_from_address(base+offset);

    return desired;
}

page_table_entry* entry_request_frm_kernel(mmu *this, addr32 virtual_address, 
    page_table_entry* entry, vm_segmentation* segment) {
        //printf("entering entry_request_\n");
    mmu_raise_page_fault(this);
    
    entry->user_supervisor = true;
    entry->present = true;
    addr32 frame_from_kernel = ask_kernel_for_frame(entry);
    entry->base_addr = frame_from_kernel >> 12;
    entry->accessed = true;

    if ((segment->permissions & WRITE) != 0) {
        entry->read_write = true;
    } else {
        entry->read_write = false;
    }
    tlb_add_pte(&(this->tlb), virtual_address, entry);
    read_page_from_disk(entry);

    return entry;
}

void directory_kernel_request(mmu* this, page_directory_entry* entry) {
    //printf("entering directory_kernel_request \n");
    mmu_raise_page_fault(this);

    entry->present = true;
    entry->user_supervisor = true;
    //entry->accessed = true;
    entry->read_write = true;

    addr32 from_kernel = ask_kernel_for_frame(NULL);
    entry->base_addr = from_kernel >> 12;
}

void* no_switch(mmu *this, addr32 virtual_address,
                                   size_t pid, int writing) {
                                       //printf("entering no switch \n");
    vm_segmentations* segments = (this->segmentations)[pid];
    vm_segmentation* segment = find_segment(segments, virtual_address);

    uint32_t offset = virtual_address & 0xfff; // bytes 0 - 11
    uint32_t base_addr = virtual_address & 0xfffff000; // bytes 12 - 31

    tlb* head = this->tlb;
    page_table_entry* tlb_page_entry = tlb_get_pte(&head, base_addr);
    int present_bit = tlb_page_entry->present;

    if (tlb_page_entry!= NULL && present_bit == 0) {
        page_table_entry* a = entry_request_frm_kernel(this, virtual_address, tlb_page_entry, segment);
        
        addr32 entry_base = a->base_addr << 12;
        if (writing) {
            a->dirty = true;
            if ((segment->permissions & WRITE) == 0) { // no writing permission!
                mmu_raise_segmentation_fault(this);
            }
        }
        a->accessed = true;
        void* desired = get_system_pointer_from_address(entry_base+offset);

        return desired;
    } else if (tlb_page_entry!= NULL && present_bit != 0) {
        fprintf(stderr, "shruth 2 \n"); // THIS IS THE ONE THAT PASSES TEST #3!!!
        addr32 entry_base = tlb_page_entry->base_addr << 12;
        tlb_page_entry->accessed = true;
        if (writing) {
            tlb_page_entry->dirty = true;
            if ((segment->permissions & WRITE) == 0) { // no writing permission!
                mmu_raise_segmentation_fault(this);
            }
        }

        void* desired = get_system_pointer_from_address(entry_base+offset);

        return desired;
    }
    else if (tlb_page_entry == NULL) {
        mmu_tlb_miss(this);
        return goto_main_mem(this, virtual_address, pid, writing);
    }
    return NULL;
}

void mmu_read_from_virtual_address(mmu *this, addr32 virtual_address,
                                   size_t pid, void *buffer, size_t num_bytes) {
    assert(this);
    assert(pid < MAX_PROCESS_ID);
    assert(num_bytes + (virtual_address % PAGE_SIZE) <= PAGE_SIZE);

    vm_segmentations* segments = (this->segmentations)[pid];

    // check if address is valid 
    if (!address_in_segmentations(segments, virtual_address)) {// not valid address!
        mmu_raise_segmentation_fault(this);
        return;
    }

    // check for permission with the segmentation
    vm_segmentation* curr_segment = find_segment(segments, virtual_address);
    addr32 seg_permission = curr_segment->permissions;
    if ((seg_permission & READ) == 0) { // no reading permission!
        mmu_raise_segmentation_fault(this);
        return;
    }

    if (pid != this->curr_pid) { // context switch
        tlb_flush(&(this->tlb));
        this->curr_pid = pid;
        mmu_tlb_miss(this);
        void* result = goto_main_mem(this, virtual_address, pid, 0);
        /*if (result == NULL) {
            printf("bitch why1\n");
        }*/
        memcpy(buffer, result, num_bytes);
    } else {
        void* result = no_switch(this, virtual_address, pid, 0);
        /*if (result == NULL) {
            printf("bitch why2\n");
        }*/
        memcpy(buffer, result, num_bytes);
    }

}

void mmu_write_to_virtual_address(mmu *this, addr32 virtual_address, size_t pid,
                                  const void *buffer, size_t num_bytes) {
    assert(this);
    assert(pid < MAX_PROCESS_ID);
    assert(num_bytes + (virtual_address % PAGE_SIZE) <= PAGE_SIZE);

    // check if address is valid 
    vm_segmentations* segments = (this->segmentations)[pid];
    if (!address_in_segmentations(segments, virtual_address)) { // not valid address!
        mmu_raise_segmentation_fault(this);
        return;
    }

    // check for permission with the segmentation
    /*vm_segmentation* curr_segment = find_segment(segments, virtual_address);
    addr32 seg_permission = curr_segment->permissions;
    if ((seg_permission & WRITE) == 0) { // no writing permission!
        mmu_raise_segmentation_fault(this);
        //return;
    }*/
    
    if (pid != this->curr_pid) { // context switch
        tlb_flush(&(this->tlb));
        this->curr_pid = pid;
        mmu_tlb_miss(this);
        void* result = goto_main_mem(this, virtual_address, pid, 1);
        /*if (result == NULL) {
            printf("bitch why3\n");
        }
        printf("exiting write 3\n");*/
        memcpy(result, buffer, num_bytes);
    } else {
        void* result = no_switch(this, virtual_address, pid, 1);
        /*if (result == NULL) {
            printf("bitch why4\n");
        }
        printf("exiting write 4\n");*/
        memcpy(result, buffer, num_bytes);
    }
}

void mmu_tlb_miss(mmu *this) {
    this->num_tlb_misses++;
}

void mmu_raise_page_fault(mmu *this) {
    this->num_page_faults++;
}

void mmu_raise_segmentation_fault(mmu *this) {
    this->num_segmentation_faults++;
}

void mmu_add_process(mmu *this, size_t pid) {
    assert(pid < MAX_PROCESS_ID);
    addr32 page_directory_address = ask_kernel_for_frame(NULL);
    this->page_directories[pid] =
        (page_directory *)get_system_pointer_from_address(
            page_directory_address);
    page_directory *pd = this->page_directories[pid];
    this->segmentations[pid] = calloc(1, sizeof(vm_segmentations));
    vm_segmentations *segmentations = this->segmentations[pid];

    // Note you can see this information in a memory map by using
    // cat /proc/self/maps
    segmentations->segments[STACK] =
        (vm_segmentation){.start = 0xBFFFE000,
                          .end = 0xC07FE000, // 8mb stack
                          .permissions = READ | WRITE,
                          .grows_down = true};

    segmentations->segments[MMAP] =
        (vm_segmentation){.start = 0xC07FE000,
                          .end = 0xC07FE000,
                          // making this writeable to simplify the next lab.
                          // todo make this not writeable by default
                          .permissions = READ | EXEC | WRITE,
                          .grows_down = true};

    segmentations->segments[HEAP] =
        (vm_segmentation){.start = 0x08072000,
                          .end = 0x08072000,
                          .permissions = READ | WRITE,
                          .grows_down = false};

    segmentations->segments[BSS] =
        (vm_segmentation){.start = 0x0805A000,
                          .end = 0x08072000,
                          .permissions = READ | WRITE,
                          .grows_down = false};

    segmentations->segments[DATA] =
        (vm_segmentation){.start = 0x08052000,
                          .end = 0x0805A000,
                          .permissions = READ | WRITE,
                          .grows_down = false};

    segmentations->segments[TEXT] =
        (vm_segmentation){.start = 0x08048000,
                          .end = 0x08052000,
                          .permissions = READ | EXEC,
                          .grows_down = false};

    // creating a few mappings so we have something to play with (made up)
    // this segment is made up for testing purposes
    segmentations->segments[TESTING] =
        (vm_segmentation){.start = PAGE_SIZE,
                          .end = 3 * PAGE_SIZE,
                          .permissions = READ | WRITE,
                          .grows_down = false};
    // first 4 mb is bookkept by the first page directory entry
    page_directory_entry *pde = &(pd->entries[0]);
    // assigning it a page table and some basic permissions
    pde->base_addr = (ask_kernel_for_frame(NULL) >> NUM_OFFSET_BITS);
    pde->present = true;
    pde->read_write = true;
    pde->user_supervisor = true;

    // setting entries 1 and 2 (since each entry points to a 4kb page)
    // of the page table to point to our 8kb of testing memory defined earlier
    for (int i = 1; i < 3; i++) {
        page_table *pt = (page_table *)get_system_pointer_from_pde(pde);
        page_table_entry *pte = &(pt->entries[i]);
        pte->base_addr = (ask_kernel_for_frame(pte) >> NUM_OFFSET_BITS);
        pte->present = true;
        pte->read_write = true;
        pte->user_supervisor = true;
    }
}

void mmu_remove_process(mmu *this, size_t pid) {
    assert(pid < MAX_PROCESS_ID);
    // example of how to BFS through page table tree for those to read code.
    page_directory *pd = this->page_directories[pid];
    if (pd) {
        for (size_t vpn1 = 0; vpn1 < NUM_ENTRIES; vpn1++) {
            page_directory_entry *pde = &(pd->entries[vpn1]);
            if (pde->present) {
                page_table *pt = (page_table *)get_system_pointer_from_pde(pde);
                for (size_t vpn2 = 0; vpn2 < NUM_ENTRIES; vpn2++) {
                    page_table_entry *pte = &(pt->entries[vpn2]);
                    if (pte->present) {
                        void *frame = (void *)get_system_pointer_from_pte(pte);
                        return_frame_to_kernel(frame);
                    }
                    remove_swap_file(pte);
                }
                return_frame_to_kernel(pt);
            }
        }
        return_frame_to_kernel(pd);
    }

    this->page_directories[pid] = NULL;
    free(this->segmentations[pid]);
    this->segmentations[pid] = NULL;

    if (this->curr_pid == pid) {
        tlb_flush(&(this->tlb));
    }
}

void mmu_delete(mmu *this) {
    for (size_t pid = 0; pid < MAX_PROCESS_ID; pid++) {
        mmu_remove_process(this, pid);
    }

    tlb_delete(this->tlb);
    free(this);
    remove_swap_files();
}