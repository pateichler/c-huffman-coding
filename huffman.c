#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HUFF_BUFF_SIZE 1024

typedef struct Node Node;

struct Node{
    char item;
    float weight;
    Node *left;
    Node *right;
};

typedef struct QueueElement
{
    Node *node;
    struct QueueElement *next;
} QueueElement;

typedef struct Queue{
    QueueElement *head;
    int size;
} Queue;

/// @brief Strucutre to represent bits of variable length
typedef struct BitCode
{
    unsigned int code;
    unsigned char numBits;
} BitCode;

typedef struct DecodeItem
{
    char item;
    BitCode code;
} DecodeItem;

typedef struct IOFile
{
    FILE *inputFile;
    FILE *outputFile;
} IOFile;

IOFile getIOFile(char *fileInputName, char *fileOutputName){
    FILE *inputFile = fopen(fileInputName, "r");
    if (inputFile == NULL){
        fprintf(stderr, "Could not open input file: %s\n", fileInputName);
        return (IOFile) {NULL,NULL};
    }
        

    FILE *outputFile = fopen(fileOutputName, "wb");
    if(outputFile == NULL){
        fclose(inputFile);

        fprintf(stderr, "Could not open input file: %s\n", fileOutputName);
        return (IOFile) {NULL,NULL};
    }

    return (IOFile) {inputFile, outputFile};
}

void closeIOFile(IOFile ioFile){
    fclose(ioFile.inputFile);
    fclose(ioFile.outputFile);
}

void print_bin(BitCode code)
{
    while(code.numBits--)
        putchar('0' + ((code.code >> code.numBits) & 1));
}


void add_queue(Queue *queue, Node *node){
    QueueElement *newElement = (QueueElement *) malloc(sizeof(QueueElement)); 
    if(newElement == NULL)
        return;

    newElement->node = node;
    newElement->next = NULL;

    queue->size++;

    // If no elements in queue, set element as first    
    if(queue->head == NULL){
        queue->head = newElement;
        return;
    }
    
    QueueElement *current = queue->head;
    QueueElement *previous = NULL;

    while (current != NULL){
        if(node->weight < current->node->weight){
            if(previous != NULL){
                previous->next = newElement;
            }
            else{
                queue->head = newElement;
            }
                
            newElement->next = current;
            return;
        }

        previous = current;
        current = current->next;
    }

    previous->next = newElement;
}

Node *popQueue(Queue *queue){
    QueueElement *h = queue->head;

    if(h == NULL)
        return NULL;
    
    Node *node = h->node;
    
    queue->head= h->next;
    queue->size--;

    free(h);

    return node;
}

Queue *create_queue(char *filename){
    FILE* stream = fopen(filename, "r");

    if (stream == NULL){
        fprintf(stderr, "Could not find file!\n");
        return NULL;
    }

    Queue *queue = (Queue *) malloc(sizeof(Queue)); ;
    if(queue != NULL){
        char line[HUFF_BUFF_SIZE];
        while (fgets(line, sizeof(line), stream) != NULL)
        {
            //TODO: Make reading CSV more resillent to spaces and other
            char* tmp = strdup(line);
            char c = tmp[0];
            float val = strtof(tmp + 2, NULL);

            Node *newNode = (Node *) malloc(sizeof(Node)); 
            
            if (newNode == NULL)
                return NULL;

            *newNode = (Node){c, val, NULL, NULL};
            add_queue(queue, newNode);
            
            free(tmp);
        }
    }

    fclose(stream);
    return queue;
}

Node *create_node_tree(Queue *queue){
    while(queue->size > 1){
        Node *node1 = popQueue(queue);
        Node *node2 = popQueue(queue);

        Node *parentNode = (Node *) malloc(sizeof(Node));
        *parentNode = (Node){'_', node1->weight + node2->weight, node1, node2};
        add_queue(queue, parentNode);
    }

    return popQueue(queue);
}

void set_decode_table(Node *curNode, DecodeItem **ptr_item, int val, int numBits){
    if(curNode->left == NULL || curNode->right == NULL){
        **ptr_item = (DecodeItem){curNode->item, (BitCode){val, numBits}};
        (*ptr_item)++;
        return;
    }

    int valShift = val << 1;
    set_decode_table(curNode->left, ptr_item, valShift, numBits+1);
    set_decode_table(curNode->right, ptr_item, valShift+1, numBits+1);
}

DecodeItem *create_decode_table(Node *rootNode, int size){
    DecodeItem *table = (DecodeItem *)malloc((size+1) * sizeof(DecodeItem));
    table[size] = (DecodeItem){'\0', {0,0}};
    
    if(table == NULL)
        return table;

    DecodeItem *ptr_item = table;
    set_decode_table(rootNode, &ptr_item, 0, 0);

    return table;
}

BitCode get_decode_item(DecodeItem *table, char item){
    while(table->item != '\0'){
        if (table->item == item) 
            return table->code;
        
        table++;
    }
    
    fprintf(stderr, "No decode item found! Attempted to get character: %c\n", item);
    return (BitCode) {0,0};
}

/// @brief Peeks next character in file stream
/// @param stream the open file stream
/// @return next character casted as an int
int peekNextChar(FILE *stream)
{
    int c;
    c = fgetc(stream);
    ungetc(c, stream);

    return c;
}

void add_char_to_buffer(char *buffer, char data, unsigned int *buffBytePos, FILE *file){
    buffer[*buffBytePos] = data;

    if(++(*buffBytePos) == HUFF_BUFF_SIZE){
        fwrite(buffer, HUFF_BUFF_SIZE, 1, file);
        //TODO: Find better way of setting
        memset(buffer, 0, sizeof(char) * HUFF_BUFF_SIZE);
        *buffBytePos = 0;
    }
}

int decode_file(IOFile ioFile, Node* rootNode){
    unsigned char inputBuffer[HUFF_BUFF_SIZE];
    char outputBuffer[HUFF_BUFF_SIZE];

    unsigned int outBuffPos = 0;
    Node *curNode = rootNode;

    int endBits = fgetc(ioFile.inputFile);

    size_t readLength = HUFF_BUFF_SIZE;
    while (readLength == HUFF_BUFF_SIZE)
    {
        readLength = fread(inputBuffer, sizeof(char), HUFF_BUFF_SIZE, ioFile.inputFile);
        int nextChar = peekNextChar(ioFile.inputFile);

        if(ferror(ioFile.inputFile) != 0){
            fprintf(stderr, "Could not read file!\n");
            closeIOFile(ioFile);
            return 1;
        }
        
        for(int c = 0; c < readLength; c++){
            unsigned char curChar = inputBuffer[c];
            
            int numBits = nextChar == EOF && (c == readLength - 1) ? endBits : 8;
            for(int b = 0; b < numBits; b++){
                curNode = (curChar & (unsigned char)128) == 0 ? curNode->left : curNode->right;
                curChar <<= 1;
                
                //Probably want to check for null char on item in future
                if(curNode->left == NULL || curNode->right == NULL){
                    add_char_to_buffer(outputBuffer, curNode->item, &outBuffPos, ioFile.outputFile);
                    curNode = rootNode;
                }
            }
        }
    }

    // Flush out remaining buffer
    if (outBuffPos > 0)
        fwrite(outputBuffer, outBuffPos, 1, ioFile.outputFile);


    closeIOFile(ioFile);
    return 0;
}

void add_bit_code_to_buffer(char *outputBuffer, BitCode *data, unsigned int *buffBytePos, unsigned char *outBuffBitPos){
    if(data->numBits <= 0)
        return;

    while (data->numBits > 0 && *buffBytePos < HUFF_BUFF_SIZE)
    {
        char diff = data->numBits - (8 - *outBuffBitPos);
        char rightShift = diff > 0 ? diff : 0;

        char copyBits = diff < 0 ? data->numBits : (8 - *outBuffBitPos);
        
        int mask = (1 << (rightShift + copyBits)) - 1;

        unsigned int tmp = (data->code & mask) >> rightShift;
        
        outputBuffer[*buffBytePos] <<= copyBits;
        outputBuffer[*buffBytePos] |= tmp;
        
        data->numBits -= copyBits;
        *outBuffBitPos += copyBits;

        // Reached end of byte ... go to next byte
        if(*outBuffBitPos == 8){
            *outBuffBitPos = 0;
            *buffBytePos += 1;
        }
    }
}

int encode_file(IOFile ioFile, DecodeItem *decodeTable){
    char inputBuffer[HUFF_BUFF_SIZE];
    char outputBuffer[HUFF_BUFF_SIZE] = {0};

    unsigned int outBuffByte = 0;
    unsigned char outBuffBit = 0;

    // Add blank metadata for number of bits at the end
    fwrite(&outBuffBit, 1, 1, ioFile.outputFile);

    size_t readLength = HUFF_BUFF_SIZE;
    while (readLength == HUFF_BUFF_SIZE)
    {
        readLength = fread(inputBuffer, sizeof(char), HUFF_BUFF_SIZE, ioFile.inputFile);

        if(ferror(ioFile.inputFile) != 0){
            fprintf(stderr, "Could not read file!\n");
            closeIOFile(ioFile);
            return 1;
        }
        
        for(int c = 0; c < readLength; c++){
            BitCode b = get_decode_item(decodeTable, inputBuffer[c]);
            if(b.numBits == 0){
                fprintf(stderr, "Character %c is not defined!\n", inputBuffer[c]);
                closeIOFile(ioFile);
                return 1;
            }

            add_bit_code_to_buffer(outputBuffer, &b, &outBuffByte, &outBuffBit);
            
            // Flush out buffer ... and write remaining bit code if not finished
            if(outBuffByte == HUFF_BUFF_SIZE){
                fwrite(outputBuffer, HUFF_BUFF_SIZE, 1, ioFile.outputFile);
                outBuffByte = 0;
                outBuffBit = 0;
                memset(outputBuffer, 0, sizeof(outputBuffer));

                add_bit_code_to_buffer(outputBuffer, &b, &outBuffByte, &outBuffBit);
            }
        }
    }

    // Flush out remaining buffer
    if(outBuffByte > 0 || outBuffBit > 0){
        // Shift over last byte
        outputBuffer[outBuffByte] <<= (8 - outBuffBit);
        
        fwrite(outputBuffer, (size_t) (outBuffByte+1), 1, ioFile.outputFile);
    }

    // Add metadata for number of bits at the end
    fseek(ioFile.outputFile, 0, SEEK_SET);
    fwrite(&outBuffBit, 1, 1, ioFile.outputFile);

    closeIOFile(ioFile);
    return 0;
}

int main(void){

    //TODO: This program can still be optimized in a number of ways
    // Some points to improve:
    // - Clean up queue memory allocation & make sure everything is deleted
    // - Change lookup table to use a binary tree
    // - Seperate parts into sepearte C files for better organization

    // Some features that would be nice:
    // - Add command line arguments to be able to execute this code from the command line
    // - Store lookup table as meta data in the encoded file
    // - Add raw character code abillity for characters not found?


    Queue *queue = create_queue("charDistribution.csv");
    
    if(queue == NULL){
        printf("Queue could not be created!");
        return 1;
    }

    int numElements = queue->size;
    
    Node* rootNode = create_node_tree(queue);
    free(queue);

    DecodeItem *decodeTable = create_decode_table(rootNode, numElements);

    if(decodeTable == NULL){
        printf("Decode table could not be constructed!");
        return 1;
    }

    IOFile ioEncode = getIOFile("testEncode.txt", "testOutput.huf");
    encode_file(ioEncode, decodeTable);
    
    IOFile ioDecode = getIOFile("testOutput.huf", "testDecode.txt");
    decode_file(ioDecode, rootNode);
}