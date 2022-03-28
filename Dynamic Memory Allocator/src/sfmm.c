/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include <errno.h>


static double aggregatePay;


int power ( int base, int exp){
    int result = 1;

    for(int i = 0; i < exp; i ++){
        result*= base;
    }
    return result;
}


//-------------According to this, sf_quick_lists[0] contains blocks of size M (32 bytes). And sf_quick_lists[1] contains blocks of size M+ALIGHN_SIZE (32 + 16 = 48 bytes). And so on.



int checkAllocBit(sf_header temp){
    temp = temp ^MAGIC;
    temp = temp << 61; //remvoe payload and block sz and unused bit
    temp = temp >> 63;
    return temp;
}

int checkPrevAllocBit(sf_header temp){
    temp = temp ^MAGIC;    
    temp = temp << 62; //remvoe payload and block sz and unused bit
    temp = temp >> 63;
    return temp;
}

int getInQuickList(sf_header temp){
    temp = temp ^MAGIC;  
    temp = temp << 63;
    temp = temp >> 63;
    return temp;
}

sf_size_t getPayloadSz(sf_header temp){
    temp = temp ^MAGIC;
    temp = temp >> 32;
    return (sf_size_t) temp;
}
// }
sf_size_t getHeaderBlockSize(sf_header temp){
    temp = temp ^MAGIC;

    temp = temp << 32; //remove the payload size
    temp = temp >> 36; //move back all the wya and remove the last 4 bits to only get the 28 bits of block size
    temp = temp << 4; // move back to original position
    return (sf_size_t)temp ;
}
/**
 * @brief combines the bits show below 
 * 
 * @param a payload size
 * @param b b is actually going to be a 28 bit, instead we will use a 32 and then push it back and forth
 * @param alloc allocated or not. 1 if alloc, 0 if not
 * @param prevAlloc previous block allocated or not. 1 if alloc, 0 if not
 * @param inQuick in quicklist OR not. 1 if in, 0 if not
 * @return sf_header this is the final result block.
 */
sf_header combineBits(sf_size_t a, sf_size_t b, int alloc, int prevAlloc, int inQuick){
    sf_header result = ((sf_header)a) << 32; //a shifted 32 bits is 64
    result |= b;
    if(alloc == 1){
        result |= THIS_BLOCK_ALLOCATED;
    }

    if(prevAlloc == 1){
        result |= (PREV_BLOCK_ALLOCATED);
    }

    if(inQuick == 1){
        result |= (IN_QUICK_LIST);
        sf_header resultDup = result;
        if(!(resultDup & (1<<2))){ //cheks the 3rd bit, if its not set then we need to set it
            result |= THIS_BLOCK_ALLOCATED;
        }
    }
    return result^ MAGIC;
}


sf_block* prevBlock (sf_block* block){
    sf_size_t sizePrev = getHeaderBlockSize(block->prev_footer); 
    void* temp  = (void*)block;
    temp -= sizePrev;
    sf_block* prevBlock = (sf_block*) temp;
    return prevBlock;
}

sf_block* nextBlock(sf_block* block){
    sf_size_t size = getHeaderBlockSize(block->header);
    void* temp = (void*)block;
    temp += size;
    sf_block* footer = (sf_block*) temp;
    return footer;
}
//returns address.
void* writeFooter(sf_block* block){
    sf_block* footer = nextBlock(block);
    footer->prev_footer = block->header;

    return footer;
}

sf_header changeHeaderSize(sf_header header, sf_size_t newSize){
    int allocBit = checkAllocBit(header);
    int preAlloc = checkPrevAllocBit(header);
    int quickLst = getInQuickList(header);
    sf_size_t payload = getPayloadSz(header);

    sf_header result  = combineBits(payload, newSize, allocBit, preAlloc, quickLst);
    return result;       
}


sf_header changePrevAllocHEader(sf_header header, int newPrevAlloc){
    int allocBit = checkAllocBit(header);
    int quickLst = getInQuickList(header);
    sf_size_t size = getHeaderBlockSize(header);
    sf_size_t payload = getPayloadSz(header);

    sf_header result  = combineBits(payload, size, allocBit, newPrevAlloc, quickLst);
    return result;       
}
sf_header changeAllocHeader(sf_header header, int newAlloc){
    int preAlloc = checkPrevAllocBit(header);
    int quickLst = getInQuickList(header);
    sf_size_t size = getHeaderBlockSize(header);
    sf_size_t payload = getPayloadSz(header);

    sf_header result  = combineBits(payload, size, newAlloc, preAlloc, quickLst);
    return result;       
}

sf_header changeQckLstHeader(sf_header header, int newQck){
    int preAlloc = checkPrevAllocBit(header);
    int allocBit = checkAllocBit(header);
    sf_size_t size = getHeaderBlockSize(header);
    sf_size_t payload = getPayloadSz(header);

    sf_header result  = combineBits(payload, size, allocBit, preAlloc, newQck);
    return result;       
}

sf_header changePayloadHeader(sf_header header, sf_size_t newPaylod){
    int preAlloc = checkPrevAllocBit(header);
    int allocBit = checkAllocBit(header);
    sf_size_t size = getHeaderBlockSize(header);
 int quickLst = getInQuickList(header);

    sf_header result  = combineBits(newPaylod, size, allocBit, preAlloc, quickLst);
    return result;       
}



//input must be 16 bit aligned and > 32
void putInFreeList(sf_size_t size, sf_block* block){
    double tempSize = size/32;
    for(int i =0; i < NUM_FREE_LISTS; i++){
        if(power(2, i) >= tempSize){

            sf_block* temp = sf_free_list_heads[i].body.links.next;
            block->header = changeAllocHeader(block->header, 0);
            block->header = changeQckLstHeader(block->header, 0);
            writeFooter(block);
            
            sf_free_list_heads[i].body.links.next = block; //set previous to block
            block->body.links.next = temp;
            block->body.links.prev = &sf_free_list_heads[i];
            temp->body.links.prev = block;
            //  coalesce(block);
            break;
        }else if(i + 1 == NUM_FREE_LISTS){
            sf_block* temp = sf_free_list_heads[i].body.links.next;
            block->header = changeAllocHeader(block->header, 0);
            block->header = changeQckLstHeader(block->header, 0);
                     writeFooter(block);
            sf_free_list_heads[i].body.links.next = block; //set previous to block
            block->body.links.next = temp;
            block->body.links.prev = &sf_free_list_heads[i];
            temp->body.links.prev = block;
   
            // coalesce(block);
            break;
        }
    }
//ok what how do we maintian this doubly linked list do we shift everything LIFO
}


void takeOutOfFree(sf_block* block){
    sf_block* nextB = block->body.links.next;
    sf_block* prevB = block->body.links.prev;
    if(nextB != NULL && prevB != NULL){
        prevB->body.links.next = nextB;
        nextB->body.links.prev = prevB;

        block->body.links.next = NULL;
        block->body.links.prev = NULL;
    }
   
}



//Coalsce Method: return a 1 or 0, 1 if the coalsce was successful, 0 if not because of an allocated block being coalsce

//first check must be for the block immediately beofre it. (switch case kinda)

sf_block* findQuick(sf_size_t size){
    for(int i = 0; i < NUM_QUICK_LISTS; i++){
        sf_size_t x = 32+(i)*(16);
        if(size == x){
            if(sf_quick_lists[(x-32)/16].length > 0){
                sf_quick_lists[(x-32)/16].first = sf_quick_lists[(x-32)/16].first->body.links.next;
                sf_quick_lists[(x-32)/16].length = sf_quick_lists[(x-32)/16].length-1;
                return sf_quick_lists[(x-32)/16].first;
            }
        }
    }
    return NULL;
}

//if return NULL, not block found.
sf_block* findFree(sf_size_t size){
    double tempSize = size/32;
    for(int i =0; i < NUM_FREE_LISTS; i++){
        if(power(2, i) >= tempSize){
            sf_block* currentB = sf_free_list_heads[i].body.links.next;
            if(currentB != &sf_free_list_heads[i]){

                if(getHeaderBlockSize(currentB->header) >= size){
                    return currentB;
                }else{
                    while((getHeaderBlockSize(currentB->header) < size)
                    && (currentB != &sf_free_list_heads[i])){
                        currentB = currentB->body.links.next;
                    }
                    if((getHeaderBlockSize(currentB->header)) >= size
                    && (currentB != &sf_free_list_heads[i])){
                        return currentB;
                    }else{
                        return NULL;
                    }
                }
            }
        }else if(i + 1 == NUM_FREE_LISTS){
            sf_block* currentB = sf_free_list_heads[i].body.links.next;
            if(currentB != &sf_free_list_heads[i]){
   
                if(getHeaderBlockSize(currentB->header) >= size){
                    return currentB;
                }else{
                    while((getHeaderBlockSize(currentB->header) < size)
                    && (currentB != &sf_free_list_heads[i])){
                        currentB = currentB->body.links.next;
                    }
                    if((getHeaderBlockSize(currentB->header)) >= size
                    && (currentB != &sf_free_list_heads[i])){
                        return currentB;
                    }else{
                        return NULL;
                    }
                }
            }
        }
    }
    return NULL;
}


sf_block* coalesce(sf_block* block){
    int preAlloc = checkPrevAllocBit(block->header);
    int alloc = checkAllocBit(block->header);
    int inQck = getInQuickList(block->header);

    sf_footer prevFooter = block->prev_footer;
    sf_size_t sizePrev = getHeaderBlockSize(prevFooter); 
    sf_size_t sizeCurrent = getHeaderBlockSize(block->header);
    sf_size_t sum = sizePrev + sizeCurrent;

    if(inQck == 1){
        alloc = 0;
    }
    if(alloc == 1){
        return NULL;
    }
    if(preAlloc == 0 ){
        sf_block* prev = prevBlock(block);
        if(inQck == 0){
            takeOutOfFree(prev);
            takeOutOfFree(block);
        }


        sf_header newHeader= changeHeaderSize(prev->header, sum); 
        prev->header = newHeader;
        writeFooter(prev);
        block = prev;
        putInFreeList(getHeaderBlockSize(block->header), block);
    }
    
    sf_block* nextB = nextBlock(block);
    if((void*) nextB > sf_mem_end() ||(void*) nextB < sf_mem_start()){
        return block;
    }
    int nextAlloc = checkAllocBit(nextB->header);
    if(nextAlloc == 0){
        if(inQck==0){
            takeOutOfFree(nextB);
            takeOutOfFree(block);
        }
        
        sf_size_t sizeNext = getHeaderBlockSize(nextB->header); 
        sf_size_t sizeCurrent = getHeaderBlockSize(block->header);
        sf_size_t sum2 = sizeNext + sizeCurrent;

        sf_header newHead = changeHeaderSize(block->header, sum2);
        block->header = newHead;
        // sf_block* nextNext = nextBlock(nextB);
        // nextNext->prev_footer = block->header;

        writeFooter(block);
        putInFreeList(getHeaderBlockSize(block->header), block);
    }
    // sf_show_heap();
    return block;
} 

int splitBlock(sf_block* block, sf_size_t blockSize,  sf_size_t payload){
    sf_size_t size = getHeaderBlockSize(block->header);
    sf_size_t newSz = size - blockSize;
    sf_header newBig = changeHeaderSize(block->header, newSz);
    newBig = changePrevAllocHEader(newBig, 1);
    int preAllocBit = checkPrevAllocBit(newBig);
    block->header = combineBits(payload, blockSize, 1, preAllocBit, 0);
    sf_block* nextB = nextBlock(block);
    nextB->header = newBig;
    writeFooter(nextB);

    putInFreeList(newSz,nextB);
    coalesce(nextB);
    return 0;
}
void removeFromQuick(sf_block* first, sf_size_t x){
    sf_quick_lists[x].first = first->body.links.next;
}

void flushQuick(sf_size_t x){
    sf_block* temp = sf_quick_lists[x].first;
    removeFromQuick(temp, x);
    
    for(int i = 0; i < QUICK_LIST_MAX; i ++){
        // sf_show_heap();
        sf_block* block = coalesce(temp);
        if(block==NULL){
            break;
        }
        // sf_show_heap();
        putInFreeList(getHeaderBlockSize(block->header),block);
        sf_block* temp = sf_quick_lists[x].first;
        removeFromQuick(temp, x);
     
                // sf_show_heap(); 
    }
}

int putInQuick(sf_size_t size, sf_block* block){
    for(int i = 0; i < NUM_QUICK_LISTS; i++){
        sf_size_t x = 32+(i)*(16);
        if(size == x){
            if(sf_quick_lists[(x-32)/16].length < QUICK_LIST_MAX){
                sf_block* temp =sf_quick_lists[(x-32)/16].first;
                block->header = changeQckLstHeader(block->header, 1);
                if(temp == NULL){
                    sf_quick_lists[(x-32)/16].first = block;
                    sf_quick_lists[(x-32)/16].length = sf_quick_lists[(x-32)/16].length+1;
                    
                    return 1;
                }else{
                    sf_quick_lists[(x-32)/16].first = block;
                    sf_quick_lists[(x-32)/16].length = sf_quick_lists[(x-32)/16].length+1;
                    block->body.links.next = temp;
                    temp->body.links.prev = block;
                   
                    return 1;
                }
                
            }else if(sf_quick_lists[(x-32)/16].length >= QUICK_LIST_MAX){
                // sf_show_quick_lists();
                flushQuick((x-32)/16);
                sf_quick_lists[(x-32)/16].first = block;
                sf_quick_lists[(x-32)/16].length = sf_quick_lists[(x-32)/16].length+1;
                return 1;
            }
        }
    }
    return 0; //didnt find a place to put it.
}

// sf_block* createBlock(sf_block* input, sf_footer prev_footer, sf_header header){
//     sf_block* result;
//     return result;
//     //if we have a value in sfblock then t pld:        0, al: 0, pal: 0][prev:(nil), next:(nil)]***ZERO SIZE BLOCK***
// }
// This function initializes the heap with alignment padding, prologue header + footer, epilogue header

void sf_init(void){

    // next up is the header prologue 
   // WRITE(sf_mem_start()+sizeof(sf_header), combineBits(0, 0, 1, 0, 0)); //allocate 64 bits
    for (int i = 0 ; i< NUM_FREE_LISTS; i++){
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }
    
    sf_block *prologue = (sf_block *)sf_mem_start();
    sf_size_t blockSz = 32;
    prologue->header = combineBits(0, blockSz, 1, 0, 0) ;

    sf_block* firstBlock = (sf_block *)(sf_mem_start()+32);
    sf_size_t remainSpace = PAGE_SZ - 32 - (2*8);
    firstBlock->header = combineBits(0, remainSpace, 0, 1, 0); //not allocated.
    writeFooter(firstBlock);
    //page size - blocksize of prologue - size  of epilogue
    

    putInFreeList(remainSpace, firstBlock);
    

    sf_block *epilogue = (sf_block *)(sf_mem_end()-16);
    epilogue->header = combineBits(0, 0, 1, 0, 0);

    
    // sf_show_block(epilogue);
    //rememeber to xor each head + fosoterd
    //sizeof(sf_header)
}

//If the allocation is not successful,
// then NULL is returned and sf_errno is set to ENOMEM.

void *sf_malloc(sf_size_t size) {
    void* result;
    sf_size_t padded = 0;
    padded += 1;
    sf_size_t tempSize = size;


    if(size == 0){
        return NULL; //Return n
    }
    size += 8; //includes the header.
    //------------determind how much padding is needed
    if(size < 32){
        padded = 32;

    }else if(size == 32){
        padded = 32;
 
    }else{
        if(size %16 != 0){
            int divided = size/16;
            divided++;
            padded = divided*16;
            //internal frag padded - size 
            // tempSize = padded-8; //subtract the header for the payload.
        }else{
            padded = size;
        }
    }
    //------------determind how much padding is needed

    if((char *)sf_mem_start() == (char *)sf_mem_end()){
       if((result = sf_mem_grow()) != NULL){
            sf_init(); //initializes the heap
       }
    }


    sf_block* x = NULL;
    while((x = findFree(padded)) == NULL){
        sf_block *oldEpilogue = (sf_block *)(sf_mem_end()-16);

        if((result = sf_mem_grow()) != NULL){
            sf_block *epilogue = (sf_block *)(sf_mem_end()-16);
            epilogue->header = combineBits(0, 0, 1, 0, 0);

            //set the new epilogue to free so we can coalsce so when we coalsce we dont include it.

            oldEpilogue->header = combineBits(0,PAGE_SZ,0,checkPrevAllocBit(oldEpilogue->header), 0);

            writeFooter(oldEpilogue);
            putInFreeList(getHeaderBlockSize(oldEpilogue->header),oldEpilogue);
            // sf_show_free_lists();
            
            coalesce(oldEpilogue);
            // sf_show_free_lists();
//if the prvious block is allocated or not.cuase we wnat to chane the header of old prologue its now a free block
//with additional pading below 
        }else{
            sf_errno = ENOMEM;
            return NULL;
        }
        
    }
//okay so we get the epilogue wdont we need to collasec it with somehting else
//so if we change the current epilogue to thge header thats not allocated as its free

    sf_block* perfect= findQuick(padded);
    if(perfect == NULL){
        // sf_show_free_lists();
        perfect = findFree(padded);
        // sf_show_heap();

        takeOutOfFree(perfect);
        sf_size_t perfectSz = getHeaderBlockSize(perfect->header);
        if(perfectSz > padded){
            if(perfectSz - padded >= 32){
                splitBlock(perfect, padded, tempSize);
                            // sf_show_heap();
                aggregatePay += getPayloadSz(perfect->header);
                return (void*) perfect+16;

                //CHANGE BOTH THE HEADER TO INCLUDE THE NEW PAYLOAD, SIZE, AND ALLOCATED
                //THen we changet he header of the next to prealloc to now is allocated!
            }else if(perfectSz - padded < 32){ //there is a splinter but we still gotta change the footer to allocated now
                perfect->header = combineBits(tempSize,perfectSz, 1,checkPrevAllocBit(perfect->header) ,0);
                nextBlock(perfect)->header = changePrevAllocHEader(nextBlock(perfect)->header, 1);
                aggregatePay += getPayloadSz(perfect->header);
                return (void*) perfect+16;
            }       
        }else if(perfectSz == padded){
            perfect->header = combineBits(tempSize,perfectSz, 1,checkPrevAllocBit(perfect->header) ,0);
            nextBlock(perfect)->header = changePrevAllocHEader(nextBlock(perfect)->header, 1);
            aggregatePay += getPayloadSz(perfect->header);
            return (void*) perfect+16;
        }
    }else{
        sf_size_t perfectSz = getHeaderBlockSize(perfect->header);
        if(perfectSz > padded){
            if(perfectSz - padded >= 32){
                splitBlock(perfect, padded, tempSize);
                            // sf_show_heap();
                aggregatePay += getPayloadSz(perfect->header);
                return (void*) perfect+16;
            }else if(perfectSz - padded < 32){ //there is a splinter but we still gotta change the footer to allocated now
                perfect->header = combineBits(tempSize,perfectSz, 1,checkPrevAllocBit(perfect->header) ,0);
                nextBlock(perfect)->header = changePrevAllocHEader(nextBlock(perfect)->header, 1);
                aggregatePay += getPayloadSz(perfect->header);
                return (void*) perfect+16;
            }    
        }else if(perfectSz == padded){
            perfect->header = combineBits(tempSize,perfectSz, 1,checkPrevAllocBit(perfect->header) ,0);
            nextBlock(perfect)->header = changePrevAllocHEader(nextBlock(perfect)->header, 1); 
            aggregatePay += getPayloadSz(perfect->header);
            return (void*) perfect+16;
        }
    }
        
    if(result == NULL){
        sf_errno = ENOMEM;
        return NULL;
    }
    
    return result;

}

int validatePointer(void* pp){
    if(pp == NULL || (uintptr_t)pp %16 != 0){
        return 0;
    }
    pp-=16;
    sf_block* temp = (sf_block*)pp;
    if(getHeaderBlockSize(temp->header) < 32 || getHeaderBlockSize(temp->header) % 16 !=0 ||
        checkAllocBit(temp->header) == 0 ||
     ((checkPrevAllocBit(temp->header) == 0)&& (checkAllocBit(prevBlock(temp)->header) != 0 ))){
        return 0;
    }
    if(pp < sf_mem_start() + 32 || pp > sf_mem_end()-16){
        return 0;
    }

    return 1;
}

void sf_free(void *pp) {
    pp -= 16;
   if(validatePointer(pp+16) == 0){
       abort();
   }
   sf_block* block =(sf_block*) pp;
   if(putInQuick(getHeaderBlockSize(block->header), block) == 0){
       putInFreeList(getHeaderBlockSize(block->header), block);
       coalesce(block);
   }
}

//rsize is the new size 
void *sf_realloc(void *pp, sf_size_t rsize) {
        
    pp -=16;
    if(validatePointer(pp+16) == 0){
        sf_errno =EINVAL;
        return NULL;
    }

    if(rsize == 0){
        sf_free(pp+16);
        return NULL;
    }

    sf_block* block = (sf_block*)pp;
    sf_size_t oldSize = getHeaderBlockSize(block->header);
sf_size_t payload = getPayloadSz(block->header);
    if(rsize > payload){
        if(oldSize-8 >= rsize){
            return pp+16;
        }

        
        void* newBlock = sf_malloc(rsize);
        newBlock -=16;
        if(newBlock == NULL){
            return NULL;
        }
        
        memcpy(newBlock+16, pp+16, payload);
        block->header = changePayloadHeader(block->header, payload);

        sf_free(pp+16);
      
        return (newBlock+16);
    }else if(rsize < payload){
        if(payload - rsize < 32){ //32 bytes 
            block->header = changePayloadHeader(block->header, rsize);
            return pp+16;
        }else{
            // sf_show_heap();
            sf_size_t padded = 0;
            sf_size_t size = rsize +8;
            if(size < 32){
                padded = 32;

            }else if(size == 32){
                padded = 32;
        
            }else{
                if(size %16 != 0){
                    int divided = size/16;
                    divided++;
                    padded = divided*16;
                }else{
                    padded = size;
                }
            }
            splitBlock(block, padded, rsize);
            block->header = changeAllocHeader(block->header, 1);
            return block+16;
        }
    }
    return pp+16;
}

double sf_internal_fragmentation() {
    double payload = 0.0;
    double totalSz = 0.0;
    sf_block* current = nextBlock((sf_block*) (sf_mem_start()));
    while(getHeaderBlockSize(current->header) != 0){
        if(checkAllocBit(current->header) == 1 && getInQuickList(current->header) == 0){
            totalSz += getHeaderBlockSize(current->header);
            payload += getPayloadSz(current->header);
        }
        current = nextBlock(current);
    }
    return payload/totalSz;
}

double sf_peak_utilization() {
    if((char *)sf_mem_start() == (char *)sf_mem_end()){
        return 0.0;
    }
    double heapSize = (double)((char *)sf_mem_end() - (char *)sf_mem_start());

    return aggregatePay/heapSize; 
}
