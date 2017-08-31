
#include "board.h"
#include "FreeRTOS.h"
#include "task.h"
#include "string.h"
#include <stdio.h>
#include "GHMatrix.h"

#define PATTERNSIZE       32
#define BUFFERARRAYSIZE   2000
#define PATTERNSNEEDED     4
#define PIN_OUTPUT         2
#define PIN_INPUT          3
#define PIN_SWITCH        21
#define RATE              500 //1000Hz
#define ORDER              5
#define N                 12
#define K                  8
#define LENGTH             3


unsigned char* message = "21SJSU";
unsigned char* ackMessage = "21Acknowledged.";
int scrambleOrder, descrambleOrder;
unsigned char source, destination;
unsigned char unscrambledPayload[PATTERNSIZE*8];
uint32_t bufferArray[BUFFERARRAYSIZE];
unsigned char twelveBits[12];
unsigned char syndromeWord[4];
unsigned char payLoadToSend[PATTERNSIZE*8];
unsigned char payLoad[PATTERNSIZE];
unsigned char encodedPayload[LENGTH * (8+4)];
unsigned char C[N] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
unsigned char subArr[K] = {0, 0, 0, 0, 0, 0, 0, 0};
unsigned char endSequence[32] = {1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1};
unsigned char patternField[PATTERNSIZE] = { 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
											0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF };
unsigned char syncField[PATTERNSIZE*8] =    {0, 1, 0, 1, 0, 0, 0, 0,
											0, 1, 0, 1, 0, 0, 0, 1,
											0, 1, 0, 1, 0, 0, 1, 0,
											0, 1, 0, 1, 0, 0, 1, 1,
											0, 1, 0, 1, 0, 1, 0, 0,
											0, 1, 0, 1, 0, 1, 0, 1,
											0, 1, 0, 1, 0, 1, 1, 0,
											0, 1, 0, 1, 0, 1, 1, 1,
											0, 1, 0, 1, 1, 0, 0, 0,
											0, 1, 0, 1, 1, 0, 0, 1,
											0, 1, 0, 1, 1, 0, 1, 0,
											0, 1, 0, 1, 1, 0, 1, 1,
											0, 1, 0, 1, 1, 1, 0, 0,
											0, 1, 0, 1, 1, 1, 0, 1,
											0, 1, 0, 1, 1, 1, 1, 0,
											0, 1, 0, 1, 1, 1, 1, 1,
											1, 0, 1, 0, 0, 0, 0, 0,
											1, 0, 1, 0, 0, 0, 0, 1,
											1, 0, 1, 0, 0, 0, 1, 0,
											1, 0, 1, 0, 0, 0, 1, 1,
											1, 0, 1, 0, 0, 1, 0, 0,
											1, 0, 1, 0, 0, 1, 0, 1,
											1, 0, 1, 0, 0, 1, 1, 0,
											1, 0, 1, 0, 0, 1, 1, 1,
											1, 0, 1, 0, 1, 0, 0, 0,
											1, 0, 1, 0, 1, 0, 0, 1,
											1, 0, 1, 0, 1, 0, 1, 0,
											1, 0, 1, 0, 1, 0, 1, 1,
											1, 0, 1, 0, 1, 1, 0, 0,
											1, 0, 1, 0, 1, 1, 0, 1,
											1, 0, 1, 0, 1, 1, 1, 0,
											1, 0, 1, 0, 1, 1, 1, 1};

static void prvSetupHardware(void){

	SystemCoreClockUpdate();
	Board_Init();
	Board_LED_Set(0, true);

	Chip_GPIO_SetPinDIR(LPC_GPIO, 0, PIN_OUTPUT, 1); //makes P0.2 an output
	Chip_GPIO_SetPinDIR(LPC_GPIO, 0, PIN_SWITCH, 0); //sets P0.21 an input/receive
	Chip_GPIO_WritePortBit(LPC_GPIO, 0, PIN_OUTPUT, false); //makes P0.2 a '0' initially
	Chip_GPIO_SetPinDIR(LPC_GPIO, 0, PIN_INPUT, false); //makes P0.3 an input/receive
	//Chip_GPIO_WritePortBit(LPC_GPIO, 0, PIN_SWITCH, false); //sets P0.21 as low to begin with
}

static void sendActionTask(void* pvParameters){
	int signal = Chip_GPIO_ReadPortBit(LPC_GPIO, 0, PIN_SWITCH); //sets signal to pin value

	while(1){ //constantly reading if switch is activated
			signal = Chip_GPIO_ReadPortBit(LPC_GPIO, 0, PIN_SWITCH); //sets signal to pin value

			if(signal != Chip_GPIO_ReadPortBit(LPC_GPIO, 0, PIN_SWITCH)){ //if signal ever changes
				signal = Chip_GPIO_ReadPortBit(LPC_GPIO, 0, PIN_SWITCH); //sets signal again
				printf("Signal: %i\n", signal);
				sendMessage(message); //send message only when signal changes
			}
			else;
	}
}

int payLoadFill(char* input){ //converts the string into binary to store into payLoadToSend buffer

	//printf("Sending the payload: %s\n", input);
	int size = 0;

	//storing payload into payLoadToSend[]
	while(*input){ //for every char of the input?
		int output = *input; //makes the string into an int value?

		for(int i = 7; i >= 0 ; i--){
			payLoadToSend[size * 8 + i] = output & 0b1; //gets binary of each char and stores it into payLoadToSend array
			//printf("Stored %i\n", (output & 0b1));
			//printf("size: %i , index i: %i\n", size, i);
			output = output >> 1; //shifts each bit of each char?
		}
		input++; //shifts each char
		size++; //for the for loop index
		//printf("size: %i\n", size);
	}

	//printf("Returning size: %i to sendMessage()\n", size);
	return size;
}

/*void send(unsigned char* input, int size){ //actually changing the output of the pin, given the arrays and size
	for(int index = 0; index < size; index++){
		Chip_GPIO_WritePortBit(LPC_GPIO, 0 , PIN_OUTPUT, input[index]);
		vTaskDelay(configTICK_RATE_HZ/RATE);
		//printf("Sending bit: %i \n", input[index]);
	}
}

void receive(uint32_t* input, int size){
	for(int index = 0; index < size; index++){
		input[index] = Chip_GPIO_ReadPortBit(LPC_GPIO, 0, PIN_INPUT);
		//printf("Receiving bit: %i \n", input[index]);
		vTaskDelay(configTICK_RATE_HZ/RATE);
	}
}*/


void printAll(unsigned char arr[], int size, int index){
	for(int i = index; i < index+size; i++){
		vTaskDelay(configTICK_RATE_HZ/100);
		printf("%i ", arr[i]);
	}
	printf("\n");
}


void shiftRight(unsigned char arr[], int size, unsigned char firstBit){
    for(int i = size; i > 0; i--){
        arr[i] = arr[i-1];
    }
    arr[0] = firstBit;
}

void initializeArr(unsigned char arr[], int size){
    for(int i = 0; i < size; i++){
        arr[i] = 0;
    }
}

void scramble(unsigned char arr[], int size, int order){
    unsigned char buffer[order];
    unsigned char newPayload[size];
    initializeArr(buffer, order);
    initializeArr(newPayload, size);

    for(int j = 0; j < size; j++){ //going through payload
        unsigned char xor1 = buffer[(order/2)] ^ buffer[order - 1];
        unsigned char xor2 = arr[j] ^ xor1;
        newPayload[j] = xor2;
        shiftRight(buffer, order, xor2); //shifts to the right and adds xor2
    }
    //setting newPayload into arr
    for(int k = 0; k < size; k++){
        arr[k] = newPayload[k];
    }

}

void deScramble(unsigned char arr[], int size, int order){
    unsigned char buffer[order];
    unsigned char newPayload[size];
    initializeArr(buffer, order);
    initializeArr(newPayload, size);

    for(int k = 0; k < size; k++){
    	vTaskDelay(configTICK_RATE_HZ/100);
        unsigned char xor1 = buffer[(order/2)] ^ buffer[order - 1];
        unsigned char xor2 = arr[k] ^ xor1;
        shiftRight(buffer, order, arr[k]);
        newPayload[k] = xor2;
    }

    for(int k = 0; k < size; k++){
        arr[k] = newPayload[k];
    }
}



void encode(unsigned char originalPayload[], int sizeOriginal){
	initializeArr(encodedPayload, LENGTH*(8+4)); //make sure they're all 0s
	int encodedIndex = 0;
	int subArrIndex = 0;


	//make every 8 bits turn into 12 bit coded messages
	for(int i = 0; i <= sizeOriginal; i++){ //goes through each bit of the payLoadToSend

		if(subArrIndex == 8){ //at the eighth bit
			//printf("subArr: ");
			//printAll(subArr, 8, 0);
			turnIntoC(subArr); //uses subArr's 8 bits to turn it into C's 12 bits
			//printf("C: ");
			//printAll(C, 12, 0);
			storeIntoEncodedPayload(C, N, encodedIndex); //saves C's 12 bits into encodedPayload
			encodedIndex += N;
			subArrIndex = 0; //resets index for subarr
			i--;
		}
		else{
			subArr[subArrIndex] = originalPayload[i]; //stores it into the subArr
			//printf("subArr[%i]: %i, payLoad[%i]: %i\n", subArrIndex, subArr[subArrIndex], i, originalPayload[i]);
			subArrIndex++;
		}
	}
}

void turnIntoC(unsigned char arr[]){
	initializeArr(C, N); //makes sure C are all 0's

	for(int i = 0; i < N; i++){
		for(int j = 0; j < K; j++){
			C[i] = C[i] + (arr[j] * G[j*N+i]); //gives the C_1,n from subArr_1,k
		}
		C[i] = C[i]%2;
	}
}

void storeIntoEncodedPayload(unsigned char arr[], int size, int index){
	for(int i = index, cIndex = 0; i < index+size; i++, cIndex++){
		//printf("%i\n", i);
		encodedPayload[i] = arr[cIndex];
	}
}

/*void splitIntoPayload(unsigned char arr[], int size){
	initializeArr(fourBits, 4);
	initializeArr(eightBits, 8);
	int fourBitIndex = 0;

	for(int i = 0; i < size; i++){
		if(i < 8){
			eightBits[i] = arr[i];
			printf("eightBits[%i]: %i\n", i, eightBits[i]);
		}
		else if(i >= 8){
			fourBits[fourBitIndex] = arr[i];
			printf("fourBits[%i]: %i\n", fourBitIndex, fourBits[fourBitIndex]);
			fourBitIndex++;
		}
	}
}*/




void sendMessage(char* someMessage){ //sends pattern then payload

	int size = payLoadFill(someMessage);
	scrambleOrder = 1;
	printf("What order would you like to scramble in?: ");
	scanf("%d", &scrambleOrder);


	scramble(payLoadToSend, size*8, scrambleOrder);
	//printAll(payLoadToSend, size*8, 0);
	encode(payLoadToSend, size*8);
	//printAll(encodedPayload, size*(8+4), 0);



	//printf("Sending pattern...\n");
	//send(syncField, PATTERNSIZE*8);
	//printf("Sending payLoad...\n");
	//send(payLoadToSend, size*8);
	//printf("Sending endSequence...\n");
	//send(endSequence, 16);

	for(int index = 0; index < PATTERNSIZE*8; index++){
			Chip_GPIO_WritePortBit(LPC_GPIO, 0 , PIN_OUTPUT, syncField[index]);
			vTaskDelay(configTICK_RATE_HZ/RATE);
			//printf("Sending bit: %i \n", input[index]);
		}

	for(int index = 0; index < size*(8+4); index++){
			Chip_GPIO_WritePortBit(LPC_GPIO, 0 , PIN_OUTPUT, encodedPayload[index]);
			vTaskDelay(configTICK_RATE_HZ/RATE);
			//printf("Sending bit: %i \n", encodedPayload[index]);
		}

	for(int index = 0; index < 32; index++){
			Chip_GPIO_WritePortBit(LPC_GPIO, 0 , PIN_OUTPUT, endSequence[index]);
			vTaskDelay(configTICK_RATE_HZ/RATE);
			//printf("Sending bit: %i \n", input[index]);
		}


	printf("Message sent.\n\n");
	Chip_GPIO_WritePortBit(LPC_GPIO, 0, PIN_OUTPUT, false);
}

static void bufferFill(void* pvParameters){

	int found = false;
	while(!found){

		//filling in the buffer with infos.
		printf("Filling buffer...\n");
		for(int index= 0; index < BUFFERARRAYSIZE; index++){
			bufferArray[index] = Chip_GPIO_ReadPortBit(LPC_GPIO, 0, PIN_INPUT);
			//printf("Receiving bit: %i \n", input[index]);
			vTaskDelay(configTICK_RATE_HZ/RATE);
		}
		//receive(bufferArray, BUFFERARRAYSIZE);
		printf("Buffer full! Checking buffer for sync window..\n");

		found = bufferCheck(); //bufferCheck returns true if found, false if not found
		if(found){
			printf("Message found.\n");
			printf("Source: %c Destination: %c\n", source, destination);
			printf("Payload = %s \n\n", payLoad);
			clearBuffer();
			if(source != 2){
				printf("Sending acknowledgement...\n");
				sendMessage(ackMessage);
			}else;

			found = false; //turns found back to false to reset value
		}else{
			printf("No message found.\n\n");
		}
	}
}

void clearBuffer(){
	for(int payLoadIndex = 0; payLoadIndex < PATTERNSIZE; payLoadIndex++){
		payLoad[payLoadIndex] = 0;
	}
	printf("Payload cleared.\n");
}

int bufferCheck(){

	unsigned char subBufferSection = 0x0;
	int counter = 0;
	int bufferIndex, bitIndex;

	//go through buffer size
	for(bufferIndex = 0; bufferIndex <= (BUFFERARRAYSIZE - PATTERNSIZE*8); bufferIndex++){  //shifts through buffer bit by bit and copies it into subbuffer byte

		counter = 0; //resets counter until there are consecutive patterns
		//go through byte
		for(bitIndex = 0; bitIndex < (PATTERNSIZE*8); bitIndex++){
			subBufferSection = (subBufferSection << 1) | bufferArray[bufferIndex + bitIndex];

			if (bitIndex % 8 == 7) {
				if (patternField[bitIndex/8] == subBufferSection){ //Copies it to subbuffer bit by bit until byte
					//printf("Pattern: %x subBufferSection: %x\n", patternField[bitIndex/8], subBufferSection);
					counter++; //adds 1 to counter when pattern in patternField == subBufferSection
					//printf("counter is now: %i\n", counter);
				}
				if (counter == PATTERNSNEEDED){ //if counter met with patterns needed, then it will break from bitIndex loop
					break;
				}
				subBufferSection = 0x0; //resets the subBufferSection to 0x0
			}
		}
		if(counter == PATTERNSNEEDED){ //break from bufferIndex if counter is = to patterns needed
			break;
		}
	}

	//calculate where payload will start
	if (counter >= PATTERNSNEEDED)
		{
			int payloadStart = bufferIndex + bitIndex; //current bit in buffer
			printf("Current byte read is %x at index %i. \n", subBufferSection, payloadStart);
			unsigned char marker = subBufferSection & 0xF; //looks at the right hand byte
			int amount = (15-marker)*8 + 1; //looks at the number marker, and adds the needed amount required

			int spacing = 0;
			spacing += ((subBufferSection & 0x50) == 0x50) ? (128 + amount) : amount; //if it's a 5 first, then add an additional 128, if not, then just amount

			printf("Start of payload is %i bits ahead. \n", spacing);
			payloadStart += spacing; //index at which the payload should be read from
			printf("Payload starts at index %i \n\n", payloadStart);


			//
			int bitIndex = 0;
			int payLoadIndex = 0;
			int unscrambleBit = 0;


			//printf("subBufferSection: %x before while loop\n", subBufferSection);
			subBufferSection = 0x0;
			//printf("subBufferSection: %x before while loop, after change\n", subBufferSection);
			while (subBufferSection != 0x99){
				//printf("subBufferSection: %x inside while loop\n", subBufferSection);
				vTaskDelay(configTICK_RATE_HZ/100);
				if(payloadStart > BUFFERARRAYSIZE-1){
					printf("Payload cuts off at end of buffer -OR- no end sequence found :( \n");
					break;
				}
				subBufferSection = (subBufferSection << 1) | bufferArray[payloadStart];
				unscrambledPayload[unscrambleBit] = bufferArray[payloadStart]; //stores everything in as bits
				unscrambleBit++;
				if (bitIndex % 8 == 7){ //changed so that it would copy in 12 bits instead of 8
					//printf("subBufferSection: %x\n", subBufferSection);
					if(subBufferSection == 0x99){
						printf("\nFound end sequence: %x\n", subBufferSection);
						break;
					}
					//printf("Adding %x to payload at index %i. \n", subBufferSection, index);
					//payLoad[payLoadIndex] = subBufferSection;
					//printf("payLoad[%i] =%x\n", payLoadIndex, subBufferSection);
					//payLoadIndex++;
					subBufferSection = 0x0;
				}
				bitIndex++;
				payloadStart++;
			}
			unscrambleBit = unscrambleBit -12;
			printf("unscrambleBit: %i\n", unscrambleBit);

			//printAll(unscrambledPayload, unscrambleBit, 0);
			int variableIndex = 0;
			for(int i = 0; i < unscrambleBit; i++){
				vTaskDelay(configTICK_RATE_HZ/100);
				twelveBits[i%12] = unscrambledPayload[i];
				if(i%12 == 11){
					initializeArr(syndromeWord, 4);
					//printAll(twelveBits, 12, 0);
					for(int k = 0; k < (N-K); k++){
						for(int j = 0; j < N; j++){
							syndromeWord[k] = (syndromeWord[k] + twelveBits[j] * H[4*j+k]) % 2;
						}
					}
					printf("syndromeWord: ");
					printAll(syndromeWord, 4, 0);
					//initializeArr(syndromeWord, 4);
				}
				if(i%12 < 8){
					unscrambledPayload[variableIndex] = twelveBits[i%12];
					variableIndex++;
				}
			}
			while(variableIndex < unscrambleBit){
					vTaskDelay(configTICK_RATE_HZ/100);
					unscrambledPayload[variableIndex] = 0;
					variableIndex++;
				}

			//printAll(unscrambledPayload, variableIndex, 0);
			//printf("BitIndex: %d\n", bitIndex);
			printf("Payload recieved.\n");
			printf("Payload = %s \n\n", payLoad);
			//printf("What order would you like to descamble?");
			//
			//printf("Descrambling...\n");

			source = 0x0;
			destination = 0x0;
			descrambleOrder = 1;
			printf("What order would you like to descramble in?: ");
			scanf("%d", &descrambleOrder);
			//unscrambleBit -= 16;
			deScramble(unscrambledPayload, variableIndex*2/3, descrambleOrder);

			subBufferSection = 0x0;
			int index = 0;
			//unscrambleBit = unscrambleBit * 2 / 3;
			for(int i = 0, index = 0; i < variableIndex; i++){
				subBufferSection = (subBufferSection << 1) | unscrambledPayload[i];
				if(i%8 == 7){

					//printf("subBufferSection: %c\n", subBufferSection);

					if(index == 0){
						source = subBufferSection;
						subBufferSection = 0x0;
						index++;
					}
					else if(index == 1){
						destination = subBufferSection;
						subBufferSection = 0x0;
						index++;
					}
					else{
						payLoad[index-2] = subBufferSection;
						subBufferSection = 0x0;
						index++;
					}
					//printf("payLoad[%d] =%x\n", index, subBufferSection);

				}
			}
			//printf("Returned true from bufferCheck()\n");
			return true;
		}
		printf("Returned false from bufferCheck()\n");
		return false;
}


int main(void)
{
	prvSetupHardware();

	xTaskCreate(sendActionTask, (signed char *) "sendActionTask",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
				(xTaskHandle *) NULL);

	xTaskCreate(bufferFill, (signed char *) "bufferFill",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
				(xTaskHandle *) NULL);



	/* Start the scheduler of vTasks */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}
